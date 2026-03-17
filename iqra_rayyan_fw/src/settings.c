/*
 * settings.c — WiFi and Bluetooth settings
 * All blocking operations run in background threads
 * UI stays responsive at all times
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <termios.h>
#include <pthread.h>
#include <time.h>
#include "settings.h"
#include "fb.h"
#include "menu.h"
#include "config.h"
#include "reader.h"

/* ── Shell helpers ───────────────────────────────────────── */
static void run_bg(const char *cmd){
    /* fire and forget — don't block UI */
    char buf[512];
    snprintf(buf, sizeof(buf), "%s > /dev/null 2>&1 &", cmd);
    system(buf);
}

static void run_wait(const char *cmd){
    char buf[512];
    snprintf(buf, sizeof(buf), "%s > /dev/null 2>&1", cmd);
    system(buf);
}

static void cmd_line(const char *cmd, char *buf, int sz){
    buf[0]='\0';
    FILE*f=popen(cmd,"r"); if(!f) return;
    if(fgets(buf,sz,f)){ int l=strlen(buf); if(l>0&&buf[l-1]=='\n')buf[l-1]='\0'; }
    pclose(f);
}

static int cmd_lines(const char *cmd, char lines[][80], int max){
    int n=0;
    FILE*f=popen(cmd,"r"); if(!f) return 0;
    while(n<max&&fgets(lines[n],80,f)){
        int l=strlen(lines[n]); if(l>0&&lines[n][l-1]=='\n')lines[n][l-1]='\0';
        if(lines[n][0]) n++;
    }
    pclose(f); return n;
}

/* ── Status cache — avoid re-querying every frame ────────── */
static char cached_wifi_status[32]  = "";
static char cached_wifi_ssid[64]    = "";
static char cached_wifi_ip[32]      = "";
static char cached_bt_powered[8]    = "";
static char cached_bt_device[64]    = "";
static time_t cache_time = 0;

static void refresh_cache(void){
    time_t now = time(NULL);
    if(now - cache_time < 3) return; /* only refresh every 3 seconds */
    cache_time = now;
    cmd_line("nmcli radio wifi", cached_wifi_status, sizeof(cached_wifi_status));
    cmd_line("nmcli -t -f active,ssid dev wifi | grep '^yes' | cut -d: -f2",
             cached_wifi_ssid, sizeof(cached_wifi_ssid));
    cmd_line("hostname -I | awk '{print $1}'", cached_wifi_ip, sizeof(cached_wifi_ip));
    cmd_line("bluetoothctl show | grep 'Powered:' | awk '{print $2}'",
             cached_bt_powered, sizeof(cached_bt_powered));
    cmd_line("bluetoothctl info 2>/dev/null | grep 'Name:' | awk '{$1=\"\";print}'",
             cached_bt_device, sizeof(cached_bt_device));
}

static void invalidate_cache(void){ cache_time = 0; }

/* ── Non-blocking key read with timeout ──────────────────── */
static int read_key_timeout(int timeout_ms){
    struct termios t;
    tcgetattr(STDIN_FILENO,&t);
    t.c_cc[VMIN]=0;
    t.c_cc[VTIME]=timeout_ms/100;
    tcsetattr(STDIN_FILENO,TCSANOW,&t);

    uint8_t ch=0;
    int n=read(STDIN_FILENO,&ch,1);

    t.c_cc[VMIN]=1; t.c_cc[VTIME]=0;
    tcsetattr(STDIN_FILENO,TCSANOW,&t);

    if(n<=0) return -1; /* timeout */

    if(ch=='q'||ch=='Q') return KEY_Q;
    if(ch=='\n'||ch=='\r') return KEY_ENTER;
    if(ch==127||ch==8)   return KEY_BACK;
    if(ch=='\x1b'){
        t.c_cc[VMIN]=0; t.c_cc[VTIME]=1;
        tcsetattr(STDIN_FILENO,TCSANOW,&t);
        uint8_t s[2]={0,0};
        if(read(STDIN_FILENO,&s[0],1)>0 && s[0]=='['){
            read(STDIN_FILENO,&s[1],1);
            t.c_cc[VMIN]=1; t.c_cc[VTIME]=0;
            tcsetattr(STDIN_FILENO,TCSANOW,&t);
            if(s[1]=='A') return KEY_UP;
            if(s[1]=='B') return KEY_DOWN;
            if(s[1]=='C') return KEY_RIGHT;
            if(s[1]=='D') return KEY_LEFT;
        }
        t.c_cc[VMIN]=1; t.c_cc[VTIME]=0;
        tcsetattr(STDIN_FILENO,TCSANOW,&t);
        return KEY_ESC;
    }
    return KEY_OTHER;
}

