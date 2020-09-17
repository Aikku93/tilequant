/**************************************/
#pragma once
/**************************************/
#include "Bitmap.h"
#include "Colourspace.h"
#include "Tiles.h"
/**************************************/

//! Handle conversion of image, return RMS error
//! NOTE:
//!  * With ReplaceImage != 0, {Image->ColMap,Image->PxIdx} (or
//!    Image->PxBGR) will be free()'d and replaced with {PxData,Palette}.
struct BGRAf_t Qualetize(
	struct BmpCtx_t *Image,
	struct TilesData_t *TilesData,
	uint8_t *PxData,
	struct BGRAf_t *Palette,
	int MaxTilePals,
	int MaxPalSize,
	int PalUnused,
	int ReplaceImage
);

/**************************************/
//! EOF
/**************************************/

