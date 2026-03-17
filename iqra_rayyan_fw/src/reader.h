#ifndef READER_H
#define READER_H

#include <stdint.h>

extern uint8_t *buf_cur;
extern uint8_t *buf_next;
extern uint8_t *buf_prev;
extern int      cur_page;
extern volatile int running;

int  reader_init   (void);
void reader_cleanup(void);

void goto_page    (int page);
void run_reader   (int start_page);
void show_message (const char *line1, const char *line2);

#endif /* READER_H */
