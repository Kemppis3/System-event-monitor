CC = gcc

CFLAGS = -Wall -g $(shell pkg-config --cflags glib-2.0 libnotify)

LDFLAGS = $(shell pkg-config --libs glib-2.0 libnotify)

TARGET = watcher

SRCS = watcher.c

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET):$(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(OBJS)
