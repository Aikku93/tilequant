/**************************************/
#pragma once
/**************************************/
#include "Bitmap.h"
#include "Colourspace.h"
/**************************************/

//! Handle conversion of image, return RMS error.
//! Notes:
//!  -Passing RawPxOutput != NULL will store the dithered image there.
//!  -Passing TilePxOutput != NULL will store the output image there,
//!   using TilePalettes as a reference.
//!  -DiffusionBuffer[] needs to be (Image->Width+2)*2 elements in size.
struct BGRAf_t DitherImage(
	const struct BmpCtx_t *Image,
	const struct BGRA8_t *BitRange,
	struct BGRAf_t *RawPxOutput,

	int TileW,
	int TileH,
	int MaxTilePals,
	int MaxPalSize,
	int PalUnused,
	const int32_t *TilePalIndices,
	const struct BGRAf_t *TilePalettes,
	uint8_t *TilePxOutput,

	int   DitherType,
	float DitherLevel,
	struct BGRAf_t *DiffusionBuffer
);

/**************************************/
//! EOF
/**************************************/
