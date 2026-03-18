/*
 * audio.c — MP3 player (no speed control)
 * Core 1: monitor thread (auto-advance)
 * Core 2: duration thread (remaining time)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <sched.h>
#include <termios.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <time.h>
#include "audio.h"
#include "fb.h"
#include "menu.h"
#include "config.h"
#include "data.h"
#include "reader.h"

#define MP3_BASE "/home/ibbu98/iqra_pad_rayyan_version/quran_mp3"

static const char *RECITERS[] = {
    "As-Sudais","As-Shuraim","Jabir_Ali","Badr_Al-Turki",
    "Yasser_Al-Dosari","Mishary_Rashid","Maher_al_muaiqly",
};
#define RECITER_COUNT 7
static const char *RECITER_DISPLAY[] = {
    "As-Sudais","As-Shuraim","Jabir Ali","Badr Al-Turki",
    "Yasser Al-Dosari","Mishary Rashid","Maher Al-Muaiqly",
};

/* ── Public state ────────────────────────────────────────── */
volatile int  audio_state  = AUDIO_STOPPED;
volatile int  audio_surah  = 0;
char          audio_reciter[64] = "";
static int    current_vol  = 80;

static int reciter_idx_of(const char*n){
    for(int i=0;i<RECITER_COUNT;i++)
        if(strcmp(RECITERS[i],n)==0) return i;
    return 0;
}

/* ── mpg123 ──────────────────────────────────────────────── */
static pid_t  mpg_pid  = -1;
static FILE  *mpg_in   = NULL;
static int    mpg_outfd= -1;

static void mpg_write(const char*c){
    if(mpg_in){ fprintf(mpg_in,"%s\n",c); fflush(mpg_in); }
}

static void mpg_start(void){
    if(mpg_pid>0) return;
    int ip[2],op[2];
    if(pipe(ip)<0||pipe(op)<0) return;
    pid_t pid=fork();
    if(pid<0){ close(ip[0]);close(ip[1]);close(op[0]);close(op[1]); return; }
    if(pid==0){
        dup2(ip[0],STDIN_FILENO); dup2(op[1],STDOUT_FILENO);
        int dn=open("/dev/null",O_WRONLY); dup2(dn,STDERR_FILENO); close(dn);
        close(ip[0]);close(ip[1]);close(op[0]);close(op[1]);
        setpriority(PRIO_PROCESS,0,-10);
        execlp("mpg123","mpg123","-R","--quiet",NULL); _exit(1);
    }
    close(ip[0]); close(op[1]);
    mpg_in=fdopen(ip[1],"w"); mpg_pid=pid; mpg_outfd=op[0];
    fcntl(mpg_outfd,F_SETFL,fcntl(mpg_outfd,F_GETFL,0)|O_NONBLOCK);
    char v[32]; snprintf(v,sizeof(v),"volume %d",current_vol); mpg_write(v);
}

static void mpg_kill(void){
    if(mpg_in){ mpg_write("quit"); fclose(mpg_in); mpg_in=NULL; }
    if(mpg_pid>0){ waitpid(mpg_pid,NULL,WNOHANG); kill(mpg_pid,SIGTERM);
                   waitpid(mpg_pid,NULL,0); mpg_pid=-1; }
    if(mpg_outfd>=0){ close(mpg_outfd); mpg_outfd=-1; }
}

/* ── Monitor thread — Core 1 ─────────────────────────────── */
static pthread_t mon_tid;
static volatile int mon_run=0;

static void *monitor_thread(void *arg){
    (void)arg;
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(1,&cs);
    pthread_setaffinity_np(pthread_self(),sizeof(cs),&cs);
    setpriority(PRIO_PROCESS,0,-10);
    char buf[256];
    while(mon_run){
        usleep(200000);
        if(mpg_outfd<0||audio_state!=AUDIO_PLAYING) continue;
        int n=read(mpg_outfd,buf,sizeof(buf)-1);
        if(n>0){
            buf[n]='\0';
            if(strstr(buf,"@P 0")){
                int next=audio_surah+1; if(next>114) next=1;
                audio_play(audio_reciter,next);
            }
        }
    }
    return NULL;
}

