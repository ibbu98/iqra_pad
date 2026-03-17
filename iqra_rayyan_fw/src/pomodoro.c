/*
 * pomodoro.c — Pomodoro Timer + Time/Date management
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <stdint.h>
#include "pomodoro.h"
#include "fb.h"
#include "menu.h"
#include "config.h"
#include "reader.h"

/* ── Session types ───────────────────────────────────────── */
#define POMO_WORK   25
#define POMO_SHORT   5
#define POMO_LONG   15

typedef enum { SESSION_WORK=0, SESSION_SHORT, SESSION_LONG } SessionType;
static const char *SESSION_NAMES[] = {"Work", "Short Break", "Long Break"};

/* custom durations */
static int custom_work  = POMO_WORK;
static int custom_short = POMO_SHORT;
static int custom_long  = POMO_LONG;

/* ── Session log ─────────────────────────────────────────── */
#define LOG_FILE "/home/ibbu98/iqra_pad_rayyan_version/pomodoro_log.txt"

static void log_session(SessionType type, int mins_completed){
    FILE*f=fopen(LOG_FILE,"a"); if(!f) return;
    time_t t=time(NULL); struct tm*tm=localtime(&t);
    fprintf(f,"%04d-%02d-%02d %02d:%02d | %-12s | %d min\n",
            tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,
            tm->tm_hour,tm->tm_min,
            SESSION_NAMES[type], mins_completed);
    fclose(f);
}

static void show_log(void){
    /* read log and show today's summary */
    px_rect(0,0,FB_W,FB_H,COL_BLACK);
    draw_title_px(0,"Pomodoro Log - Today");

    time_t now=time(NULL); struct tm*tm=localtime(&now);
    char today[12];
    snprintf(today,sizeof(today),"%04d-%02d-%02d",
             tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday);

    FILE*f=fopen(LOG_FILE,"r");
    int work_mins=0, work_sessions=0;
    int short_mins=0, long_mins=0;
    char line[128];
    if(f){
        while(fgets(line,sizeof(line),f)){
            if(strncmp(line,today,10)==0){
                int mins=0;
                if(strstr(line,"Work"))  { sscanf(strstr(line,"|")+1,"%*s %*s %d",&mins); work_mins+=mins; work_sessions++; }
                if(strstr(line,"Short")) { sscanf(strstr(line,"|")+1,"%*s %*s %d",&mins); short_mins+=mins; }
                if(strstr(line,"Long"))  { sscanf(strstr(line,"|")+1,"%*s %*s %d",&mins); long_mins+=mins; }
            }
        }
        fclose(f);
    }

    int ty=TITLE_PH+24;
    char buf[64];

    snprintf(buf,sizeof(buf),"Date:  %s",today);
    int tw=px_strw(buf,SCALE); px_str((FB_W-tw)/2,ty,buf,COL_GRAY,SCALE);
    ty+=CH_H+20;

    snprintf(buf,sizeof(buf),"Work Sessions:  %d",work_sessions);
    tw=px_strw(buf,SCALE); px_str((FB_W-tw)/2,ty,buf,COL_WHITE,SCALE);
    ty+=CH_H+12;

    snprintf(buf,sizeof(buf),"Total Work:     %d hr %d min",work_mins/60,work_mins%60);
    tw=px_strw(buf,SCALE); px_str((FB_W-tw)/2,ty,buf,COL_WHITE,SCALE);
    ty+=CH_H+12;

    snprintf(buf,sizeof(buf),"Short Breaks:   %d min",short_mins);
    tw=px_strw(buf,SCALE); px_str((FB_W-tw)/2,ty,buf,COL_GRAY,SCALE);
    ty+=CH_H+12;

    snprintf(buf,sizeof(buf),"Long Breaks:    %d min",long_mins);
    tw=px_strw(buf,SCALE); px_str((FB_W-tw)/2,ty,buf,COL_GRAY,SCALE);
    ty+=CH_H+28;

    /* streak */
    snprintf(buf,sizeof(buf),"Well done! Keep it up!");
    if(work_sessions==0) snprintf(buf,sizeof(buf),"No sessions yet today.");
    tw=px_strw(buf,SCALE); px_str((FB_W-tw)/2,ty,buf,COL_WHITE,SCALE);

    px_str(BOX_X+8,FB_H-40,"Press any key...",COL_GRAY,SCALE);
    menu_blit_full();
    read_key();
}
static void play_beep(void){
    /* generate a short sine wave beep using sox or ffmpeg */
    system("ffmpeg -y -f lavfi -i 'sine=frequency=880:duration=0.5' "
           "-q:a 2 /tmp/beep.mp3 >/dev/null 2>&1 && "
           "mpg123 -q /tmp/beep.mp3 >/dev/null 2>&1 &");
}

