# p2padga: terminal BitTorrent client (libtorrent + ncurses)
#
#   make                 build bin/p2padga
#   make install         install to $(PREFIX)/bin (default /usr/local)
#   make uninstall
#   make clean

VERSION  := 1.0.0
PREFIX   ?= /usr/local
DESTDIR  ?=
CXX      ?= g++
PKGS     := libtorrent-rasterbar ncursesw

PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS))
PKG_LIBS   := $(shell pkg-config --libs $(PKGS))
ifeq ($(PKG_LIBS),)
$(error pkg-config could not find $(PKGS); install the libtorrent-rasterbar and ncurses development packages)
endif

CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++20 -Wall -Wextra -Wshadow -Wformat=2 -Wformat-security
# hardening
CPPFLAGS += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 -D_GLIBCXX_ASSERTIONS -DP2PADGA_VERSION='"$(VERSION)"'
CXXFLAGS += -fstack-protector-strong -fstack-clash-protection -fPIE -fcf-protection
LDFLAGS  += -pie -Wl,-z,relro,-z,now -Wl,-z,noexecstack

SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:src/%.cpp=build/%.o)

bin/p2padga: $(OBJ)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(PKG_LIBS)

build/%.o: src/%.cpp $(wildcard src/*.hpp)
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(PKG_CFLAGS) $(CXXFLAGS) -c -o $@ $<

install: bin/p2padga
	install -Dm755 bin/p2padga $(DESTDIR)$(PREFIX)/bin/p2padga

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/p2padga

clean:
	rm -rf build bin

.PHONY: install uninstall clean
