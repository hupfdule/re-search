CC     = cc
CFLAGS = -Wall -O2


all: re-search re-search-static

re-search: re-search.c
	$(CC) $(CFLAGS) $^ -o $@

re-search-static: re-search.c
	$(CC) $(CFLAGS) -static $^ -o $@

debug: CFLAGS += -DDEBUG -g
debug: re-search

clean:
	rm -f re-search re-search-static