/* ── Time helpers ────────────────────────────────────────── */
void get_time_str(char *buf, int bufsz){
    time_t t=time(NULL);
    struct tm *tm=localtime(&t);
    snprintf(buf,bufsz,"%02d:%02d  %02d/%02d/%04d",
             tm->tm_hour,tm->tm_min,
             tm->tm_mon+1,tm->tm_mday,tm->tm_year+1900);
}

/* ── Set time/date via on-screen keyboard ────────────────── */
static int kbd_enter_str(const char*prompt, char*out, int outsz){
    /* Simple number entry using up/down on each digit */
    /* Format: HH:MM DD/MM/YYYY */
    /* Use existing keyboard from settings — just call system date */

    /* Draw input screen */
    px_rect(0,0,FB_W,FB_H,COL_BLACK);
    draw_title_px(0,"Set Time & Date");

    int ty=TITLE_PH+20;
    int tw=px_strw(prompt,SCALE);
    px_str((FB_W-tw)/2,ty,prompt,COL_GRAY,SCALE);
    ty+=CH_H+16;

    /* show format hint */
    const char*hint="Format:  HH MM DD MM YYYY";
    tw=px_strw(hint,SCALE); px_str((FB_W-tw)/2,ty,hint,COL_GRAY,SCALE);
    ty+=CH_H+8;

    /* input field */
    char inp[32]=""; int ilen=0;
    px_rect(BOX_X,ty,BOX_BW,CH_H+8,COL_BLACK);
    px_hline(BOX_X,ty+CH_H+7,BOX_BW,COL_GRAY);

    px_str(BOX_X+8,ty+4+CH_H+16,
           "Type numbers then ENTER  ESC=cancel",COL_GRAY,SCALE);
    menu_blit_full();

    /* raw input loop */
    struct termios t;
    tcgetattr(STDIN_FILENO,&t);
    t.c_cc[VMIN]=1; t.c_cc[VTIME]=0;
    tcsetattr(STDIN_FILENO,TCSANOW,&t);

    while(1){
        uint8_t ch=0;
        if(read(STDIN_FILENO,&ch,1)<=0) continue;
        if(ch=='\x1b'){ out[0]='\0'; return 0; }
        if(ch=='\n'||ch=='\r'){ strncpy(out,inp,outsz-1); return 1; }
        if((ch==127||ch==8)&&ilen>0){ inp[--ilen]='\0'; }
        else if(ch>=' '&&ch<='~'&&ilen<outsz-2){ inp[ilen++]=ch; inp[ilen]='\0'; }
        /* redraw input */
        px_rect(BOX_X,ty,BOX_BW,CH_H+8,COL_BLACK);
        px_hline(BOX_X,ty+CH_H+7,BOX_BW,COL_GRAY);
        char disp[64]; snprintf(disp,sizeof(disp),"%s|",inp);
        px_str(BOX_X+8,ty+4,disp,COL_WHITE,SCALE);
        menu_blit_rows(ty,ty+CH_H+10);
    }
}

