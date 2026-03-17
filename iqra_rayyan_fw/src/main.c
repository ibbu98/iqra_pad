/*
 * main.c — Entry point for Iqra Pad (Rayyan Edition)
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "config.h"
#include "fb.h"
#include "menu.h"
#include "reader.h"
#include "state.h"
#include "audio.h"
#include "settings.h"

/* Runtime book state — defined here, declared extern in config.h */
const char *CACHE_DIR     = NULL;
const char *STATE_FILE    = NULL;
const char *BOOKMARK_FILE = NULL;
int         TOTAL_PAGES   = 0;

int main(void){
    /* Open framebuffer */
    if(fb_open() < 0) return 1;

    /* Pin main UI thread to Core 0 */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset); CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    /* Alloc page buffers + start prefetch thread */
    if(reader_init() < 0) return 1;

    /* Start audio watchdog thread (RT, Core 1) */
    audio_init();

    /* Default book = 13-line at startup */
    CACHE_DIR     = BOOK_13_CACHE;
    STATE_FILE    = BOOK_13_STATE;
    BOOKMARK_FILE = BOOK_13_BM;
    TOTAL_PAGES   = BOOK_13_PAGES;
    state_load();
    bookmarks_load();

    /* Hide cursor, go raw */
    printf("\033[?25l\033[2J\033[H"); fflush(stdout);
    term_raw();

    /* Clear screen */
    px_rect(0, 0, FB_W, FB_H, COL_BLACK);
    menu_blit_full();

    /* Run */
    menu_main();

    /* Clean exit */
    term_restore();
    printf("\033[?25h\033[2J\033[H"); fflush(stdout);
    px_rect(0, 0, FB_W, FB_H, COL_BLACK);
    menu_blit_full();

    audio_cleanup();
    reader_cleanup();
    fb_close();

    printf("\nJazakallah Khair. Ma'assalama!\n\n");
    return 0;
}
