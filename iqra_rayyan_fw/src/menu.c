/*
 * menu.c — Terminal input, framebuffer menus, navigation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include "menu.h"
#include "fb.h"
#include "config.h"
#include "data.h"
#include "state.h"
#include "reader.h"
#include "audio.h"
#include "settings.h"
#include "pomodoro.h"

/* ── Terminal raw mode ───────────────────────────────────── */
static struct termios orig_term;

void term_raw(void){
    struct termios t;
    tcgetattr(STDIN_FILENO, &orig_term);
    t = orig_term;
    t.c_lflag &= ~(ICANON|ECHO);
    t.c_cc[VMIN] = 1; t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

void term_restore(void){
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
}

/* ── Keyboard ────────────────────────────────────────────── */
int read_key(void){
    uint8_t ch = 0;
    if(read(STDIN_FILENO, &ch, 1) <= 0) return KEY_OTHER;
    if(ch=='q'||ch=='Q')          return KEY_Q;
    if(ch=='d'||ch=='D')          return KEY_D;
    if(ch=='b'||ch=='B')          return KEY_B;
    if(ch=='x'||ch=='X')          return KEY_X;
    if(ch==' ')                   return KEY_SPACE;
    if(ch=='\n'||ch=='\r')        return KEY_ENTER;
    if(ch==127||ch==8)            return KEY_BACK;
    if(ch=='\x1b'){
        /* set non-blocking with short timeout to detect plain ESC */
        struct termios t;
        tcgetattr(STDIN_FILENO, &t);
        t.c_cc[VMIN]  = 0;
        t.c_cc[VTIME] = 1;  /* 100ms timeout */
        tcsetattr(STDIN_FILENO, TCSANOW, &t);

        uint8_t s[2] = {0, 0};
        int r = read(STDIN_FILENO, &s[0], 1);

        /* restore blocking */
        t.c_cc[VMIN]  = 1;
        t.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &t);

        if(r <= 0 || s[0] != '[') return KEY_ESC;  /* plain ESC */
        read(STDIN_FILENO, &s[1], 1);
        if(s[1]=='A') return KEY_UP;
        if(s[1]=='B') return KEY_DOWN;
        if(s[1]=='C') return KEY_RIGHT;
        if(s[1]=='D') return KEY_LEFT;
        return KEY_ESC;
    }
    return KEY_OTHER;
}

/* ── Menu pixel helpers ──────────────────────────────────── */
int item_y(int title_y, int i){
    return title_y + TITLE_PH + 4 + i * ITEM_PH;
}

void draw_item_px(int y, const char *text, int selected){
    uint16_t bg  = selected ? COL_HIBG  : COL_BLACK;
    uint16_t fg  = COL_WHITE;
    px_rect(BOX_X+1, y, BOX_BW-2, ITEM_PH-2, bg);
    if(selected) px_rect(BOX_X+1, y, 4, ITEM_PH-2, COL_WHITE);
    int tx = BOX_X + 16;
    int ty = y + (ITEM_PH - CH_H) / 2;
    px_str(tx, ty, text, fg, SCALE);
    px_hline(BOX_X+1, y+ITEM_PH-2, BOX_BW-2, COL_GRAY);
}

void draw_title_px(int y, const char *title){
    px_rect(BOX_X, y, BOX_BW, TITLE_PH, COL_GRAY);
    int tw = px_strw(title, SCALE);
    int tx = BOX_X + (BOX_BW - tw) / 2;
    int ty = y + (TITLE_PH - CH_H) / 2;
    px_str(tx, ty, title, COL_BLACK, SCALE);
}

