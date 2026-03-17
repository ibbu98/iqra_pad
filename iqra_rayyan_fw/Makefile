# Iqra Pad — Rayyan Edition
# Usage: make        → build
#        make clean  → remove binary and .o files

CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -Isrc
LDFLAGS = -lpthread
TARGET  = iqra_core
SRCDIR  = src

SRCS = $(SRCDIR)/main.c      \
       $(SRCDIR)/fb.c        \
       $(SRCDIR)/menu.c      \
       $(SRCDIR)/reader.c    \
       $(SRCDIR)/state.c     \
       $(SRCDIR)/data.c      \
       $(SRCDIR)/audio.c     \
       $(SRCDIR)/settings.c  \
       $(SRCDIR)/pomodoro.c

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
