CC       = cc
CFLAGS   = -Wall -O2
BIN_DIR  = bin
DIST_DIR = dist
SOURCES  = src/re-search.c
HEADERS  = src/config.h src/version.h

.PHONY: all debug clean package

all: $(BIN_DIR)/re-search $(BIN_DIR)/re-search-static

$(BIN_DIR)/re-search: $(SOURCES) $(HEADERS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/re-search-static: $(SOURCES) $(HEADERS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -static $< -o $@

debug: CFLAGS += -DDEBUG -g
debug: $(BIN_DIR)/re-search

package: all
	@sh package.sh

clean:
	rm -rf $(BIN_DIR) $(DIST_DIR)