/* ── Status bar — date left, title center, time right ────── */
void draw_statusbar(const char *title){
    time_t t=time(NULL); struct tm *tm=localtime(&t);

    /* background */
    px_rect(0, 0, FB_W, TITLE_PH, COL_GRAY);

    int ty = (TITLE_PH-CH_H)/2;

    /* Date — left  MM/DD/YYYY */
    char date[16];
    snprintf(date,sizeof(date),"%02d/%02d/%04d",
             tm->tm_mon+1,tm->tm_mday,tm->tm_year+1900);
    px_str(8, ty, date, COL_BLACK, SCALE);

    /* Title — center */
    int tw=px_strw(title,SCALE);
    px_str((FB_W-tw)/2, ty, title, COL_BLACK, SCALE);

    /* Time — right */
    char timstr[8];
    snprintf(timstr,sizeof(timstr),"%02d:%02d",
             tm->tm_hour,tm->tm_min);
    tw=px_strw(timstr,SCALE);
    px_str(FB_W-tw-8, ty, timstr, COL_BLACK, SCALE);
}

void draw_help_px(int y, const char *msg){
    px_rect(BOX_X, y, BOX_BW, CH_H+8, COL_BLACK);
    px_str(BOX_X+8, y+4, msg, COL_GRAY, SCALE);
}

void draw_counter_px(int title_y, int sel, int count){
    char buf[24];
    snprintf(buf, sizeof(buf), "[%d/%d]", sel+1, count);
    int tw = px_strw(buf, SCALE);
    int tx = BOX_RX - tw - 8;
    int ty = title_y + (TITLE_PH - CH_H) / 2;
    px_rect(BOX_RX-120, ty, 120, CH_H, COL_GRAY);
    px_str(tx, ty, buf, COL_BLACK, SCALE);
}

/* ── Generic small menu (2-6 items, all visible) ─────────── */
int run_menu_fb(const char *title, const char **items, int count){
    int sel = 0;
    int total_h = TITLE_PH + count*ITEM_PH + (CH_H+16);
    int title_y = (FB_H - total_h) / 2;
    if(title_y < 20) title_y = 20;

    px_rect(0, 0, FB_W, FB_H, COL_BLACK);
    draw_title_px(title_y, title);
    for(int i=0; i<count; i++)
        draw_item_px(item_y(title_y,i), items[i], i==sel);
    draw_help_px(item_y(title_y,count)+4, "UP/DOWN  ENTER=ok  ESC=back");
    menu_blit_full();

    while(1){
        int k = read_key();
        if(k==KEY_ENTER) return sel;
        if(k==KEY_ESC||k==KEY_BACK||k==KEY_Q) return -1;
        if(k==KEY_UP||k==KEY_DOWN){
            int prev = sel;
            if(k==KEY_UP)   sel = (sel-1+count) % count;
            if(k==KEY_DOWN) sel = (sel+1)        % count;
            draw_item_px(item_y(title_y,prev), items[prev], 0);
            draw_item_px(item_y(title_y,sel),  items[sel],  1);
            int y1 = item_y(title_y, prev<sel ? prev : sel);
            int y2 = item_y(title_y, prev<sel ? sel  : prev) + ITEM_PH;
            menu_blit_rows(y1, y2);
        }
    }
}

/* ── Scrollable list (Juz / Surah / Bookmarks) ───────────── */

void run_list_fb(const char *title, char (*labels)[52], int count,
                 int *out_sel, int *out_page){
    int sel=0, offset=0;
    *out_sel = -1; *out_page = -1;

    int total_h = TITLE_PH + LIST_VIS*ITEM_PH + (CH_H+16);
    int title_y = (FB_H - total_h) / 2;
    if(title_y < 10) title_y = 10;

    void full_draw(void){
        px_rect(0, 0, FB_W, FB_H, COL_BLACK);
        draw_title_px(title_y, title);
        draw_counter_px(title_y, sel, count);
        int end = offset + LIST_VIS; if(end > count) end = count;
        for(int i=offset; i<end; i++)
            draw_item_px(item_y(title_y, i-offset), labels[i], i==sel);
        for(int i=end-offset; i<LIST_VIS; i++)
            draw_item_px(item_y(title_y, i), "", 0);
        draw_help_px(item_y(title_y, LIST_VIS)+4,
                     "UP/DOWN  ENTER=open  ESC=back");
        menu_blit_full();
    }
    full_draw();

    while(1){
        int k = read_key();
        if(k==KEY_ESC||k==KEY_BACK||k==KEY_Q) return;
        if(k==KEY_ENTER){ *out_sel=sel; return; }
        if(k==KEY_X)    { *out_sel=sel; *out_page=-2; return; }

        if(k==KEY_UP||k==KEY_DOWN){
            int prev=sel, prev_off=offset;
            if(k==KEY_DOWN){ if(++sel>=count) sel=count-1; if(sel>=offset+LIST_VIS) offset++; }
            if(k==KEY_UP)  { if(--sel<0) sel=0;            if(sel<offset) offset--;            }
            if(offset != prev_off){
                full_draw();
            } else {
                draw_item_px(item_y(title_y, prev-offset), labels[prev], 0);
                draw_item_px(item_y(title_y, sel -offset), labels[sel],  1);
                draw_counter_px(title_y, sel, count);
                int y1 = item_y(title_y, (prev<sel?prev:sel)-offset);
                int y2 = item_y(title_y, (prev<sel?sel:prev)-offset) + ITEM_PH;
                menu_blit_rows(title_y, title_y+TITLE_PH);
                menu_blit_rows(y1, y2);
            }
        }
    }
}

