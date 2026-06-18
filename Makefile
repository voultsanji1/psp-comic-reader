# PSP Comic Reader — Makefile for local PSPSDK builds
# Usage:  make  (requires PSPSDK in $PATH)

TARGET = PSPComicReader
OBJS   = src/main.o \
         src/display.o \
         src/input.o \
         src/archive.o \
         src/image.o \
         src/browser.o \
         src/bookmarks.o \
         src/settings.o \
         src/ui.o \
         src/libs/miniz/miniz.o

CFLAGS  = -O2 -G0 -Wall -fno-strict-aliasing \
          -I$(PSPSDK)/include \
          -Isrc \
          -Isrc/libs/miniz \
          -Isrc/libs/stb

CXXFLAGS = $(CFLAGS) -std=c++17 -fno-exceptions -fno-rtti

LIBS = -lpspgu -lpspgum -lpspctrl -lpspdisplay \
       -lpspge  -lpspvfpu -lpsputility -lpsppower \
       -lpsprtc -lm -lc -lstdc++

PSPSDK  = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
