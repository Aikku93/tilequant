/**************************************/
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/**************************************/
#include "Bitmap.h"
#include "Colourspace.h"
#include "Qualetize.h"
#include "Tiles.h"
/**************************************/

//! When not zero, the PSNR for each channel will be displayed
#define MEASURE_PSNR 1

/**************************************/

//! strcmp() implementation that ACTUALLY returns the difference between
//! characters instead of just the signs. Blame the C standard -_-
static int mystrcmp(const char *s1, const char *s2) {
	while(*s1 && *s1 == *s2) s1++, s2++;
	return *s1 - *s2;
}

/**************************************/

int main(int argc, const char *argv[]) {
	//! Check arguments
	if(argc < 3) {
		printf(
			"tilequant - Tiled colour-quantization tool\n"
			"Usage:\n"
			" tilequant Input.bmp Output.bmp [options]\n"
			"Options:\n"
			" -np:16            - Set number of palettes available\n"
			" -ps:16            - Set number of colours per palette\n"
			" -tw:8             - Set tile width\n"
			" -th:8             - Set tile height\n"
			" -bgra:5551        - Set BGRA bit depth\n"
			" -dither:floyd,1.0 - Set dither mode, level\n"
			" -tilepasses:0     - Set tile cluster passes (0 = default)\n"
			" -colourpasses:0   - Set colour cluster passes (0 = default)\n"
			"Dither modes available (and default level):\n"
			" -dither:none       - No dithering\n"
			" -dither:floyd,1.0  - Floyd-Steinberg\n"
			" -dither:ord2,0.5   - 2x2 ordered dithering\n"
			" -dither:ord4,0.5   - 4x4 ordered dithering\n"
			" -dither:ord8,0.5   - 8x8 ordered dithering\n"
			" -dither:ord16,0.5  - 16x16 ordered dithering\n"
			" -dither:ord32,0.5  - 32x32 ordered dithering\n"
			" -dither:ord64,0.5  - 64x64 ordered dithering\n"
		);
		return 1;
	}

	//! Parse arguments
	int     nPalettes = 16;
	int     nColoursPerPalette = 16;
	int     nUnusedColoursPerPalette = 1;
	int     nTileClusterPasses   = 0;
	int     nColourClusterPasses = 0;
	int     TileW = 8;
	int     TileH = 8;
	struct BGRA8_t BitRange = {.b = 0x1F, .g = 0x1F, .r = 0x1F, .a = 0x01};
	int     DitherMode  = DITHER_FLOYDSTEINBERG;
	float   DitherLevel = 1.0f; {
		int argi;
		for(argi=3;argi<argc;argi++) {
			int ArgOk = 0;

			const char *ArgStr;
#define ARGMATCH(Input, Target) \
	ArgStr = Input + strlen(Target); \
	if(!memcmp(Input, Target, strlen(Target)))
			//! nPalettes
			ARGMATCH(argv[argi], "-np:") ArgOk = 1, nPalettes = atoi(ArgStr);

			//! nColoursPerPalette
			ARGMATCH(argv[argi], "-ps:") ArgOk = 1, nColoursPerPalette = atoi(ArgStr);

			//! nUnusedColoursPerPalette

			//! TileW
			ARGMATCH(argv[argi], "-tw:") ArgOk = 1, TileW = atoi(ArgStr);

			//! TileH
			ARGMATCH(argv[argi], "-th:") ArgOk = 1, TileH = atoi(ArgStr);

			//! BitRange
			ARGMATCH(argv[argi], "-bgra:") {
				ArgOk = 1;
				BitRange.b = (1 << (*ArgStr++ - '0')) - 1;
				BitRange.g = (1 << (*ArgStr++ - '0')) - 1;
				BitRange.r = (1 << (*ArgStr++ - '0')) - 1;
				BitRange.a = (1 << (*ArgStr++ - '0')) - 1;
			}

			//! DitherMode,DitherLevel
			ARGMATCH(argv[argi], "-dither:") {
				int d;
#define DITHERMODE_MATCH(Input, Target, ModeValue, DefaultLevel) \
	d = mystrcmp(Input, Target); \
	if(!d || d == ',') { \
		ArgOk = 1; \
		DitherMode  = ModeValue; \
		DitherLevel = !d ? DefaultLevel : atof(strchr(Input, ',')+1); \
	}
				DITHERMODE_MATCH(ArgStr, "none",  DITHER_NONE,           0.0f);
				DITHERMODE_MATCH(ArgStr, "floyd", DITHER_FLOYDSTEINBERG, 1.0f);
				DITHERMODE_MATCH(ArgStr, "ord2",  DITHER_ORDERED(1),     0.5f);
				DITHERMODE_MATCH(ArgStr, "ord4",  DITHER_ORDERED(2),     0.5f);
				DITHERMODE_MATCH(ArgStr, "ord8",  DITHER_ORDERED(3),     0.5f);
				DITHERMODE_MATCH(ArgStr, "ord16", DITHER_ORDERED(4),     0.5f);
				DITHERMODE_MATCH(ArgStr, "ord32", DITHER_ORDERED(5),     0.5f);
				DITHERMODE_MATCH(ArgStr, "ord64", DITHER_ORDERED(6),     0.5f);
#undef DITHERMODE_MATCH
				if(!ArgOk) printf("Unrecognized dither mode: %s\n", ArgStr);
				ArgOk = 1;
			}

			//! nTileClusterPasses
			ARGMATCH(argv[argi], "-tilepasses:") {
				ArgOk = 1;
				nTileClusterPasses = atoi(ArgStr);
			}

			//! nColourClusterPasses
			ARGMATCH(argv[argi], "-colourpasses:") {
				ArgOk = 1;
				nColourClusterPasses = atoi(ArgStr);
			}
#undef ARGMATCH
			//! Unrecognized?
			if(!ArgOk) printf("Unrecognized argument: %s\n", ArgStr);
		}
	}

	//! Get input image
	struct BmpCtx_t Image;
	if(!BmpCtx_FromFile(&Image, argv[1])) {
		printf("Unable to read input file\n");
		return -1;
	}
	if(Image.Width%TileW || Image.Height%TileH) {
		printf("Image not a multiple of tile size (%dx%d)\n", TileW, TileH);
		BmpCtx_Destroy(&Image);
		return -1;
	}

	//! Perform processing
	//! NOTE: PxData and Palette will be assigned to image; do NOT destroy
	struct TilesData_t *TilesData = TilesData_FromBitmap(&Image, TileW, TileH, &BitRange, DitherMode, DitherLevel);
	       uint8_t     *PxData    = malloc(Image.Width * Image.Height * sizeof(uint8_t));
	struct BGRAf_t     *Palette   = calloc(BMP_PALETTE_COLOURS, sizeof(struct BGRAf_t));
	if(!TilesData || !PxData || !Palette) {
		printf("Out of memory; image not processed\n");
		free(Palette);
		free(PxData);
		free(TilesData);
		BmpCtx_Destroy(&Image);
		return -1;
	}
	struct BGRAf_t RMSE = Qualetize(
		&Image,
		TilesData,
		PxData,
		Palette,
		nPalettes,
		nColoursPerPalette,
		nUnusedColoursPerPalette,
		nTileClusterPasses,
		nColourClusterPasses,
		&BitRange,
		DitherMode,
		DitherLevel,
		1
	);
	free(TilesData);

	//! Output PSNR
#if MEASURE_PSNR
	RMSE.b = -8.68588963f*logf(RMSE.b / 255.0f); //! -20*Log10[RMSE/255] == -20/Log[10] * Log[RMSE/255]
	RMSE.g = -8.68588963f*logf(RMSE.g / 255.0f);
	RMSE.r = -8.68588963f*logf(RMSE.r / 255.0f);
	RMSE.a = -8.68588963f*logf(RMSE.a / 255.0f);
	printf("PSNR = {%.3fdB, %.3fdB, %.3fdB, %.3fdB}\n", RMSE.b, RMSE.g, RMSE.r, RMSE.a);
#else
	(void)RMSE;
#endif
	//! Output image
	if(!BmpCtx_ToFile(&Image, argv[2])) {
		printf("Unable to write output file\n");
		BmpCtx_Destroy(&Image);
		return -1;
	}

	//! Success
	BmpCtx_Destroy(&Image);
	printf("Ok\n");
	return 0;
}

/**************************************/
//! EOF
/**************************************/
