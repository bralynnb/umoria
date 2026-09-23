// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <deque>
#include <ucontext.h>

// Only the optional Linux browser target includes this file. Stable session
// objects keep pointers into player inventories valid across suspended prompts.
struct CoopWindow {
    std::array<char, 80 * 24> cells;
    int row = 0, col = 0;
    CoopWindow() { cells.fill(' '); }
};
struct CoopSession {
    Player_t player;
    Game_t state;
    Panel_t panel{};
    ucontext_t context{};
    std::array<char, 1024 * 1024> stack;
    CoopWindow window, saved_window;
    CoopWindow *saved = &saved_window;
    std::deque<int> input;
    bool started = false, joined = false, finished = false, map_input = false;
    bool terminal_on = false, screen_changed = false, message_ready = false;
    bool panic = false;
    int eof = 0, epoch = 0;
    int16_t message_id = 0;
    vtype_t history[MESSAGE_HISTORY_SIZE]{};
    bool openarea = false, breakright = false, breakleft = false;
    int prevdir = 0, direction = 0;
    int16_t increment = 0;
    int fxx=0, fxy=0, fyx=0, fyy=0, places_seen=0, rocks=0;
    bool no_query=false;
    vtype_t roff{};
    char *roff_pointer = nullptr;
    int roff_line=0;
};
struct CoopLevelChanged {};
CoopSession &coopSession();
int coopSlot();
bool coopBegin();
void coopWorldReady();
void coopJoinDungeon();
void coopGenerateLevel();
char coopReadKey();
void coopTurnBoundary();
[[noreturn]] void coopFinish();
bool coopOtherAt(Coord_t coord);
bool coopShopBusy();
struct CoopTarget {
    int previous;
    CoopTarget(bool enabled, Coord_t coord, bool exact=false);
    ~CoopTarget();
};
struct CoopStoreGuard {
    int store;
    bool acquired;
    explicit CoopStoreGuard(int id);
    ~CoopStoreGuard();
};
#define py (coopSession().player)
#define game (coopSession().state)
#define screen_has_changed (coopSession().screen_changed)
#define message_ready_to_print (coopSession().message_ready)
#define messages (coopSession().history)
#define last_message_id (coopSession().message_id)
#define eof_flag (coopSession().eof)
#define panic_save (coopSession().panic)
#define curses_on (coopSession().terminal_on)
#define save_screen (coopSession().saved)
#define find_openarea (coopSession().openarea)
#define find_breakright (coopSession().breakright)
#define find_breakleft (coopSession().breakleft)
#define find_prevdir (coopSession().prevdir)
#define find_direction (coopSession().direction)
#define store_last_increment (coopSession().increment)
#define los_fxx (coopSession().fxx)
#define los_fxy (coopSession().fxy)
#define los_fyx (coopSession().fyx)
#define los_fyy (coopSession().fyy)
#define los_num_places_seen (coopSession().places_seen)
#define los_hack_no_query (coopSession().no_query)
#define los_rocks_and_objects (coopSession().rocks)
#define roff_buffer (coopSession().roff)
#define roff_buffer_pointer (coopSession().roff_pointer)
#define roff_print_line (coopSession().roff_line)
