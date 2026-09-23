// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise real original engine commands through the two independent stacks.
#include <cassert>
#define main coopProtocolMain
#include "../src/coop.cpp"
#undef main

static void step(int slot,int key=-1) {
    selectSession(slot);auto &s=sessions[slot];
    if(!s.started) {
        s.started=true;getcontext(&s.context);
        s.context.uc_stack.ss_sp=s.stack.data();s.context.uc_stack.ss_size=s.stack.size();
        s.context.uc_link=&scheduler;makecontext(&s.context,entry,0);
    }
    if(key>=0) s.input.push_back(key);
    if(!s.finished) swapcontext(&scheduler,&s.context);
}
static void keys(int slot,const std::string &input) {for(unsigned char c:input) step(slot,c);}
static void arena() {
    // Fixture: safe empty town floor, no random creatures interfering with assertions.
    for(auto &row:dg.floor) for(auto &tile:row) tile={0,0,TILE_LIGHT_FLOOR,true,true,true,false};
    next_free_monster_id=config::monsters::MON_MIN_INDEX_ID;
    for(auto &m:monsters) m=Monster_t{};
    for(int i=0;i<2;++i) {
        sessions[i].player.pos={10,20+i*2};
        sessions[i].player.flags.food=10000;
        dg.floor[10][20+i*2].creature_id=1;
    }
    setRandomSeed(42);
}
int main() {
    seed=42;
    // Interleave creation: player two may finish before player one.
    step(0);step(1);
    keys(1," am\033aBob\n ");
    assert(sessions[1].joined);
    auto before=dg.floor[2][2].feature_id;
    keys(0," am\033aAlice\n ");
    assert(sessions[0].joined && dg.floor[2][2].feature_id==before);
    assert(std::string(sessions[0].player.misc.name)=="Alice");
    assert(std::string(sessions[1].player.misc.name)=="Bob");
    assert(sessions[0].player.pos.x!=sessions[1].player.pos.x || sessions[0].player.pos.y!=sessions[1].player.pos.y);
    assert(sessions[0].state.town_seed==sessions[1].state.town_seed);
    arena();
    step(0,'6');assert(sessions[0].player.pos.x==21);
    step(1,'4');assert(sessions[1].player.pos.x==22); // teammate blocks movement
    step(1,'6');assert(sessions[1].player.pos.x==23);
    assert(sessions[0].player.pos.x==21);
    // A modal inventory prompt does not prevent the other player moving.
    step(0,'i');auto screen=sessions[0].window.cells;
    step(1,'6');assert(sessions[1].player.pos.x==24);
    assert(sessions[0].window.cells==screen);
    step(0,27);
    // Private inventory and shared ground items. Drop a ration then walk away.
    selectSession(0);int item_count=py.inventory[0].items_count;
    inventoryDropItem(0,false);
    assert(py.inventory[0].items_count==item_count-1 || py.inventory[0].category_id!=TV_FOOD);
    auto dropped=dg.floor[py.pos.y][py.pos.x].treasure_id;
    assert(dropped!=0);auto category=game.treasure.list[dropped].category_id;
    step(0,'4');
    keys(1,"444");
    assert(sessions[1].player.pos.x==21);
    assert(dg.floor[10][21].treasure_id==0);
    bool found=false;for(auto &item:sessions[1].player.inventory) if(item.category_id==category)found=true;
    assert(found);
    // Nearest-target selection and area/melee target selection are per player.
    selectSession(0);
    {CoopTarget target(true,sessions[1].player.pos);assert(coopSlot()==1);}
    assert(coopSlot()==0);
    int hp0=sessions[0].player.misc.current_hp, hp1=sessions[1].player.misc.current_hp;
    {CoopTarget target(true,sessions[1].player.pos,true);playerTakesHit(2,"test");}
    assert(sessions[0].player.misc.current_hp==hp0);
    assert(sessions[1].player.misc.current_hp==hp1-2);
    // Real monster AI attacks the nearer companion, including while they are idle.
    arena();
    dg.floor[10][20].creature_id=0; sessions[0].player.pos={10,5}; dg.floor[10][5].creature_id=1;
    sessions[1].player.misc.current_hp=1000;
    int kobold=-1;
    for(int n=0;n<MON_MAX_CREATURES;++n) if(std::string(creatures_list[n].name)=="Kobold") {kobold=n;break;}
    assert(kobold>=0);selectSession(0);assert(monsterPlaceNew({10,23},kobold,false));
    hp0=sessions[0].player.misc.current_hp;
    for(int n=0;n<25;++n) updateMonsters(true);
    assert(sessions[1].player.misc.current_hp<1000 && sessions[0].player.misc.current_hp==hp0);
    // Concurrent store ownership is bounded and releases on scope exit.
    {CoopStoreGuard first(0);assert(first.acquired);selectSession(1);CoopStoreGuard second(0);assert(!second.acquired);}
    assert(!coopShopBusy());
    // Level changes safely cancel the companion's suspended menu.
    step(1,'i');selectSession(0);dg.current_level=1;coopGenerateLevel();
    assert(dg.floor[sessions[0].player.pos.y][sessions[0].player.pos.x].creature_id==1);
    assert(dg.floor[sessions[1].player.pos.y][sessions[1].player.pos.x].creature_id==1);
    step(1,27);assert(sessions[1].epoch==epoch && sessions[1].map_input);
    // One death leaves the other alive on the same level.
    selectSession(1);auto corpse=py.pos;playerTakesHit(10000,"test monster");
    assert(!dg.generate_new_level && dg.floor[corpse.y][corpse.x].creature_id==0);
    step(1,'5');assert(sessions[1].finished);
    assert(!sessions[0].state.character_is_dead && !sessions[0].finished);
    std::cout<<"PASS: interleaved creation, shared world, independent movement/inventories, collision, prompts, pickup, targeting, stores, level transition, death\n";
}
