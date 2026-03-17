#ifndef CONFIG_H
#define CONFIG_H

/* ── Framebuffer ─────────────────────────────────────────── */
#define FB_DEV   "/dev/fb0"
#define FB_W     1024
#define FB_H     600
#define FB_BPP   2
#define FB_SIZE  (FB_W * FB_H * FB_BPP)

/* ── Font ────────────────────────────────────────────────── */
#define FONT_W   8
#define FONT_H   16

/* ── Colors RGB565 ───────────────────────────────────────── */
#define COL_BLACK   0x0000
#define COL_WHITE   0xFFFF
#define COL_GRAY    0x8410
#define COL_HIBG    0x2945   /* selected item background */

/* ── Menu pixel layout ───────────────────────────────────── */
#define SCALE      2
#define CH_W       (FONT_W * SCALE)   /* 16px per char  */
#define CH_H       (FONT_H * SCALE)   /* 32px per char  */
#define ITEM_PH    (CH_H + 12)        /* 44px per item  */
#define TITLE_PH   (CH_H + 20)        /* 52px for title */
#define BOX_X      60
#define BOX_RX     (FB_W - 60)
#define BOX_BW     (BOX_RX - BOX_X)

/* ── Book paths ──────────────────────────────────────────── */
#define BOOK_13_CACHE  "/home/ibbu98/iqra_pad_rayyan_version/bookshelf/quran_13/cache"
#define BOOK_13_STATE  "/home/ibbu98/iqra_pad_rayyan_version/bookshelf/quran_13/state.dat"
#define BOOK_13_BM     "/home/ibbu98/iqra_pad_rayyan_version/bookshelf/quran_13/bookmarks.dat"
#define BOOK_13_PAGES  424

#define BOOK_15_CACHE  "/home/ibbu98/iqra_pad_rayyan_version/bookshelf/quran_15/cache"
#define BOOK_15_STATE  "/home/ibbu98/iqra_pad_rayyan_version/bookshelf/quran_15/state.dat"
#define BOOK_15_BM     "/home/ibbu98/iqra_pad_rayyan_version/bookshelf/quran_15/bookmarks.dat"
#define BOOK_15_PAGES  310

/* ── Runtime book state (set by open_book) ───────────────── */
extern const char *CACHE_DIR;
extern const char *STATE_FILE;
extern const char *BOOKMARK_FILE;
extern int         TOTAL_PAGES;

#endif /* CONFIG_H */