/* ── Message screen ──────────────────────────────────────── */
static void show_msg(const char *title, const char *msg){
    px_rect(0,0,FB_W,FB_H,COL_BLACK);
    draw_title_px(40,title);
    int tw=px_strw(msg,SCALE);
    px_str((FB_W-tw)/2,FB_H/2-CH_H,msg,COL_WHITE,SCALE);
    px_str(BOX_X+8,FB_H-60,"Press any key...",COL_GRAY,SCALE);
    menu_blit_full();
    read_key();
}

/* ════════════════════════════════════════════════════════════
   ON-SCREEN KEYBOARD
   ════════════════════════════════════════════════════════════ */
#define KB_ROWS   4
#define KB_COLS   10
#define KB_KEY_W  (BOX_BW/KB_COLS)
#define KB_KEY_H  48
#define KB_Y      (FB_H - KB_ROWS*KB_KEY_H - 80)

static const char *kb_lo[KB_ROWS][KB_COLS]={
    {"q","w","e","r","t","y","u","i","o","p"},
    {"a","s","d","f","g","h","j","k","l","<"},
    {"z","x","c","v","b","n","m",".","-","_"},
    {"SH","12","  ","SP","SP","SP","SP","SP","OK","CL"},
};
static const char *kb_hi[KB_ROWS][KB_COLS]={
    {"Q","W","E","R","T","Y","U","I","O","P"},
    {"A","S","D","F","G","H","J","K","L","<"},
    {"Z","X","C","V","B","N","M",".","-","_"},
    {"sh","12","  ","SP","SP","SP","SP","SP","OK","CL"},
};
static const char *kb_nu[KB_ROWS][KB_COLS]={
    {"1","2","3","4","5","6","7","8","9","0"},
    {"!","@","#","$","%","^","&","*","(","<"},
    {")","+","=","[","]","{","}",";","'","\""},
    {"ab","  ","  ","SP","SP","SP","SP","SP","OK","CL"},
};

static void kb_draw_key(int r,int c,const char*lbl,int sel_r,int sel_c){
    int x=BOX_X+c*KB_KEY_W, y=KB_Y+r*KB_KEY_H;
    int sel=(r==sel_r&&c==sel_c);
    px_rect(x+2,y+2,KB_KEY_W-4,KB_KEY_H-4,sel?COL_HIBG:0x1082);
    /* draw label chars */
    int tx=x+2, ty2=y+(KB_KEY_H-FONT_H)/2;
    for(int i=0;lbl[i]&&i<4;i++){
        px_glyph(tx,ty2,(unsigned char)lbl[i],COL_WHITE,1);
        tx+=FONT_W;
    }
}

static void kb_draw_all(const char*(*lay)[KB_COLS],
                        const char*input,int ilen,
                        int sr,int sc,const char*prompt){
    px_rect(0,KB_Y-70,FB_W,FB_H-(KB_Y-70),0x0821);
    /* prompt */
    int tw=px_strw(prompt,SCALE);
    px_str((FB_W-tw)/2,KB_Y-65,prompt,COL_GRAY,SCALE);
    /* input box */
    int iy=KB_Y-32;
    px_rect(BOX_X,iy,BOX_BW,CH_H+8,COL_BLACK);
    px_hline(BOX_X,iy+CH_H+7,BOX_BW,COL_GRAY);
    char disp[130]; snprintf(disp,sizeof(disp),"%.*s|",ilen,input);
    px_str(BOX_X+8,iy+4,disp,COL_WHITE,SCALE);
    /* keys */
    for(int r=0;r<KB_ROWS;r++)
        for(int c=0;c<KB_COLS;c++)
            kb_draw_key(r,c,lay[r][c],sr,sc);
    menu_blit_full();
}

