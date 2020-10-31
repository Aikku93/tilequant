/**************************************/
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
/**************************************/
#include "Bitmap.h"
#include "Qualetize.h"
#include "Tiles.h"
/**************************************/

//! Pointer arguments:
//!  For BGRA images:
//!   SrcPxData = (struct BGRA8_t)[Width*Height]
//!   SrcPxPal  = NULL
//!  For paletted images:
//!   SrcPxData = uint8_t[Width*Height]
//!   SrcPxPal  = (struct BGRA8_t)[]
//!  General:
//!   DstPxIdx   = uint8_t[Width*Height]
//!   DstPal     = (struct BGRA8_t)[nPalettes * nColoursPerPalette]
//!    NOTE: DstPal must have enough space to accomodate:
//!     (struct BGRAf_t)[nPalettes * nColoursPerPalette]
//!   TilePalIdx = NULL or int32_t[(Width*Height) / (TileW*TileH)]
//!   DitherMode = Dither mode to use: 0 = DITHER_NONE, -1 = DITHER_FLOYDSTEINBERG, n = DITHER_ORDERED(n)
//! OutputPaletteIs24bitRGB outputs RGB (byte order: {RR, GG, BB})
//! colours without an alpha channel; the default is to output to
//! BGRA (byte order: {BB, GG, RR, AA}).
DECLSPEC int QualetizeFromRawImage(
	//! Image specification
	int ImgWidth,
	int ImgHeight,
	const uint8_t *SrcPxData,
	const uint8_t *SrcPxPal,
	      uint8_t *DstPxIdx,
	      uint8_t *DstPal,
	      int      nUnusedColoursPerPalette,
	      int      OutputPaletteIs24bitRGB,

	//! Quantization control
	int      nPalettes,
	int      nColoursPerPalette,
	int      TileW,
	int      TileH,
	int32_t *TilePalIdx,
	const uint8_t BitRange[4],
	int           DitherMode
) {
	//! Create image context
	//! NOTE: 'const' violations in image data, but not modified so this is safe
	struct BmpCtx_t Ctx;
	Ctx.Width  = ImgWidth;
	Ctx.Height = ImgHeight;
	Ctx.ColPal = (struct BGRA8_t*)SrcPxPal;
	if(SrcPxPal) Ctx.PxIdx = (       uint8_t*)SrcPxData;
	else         Ctx.PxBGR = (struct BGRA8_t*)SrcPxData;

	//! Do processing
	//! NOTE: Do NOT allow image replacing, or things will go
	//! very wrong when Qualetize() tries to free the pointers.
	struct TilesData_t *TilesData = TilesData_FromBitmap(&Ctx, TileW, TileH);
	if(!TilesData) return 0;
	(void)Qualetize(
		&Ctx, TilesData, DstPxIdx, (struct BGRAf_t*)DstPal, nPalettes, nColoursPerPalette, nUnusedColoursPerPalette, (const struct BGRA8_t*)BitRange, DitherMode, 0
	);

	//! Store tile palette indices
	if(TilePalIdx) {
		int i;
		      int32_t *Dst = TilePalIdx;
		const int32_t *Src = TilesData->TilePalIdx;
		for(i=0;i<(ImgWidth*ImgHeight)/(TileW*TileH);i++) *Dst++ = *Src++;
	}

	//! Convert palette to RRGGBB if needed
	//! NOTE: Pointer aliasing, but target format is smaller than the source
	if(OutputPaletteIs24bitRGB) {
		int nCol = nPalettes * nColoursPerPalette;
		uint8_t *Dst = DstPal;
		const struct BGRA8_t *Src = (const struct BGRA8_t*)DstPal;
		if(nCol) do {
			struct BGRA8_t x = *Src++;
			*Dst++ = x.r;
			*Dst++ = x.g;
			*Dst++ = x.b;
		} while(--nCol);
	}

	//! Destroy tiling context, and all done
	free(TilesData);
	return 1;
}

/**************************************/
//! EOF
/**************************************/
