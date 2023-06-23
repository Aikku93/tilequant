/**************************************/
#pragma once
/**************************************/
#include <stdint.h>
/**************************************/
#include "Bitmap.h"
#include "Colourspace.h"
/**************************************/

union TilePx_t {
	struct BGRAf_t *PxBGRAf;
	struct BGR8_t  *PxBGRA8;
	uint8_t PxIdx;
};

struct TilesData_t {
	int TileW,  TileH;
	int TilesX, TilesY;
	union TilePx_t *TilePxPtr;  //! Tile pixel pointers
	struct BGRAf_t *TileValue;  //! Tile values (for quantization comparisons)
	struct BGRAf_t *PxData;     //! Tile pixel data
	struct BGRAf_t *PxTemp;     //! Temporary processing data
	int32_t        *PxTempIdx;  //! Temporary processing data (palette entry indices)
	int32_t        *TilePalIdx; //! Tile palette indices
};

/**************************************/

//! Convert bitmap to tiles
//! NOTE: To destroy, call free() on the returned pointer
struct TilesData_t *TilesData_FromBitmap(const struct BmpCtx_t *Ctx, int TileW, int TileH);

//! Create quantized palette
//! NOTE: PalUnusedEntries is used for 'padding', such as on
//! the GBA/NDS where index 0 of every palette is transparent
//! NOTE: Palette is generated in YUVA mode
int TilesData_QuantizePalettes(
	struct TilesData_t *TilesData,
	struct BGRAf_t *Palette,
	int MaxTilePals,
	int MaxPalSize,
	int PalUnusedEntries,
	int nTileClusterPasses,
	int nColourClusterPasses
);

/**************************************/
//! EOF
/**************************************/