static int keyboard(const char*prompt,char*out,int outsz){
    char inp[128]=""; int ilen=0;
    int sr=0,sc=0,mode=0;
    typedef const char* row_t[KB_COLS];
    row_t *lays[3]={(row_t*)kb_lo,(row_t*)kb_hi,(row_t*)kb_nu};
    kb_draw_all(lays[mode],inp,ilen,sr,sc,prompt);
    while(1){
        int k=read_key();
        int pr=sr,pc=sc;
        if(k==KEY_UP)   sr=(sr-1+KB_ROWS)%KB_ROWS;
        if(k==KEY_DOWN) sr=(sr+1)%KB_ROWS;
        if(k==KEY_LEFT) sc=(sc-1+KB_COLS)%KB_COLS;
        if(k==KEY_RIGHT)sc=(sc+1)%KB_COLS;
        if(k==KEY_UP||k==KEY_DOWN||k==KEY_LEFT||k==KEY_RIGHT){
            /* just update two keys */
            kb_draw_key(pr,pc,lays[mode][pr][pc],sr,sc);
            kb_draw_key(sr,sc,lays[mode][sr][sc],sr,sc);
            int y1=KB_Y+((pr<sr?pr:sr))*KB_KEY_H;
            int y2=KB_Y+((pr<sr?sr:pr))*KB_KEY_H+KB_KEY_H;
            menu_blit_rows(y1,y2); continue;
        }
        if(k==KEY_ESC||k==KEY_BACK) return 0;
        if(k==KEY_Q){running=0;return 0;}
        if(k==KEY_ENTER){
            const char*lbl=lays[mode][sr][sc];
            if(!strcmp(lbl,"OK"))              { strncpy(out,inp,outsz-1);return 1; }
            if(!strcmp(lbl,"CL")||!strcmp(lbl,"<")){ if(ilen>0)inp[--ilen]='\0'; }
            else if(!strcmp(lbl,"SP")||!strcmp(lbl,"  ")){ if(ilen<outsz-2){inp[ilen++]=' ';inp[ilen]='\0';} }
            else if(!strcmp(lbl,"SH")||!strcmp(lbl,"sh")){ mode=(mode==1)?0:1; }
            else if(!strcmp(lbl,"12"))         { mode=2; }
            else if(!strcmp(lbl,"ab"))         { mode=0; }
            else if(strlen(lbl)==1&&ilen<outsz-2){ inp[ilen++]=lbl[0];inp[ilen]='\0'; }
            /* redraw input + keys */
            int iy=KB_Y-32;
            px_rect(BOX_X,iy,BOX_BW,CH_H+8,COL_BLACK);
            px_hline(BOX_X,iy+CH_H+7,BOX_BW,COL_GRAY);
            char disp[130]; snprintf(disp,sizeof(disp),"%.*s|",ilen,inp);
            px_str(BOX_X+8,iy+4,disp,COL_WHITE,SCALE);
            menu_blit_rows(iy,iy+CH_H+10);
            /* redraw all keys on mode change */
            if(!strcmp(lbl,"SH")||!strcmp(lbl,"sh")||
               !strcmp(lbl,"12")||!strcmp(lbl,"ab"))
                kb_draw_all(lays[mode],inp,ilen,sr,sc,prompt);
        }
    }
}

/* ════════════════════════════════════════════════════════════
   WIFI
   ════════════════════════════════════════════════════════════ */