void menu_set_datetime(void){
    px_rect(0,0,FB_W,FB_H,COL_BLACK);
    draw_title_px(0,"Set Time & Date");

    /* Show current time */
    int ty=TITLE_PH+20;
    char cur[64]; get_time_str(cur,sizeof(cur));
    char line[80]; snprintf(line,sizeof(line),"Current: %s",cur);
    int tw=px_strw(line,SCALE); px_str((FB_W-tw)/2,ty,line,COL_WHITE,SCALE);
    ty+=CH_H+24;

    /* Instructions */
    const char*lines[]={
        "Enter each value and press ENTER",
        "Values: Hour  Minute  Day  Month  Year",
        "",
        "Example: 14 30 25 03 2026",
    };
    for(int i=0;i<4;i++){
        tw=px_strw(lines[i],SCALE);
        px_str((FB_W-tw)/2,ty,lines[i],COL_GRAY,SCALE);
        ty+=CH_H+6;
    }
    menu_blit_full();
    sleep(2);

    /* Get input */
    char inp[64]="";
    if(!kbd_enter_str("Enter: HH MM DD MM YYYY",inp,sizeof(inp))) return;

    int hh=0,mm=0,dd=0,mo=0,yy=0;
    if(sscanf(inp,"%d %d %d %d %d",&hh,&mm,&dd,&mo,&yy)!=5){
        px_rect(0,0,FB_W,FB_H,COL_BLACK);
        draw_title_px(0,"Set Time & Date");
        const char*err="Invalid format. Try: 14 30 25 03 2026";
        tw=px_strw(err,SCALE); px_str((FB_W-tw)/2,FB_H/2,err,COL_WHITE,SCALE);
        menu_blit_full(); sleep(2); return;
    }

    /* Apply via sudo date */
    char cmd[128];
    snprintf(cmd,sizeof(cmd),
             "sudo date -s '%04d-%02d-%02d %02d:%02d:00' >/dev/null 2>&1",
             yy,mo,dd,hh,mm);
    system(cmd);

    /* Confirm */
    get_time_str(cur,sizeof(cur));
    snprintf(line,sizeof(line),"Set to: %s",cur);
    px_rect(0,0,FB_W,FB_H,COL_BLACK);
    draw_title_px(0,"Time Set!");
    tw=px_strw(line,SCALE); px_str((FB_W-tw)/2,FB_H/2,line,COL_WHITE,SCALE);
    menu_blit_full(); sleep(2);
}

/* ════════════════════════════════════════════════════════════
   POMODORO TIMER
   ════════════════════════════════════════════════════════════ */
static void draw_pomo_screen(SessionType type, int remaining_sec,
                              int session_num, int running_flag){
    px_rect(0,0,FB_W,FB_H,COL_BLACK);
    draw_title_px(0,"Pomodoro Timer");

    /* session number */
    int ty=TITLE_PH+16;
    char line[64];
    snprintf(line,sizeof(line),"Session %d   %s",
             session_num, SESSION_NAMES[type]);
    int tw=px_strw(line,SCALE);
    px_str((FB_W-tw)/2,ty,line,COL_GRAY,SCALE);
    ty+=CH_H+20;

    /* big countdown */
    int mins=remaining_sec/60, secs=remaining_sec%60;
    char timer[16]; snprintf(timer,sizeof(timer),"%02d:%02d",mins,secs);

    /* draw at 4x scale for visibility */
    int timer_scale=4;
    int timer_w=px_strw(timer,timer_scale);
    int timer_x=(FB_W-timer_w)/2;

    /* color: green=work, blue=short, purple=long */
    uint16_t timer_col = type==SESSION_WORK  ? 0x07E0 :
                         type==SESSION_SHORT ? 0x001F : 0xF81F;

    /* draw each char at scale 4 */
    int cx=timer_x;
    for(int i=0;timer[i];i++){
        px_glyph(cx,ty,(unsigned char)timer[i],timer_col,timer_scale);
        cx+=FONT_W*timer_scale;
    }
    ty+=FONT_H*timer_scale+20;

    /* progress bar */
    int total_sec;
    if(type==SESSION_WORK)       total_sec=custom_work*60;
    else if(type==SESSION_SHORT) total_sec=custom_short*60;
    else                         total_sec=custom_long*60;

    int elapsed=total_sec-remaining_sec;
    int bar_x=BOX_X+16, bar_w=BOX_BW-32, bar_h=16;
    px_rect(bar_x,ty,bar_w,bar_h,0x2104);
    if(total_sec>0){
        int filled=(elapsed*bar_w)/total_sec;
        if(filled>bar_w)filled=bar_w;
        if(filled>0) px_rect(bar_x,ty,filled,bar_h,timer_col);
    }
    ty+=bar_h+20;

    /* status */
    const char*status=running_flag?"[ RUNNING ]":"[ PAUSED  ]";
    tw=px_strw(status,SCALE);
    uint16_t sc=running_flag?0x0580:0x8400;
    int sw=tw+24,sx=(FB_W-sw)/2;
    px_rect(sx,ty,sw,CH_H+8,sc);
    px_str(sx+12,ty+4,status,COL_WHITE,SCALE);
    ty+=CH_H+24;

    /* current time */
    char tstr[32]; get_time_str(tstr,sizeof(tstr));
    tw=px_strw(tstr,SCALE); px_str((FB_W-tw)/2,ty,tstr,COL_GRAY,SCALE);

    /* help at bottom */
    int hy=FB_H-CH_H-16;
    px_hline(BOX_X,hy-6,BOX_BW,COL_GRAY);
    px_str(BOX_X+8,hy,"SPACE=pause  n=next  r=reset  ESC=back",COL_GRAY,SCALE);

    menu_blit_full();
}

