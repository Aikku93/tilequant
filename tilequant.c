/**************************************/
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
/**************************************/
#include "Compat.h"
#include "Bitmap.h"
#include "PxType.h"
#include "Tiles.h"
/**************************************/
#define TILE_W 8
#define TILE_H 8
#define PALUNUSED 1 //! These will be filled with {0,0,0,0} but still be used for matching
#define BITRANGE (const struct BGRA8_t){0x1F,0x1F,0x1F,0x01} //! None of these may be 0
/**************************************/
#define DITHER 1
#define DITHER_SERPENTINE 1
#define MEASURE_PSNR 1
/**************************************/

//! Palette entry matching
static inline int FindPaletteEntry(const struct YUVAf_t *Px, const struct YUVAf_t *Pal, int MaxPalSize) {
	int   i;
	int   MinIdx = 0;
	float MinDst = 1.0e30f;
	for(i=PALUNUSED-1;i<MaxPalSize;i++) {
		float Dst = YUVAf_ColDistance(Px, &Pal[i]);
		if(Dst < MinDst) MinIdx = i, MinDst = Dst;
	}
	return MinIdx;
}

/**************************************/

//! Handle conversion of image with given palette, return RMS error
//! NOTE: Lots of pointer aliasing to avoid even more memory consumption
static inline struct YUVAf_t ProcessImage(struct BmpCtx_t *Image, struct TilesData_t *TilesData, uint8_t *PxData, struct YUVAf_t *Palette, int MaxTilePals, int MaxPalSize) {
	int i;

	//! Do processing
	TilesData_QuantizePalettes(TilesData, Palette, MaxTilePals, MaxPalSize, PALUNUSED);

	//! Convert pixels to palettized
	int x, y;
	int ImgW = Image->Width;
	int ImgH = Image->Height;
	int TileW = TilesData->TileW;
	int TileH = TilesData->TileH;
	int32_t *TilePalIdx = TilesData->TilePalIdx;
#if MEASURE_PSNR
	      struct YUVAf_t  RMSE     = (struct YUVAf_t){0,0,0,0};
#endif
	const        uint8_t *PxSrcIdx = Image->ColPal ? Image->PxIdx  : NULL;
	const struct BGRA8_t *PxSrcBGR = Image->ColPal ? Image->ColPal : Image->PxBGR;
#if DITHER
	      struct YUVAf_t *PxDiffuse = TilesData->PxData;
	for(y=0;y<ImgH;y++) for(x=0;x<ImgW;x++) PxDiffuse[y*ImgW+x] = (struct YUVAf_t){0,0,0,0};
#endif
	for(y=0;y<ImgH;y++) for(x=0;x<ImgW;x++) {
#if DITHER && DITHER_SERPENTINE
		if(y%2) x = ImgW-1 - x;
#endif
		//! Get original pixel data
		struct YUVAf_t pYUVA, pYUVA_Original; {
			struct BGRA8_t pBGR;
			if(PxSrcIdx) pBGR = PxSrcBGR[PxSrcIdx[y*ImgW + x]];
			else         pBGR = PxSrcBGR[         y*ImgW + x ];
			pYUVA = pYUVA_Original = YUVAf_FromBGRA8(&pBGR);
		}
#if DITHER
		//! Adjust for diffusion error
		pYUVA = YUVAf_Add(&pYUVA, &PxDiffuse[y*ImgW + x]);
#endif
		//! Find matching palette entry and store
		int PalIdx = TilePalIdx[(y/TileH)*(ImgW/TileW) + (x/TileW)];
		int PalCol = FindPaletteEntry(&pYUVA, Palette + PalIdx*MaxPalSize, MaxPalSize);
		PxData[y*ImgW + x] = PalIdx*MaxPalSize + PalCol;
#if MEASURE_PSNR || DITHER
		struct YUVAf_t Error = YUVAf_Sub(&pYUVA_Original, &Palette[PxData[y*ImgW + x]]);
#endif
#if DITHER
		//! Store error diffusion
#if DITHER_SERPENTINE
		if(y%2) {
			if(y+1 < ImgH) {
				if(x+1 < ImgW) {
					struct YUVAf_t t = YUVAf_Muli(&Error, 3.0f/16);
					PxDiffuse[(y+1)*ImgW+(x+1)] = YUVAf_Add(&PxDiffuse[(y+1)*ImgW+(x+1)], &t);
				}
				if(1) {
					struct YUVAf_t t = YUVAf_Muli(&Error, 5.0f/16);
					PxDiffuse[(y+1)*ImgW+(x  )] = YUVAf_Add(&PxDiffuse[(y+1)*ImgW+(x  )], &t);
				}
				if(x > 0) {
					struct YUVAf_t t = YUVAf_Muli(&Error, 1.0f/16);
					PxDiffuse[(y+1)*ImgW+(x-1)] = YUVAf_Add(&PxDiffuse[(y+1)*ImgW+(x-1)], &t);
				}
			}
			if(x > 0) {
					struct YUVAf_t t = YUVAf_Muli(&Error, 7.0f/16);
					PxDiffuse[(y  )*ImgW+(x-1)] = YUVAf_Add(&PxDiffuse[(y  )*ImgW+(x-1)], &t);
			}
		} else {
#endif
			if(y+1 < ImgH) {
				if(x > 0) {
					struct YUVAf_t t = YUVAf_Muli(&Error, 3.0f/16);
					PxDiffuse[(y+1)*ImgW+(x-1)] = YUVAf_Add(&PxDiffuse[(y+1)*ImgW+(x-1)], &t);
				}
				if(1) {
					struct YUVAf_t t = YUVAf_Muli(&Error, 5.0f/16);
					PxDiffuse[(y+1)*ImgW+(x  )] = YUVAf_Add(&PxDiffuse[(y+1)*ImgW+(x  )], &t);
				}
				if(x+1 < ImgW) {
					struct YUVAf_t t = YUVAf_Muli(&Error, 1.0f/16);
					PxDiffuse[(y+1)*ImgW+(x+1)] = YUVAf_Add(&PxDiffuse[(y+1)*ImgW+(x+1)], &t);
				}
			}
			if(x+1 < ImgW) {
					struct YUVAf_t t = YUVAf_Muli(&Error, 7.0f/16);
					PxDiffuse[(y  )*ImgW+(x+1)] = YUVAf_Add(&PxDiffuse[(y  )*ImgW+(x+1)], &t);
			}
#if DITHER_SERPENTINE
		}
		if(y%2) x = ImgW-1 - x;
#endif
#endif
#if MEASURE_PSNR
		//! Add error to RMSE
		Error = YUVAf_Mul(&Error, &Error);
		RMSE  = YUVAf_Add(&RMSE, &Error);
#endif
	}

	//! Convert palette to BGRA
	//! NOTE: This aliases over the original palette, but is
	//! safe because BGRA8_t is smaller than YUVAf_t
	struct BGRA8_t *PalBGR = (struct BGRA8_t*)Palette;
	for(i=0;i<BMP_PALETTE_COLOURS;i++) PalBGR[i] = BGRA8_FromYUVAf(&Palette[i]);

	//! Store new image data
	if(Image->ColPal) {
		free(Image->ColPal);
		free(Image->PxIdx);
	} else free(Image->PxBGR);
	Image->ColPal = PalBGR;
	Image->PxIdx  = PxData;

	//! Return error
#if MEASURE_PSNR
	RMSE = YUVAf_Divi(&RMSE, ImgW*ImgH);
	RMSE = YUVAf_Sqrt(&RMSE);
	return RMSE;
#else
	return (struct YUVAf_t){0,0,0,0};
#endif
}

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
	struct YUVAf_t     *Palette   = calloc(BMP_PALETTE_COLOURS, sizeof(struct YUVAf_t));
	if(!TilesData || !PxData || !Palette) {
		printf("Out of memory; image not processed\n");
		free(Palette);
		free(PxData);
		free_aligned(TilesData);
		BmpCtx_Destroy(&Image);
		return -1;
	}
	struct YUVAf_t RMSE = ProcessImage(&Image, TilesData, PxData, Palette, MaxTilePals, MaxPalSize);
	free_aligned(TilesData);

	//! Output PSNR
#if MEASURE_PSNR
	RMSE.y = -20.0f*log10f(RMSE.y / 255.0f);
	RMSE.u = -20.0f*log10f(RMSE.u / 255.0f);
	RMSE.v = -20.0f*log10f(RMSE.v / 255.0f);
	RMSE.a = -20.0f*log10f(RMSE.a / 255.0f);
	printf("PSNR = {%.3fdB, %.3fdB, %.3fdB, %.3fdB}\n", RMSE.y, RMSE.u, RMSE.v, RMSE.a);
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