/* ── Duration thread — Core 2 ────────────────────────────── */
static pthread_t  dur_tid;
static volatile int dur_run=0;
static pthread_mutex_t dur_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  dur_cond =PTHREAD_COND_INITIALIZER;
static char  dur_path_req[256]="";
static volatile int dur_pending=0;
static int   dur_total_sec=0;
static time_t dur_play_start=0;
static int    dur_elapsed_at_pause=0;
static int    dur_is_paused=0;

static void *dur_thread(void *arg){
    (void)arg;
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(2,&cs);
    pthread_setaffinity_np(pthread_self(),sizeof(cs),&cs);
    setpriority(PRIO_PROCESS,0,5);
    while(dur_run){
        pthread_mutex_lock(&dur_mutex);
        while(!dur_pending&&dur_run)
            pthread_cond_wait(&dur_cond,&dur_mutex);
        char path[256]; strncpy(path,dur_path_req,255);
        dur_pending=0;
        pthread_mutex_unlock(&dur_mutex);
        if(!dur_run) break;
        char cmd[400];
        snprintf(cmd,sizeof(cmd),
            "timeout 5 ffprobe -v quiet -show_entries format=duration "
            "-of default=noprint_wrappers=1:nokey=1 \"%s\" 2>/dev/null",path);
        FILE*f=popen(cmd,"r");
        if(f){ float d=0; fscanf(f,"%f",&d); pclose(f);
               if(d>0) dur_total_sec=(int)d; }
    }
    return NULL;
}

static void dur_request(const char*path){
    pthread_mutex_lock(&dur_mutex);
    strncpy(dur_path_req,path,255);
    dur_pending=1;
    pthread_cond_signal(&dur_cond);
    pthread_mutex_unlock(&dur_mutex);
    dur_total_sec=0;
    dur_elapsed_at_pause=0;
    dur_is_paused=0;
    dur_play_start=time(NULL);
}

/* ════════════════════════════════════════════════════════════
   PUBLIC API
   ════════════════════════════════════════════════════════════ */
void audio_init(void){
    mpg_start();
    mon_run=1; pthread_create(&mon_tid,NULL,monitor_thread,NULL);
    dur_run=1; pthread_create(&dur_tid,NULL,dur_thread,NULL);
}

void audio_play(const char*rec,int sn){
    if(sn<1||sn>114) return;
    char path[256];
    snprintf(path,sizeof(path),"%s/%s/%03d.mp3",MP3_BASE,rec,sn);
    strncpy(audio_reciter,rec,63);
    audio_surah=sn; audio_state=AUDIO_PLAYING;
    mpg_start();
    char cmd[320]; snprintf(cmd,sizeof(cmd),"load %s",path); mpg_write(cmd);
    dur_request(path);
}

void audio_pause(void){
    if(audio_state==AUDIO_PLAYING){
        mpg_write("pause"); audio_state=AUDIO_PAUSED;
        dur_elapsed_at_pause=(int)(time(NULL)-dur_play_start);
        dur_is_paused=1;
    } else if(audio_state==AUDIO_PAUSED){
        mpg_write("pause"); audio_state=AUDIO_PLAYING;
        dur_play_start=time(NULL)-dur_elapsed_at_pause;
        dur_is_paused=0;
    }
}

void audio_stop(void){
    mpg_write("stop"); audio_state=AUDIO_STOPPED; audio_surah=0;
}

void audio_next(void){
    if(audio_surah<=0) return;
    int n=audio_surah+1; if(n>114)n=1; audio_play(audio_reciter,n);
}

void audio_prev(void){
    if(audio_surah<=0) return;
    int p=audio_surah-1; if(p<1)p=114; audio_play(audio_reciter,p);
}

