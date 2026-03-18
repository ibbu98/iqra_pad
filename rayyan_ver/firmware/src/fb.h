#ifndef FB_HDR_H
#define FB_HDR_H

#include <stdint.h>
#include "config.h"

/* ── Globals ─────────────────────────────────────────────── */
extern uint16_t *fb_mem;
extern uint16_t  menu_buf[FB_H][FB_W];
extern int        fb_fd;

/* ── Init / cleanup ──────────────────────────────────────── */
int  fb_open(void);
void fb_close(void);

/* ── Raw pixel ops ───────────────────────────────────────── */
void px_set  (int x, int y, uint16_t col);
void px_rect (int x, int y, int w, int h, uint16_t col);
void px_hline(int x, int y, int w, uint16_t col);

/* ── Text rendering ──────────────────────────────────────── */
void px_glyph(int x, int y, unsigned char c, uint16_t fg, int scale);
void px_str  (int x, int y, const char *s,  uint16_t fg, int scale);
int  px_strw (const char *s, int scale);

/* ── Blit to framebuffer ─────────────────────────────────── */
void menu_blit_full(void);
void menu_blit_rows(int y1, int y2);

/* ── Vsync page flip (for Quran pages) ───────────────────── */
void fb_flip_page(const uint8_t *buf);
void fb_fill     (uint16_t col);

#endif /* FB_HDR_H */
