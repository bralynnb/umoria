// SPDX-License-Identifier: GPL-3.0-or-later
// One authoritative engine, two stackful input coroutines. No engine threads:
// a command runs serially until an explicit input/scheduler boundary.
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include "headers.h"

TreasureHeap coop_treasure;
static CoopSession sessions[2];
static int active=0, epoch=0;
static bool initialized=false, generated=false;
static ucontext_t scheduler;
static int shop_owner[6]={-1,-1,-1,-1,-1,-1};
static uint32_t seed=0, town_seed=0, magic_seed=0;
static uint32_t session_clock=1700000000;
uint32_t coopClock() {return session_clock;}

CoopSession &coopSession() {return sessions[active];}
int coopSlot() {return active;}
static void selectSession(int slot) {
    sessions[active].panel=dg.panel;
    active=slot;
    dg.panel=sessions[active].panel;
}
static bool living(int id) {
    return sessions[id].joined && !sessions[id].finished && !sessions[id].state.character_is_dead;
}
bool coopOtherAt(Coord_t c) {
    int other=1-active;
    return living(other) && sessions[other].player.pos.y==c.y && sessions[other].player.pos.x==c.x;
}
static void yield() {
    sessions[active].panel=dg.panel;
    swapcontext(&sessions[active].context,&scheduler);
}
static void redraw() {
    if(!living(active) || !sessions[active].map_input) return;
    // Preserve prompt and message history; map refresh must never ask for input.
    auto top=sessions[active].window;
    auto rng_state=getRandomSeed();
    message_ready_to_print=false;
    coordOutsidePanel(py.pos,true);
    updateMonsters(false);
    drawDungeonPanel();
    printCharacterStatsBlock();
    printCharacterCurrentDepth();
    std::copy(top.cells.begin(),top.cells.begin()+80,sessions[active].window.cells.begin());
    setRandomSeed(rng_state);
}
static Coord_t freeNear(Coord_t center) {
    for(int r=1;r<MAX_WIDTH;++r)
        for(int y=std::max(1,center.y-r);y<=std::min(int(dg.height)-2,center.y+r);++y)
            for(int x=std::max(1,center.x-r);x<=std::min(int(dg.width)-2,center.x+r);++x)
                if(dg.floor[y][x].feature_id<=MAX_OPEN_SPACE && dg.floor[y][x].creature_id==0 && dg.floor[y][x].treasure_id==0)
                    return {y,x};
    throw std::runtime_error("No free spawn tile");
}
bool coopBegin() {
    bool first=!initialized;
    initialized=true;
    if(!first) { game.town_seed=town_seed; game.magic_seed=magic_seed; }
    return first;
}
void coopWorldReady() {town_seed=game.town_seed;magic_seed=game.magic_seed;}
void coopJoinDungeon() {
    if(!generated) {
        town_seed=game.town_seed; magic_seed=game.magic_seed;
        generateCave(); generated=true;
    } else {
        int other=1-active;
        py.pos=freeNear(sessions[other].player.pos);
        dg.floor[py.pos.y][py.pos.x].creature_id=1;
        dg.panel=sessions[other].panel;
    }
    sessions[active].joined=true;
    sessions[active].epoch=epoch;
}
void coopGenerateLevel() {
    generateCave();
    dg.floor[py.pos.y][py.pos.x].creature_id=1;
    ++epoch;
    sessions[active].epoch=epoch;
    int other=1-active;
    if(living(other)) {
        sessions[other].player.pos=freeNear(py.pos);
        auto c=sessions[other].player.pos;
        dg.floor[c.y][c.x].creature_id=1;
        sessions[other].player.flags.rest=0;
        sessions[other].player.running_tracker=0;
        sessions[other].state.command_count=0;
        sessions[other].state.teleport_player=false;
        sessions[other].panel=dg.panel;
    }
}
char coopReadKey() {
    auto &s=sessions[active];
    while(s.input.empty()) yield();
    if(s.joined && s.epoch!=epoch) {
        s.epoch=epoch;s.input.clear();s.map_input=false;
        throw CoopLevelChanged{};
    }
    if(s.joined && s.state.character_is_dead) coopFinish();
    int key=s.input.front();s.input.pop_front();
    return char(key);
}
void coopTurnBoundary() {
    // Resting and running consume one turn per scheduler tick, preventing
    // unbounded fast-forward while the companion is thinking.
    if(py.flags.rest!=0 || py.running_tracker!=0 || game.command_count>0 || py.flags.paralysis>0) {
        sessions[active].map_input=true;
        yield();
        sessions[active].map_input=false;
        if(sessions[active].epoch!=epoch) {sessions[active].epoch=epoch;throw CoopLevelChanged{};}
        if(!sessions[active].input.empty()) {sessions[active].input.clear();playerDisturb(1,0);}
    }
}
[[noreturn]] void coopFinish() {
    auto &s=sessions[active];
    if(s.joined && dg.floor[py.pos.y][py.pos.x].creature_id==1) dg.floor[py.pos.y][py.pos.x].creature_id=0;
    s.finished=true;s.map_input=false;
    s.window.cells.fill(' ');
    std::string title=game.total_winner?"You defeated the Balrog!":game.character_is_dead?"Your adventure has ended.":"You have left the dungeon.";
    std::copy(title.begin(),title.end(),s.window.cells.begin()+80*8+15);
    std::string why=game.character_died_from;
    std::copy(why.begin(),why.end(),s.window.cells.begin()+80*10+15);
    for(;;) yield();
}
CoopTarget::CoopTarget(bool enabled,Coord_t coord,bool exact):previous(active) {
    if(!enabled) return;
    int chosen=active, best=10000;
    for(int i=0;i<2;++i) if(living(i)) {
        int dist=coordDistanceBetween(sessions[i].player.pos,coord);
        if((!exact || dist==0) && dist<best) {chosen=i;best=dist;}
    }
    selectSession(chosen);
}
CoopTarget::~CoopTarget() {selectSession(previous);}
CoopStoreGuard::CoopStoreGuard(int id):store(id),acquired(shop_owner[id]<0) {
    if(acquired) shop_owner[id]=active;
}
CoopStoreGuard::~CoopStoreGuard() {if(acquired) shop_owner[store]=-1;}
bool coopShopBusy() {for(int owner:shop_owner) if(owner>=0) return true;return false;}

