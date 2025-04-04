CC      = cc
CFLAGS  = -Wall -O2
BIN_DIR = bin
SOURCES = src/re-search.c
HEADERS = src/config.h src/version.h

.PHONY: all debug clean

all: $(BIN_DIR)/re-search $(BIN_DIR)/re-search-static

$(BIN_DIR)/re-search: $(SOURCES) $(HEADERS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/re-search-static: $(SOURCES) $(HEADERS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -static $< -o $@

debug: CFLAGS += -DDEBUG -g
debug: $(BIN_DIR)/re-search

clean:
	rm -rf $(BIN_DIR)
