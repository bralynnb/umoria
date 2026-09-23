// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// An 80x24 character terminal, not a gameplay reimplementation.
using WINDOW = CoopWindow;
constexpr int LINES=24, COLS=80, ERR=-1;
#define stdscr (&coopSession().window)
#define curscr stdscr
#define getyx(w,y,x) do { (y)=(w)->row; (x)=(w)->col; } while (0)
inline void initscr() {}
inline WINDOW *newwin(int,int,int,int) { return &coopSession().saved_window; }
inline int raw() {return 0;}
inline int noecho() {return 0;}
inline int nonl() {return 0;}
inline int keypad(WINDOW*,bool) {return 0;}
inline int endwin() {return 0;}
inline int refresh() {return 0;}
inline int wrefresh(WINDOW*) {return 0;}
inline int touchwin(WINDOW*) {return 0;}
inline int overwrite(WINDOW *a,WINDOW *b) {*b=*a;return 0;}
inline int move(int y,int x) {
    if(y<0 || y>=LINES || x<0 || x>=COLS) return ERR;
    stdscr->row=y;stdscr->col=x;return 0;
}
inline int mvcur(int,int,int y,int x) {return move(y,x);}
inline int clear() {stdscr->cells.fill(' ');return move(0,0);}
inline int clrtoeol() {
    for(int x=stdscr->col;x<COLS;++x) stdscr->cells[stdscr->row*COLS+x]=' ';
    return 0;
}
inline int clrtobot() {
    for(int n=stdscr->row*COLS+stdscr->col;n<COLS*LINES;++n) stdscr->cells[n]=' ';
    return 0;
}
inline int addch(char c) {
    if(c=='\n') {stdscr->row=(stdscr->row+1)%LINES;stdscr->col=0;return 0;}
    stdscr->cells[stdscr->row*COLS+stdscr->col]=c;
    if(++stdscr->col>=COLS) {stdscr->col=0;stdscr->row=(stdscr->row+1)%LINES;}
    return 0;
}
inline int addstr(const char *s) {while(*s) addch(*s++);return 0;}
inline int mvaddch(int y,int x,char c) {return move(y,x)==ERR?ERR:addch(c);}
inline int mvaddstr(int y,int x,const char *s) {return move(y,x)==ERR?ERR:addstr(s);}
inline int getch() {return coopReadKey();}
