/**************************************/
#include <stdlib.h>
/**************************************/
#include "Bitmap.h"
#include "Colourspace.h"
#include "Dither.h"
#include "Qualetize.h"
#include "Tiles.h"
/**************************************/

//! Handle conversion of image with given palette, return RMS error
//! NOTE: Lots of pointer aliasing to avoid even more memory consumption
struct BGRAf_t Qualetize(
	struct BmpCtx_t *Image,
	struct TilesData_t *TilesData,
	uint8_t *PxData,
	struct BGRAf_t *Palette,
	int   MaxTilePals,
	int   MaxPalSize,
	int   PalUnused,
	int   nTileClusterPasses,
	int   nColourClusterPasses,
	const struct BGRA8_t *BitRange,
	int   DitherType,
	float DitherLevel,
	int   ReplaceImage
) {
	int i;

	//! Do palette allocation and colour clustering
	TilesData_QuantizePalettes(
		TilesData,
		Palette,
		MaxTilePals,
		MaxPalSize,
		PalUnused,
		nTileClusterPasses,
		nColourClusterPasses
	);

	//! Convert palette to BGRA and reduce range
	for(i=0;i<MaxTilePals*MaxPalSize;i++) {
		struct BGRAf_t p = BGRAf_FromYUV(&Palette[i]);
		struct BGRA8_t p2 = BGRA_FromBGRAf(&p, BitRange);
		Palette[i] = BGRAf_FromBGRA(&p2, BitRange);
	}

	//! Do final dithering+palette processing
	struct BGRAf_t RMSE = DitherImage(
		Image,
		BitRange,
		NULL,
		TilesData->TileW,
		TilesData->TileH,
		MaxTilePals,
		MaxPalSize,
		PalUnused,
		TilesData->TilePalIdx,
		Palette,
		PxData,
		DitherType,
		DitherLevel,
		TilesData->PxTemp
	);

	//! Store the final palette
	//! NOTE: This aliases over the original palette, but is
	//! safe because BGRA8_t is smaller than BGRAf_t
	struct BGRA8_t *PalBGR = (struct BGRA8_t*)Palette;
	for(i=0;i<BMP_PALETTE_COLOURS;i++) {
		PalBGR[i] = BGRA8_FromBGRAf(&Palette[i]);
	}

	//! Store new image data
	if(ReplaceImage) {
		if(Image->ColPal) {
			free(Image->ColPal);
			free(Image->PxIdx);
		} else free(Image->PxBGR);
		Image->ColPal = PalBGR;
		Image->PxIdx  = PxData;
	}

	//! Return error
	return RMSE;
}

/**************************************/
//! EOF
/**************************************/