static void pomo_beep(int times){
    for(int i=0;i<times;i++){
        play_beep();
        if(i<times-1) sleep(1);
    }
}

static void run_pomodoro_session(SessionType type, int *session_num){
    int total_mins;
    if(type==SESSION_WORK)       total_mins=custom_work;
    else if(type==SESSION_SHORT) total_mins=custom_short;
    else                         total_mins=custom_long;

    int remaining=total_mins*60;
    int is_running=1;

    /* non-blocking input */
    struct termios t;
    tcgetattr(STDIN_FILENO,&t);
    t.c_cc[VMIN]=0; t.c_cc[VTIME]=0;
    tcsetattr(STDIN_FILENO,TCSANOW,&t);

    draw_pomo_screen(type,remaining,*session_num,is_running);

    time_t last_tick=time(NULL);

    while(remaining>0){
        usleep(100000); /* check 10x per second */

        /* tick */
        if(is_running){
            time_t now=time(NULL);
            int diff=(int)(now-last_tick);
            if(diff>=1){
                remaining-=diff;
                last_tick=now;
                if(remaining<0)remaining=0;
                draw_pomo_screen(type,remaining,*session_num,is_running);
            }
        }

        /* check key */
        uint8_t ch=0;
        if(read(STDIN_FILENO,&ch,1)>0){
            if(ch=='\x1b'){ /* ESC */
                /* restore */
                t.c_cc[VMIN]=1; t.c_cc[VTIME]=0;
                tcsetattr(STDIN_FILENO,TCSANOW,&t);
                return;
            }
            if(ch==' '){
                is_running=!is_running;
                if(is_running) last_tick=time(NULL);
                draw_pomo_screen(type,remaining,*session_num,is_running);
            }
            if(ch=='n'||ch=='N'){ remaining=0; break; } /* skip */
            if(ch=='r'||ch=='R'){
                remaining=total_mins*60;
                last_tick=time(NULL);
                draw_pomo_screen(type,remaining,*session_num,is_running);
            }
        }
    }

    /* restore input */
    t.c_cc[VMIN]=1; t.c_cc[VTIME]=0;
    tcsetattr(STDIN_FILENO,TCSANOW,&t);

    /* session done! */
    if(remaining==0){
        /* calculate actual minutes completed */
        int total_mins2 = type==SESSION_WORK?custom_work:
                          type==SESSION_SHORT?custom_short:custom_long;
        log_session(type, total_mins2);
        /* beep: 1 for break, 3 for work done */
        pomo_beep(type==SESSION_WORK?3:1);

        /* show completion screen */
        px_rect(0,0,FB_W,FB_H,COL_BLACK);
        draw_title_px(0,"Session Complete!");
        char msg[64];
        snprintf(msg,sizeof(msg),"%s done!",SESSION_NAMES[type]);
        int tw=px_strw(msg,SCALE); px_str((FB_W-tw)/2,FB_H/2-CH_H,msg,COL_WHITE,SCALE);
        const char*next_hint=type==SESSION_WORK?"Take a break!":"Back to work!";
        tw=px_strw(next_hint,SCALE);
        px_str((FB_W-tw)/2,FB_H/2+CH_H,next_hint,COL_GRAY,SCALE);
        px_str(BOX_X+8,FB_H-40,"Press any key to continue...",COL_GRAY,SCALE);
        menu_blit_full();

        /* restore and wait for key */
        t.c_cc[VMIN]=1; t.c_cc[VTIME]=0;
        tcsetattr(STDIN_FILENO,TCSANOW,&t);
        read_key();

        if(type==SESSION_WORK) (*session_num)++;
    }
}

