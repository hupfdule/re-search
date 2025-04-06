CC       = cc
CFLAGS   = -Wall -O2
BIN_DIR  = bin
DIST_DIR = dist
SOURCES  = src/re-search.c
HEADERS  = src/config.h src/version.h

.PHONY: all debug clean package deb

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

deb: $(BIN_DIR)/re-search
	@mkdir -p $(DIST_DIR)
	@mkdir -p debian_pkg/DEBIAN
	@mkdir -p debian_pkg/usr/bin
	install -m 755 $(BIN_DIR)/re-search debian_pkg/usr/bin/
	@echo "Version: $(shell grep 'define VERSION' src/version.h | cut -d '"' -f 2)" >> debian_pkg/DEBIAN/control
	@echo "Architecture: $(shell dpkg --print-architecture)" >> debian_pkg/DEBIAN/control
	@echo "Maintainer: $(DEB_MAINTAINER)" >> debian_pkg/DEBIAN/control
	@echo "Description: Regular expression search tool" >> debian_pkg/DEBIAN/control
	dpkg-deb --build debian_pkg $(DIST_DIR)/re-search_$(shell grep 'define VERSION' src/version.h | cut -d '"' -f 2)_$(shell dpkg --print-architecture).deb
	@rm -rf debian_pkg


clean:
	rm -rf $(BIN_DIR) $(DIST_DIR)
