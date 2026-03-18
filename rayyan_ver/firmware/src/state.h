#ifndef STATE_H
#define STATE_H

#define MAX_BOOKMARKS 256
#define BM_LABEL_LEN   48

typedef struct {
    int  page;
    char label[BM_LABEL_LEN];
} Bookmark;

extern Bookmark bookmarks[MAX_BOOKMARKS];
extern int      bm_count;
extern int      last_read_page;

void state_load    (void);
void state_save    (int page);
void bookmarks_load(void);
void bookmarks_save(void);
void bookmark_add  (int page);
void bookmark_delete(int idx);

#endif /* STATE_H */