static void menu_pomo_settings(void){
    while(1){
        char w[40],s[40],l[40];
        snprintf(w,sizeof(w),"Work session:    %d min",custom_work);
        snprintf(s,sizeof(s),"Short break:     %d min",custom_short);
        snprintf(l,sizeof(l),"Long break:      %d min",custom_long);
        const char*items[]={w,s,l,"Reset defaults","Back"};
        int sel=run_menu_fb("Pomodoro Settings",items,5);
        if(sel<0||sel==4) return;

        /* adjust with up/down */
        int *target = sel==0?&custom_work:sel==1?&custom_short:&custom_long;
        if(sel==3){ custom_work=POMO_WORK; custom_short=POMO_SHORT; custom_long=POMO_LONG; continue; }

        char title[40];
        snprintf(title,sizeof(title),"Set minutes (current: %d)",*target);
        const char*adj[]={"+ 1 minute","- 1 minute","+ 5 minutes","- 5 minutes","Done"};
        while(1){
            char cur[40]; snprintf(cur,sizeof(cur),"Current: %d minutes",*target);
            const char*aditems[]={cur,adj[0],adj[1],adj[2],adj[3],adj[4]};
            int a=run_menu_fb(title,aditems,6);
            if(a<0||a==5) break;
            if(a==1){ (*target)++; if(*target>99)*target=99; }
            if(a==2){ (*target)--; if(*target<1)*target=1; }
            if(a==3){ (*target)+=5; if(*target>99)*target=99; }
            if(a==4){ (*target)-=5; if(*target<1)*target=1; }
        }
    }
}

void menu_pomodoro(void){
    int session_num=1;
    while(1){
        /* show current time in title */
        char title[64]="Pomodoro Timer";
        char tstr[32]; get_time_str(tstr,sizeof(tstr));

        char w[40],s[40],l[40],dt[52];
        snprintf(w,sizeof(w),"Work          %d min",custom_work);
        snprintf(s,sizeof(s),"Short Break   %d min",custom_short);
        snprintf(l,sizeof(l),"Long Break    %d min",custom_long);
        snprintf(dt,sizeof(dt),"Set Time/Date   [%s]",tstr);

        const char*items[]={w,s,l,dt,"View Today's Log","Settings","Back"};
        int sel=run_menu_fb(title,items,7);
        if(sel<0||sel==6) return;

        if(sel==0) run_pomodoro_session(SESSION_WORK,&session_num);
        if(sel==1) run_pomodoro_session(SESSION_SHORT,&session_num);
        if(sel==2) run_pomodoro_session(SESSION_LONG,&session_num);
        if(sel==3) menu_set_datetime();
        if(sel==4) show_log();
        if(sel==5) menu_pomo_settings();
        if(!running) return;
    }
}