/* ── Juz menu ────────────────────────────────────────────── */
static void menu_juz(const int *juz_page){
    static char labels[30][52];
    for(int i=0; i<30; i++)
        snprintf(labels[i], 52, "Juz %2d   --   Page %d", i+1, juz_page[i+1]);
    while(1){
        int sel, pg;
        run_list_fb("Select Juz", labels, 30, &sel, &pg);
        if(sel < 0) return;
        run_reader(juz_page[sel+1]);
        if(!running) return;
    }
}

/* ── Surah menu ──────────────────────────────────────────── */
static void menu_surah(const int *surah_page){
    static char labels[114][52];
    for(int i=0; i<114; i++)
        snprintf(labels[i], 52, "%3d. %-20s p.%d",
                 i+1, SURAH_NAME[i+1], surah_page[i+1]);
    while(1){
        int sel, pg;
        run_list_fb("Select Surah", labels, 114, &sel, &pg);
        if(sel < 0) return;
        run_reader(surah_page[sel+1]);
        if(!running) return;
    }
}

/* ── Bookmark menu ───────────────────────────────────────── */
static void menu_bookmarks(void){
    while(1){
        if(bm_count == 0){
            show_message("No bookmarks yet.",
                         "Press 'd' while reading  then any key...");
            read_key(); return;
        }
        static char labels[MAX_BOOKMARKS][52];
        for(int i=0; i<bm_count; i++)
            snprintf(labels[i], 52, "%s", bookmarks[i].label);
        int sel, pg;
        run_list_fb("Bookmarks", labels, bm_count, &sel, &pg);
        if(sel < 0) return;
        if(pg == -2){ bookmark_delete(sel); continue; }
        run_reader(bookmarks[sel].page);
        if(!running) return;
    }
}

/* ── Generic quran menu (shared by 13-line and 15-line) ───── */
static void menu_quran(const char *title,
                       const int *juz_page,
                       const int *surah_page){
    while(1){
        char last_label[52], bm_label[52];
        snprintf(last_label, 52, "Last Read  (page %d)", last_read_page);
        snprintf(bm_label,   52, "Bookmarks  (%d saved)", bm_count);
        const char *items[] = {
            "Read from Beginning",
            "By Juz    (1-30)",
            "By Surah  (1-114)",
            last_label, bm_label,
        };
        int sel = run_menu_fb(title, items, 5);
        if(sel < 0) return;
        switch(sel){
            case 0: run_reader(1);               break;
            case 1: menu_juz(juz_page);          break;
            case 2: menu_surah(surah_page);      break;
            case 3: run_reader(last_read_page);  break;
            case 4: menu_bookmarks();            break;
        }
        if(!running) return;
    }
}

