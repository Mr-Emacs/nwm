CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -Wpedantic -Wstrict-aliasing  -DGIT_VERSION=\"$(GIT_VERSION)\"

SRC      = src/nwm.cpp src/bar.cpp src/tiling.cpp src/systray.cpp src/animations.cpp
OBJ      = src/nwm.o src/bar.o src/tiling.o src/systray.o src/animations.o
DEPS     = src/nwm.hpp src/bar.hpp src/tiling.hpp src/config.hpp src/systray.hpp src/animations.hpp

LDFLAGS  = $(shell pkg-config --cflags freetype2 fontconfig xft x11 xrandr xinerama)
LDLIBS   = $(shell pkg-config --libs freetype2 fontconfig xft x11 xrandr xinerama) -lXrender -lm

GIT_VERSION = "$(shell git rev-parse HEAD)"

PREFIX   ?= /usr/local
BINDIR   ?= $(PREFIX)/bin
XSESSIONSDIR ?= $(PREFIX)/share/xsessions

MAJOR    = 1
MINOR    = 0
PATCH    = 0

.PHONY: copy all install clean uninstall

all: copy nwm
copy:
	@if [ ! -f src/config.hpp ]; then \
		cp src/default-config.hpp src/config.hpp; \
		echo "Copied default-config.hpp to config.hpp"; \
	else \
		echo "config.hpp already exists, skipping"; \
	fi

version: src/nwm.hpp
	@awk -v major="$(MAJOR)" -v minor="$(MINOR)" -v patch="$(PATCH)" ' \
		BEGIN { in_version_block = 0 } \
		/^#ifndef NWM_HPP/ { \
			print; \
			getline; \
			print; \
			print ""; \
			print "#define MAJOR_VERSION " major; \
			print "#define MINOR_VERSION " minor; \
			print "#define PATCH_VERSION " patch; \
			print ""; \
			in_version_block = 1; \
			next \
		} \
		/^#define (MAJOR|MINOR|PATCH)_VERSION/ { next } \
		in_version_block && /^$$/ && ++empty_count == 2 { in_version_block = 0; next } \
		in_version_block && /^$$/ { next } \
		{ print; in_version_block = 0 } \
	' src/nwm.hpp > temp_nwm.hpp && mv temp_nwm.hpp src/nwm.hpp

src/%.o: src/%.cpp $(DEPS) version
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -c $< -o $@

nwm: $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o nwm $(LDLIBS)

extension: zoomer

zoomer:
	$(MAKE) -C src/extension/zoomer

install: nwm
	mkdir -p $(BINDIR)
	mkdir -p $(XSESSIONSDIR)
	install -Dm755 nwm $(BINDIR)/nwm
	install -Dm644 nwm.desktop $(XSESSIONSDIR)/nwm.desktop
	@echo "Installed nwm to $(BINDIR)"
	@echo "Installed nwm.desktop to $(XSESSIONSDIR)"

clean:
	$(RM) nwm $(OBJ)

uninstall:
	$(RM) $(BINDIR)/nwm temp.gen
	$(RM) $(XSESSIONSDIR)/nwm.desktop
	@echo "Uninstalled nwm"
