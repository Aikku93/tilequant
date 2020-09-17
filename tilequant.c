/**************************************/
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
/**************************************/
#include "Bitmap.h"
#include "Colourspace.h"
#include "Qualetize.h"
#include "Tiles.h"
/**************************************/
#define TILE_W 8
#define TILE_H 8
#define PALUNUSED 1 //! These will be filled with {0,0,0,0} but still be used for matching
#define BITRANGE (const struct BGRA8_t){0x1F,0x1F,0x1F,0x01} //! None of these may be 0
/**************************************/

//! When not zero, the PSNR for each channel will be displayed
#define MEASURE_PSNR 1

/**************************************/

int main(int argc, const char *argv[]) {
	//! Check arguments
	if(argc != 5) {
		printf(
			"tilequant - Tiled colour-quantization tool\n"
			"Usage: tilequant Input.bmp Output.bmp (no. of palettes) (entries/palette)\n"
		);
		return 1;
	}

	//! Parse arguments
	int MaxTilePals = atoi(argv[3]);
	int MaxPalSize  = atoi(argv[4]);

	//! Get input image
	struct BmpCtx_t Image;
	if(!BmpCtx_FromFile(&Image, argv[1])) {
		printf("Unable to read input file\n");
		return -1;
	}
	if(Image.Width%TILE_W || Image.Height%TILE_H) {
		printf("Image not a multiple of tile size (%dx%d)\n", TILE_W, TILE_H);
		BmpCtx_Destroy(&Image);
		return -1;
	}

	//! Perform processing
	//! NOTE: PxData and Palette will be assigned to image; do NOT destroy
	struct TilesData_t *TilesData = TilesData_FromBitmap(&Image, TILE_W, TILE_H, &BITRANGE);
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
	struct BGRAf_t RMSE = Qualetize(&Image, TilesData, PxData, Palette, MaxTilePals, MaxPalSize, PALUNUSED, 1);
	free(TilesData);

	//! Output PSNR
#if MEASURE_PSNR
	RMSE.b = -0x1.15F2CFp3f*logf(RMSE.b / 255.0f); //! -20*Log10[RMSE/255] == -20/Log[10] * Log[RMSE/255]
	RMSE.g = -0x1.15F2CFp3f*logf(RMSE.g / 255.0f);
	RMSE.r = -0x1.15F2CFp3f*logf(RMSE.r / 255.0f);
	RMSE.a = -0x1.15F2CFp3f*logf(RMSE.a / 255.0f);
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
