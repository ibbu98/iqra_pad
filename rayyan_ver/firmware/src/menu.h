#ifndef MENU_H
#define MENU_H

/* ── List scroll window size ─────────────────────────────── */
#define LIST_VIS 9

/* ── Key codes ───────────────────────────────────────────── */
#define KEY_UP    1
#define KEY_DOWN  2
#define KEY_LEFT  3
#define KEY_RIGHT 4
#define KEY_ENTER 5
#define KEY_ESC   6
#define KEY_BACK  7
#define KEY_Q     8
#define KEY_D     9
#define KEY_SPACE 10
#define KEY_B     11
#define KEY_X     12
#define KEY_OTHER 99

/* ── Terminal ────────────────────────────────────────────── */
void term_raw    (void);
void term_restore(void);
int  read_key    (void);

/* ── Menu pixel helpers ──────────────────────────────────── */
int  item_y        (int title_y, int i);
void draw_item_px  (int y, const char *text, int selected);
void draw_title_px (int y, const char *title);
void draw_statusbar(const char *title);   /* date left, title center, time right */
void draw_help_px  (int y, const char *msg);
void draw_counter_px(int title_y, int sel, int count);

/* ── Menu runners ────────────────────────────────────────── */
int  run_menu_fb (const char *title, const char **items, int count);
void run_list_fb (const char *title, char (*labels)[52], int count,
                  int *out_sel, int *out_page);

/* ── Top-level menus ─────────────────────────────────────── */
void menu_main(void);

#endif /* MENU_H */