void audio_vol_up(void){
    current_vol+=5; if(current_vol>100)current_vol=100;
    char c[32]; snprintf(c,sizeof(c),"volume %d",current_vol); mpg_write(c);
}

void audio_vol_dn(void){
    current_vol-=5; if(current_vol<0)current_vol=0;
    char c[32]; snprintf(c,sizeof(c),"volume %d",current_vol); mpg_write(c);
}

void audio_cleanup(void){
    mon_run=0; pthread_join(mon_tid,NULL);
    dur_run=0;
    pthread_mutex_lock(&dur_mutex); pthread_cond_signal(&dur_cond);
    pthread_mutex_unlock(&dur_mutex);
    pthread_join(dur_tid,NULL);
    mpg_kill(); audio_state=AUDIO_STOPPED;
}

/* ════════════════════════════════════════════════════════════
   NOW PLAYING SCREEN
   ════════════════════════════════════════════════════════════ */
#define BAR_X  (BOX_X+16)
#define BAR_W  (BOX_BW-32)
#define BAR_H  12

static void draw_progress(int y,int elapsed,int total){
    px_rect(BAR_X,y,BAR_W,BAR_H,0x2104);
    if(total>0){
        int f=(elapsed*BAR_W)/total; if(f>BAR_W)f=BAR_W;
        if(f>0) px_rect(BAR_X,y,f,BAR_H,0x3DEF);
        int dot=BAR_X+f;
        if(dot<=BAR_X+BAR_W-6) px_rect(dot,y-3,6,BAR_H+6,COL_WHITE);
    }
    char l[12],r[12];
    snprintf(l,sizeof(l),"%d:%02d",elapsed/60,elapsed%60);
    if(total>0) snprintf(r,sizeof(r),"%d:%02d",total/60,total%60);
    else        snprintf(r,sizeof(r),"--:--");
    px_str(BAR_X,y+BAR_H+6,l,COL_WHITE,SCALE);
    int tw=px_strw(r,SCALE); px_str(BAR_X+BAR_W-tw,y+BAR_H+6,r,COL_WHITE,SCALE);
}

static void draw_now_playing(const char*rd,int sn){
    px_rect(0,0,FB_W,FB_H,COL_BLACK);
    draw_title_px(0,"Now Playing");

    int help_h=8+(CH_H+6)*2;
    int hy=FB_H-help_h;
    px_hline(BOX_X,hy,BOX_BW,COL_GRAY); hy+=8;
    px_str(BOX_X+8,hy,"SPACE=Pause/Play   RIGHT=Next   LEFT=Prev",COL_GRAY,SCALE);
    hy+=CH_H+6;
    px_str(BOX_X+8,hy,"UP=Vol+   DOWN=Vol-   ESC=Back to List",COL_GRAY,SCALE);

    int bar_block=BAR_H+6+CH_H+6;
    int pill_h=CH_H+8;
    int vol_h=CH_H+14;
    int items_h=CH_H+10+CH_H+16+bar_block+12+pill_h+12+vol_h;
    int content_top=TITLE_PH+10;
    int content_bot=FB_H-help_h-8;
    int ty=content_top+(content_bot-content_top-items_h)/2;
    if(ty<content_top)ty=content_top;

    char line[80]; int tw;

    snprintf(line,sizeof(line),"%s",rd);
    tw=px_strw(line,SCALE); px_str((FB_W-tw)/2,ty,line,COL_GRAY,SCALE);
    ty+=CH_H+10;

    if(sn>=1&&sn<=114){
        snprintf(line,sizeof(line),"%d.  %s",sn,SURAH_NAME[sn]);
        tw=px_strw(line,SCALE); px_str((FB_W-tw)/2,ty,line,COL_WHITE,SCALE);
    }
    ty+=CH_H+16;

    int elapsed=dur_is_paused?dur_elapsed_at_pause:(int)(time(NULL)-dur_play_start);
    if(elapsed<0)elapsed=0;
    if(dur_total_sec>0&&elapsed>dur_total_sec)elapsed=dur_total_sec;
    draw_progress(ty,elapsed,dur_total_sec);
    ty+=bar_block+12;

    const char*st=audio_state==AUDIO_PLAYING?"PLAYING":
                  audio_state==AUDIO_PAUSED ?"PAUSED":"STOPPED";
    uint16_t pc=audio_state==AUDIO_PLAYING?0x0580:
                audio_state==AUDIO_PAUSED ?0x8400:0x2104;
    int pw=px_strw(st,SCALE)+24,px2=(FB_W-pw)/2;
    px_rect(px2,ty,pw,pill_h,pc); px_str(px2+12,ty+4,st,COL_WHITE,SCALE);
    ty+=pill_h+12;

    snprintf(line,sizeof(line),"Vol  %d%%",current_vol);
    tw=px_strw(line,SCALE); px_str((FB_W-tw)/2,ty,line,COL_GRAY,SCALE);
    int vbw=(current_vol*(BAR_W/2))/100,vbx=(FB_W-BAR_W/2)/2;
    px_rect(vbx,ty+CH_H+4,BAR_W/2,6,0x2104);
    if(vbw>0) px_rect(vbx,ty+CH_H+4,vbw,6,0x3DEF);

    menu_blit_full();
}