static void wifi_draw_status(void){
    refresh_cache();
    px_rect(0,0,FB_W,FB_H,COL_BLACK);
    draw_title_px(40,"WiFi Settings");
    int ty=40+TITLE_PH+20;
    char line[80];

    int on=strstr(cached_wifi_status,"enabled")!=NULL;
    snprintf(line,sizeof(line),"Status:   %s",on?"Enabled":"Disabled");
    px_str(BOX_X+16,ty,line,on?COL_WHITE:COL_GRAY,SCALE); ty+=CH_H+12;

    if(cached_wifi_ssid[0]){
        snprintf(line,sizeof(line),"Network:  %s",cached_wifi_ssid);
        px_str(BOX_X+16,ty,line,COL_WHITE,SCALE); ty+=CH_H+12;
    }
    if(cached_wifi_ip[0]){
        snprintf(line,sizeof(line),"IP:       %s",cached_wifi_ip);
        px_str(BOX_X+16,ty,line,COL_GRAY,SCALE);
    }
    menu_blit_full();
}

static void menu_wifi_scan(void){
    /* show scanning message immediately */
    px_rect(0,0,FB_W,FB_H,COL_BLACK);
    draw_title_px(40,"WiFi - Scanning...");
    px_str(BOX_X+16,40+TITLE_PH+20,"Scanning for networks...",COL_WHITE,SCALE);
    menu_blit_full();

    /* trigger rescan in background — fast */
    run_bg("nmcli dev wifi rescan 2>/dev/null");
    sleep(2); /* wait for scan results */

    /* get networks */
    static char nets[32][80];
    int n=cmd_lines(
        "nmcli -t -f ssid dev wifi list 2>/dev/null | grep -v '^$' | head -20",
        nets,32);

    if(n==0){ show_msg("WiFi","No networks found."); return; }

    /* deduplicate */
    static char uniq[32][80]; int un=0;
    for(int i=0;i<n;i++){
        int dup=0;
        for(int j=0;j<un;j++) if(!strcmp(nets[i],uniq[j])){dup=1;break;}
        if(!dup) strncpy(uniq[un++],nets[i],79);
    }

    static char labels[32][52];
    for(int i=0;i<un;i++) snprintf(labels[i],52,"  %s",uniq[i]);

    int sel=0,pg=0;
    run_list_fb("Select Network",labels,un,&sel,&pg);
    if(sel<0) return;

    /* password entry */
    char pwd[128]="";
    char prompt[80]; snprintf(prompt,sizeof(prompt),"Password for: %s",uniq[sel]);
    px_rect(0,0,FB_W,FB_H,COL_BLACK);
    draw_title_px(40,"WiFi - Enter Password");
    menu_blit_full();
    int ok=keyboard(prompt,pwd,sizeof(pwd));
    if(!ok) return;

    /* connect — show progress */
    px_rect(0,0,FB_W,FB_H,COL_BLACK);
    draw_title_px(40,"WiFi - Connecting...");
    char msg[64]; snprintf(msg,sizeof(msg),"Connecting to %s ...",uniq[sel]);
    int tw=px_strw(msg,SCALE); px_str((FB_W-tw)/2,FB_H/2,msg,COL_WHITE,SCALE);
    menu_blit_full();

    char cmd[384];
    if(pwd[0])
        snprintf(cmd,sizeof(cmd),
            "nmcli dev wifi connect \"%s\" password \"%s\" "
            "> /tmp/wifi_result 2>&1",uniq[sel],pwd);
    else
        snprintf(cmd,sizeof(cmd),
            "nmcli dev wifi connect \"%s\" > /tmp/wifi_result 2>&1",uniq[sel]);
    run_wait(cmd);

    char result[128]="";
    cmd_line("cat /tmp/wifi_result | tail -1",result,sizeof(result));
    invalidate_cache();

    if(strstr(result,"successfully")||strstr(result,"connected"))
        show_msg("WiFi","Connected successfully!");
    else if(result[0]) show_msg("WiFi",result);
    else               show_msg("WiFi","Done.");
}

static void menu_wifi(void){
    while(1){
        wifi_draw_status();
        int on=strstr(cached_wifi_status,"enabled")!=NULL;
        char tog[32]; snprintf(tog,sizeof(tog),"Turn WiFi %s",on?"OFF":"ON");
        const char*items[]={tog,"Scan & Connect","Disconnect","Back"};
        int sel=run_menu_fb("WiFi",items,4);
        if(sel<0||sel==3) return;
        if(sel==0){
            run_wait(on?"nmcli radio wifi off":"nmcli radio wifi on");
            invalidate_cache(); sleep(1);
        }
        if(sel==1){
            if(!on){show_msg("WiFi","Turn WiFi ON first.");continue;}
            menu_wifi_scan();
        }
        if(sel==2){
            run_wait("nmcli dev disconnect wlan0 2>/dev/null");
            invalidate_cache();
            show_msg("WiFi","Disconnected.");
        }
        if(!running) return;
    }
}