static void entry() {
    terminalInitialize();
    startMoria(seed,true,false);
    coopFinish();
}
static std::string jsonString(const std::string &s) {
    std::string out="\"";
    const char *hex="0123456789abcdef";
    for(unsigned char c:s) {
        if(c=='"' || c=='\\') {out+='\\';out+=char(c);}
        else if(c<32 || c>=127) {out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
        else out+=char(c);
    }
    return out+'"';
}
static void emit() {
    int previous=active;
    std::cout<<"{\"depth\":"<<dg.current_level<<",\"turn\":"<<dg.game_turn<<",\"players\":[";
    for(int i=0;i<2;++i) {
        selectSession(i);redraw();
        auto &s=sessions[i];
        if(i)std::cout<<',';
        std::cout<<"{\"joined\":"<<(s.joined?"true":"false")<<",\"finished\":"<<(s.finished?"true":"false")
                 <<",\"automatic\":"<<((!s.finished && s.joined && (s.player.flags.rest || s.player.running_tracker || s.state.command_count || s.player.flags.paralysis))?"true":"false")
                 <<",\"name\":"<<jsonString(s.player.misc.name)<<",\"hp\":"<<s.player.misc.current_hp
                 <<",\"x\":"<<s.player.pos.x<<",\"y\":"<<s.player.pos.y<<",\"gold\":"<<s.player.misc.au
                 <<",\"screen\":"<<jsonString(std::string(s.window.cells.begin(),s.window.cells.end()))<<'}';
    }
    selectSession(previous);
    std::cout<<"]}"<<std::endl;
}
int main(int argc,char **argv) {
    if(argc>1) seed=uint32_t(std::strtoul(argv[1],nullptr,10));
    if(argc>2) session_clock=uint32_t(std::strtoul(argv[2],nullptr,10));
    std::string line;
    while(std::getline(std::cin,line)) {
        std::istringstream in(line); int slot,key;
        if(!(in>>slot>>key) || slot<0 || slot>1 || key<-2 || key>127) {emit();continue;}
        selectSession(slot);auto &s=sessions[slot];
        if(!s.started) {
            s.started=true;
            getcontext(&s.context);
            s.context.uc_stack.ss_sp=s.stack.data();
            s.context.uc_stack.ss_size=s.stack.size();
            s.context.uc_link=&scheduler;
            makecontext(&s.context,entry,0);
        }
        if(key>=0 && s.input.size()<32) s.input.push_back(key);
        if(!s.finished) swapcontext(&scheduler,&s.context);
        emit();
    }
    return 0;
}