/* ── Book switcher ───────────────────────────────────────── */
static void open_book(const char *cache, const char *state_f,
                      const char *bm_f,  int pages,
                      const char *title,
                      const int *juz_page,
                      const int *surah_page){
    CACHE_DIR     = cache;
    STATE_FILE    = state_f;
    BOOKMARK_FILE = bm_f;
    TOTAL_PAGES   = pages;

    last_read_page = 1;
    bm_count       = 0;
    cur_page       = 1;
    memset(buf_cur,  0, FB_SIZE);
    memset(buf_next, 0, FB_SIZE);
    memset(buf_prev, 0, FB_SIZE);

    state_load();
    bookmarks_load();
    menu_quran(title, juz_page, surah_page);
}

/* ── Bookshelf ───────────────────────────────────────────── */
static void menu_bookshelf(void){
    const char *items[] = { "13-Line Quran", "15-Line Quran" };
    while(1){
        int sel = run_menu_fb("Holy Quran", items, 2);
        if(sel < 0) return;
        if(sel == 0)
            open_book(BOOK_13_CACHE, BOOK_13_STATE, BOOK_13_BM,
                      BOOK_13_PAGES, "13-Line Quran",
                      JUZ13_PAGE, SURAH13_PAGE);
        if(sel == 1)
            open_book(BOOK_15_CACHE, BOOK_15_STATE, BOOK_15_BM,
                      BOOK_15_PAGES, "15-Line Quran",
                      JUZ15_PAGE, SURAH15_PAGE);
        if(!running) return;
    }
}

/* ── Main menu ───────────────────────────────────────────── */
void menu_main(void){
    const char *items[] = {
        "Holy Quran",
        "Quran MP3",
        "Pomodoro Timer",
        "Settings",
        "Shut Down",
        "Developer Mode"
    };
    while(1){
        int count=6;
        int total_h = TITLE_PH + count*ITEM_PH + (CH_H+16);
        int title_y = (FB_H - total_h) / 2;
        if(title_y < 20) title_y = 20;
        int sel=0;

        /* full draw with statusbar */
        void full_draw_main(void){
            px_rect(0,0,FB_W,FB_H,COL_BLACK);
            draw_statusbar("Iqra Pad  --  Rayyan Edition");
            /* shift title_y if statusbar is at 0 */
            int ty2 = title_y < TITLE_PH ? TITLE_PH+4 : title_y;
            for(int i=0;i<count;i++)
                draw_item_px(item_y(ty2,i),items[i],i==sel);
            draw_help_px(item_y(ty2,count)+4,"UP/DOWN  ENTER=ok");
            menu_blit_full();
        }
        full_draw_main();

        while(1){
            int k=read_key();
            if(k==KEY_UP||k==KEY_DOWN){
                int prev=sel;
                if(k==KEY_UP)   sel=(sel-1+count)%count;
                if(k==KEY_DOWN) sel=(sel+1)%count;
                int ty2=title_y<TITLE_PH?TITLE_PH+4:title_y;
                draw_item_px(item_y(ty2,prev),items[prev],0);
                draw_item_px(item_y(ty2,sel), items[sel], 1);
                int y1=item_y(ty2,prev<sel?prev:sel);
                int y2=item_y(ty2,prev<sel?sel:prev)+ITEM_PH;
                menu_blit_rows(y1,y2);
                continue;
            }
            if(k==KEY_ESC||k==KEY_BACK) continue; /* no exit from main */
            if(k==KEY_ENTER) break;
        }

        if(sel==0) menu_bookshelf();
        if(sel==1) menu_mp3();
        if(sel==2) menu_pomodoro();
        if(sel==3) menu_settings();
        if(sel==4){
            const char *c[]={"Yes, Shut Down","Cancel"};
            int confirm=run_menu_fb("Shut Down?",c,2);
            if(confirm==0){
                px_rect(0,0,FB_W,FB_H,COL_BLACK);
                const char*msg="Shutting down...";
                int tw=px_strw(msg,SCALE);
                px_str((FB_W-tw)/2,FB_H/2-CH_H,msg,COL_WHITE,SCALE);
                menu_blit_full();
                system("sudo shutdown -h now");
                continue;
            }
        }
        if(sel==5){running=0;return;}
        if(!running) return;
    }
}