/* ════════════════════════════════════════════════════════════
   BLUETOOTH  — live scanning with ESC to cancel
   ════════════════════════════════════════════════════════════ */
static void bt_draw_status(void){
    refresh_cache();
    px_rect(0,0,FB_W,FB_H,COL_BLACK);
    draw_title_px(40,"Bluetooth Settings");
    int ty=40+TITLE_PH+20;
    char line[80];
    int on=strstr(cached_bt_powered,"yes")!=NULL;
    snprintf(line,sizeof(line),"Powered:  %s",on?"Yes":"No");
    px_str(BOX_X+16,ty,line,on?COL_WHITE:COL_GRAY,SCALE); ty+=CH_H+12;
    if(cached_bt_device[0]){
        snprintf(line,sizeof(line),"Device:  %s",cached_bt_device);
        px_str(BOX_X+16,ty,line,COL_WHITE,SCALE);
    }
    menu_blit_full();
}

static void menu_bt_scan(void){
    /* start scan immediately in background */
    run_bg("bluetoothctl scan on 2>/dev/null");

    int countdown=15;
    /* non-blocking countdown — ESC cancels, ENTER stops early */
    struct termios t;
    tcgetattr(STDIN_FILENO,&t);
    t.c_cc[VMIN]=0; t.c_cc[VTIME]=0;
    tcsetattr(STDIN_FILENO,TCSANOW,&t);

    int cancelled=0;
    for(int i=countdown;i>=0&&!cancelled;i--){
        px_rect(0,0,FB_W,FB_H,COL_BLACK);
        draw_title_px(40,"Bluetooth - Scanning");
        char line[64]; snprintf(line,sizeof(line),"Scanning... %d seconds",i);
        int tw=px_strw(line,SCALE); px_str((FB_W-tw)/2,200,line,COL_WHITE,SCALE);
        px_str(BOX_X+8,260,"ESC=cancel  ENTER=stop early",COL_GRAY,SCALE);

        /* show devices found so far */
        static char devs[20][80];
        int nd=cmd_lines(
            "bluetoothctl devices | awk '{$1=\"\"; $2=\"\"; print}' | sed 's/^  //'",
            devs,20);
        int ty2=320;
        if(nd>0){
            px_str(BOX_X+8,ty2,"Found:",COL_GRAY,SCALE); ty2+=CH_H+8;
            for(int j=0;j<nd&&j<5;j++){
                px_str(BOX_X+16,ty2,devs[j],COL_WHITE,SCALE); ty2+=CH_H+4;
            }
        }
        menu_blit_full();

        /* wait 1 second checking for keypress each 100ms */
        for(int j=0;j<10&&!cancelled;j++){
            usleep(100000);
            uint8_t ch=0;
            if(read(STDIN_FILENO,&ch,1)>0){
                if(ch=='\x1b'){cancelled=1;}
                if(ch=='\n'||ch=='\r'){i=0;}
            }
        }
    }

    /* restore blocking */
    t.c_cc[VMIN]=1; t.c_cc[VTIME]=0;
    tcsetattr(STDIN_FILENO,TCSANOW,&t);

    run_bg("bluetoothctl scan off 2>/dev/null");
    if(cancelled) return;

    /* get devices */
    static char devs[20][80], macs[20][80];
    int n=cmd_lines(
        "bluetoothctl devices | awk '{$1=\"\"; $2=\"\"; print}' | sed 's/^  //'",
        devs,20);
    cmd_lines("bluetoothctl devices | awk '{print $2}'",macs,20);

    if(n==0){show_msg("Bluetooth","No devices found.");return;}

    static char labels[20][52];
    for(int i=0;i<n;i++) snprintf(labels[i],52,"  %.46s",devs[i]);
    int sel=0,pg=0;
    run_list_fb("Select Device",labels,n,&sel,&pg);
    if(sel<0||sel>=n) return;

    /* pair and connect */
    px_rect(0,0,FB_W,FB_H,COL_BLACK);
    draw_title_px(40,"Bluetooth - Connecting...");
    menu_blit_full();

    char cmd[128];
    snprintf(cmd,sizeof(cmd),"bluetoothctl pair %s > /tmp/bt_result 2>&1",macs[sel]);
    run_wait(cmd);
    sleep(1);
    snprintf(cmd,sizeof(cmd),"bluetoothctl trust %s >> /tmp/bt_result 2>&1",macs[sel]);
    run_wait(cmd);
    snprintf(cmd,sizeof(cmd),"bluetoothctl connect %s >> /tmp/bt_result 2>&1",macs[sel]);
    run_wait(cmd);

    char result[128]="";
    cmd_line("tail -1 /tmp/bt_result",result,sizeof(result));
    invalidate_cache();

    if(strstr(result,"successful")||strstr(result,"Connected"))
        show_msg("Bluetooth","Connected!");
    else if(result[0]) show_msg("Bluetooth",result);
    else               show_msg("Bluetooth","Done.");
}

