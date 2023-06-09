all:
	mkdir -p release
	$(CC) -lm -O2 -Wall -Wextra Bitmap.c Quantize.c Qualetize.c Tiles.c tilequant.c -o release/tilequant

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
	$(CC) -c -lm -O2 -Wall -fPIC -Wextra Bitmap.c Quantize.c Qualetize.c Tiles.c tilequantDLL.c -DDECLSPEC="$(DDECLSPEC)"
	$(CC) -shared -o release/$(TARGET) tilequantDLL.o Bitmap.o Quantize.o Qualetize.o Tiles.o

.PHONY: clean
clean:
	rm -rf release
