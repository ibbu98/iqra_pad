/*
 * state.c — Last-read page persistence and bookmark management
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "state.h"
#include "config.h"

Bookmark bookmarks[MAX_BOOKMARKS];
int      bm_count       = 0;
int      last_read_page = 1;

/* ── Last-read page ──────────────────────────────────────── */
void state_load(void){
    FILE *f = fopen(STATE_FILE, "r");
    if(!f){ last_read_page = 1; return; }
    fscanf(f, "%d", &last_read_page);
    fclose(f);
    if(last_read_page < 1 || last_read_page > TOTAL_PAGES)
        last_read_page = 1;
}

void state_save(int page){
    last_read_page = page;
    FILE *f = fopen(STATE_FILE, "w");
    if(!f) return;
    fprintf(f, "%d\n", page);
    fclose(f);
}

/* ── Bookmarks ───────────────────────────────────────────── */
void bookmarks_load(void){
    bm_count = 0;
    FILE *f = fopen(BOOKMARK_FILE, "r");
    if(!f) return;
    while(bm_count < MAX_BOOKMARKS){
        int  pg;
        char lbl[BM_LABEL_LEN];
        if(fscanf(f, "%d |%47[^\n]\n", &pg, lbl) != 2) break;
        bookmarks[bm_count].page = pg;
        strncpy(bookmarks[bm_count].label, lbl, BM_LABEL_LEN-1);
        bm_count++;
    }
    fclose(f);
}

void bookmarks_save(void){
    FILE *f = fopen(BOOKMARK_FILE, "w");
    if(!f) return;
    for(int i=0; i<bm_count; i++)
        fprintf(f, "%d |%s\n", bookmarks[i].page, bookmarks[i].label);
    fclose(f);
}

void bookmark_add(int page){
    if(bm_count >= MAX_BOOKMARKS) return;
    time_t t  = time(NULL);
    struct tm *tm = localtime(&t);
    snprintf(bookmarks[bm_count].label, BM_LABEL_LEN,
             "Page %-4d  %02d/%02d  %02d:%02d",
             page, tm->tm_mday, tm->tm_mon+1,
             tm->tm_hour, tm->tm_min);
    bookmarks[bm_count].page = page;
    bm_count++;
    bookmarks_save();
}

void bookmark_delete(int idx){
    if(idx < 0 || idx >= bm_count) return;
    for(int i=idx; i<bm_count-1; i++)
        bookmarks[i] = bookmarks[i+1];
    bm_count--;
    bookmarks_save();
}