static void now_playing_loop(int ridx){
    draw_now_playing(RECITER_DISPLAY[ridx],audio_surah);
    struct termios t;
    tcgetattr(STDIN_FILENO,&t);
    t.c_cc[VMIN]=0; t.c_cc[VTIME]=10;
    tcsetattr(STDIN_FILENO,TCSANOW,&t);
    int done=0;
    while(!done){
        uint8_t ch=0; int n=read(STDIN_FILENO,&ch,1); int k=KEY_OTHER;
        if(n>0){
            if(ch=='q'||ch=='Q')        k=KEY_Q;
            else if(ch==' ')            k=KEY_SPACE;
            else if(ch=='\n'||ch=='\r') k=KEY_ENTER;
            else if(ch==127||ch==8)     k=KEY_BACK;
            else if(ch=='\x1b'){
                struct termios t2=t; t2.c_cc[VMIN]=0; t2.c_cc[VTIME]=1;
                tcsetattr(STDIN_FILENO,TCSANOW,&t2);
                uint8_t s[2]={0,0};
                if(read(STDIN_FILENO,&s[0],1)>0&&s[0]=='['){
                    read(STDIN_FILENO,&s[1],1);
                    if(s[1]=='A')      k=KEY_UP;
                    else if(s[1]=='B') k=KEY_DOWN;
                    else if(s[1]=='C') k=KEY_RIGHT;
                    else if(s[1]=='D') k=KEY_LEFT;
                } else k=KEY_ESC;
                tcsetattr(STDIN_FILENO,TCSANOW,&t);
            }
        }
        switch(k){
            case KEY_ESC: case KEY_BACK: done=1; break;
            case KEY_Q:   running=0; done=1; break;
            case KEY_SPACE: audio_pause(); break;
            case KEY_RIGHT: audio_next();  break;
            case KEY_LEFT:  audio_prev();  break;
            case KEY_UP:    audio_vol_up();break;
            case KEY_DOWN:  audio_vol_dn();break;
            default: break;
        }
        if(!done) draw_now_playing(RECITER_DISPLAY[ridx],audio_surah);
    }
    t.c_cc[VMIN]=1; t.c_cc[VTIME]=0;
    tcsetattr(STDIN_FILENO,TCSANOW,&t);
}

/* ════════════════════════════════════════════════════════════
   SURAH LIST
   ════════════════════════════════════════════════════════════ */
