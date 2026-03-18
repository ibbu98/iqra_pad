/*
 * reader.c — Page loading with real-time threading
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
#include <sys/resource.h>
#include "reader.h"
#include "fb.h"
#include "state.h"
#include "menu.h"
#include "config.h"

/* ── Page buffers ────────────────────────────────────────── */
uint8_t *buf_cur  = NULL;
uint8_t *buf_next = NULL;
uint8_t *buf_prev = NULL;
int      cur_page = 1;
volatile int running = 1;

/* ── Prefetch thread ─────────────────────────────────────── */
static pthread_t   pf_tid;
static sem_t       pf_sem;           /* signals new prefetch request  */
static pthread_mutex_t pf_mutex = PTHREAD_MUTEX_INITIALIZER;
static int         pf_next_req = -1;
static int         pf_prev_req = -1;

static int load_raw(int page, uint8_t *buf){
    char path[256];
    snprintf(path, sizeof(path), "%s/p%d.raw", CACHE_DIR, page);
    int fd = open(path, O_RDONLY);
    if(fd < 0) return -1;
    ssize_t n = read(fd, buf, FB_SIZE);
    close(fd);
    return (n == (ssize_t)FB_SIZE) ? 0 : -1;
}

static void *prefetch_thread(void *arg){
    (void)arg;

    /* Pin to Core 2, low IO priority */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset); CPU_SET(2, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    /* SCHED_BATCH — yields to RT and normal threads easily */
    struct sched_param sp = { .sched_priority = 0 };
    pthread_setschedparam(pthread_self(), SCHED_BATCH, &sp);

    /* Low IO priority */
    setpriority(PRIO_PROCESS, 0, 10);

    while(running){
        /* Block until signalled — zero CPU when idle */
        sem_wait(&pf_sem);
        if(!running) break;

        pthread_mutex_lock(&pf_mutex);
        int dn = pf_next_req, dp = pf_prev_req;
        pf_next_req = pf_prev_req = -1;
        pthread_mutex_unlock(&pf_mutex);

        if(dn > 0 && dn <= TOTAL_PAGES) load_raw(dn, buf_next);
        if(dp > 0 && dp <= TOTAL_PAGES) load_raw(dp, buf_prev);
    }
    return NULL;
}

static void request_prefetch(int page){
    pthread_mutex_lock(&pf_mutex);
    pf_next_req = (page < TOTAL_PAGES) ? page+1 : -1;
    pf_prev_req = (page > 1)           ? page-1 : -1;
    pthread_mutex_unlock(&pf_mutex);
    sem_post(&pf_sem);   /* wake prefetch thread — no spin */
}

/* ── Init / cleanup ──────────────────────────────────────── */
int reader_init(void){
    buf_cur  = malloc(FB_SIZE);
    buf_next = malloc(FB_SIZE);
    buf_prev = malloc(FB_SIZE);
    if(!buf_cur || !buf_next || !buf_prev){
        fprintf(stderr, "OOM\n"); return -1;
    }
    memset(buf_cur,  0, FB_SIZE);
    memset(buf_next, 0, FB_SIZE);
    memset(buf_prev, 0, FB_SIZE);

    sem_init(&pf_sem, 0, 0);
    pthread_create(&pf_tid, NULL, prefetch_thread, NULL);
    return 0;
}

void reader_cleanup(void){
    running = 0;
    sem_post(&pf_sem);          /* wake thread so it can exit */
    pthread_join(pf_tid, NULL);
    sem_destroy(&pf_sem);
    free(buf_cur); free(buf_next); free(buf_prev);
}

/* ── Page navigation ─────────────────────────────────────── */
void goto_page(int page){
    if(page < 1 || page > TOTAL_PAGES) return;
    int dir = page - cur_page;
    if     (dir ==  1 && buf_next){ uint8_t *t=buf_cur; buf_cur=buf_next; buf_next=t; }
    else if(dir == -1 && buf_prev){ uint8_t *t=buf_cur; buf_cur=buf_prev; buf_prev=t; }
    else load_raw(page, buf_cur);
    cur_page = page;
    fb_flip_page(buf_cur);
    request_prefetch(page);
}

/* ── Helpers ─────────────────────────────────────────────── */
void show_message(const char *line1, const char *line2){
    px_rect(0, 0, FB_W, FB_H, COL_BLACK);
    int tw = px_strw(line1, SCALE);
    px_str((FB_W-tw)/2, FB_H/2 - CH_H, line1, COL_WHITE, SCALE);
    if(line2 && line2[0]){
        tw = px_strw(line2, SCALE);
        px_str((FB_W-tw)/2, FB_H/2 + 8, line2, COL_GRAY, SCALE);
    }
    menu_blit_full();
}

void run_reader(int start_page){
    goto_page(start_page);
    while(1){
        int k = read_key();
        if     (k == KEY_RIGHT || k == KEY_SPACE) goto_page(cur_page + 1);
        else if(k == KEY_LEFT  || k == KEY_B)     goto_page(cur_page - 1);
        else if(k == KEY_D){
            state_save(cur_page);
            bookmark_add(cur_page);
            char msg[52];
            snprintf(msg, sizeof(msg), "Bookmark saved  Page %d", cur_page);
            show_message(msg, "Press any key...");
            read_key();
            goto_page(cur_page);
        }
        else if(k == KEY_ESC || k == KEY_BACK){
            state_save(cur_page);
            px_rect(0, 0, FB_W, FB_H, COL_BLACK);
            menu_blit_full();
            return;
        }
        else if(k == KEY_Q){
            state_save(cur_page);
            running = 0;
            return;
        }
    }
}