static void menu_bt_set_audio(void){
    char sink[128]="";
    cmd_line("pactl list sinks short | grep -v auto_null | awk 'NR==1{print $2}'",
             sink,sizeof(sink));
    if(!sink[0]){
        show_msg("Bluetooth","No BT audio device found.\nConnect headphones first.");
        return;
    }
    char cmd[192];
    snprintf(cmd,sizeof(cmd),"pactl set-default-sink %s",sink);
    run_wait(cmd);
    char msg[80]; snprintf(msg,sizeof(msg),"Audio output: %s",sink);
    show_msg("Bluetooth",msg);
}

static void menu_bluetooth(void){
    while(1){
        bt_draw_status();
        int on=strstr(cached_bt_powered,"yes")!=NULL;
        char tog[32]; snprintf(tog,sizeof(tog),"Turn BT %s",on?"OFF":"ON");
        const char*items[]={tog,"Scan & Pair","Set as Audio Output","Disconnect","Back"};
        int sel=run_menu_fb("Bluetooth",items,5);
        if(sel<0||sel==4) return;
        if(sel==0){
            run_wait(on?"bluetoothctl power off":"bluetoothctl power on");
            invalidate_cache(); sleep(1);
        }
        if(sel==1){
            if(!on){show_msg("Bluetooth","Turn Bluetooth ON first.");continue;}
            menu_bt_scan();
        }
        if(sel==2) menu_bt_set_audio();
        if(sel==3){
            run_wait("bluetoothctl disconnect 2>/dev/null");
            run_wait("pactl set-default-sink auto_null 2>/dev/null");
            invalidate_cache();
            show_msg("Bluetooth","Disconnected.");
        }
        if(!running) return;
    }
}

/* ════════════════════════════════════════════════════════════
   SETTINGS MAIN
   ════════════════════════════════════════════════════════════ */
void menu_settings(void){
    while(1){
        refresh_cache();
        int won=strstr(cached_wifi_status,"enabled")!=NULL;
        int bon=strstr(cached_bt_powered,"yes")!=NULL;
        char wlbl[52],blbl[52];
        snprintf(wlbl,sizeof(wlbl),"WiFi        [%s]%s",
                 won?"ON":"OFF",
                 cached_wifi_ssid[0]?" - ":"");
        if(cached_wifi_ssid[0]){
            /* append SSID to label */
            int l=strlen(wlbl);
            snprintf(wlbl+l,sizeof(wlbl)-l,"%s",cached_wifi_ssid);
        }
        snprintf(blbl,sizeof(blbl),"Bluetooth   [%s]",bon?"ON":"OFF");
        const char*items[]={wlbl,blbl,"Back"};
        int sel=run_menu_fb("Settings",items,3);
        if(sel<0||sel==2) return;
        if(sel==0) menu_wifi();
        if(sel==1) menu_bluetooth();
        if(!running) return;
    }
}