static void menu_surah_mp3(int ridx){
    static char labels[114][52];
    for(int i=0;i<114;i++)
        snprintf(labels[i],52,"%3d.  %s",i+1,SURAH_NAME[i+1]);
    int sel=0,offset=0;
    if(strcmp(audio_reciter,RECITERS[ridx])==0&&audio_surah>0){
        sel=audio_surah-1; offset=(sel>4)?sel-4:0;
    }
    int total_h=TITLE_PH+LIST_VIS*ITEM_PH+(CH_H+16);
    int title_y=(FB_H-total_h)/2; if(title_y<10)title_y=10;

    void full_draw(void){
        px_rect(0,0,FB_W,FB_H,COL_BLACK);
        char title[64];
        int pl=(audio_state!=AUDIO_STOPPED&&strcmp(audio_reciter,RECITERS[ridx])==0);
        snprintf(title,sizeof(title),"%s%s",RECITER_DISPLAY[ridx],pl?" [>]":"");
        draw_title_px(title_y,title);
        draw_counter_px(title_y,sel,114);
        int end=offset+LIST_VIS; if(end>114)end=114;
        for(int i=offset;i<end;i++)
            draw_item_px(item_y(title_y,i-offset),labels[i],i==sel);
        for(int i=end-offset;i<LIST_VIS;i++)
            draw_item_px(item_y(title_y,i),"",0);
        draw_help_px(item_y(title_y,LIST_VIS)+4,"ENTER=play  SPACE=pause  ESC=back");
        menu_blit_full();
    }
    full_draw();

    while(1){
        int k=read_key();
        if(k==KEY_ESC||k==KEY_BACK) return;
        if(k==KEY_Q){running=0;return;}
        if(k==KEY_ENTER){
            audio_play(RECITERS[ridx],sel+1);
            now_playing_loop(ridx);
            if(!running) return;
            full_draw(); continue;
        }
        if(k==KEY_SPACE){audio_pause();full_draw();}
        if(k==KEY_UP||k==KEY_DOWN){
            int prev=sel,poff=offset;
            if(k==KEY_DOWN){if(++sel>=114)sel=113;if(sel>=offset+LIST_VIS)offset++;}
            if(k==KEY_UP)  {if(--sel<0)sel=0;      if(sel<offset)offset--;}
            if(offset!=poff){full_draw();}
            else{
                draw_item_px(item_y(title_y,prev-offset),labels[prev],0);
                draw_item_px(item_y(title_y,sel-offset), labels[sel], 1);
                draw_counter_px(title_y,sel,114);
                menu_blit_rows(title_y,title_y+TITLE_PH);
                int y1=item_y(title_y,(prev<sel?prev:sel)-offset);
                int y2=item_y(title_y,(prev<sel?sel:prev)-offset)+ITEM_PH;
                menu_blit_rows(y1,y2);
            }
        }
    }
}

/* ════════════════════════════════════════════════════════════
   MAIN MP3 MENU
   ════════════════════════════════════════════════════════════ */
void menu_mp3(void){
    while(1){
        int playing=(audio_state!=AUDIO_STOPPED);
        int sn=audio_surah;
        static char lbls[RECITER_COUNT+1][52];
        static const char*items[RECITER_COUNT+1];
        int count=0;
        if(playing){
            snprintf(lbls[count],52,">> Now Playing: %s",
                     (sn>=1&&sn<=114)?SURAH_NAME[sn]:"...");
            items[count]=lbls[count]; count++;
        }
        for(int i=0;i<RECITER_COUNT;i++){
            int pl=(playing&&strcmp(audio_reciter,RECITERS[i])==0);
            snprintf(lbls[count],52,"  %s%s",RECITER_DISPLAY[i],pl?" [>]":"");
            items[count]=lbls[count]; count++;
        }
        int sel=run_menu_fb("Quran MP3  --  Select Reciter",items,count);
        if(sel<0) return;
        if(playing&&sel==0)
            now_playing_loop(reciter_idx_of(audio_reciter));
        else
            menu_surah_mp3(playing?sel-1:sel);
        if(!running) return;
    }
}
