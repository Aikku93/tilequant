CFILES := Bitmap.c Quantize.c Dither.c Qualetize.c Tiles.c tilequant.c

all:
	mkdir -p release
	$(CC) -lm -O2 -Wall -Wextra $(CFILES) -o release/tilequant

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
IS_UNIX = true
endif
ifeq ($(UNAME), Darwin)
IS_UNIX = true
endif
ifdef IS_UNIX
TARGET = "libtilequant.so"
else
TARGET = "libtilequant.dll"
endif

dll:
	mkdir -p release
	$(CC) -shared -o release/$(TARGET) -lm -O2 -Wall -fPIC -Wextra $(CFILES) -DDECLSPEC="$(DDECLSPEC)"

.PHONY: clean
clean:
	rm -rf release
