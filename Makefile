PROJECT := tilequant
CFLAGS := -O2 -Wall -Wextra -Isrc
LIBS := -lm -s
CFILES := src/bitmap.c src/quantize.c src/dither.c src/qualetize.c src/tiles.c src/tilequant.c
RM := rm -rf

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
IS_UNIX = true
endif
ifeq ($(UNAME), Darwin)
IS_UNIX = true
endif
ifdef IS_UNIX
EXE = $(PROJECT)
DLL = lib$(PROJECT).so
else
EXE = $(PROJECT).exe
DLL = lib$(PROJECT).dll
endif

.PHONY: clean

all: $(EXE) $(DLL)

$(EXE): $(CFILES)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

$(DLL): $(CFILES)
	$(CC) $(CFLAGS) -shared -fPIC -DDECLSPEC="$(DDECLSPEC)" $^ $(LIBS) -o $@

clean:
	$(RM) $(EXE) $(DLL)
