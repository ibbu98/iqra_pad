#ifndef AUDIO_H
#define AUDIO_H

#define AUDIO_STOPPED  0
#define AUDIO_PLAYING  1
#define AUDIO_PAUSED   2

extern volatile int  audio_state;
extern volatile int  audio_surah;
extern char          audio_reciter[64];

void audio_init    (void);
void audio_play    (const char *reciter, int surah);
void audio_pause   (void);
void audio_stop    (void);
void audio_next    (void);
void audio_prev    (void);
void audio_vol_up  (void);
void audio_vol_dn  (void);
void audio_cleanup (void);

void menu_mp3      (void);

#endif /* AUDIO_H */
