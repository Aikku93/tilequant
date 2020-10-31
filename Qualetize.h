/**************************************/
#pragma once
/**************************************/
#include "Bitmap.h"
#include "Colourspace.h"
#include "Tiles.h"
/**************************************/

//! Dither modes available
#define DITHER_NONE           ( 0) //! No dither
#define DITHER_ORDERED(n)     ( n) //! Ordered dithering (Kernel size: (2^n) x (2^n))
#define DITHER_FLOYDSTEINBERG (-1) //! Floyd-Steinberg (diffusion)

//! Dither settings
//! NOTE: Ordered dithering gives consistent tiled results, but Floyd-Steinberg can look nicer.
//!       Recommend dither level of 0.5 for ordered, and 1.0 for Floyd-Steinberg.
//! NOTE: DITHER_NO_ALPHA disables dithering on the alpha channel.
#define DITHER_LEVEL 0.5f
#define DITHER_NO_ALPHA

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
	const struct BGRA8_t *BitRange,
	int DitherType,
	int ReplaceImage
);

/**************************************/
//! EOF
/**************************************/

