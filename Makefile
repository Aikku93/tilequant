all:
	mkdir -p release
	$(CC) -lm -O2 -Wall -Wextra Bitmap.c Quantize.c Qualetize.c Tiles.c tilequant.c -o release/tilequant

.PHONY: clean
clean:
	rm -rf release
