/**************************************/
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
/**************************************/
#include "Quantize.h"
#include "Tiles.h"
/**************************************/
#define MAX_PALETTE_INDICES_PASSES      32
#define MAX_PALETTE_QUANTIZATION_PASSES 32
/**************************************/
#define ALIGN2N(x,N) (((x) + (N)-1) &~ ((N)-1))
#define DATA_ALIGNMENT 32
#define DATA_ALIGN(x) ALIGN2N((uintptr_t)(x), DATA_ALIGNMENT) //! NOTE: Cast to uintptr_t
/**************************************/

//! Fill out the tile data
//! Separated out from the main function as there is two
//! paths (palettized, and direct) with identical code
//! except for how the pixels are fetched.
static inline void ConvertToTiles(
	struct TilesData_t *TilesData,
	const struct BGRA8_t *PxBGR,
	const        uint8_t *PxIdx,
	int TileW,
	int TileH,
	int nTileX,
	int nTileY
) {
	int tx, ty, px, py;
	union TilePx_t *TilePxPtr = TilesData->TilePxPtr;
	struct BGRAf_t *TileValue = TilesData->TileValue;
	struct BGRAf_t *PxData    = TilesData->PxData;
	for(ty=0;ty<nTileY;ty++) for(tx=0;tx<nTileX;tx++) {
		//! Copy pixels and get the mean
		struct BGRAf_t Mean; {
			struct BGRAf_t Sum = {0,0,0,0};
			for(py=0;py<TileH;py++) for(px=0;px<TileW;px++) {
				//! Get original BGR pixel
				struct BGRA8_t pBGR;
				if(PxIdx) pBGR = PxBGR[PxIdx[(ty*TileH+py)*(nTileX*TileW) + (tx*TileW+px)]];
				else      pBGR = PxBGR[      (ty*TileH+py)*(nTileX*TileW) + (tx*TileW+px) ];

				//! Store pixel and add to analysis
				struct BGRAf_t Px = BGRAf_FromBGRA8(&pBGR);
				Px = BGRAf_AsYCoCg(&Px);
				PxData[py*TileW+px] = Px;
				Sum = BGRAf_Add(&Sum, &Px);
			}
			Mean = BGRAf_Divi(&Sum, TileW*TileH);
		}

		//! Finally, get the value of this tile by the weighted mean
		//! of its pixels, with the weights being 1-Dist to the mean.
		//! This is one of the most critical criteria for good palettization
		struct BGRAf_t Value; {
			struct BGRAf_t Sum = {0,0,0,0}, SumW = {0,0,0,0};
			for(py=0;py<TileH;py++) for(px=0;px<TileW;px++) {
				struct BGRAf_t Px = PxData[py*TileW+px];
				struct BGRAf_t w  = BGRAf_Sub (&Px, &Mean);
					       w  = BGRAf_Abs (&w);
					       w  = BGRAf_Subi(&w, 1.0f); //! Technically should be 1-w, but used as weight so sign doesn't matter
					       Px = BGRAf_Mul (&Px, &w);
				Sum  = BGRAf_Add(&Sum,  &Px);
				SumW = BGRAf_Add(&SumW, &w);
			}
			Value = BGRAf_Div(&Sum, &SumW);
		}

		//! Store value and move to next tile
		(TilePxPtr++)->PxBGRAf = PxData;
		*TileValue++ = Value;
		PxData += TileW*TileH;
	}
}

/**************************************/

//! Convert bitmap to tiles
struct TilesData_t *TilesData_FromBitmap(const struct BmpCtx_t *Ctx, int TileW, int TileH) {
	//! Allocate memory for tiles
	int nPx    = Ctx->Width * Ctx->Height;
	int nTileX = (Ctx->Width  / TileW);
	int nTileY = (Ctx->Height / TileH);
	int nTiles = nTileX * nTileY;
	struct TilesData_t *TilesData = malloc(
		DATA_ALIGNMENT-1                          + //! Rounding
		DATA_ALIGN(sizeof(struct TilesData_t))    +
		DATA_ALIGN(nTiles*sizeof(union TilePx_t)) + //! TilePxPtr
		DATA_ALIGN(nTiles*sizeof(struct BGRAf_t)) + //! TileValue
		DATA_ALIGN(nPx   *sizeof(struct BGRAf_t)) + //! PxData
		DATA_ALIGN(nPx   *sizeof(struct BGRAf_t)) + //! PxTemp
		DATA_ALIGN(nPx   *sizeof(int32_t)       ) + //! PxTempIdx
		DATA_ALIGN(nTiles*sizeof(int32_t)       )   //! TilePalIdx
	);
	if(!TilesData) return NULL;

	//! Setup structure
	TilesData->TileW      = TileW;
	TilesData->TileH      = TileH;
	TilesData->TilesX     = nTileX;
	TilesData->TilesY     = nTileY;
	TilesData->TilePxPtr  = (union TilePx_t*)DATA_ALIGN(TilesData + 1);
	TilesData->TileValue  = (struct BGRAf_t*)DATA_ALIGN(TilesData->TilePxPtr + nTiles);
	TilesData->PxData     = (struct BGRAf_t*)DATA_ALIGN(TilesData->TileValue + nTiles);
	TilesData->PxTemp     = (struct BGRAf_t*)DATA_ALIGN(TilesData->PxData    + nPx);
	TilesData->PxTempIdx  = (int32_t       *)DATA_ALIGN(TilesData->PxTemp    + nPx);
	TilesData->TilePalIdx = (int32_t       *)DATA_ALIGN(TilesData->PxTempIdx + nPx);

	//! Fill tiles
	if(Ctx->ColPal) ConvertToTiles(TilesData, Ctx->ColPal, Ctx->PxIdx, TileW, TileH, nTileX, nTileY);
	else            ConvertToTiles(TilesData, Ctx->PxBGR,  NULL,       TileW, TileH, nTileX, nTileY);

	//! Return tiles array
	return TilesData;
}

/**************************************/

//! Create quantized palette
int TilesData_QuantizePalettes(struct TilesData_t *TilesData, struct BGRAf_t *Palette, int MaxTilePals, int MaxPalSize, int PalUnusedEntries) {
	int i, j, k;
	int nPxTile = TilesData->TileW  * TilesData->TileH;
	int nTiles  = TilesData->TilesX * TilesData->TilesY;

	//! Unused entries should not count towards
	//! the maximum palette size
	MaxPalSize -= PalUnusedEntries;

	//! Allocate clusters
	struct QuantCluster_t *Clusters, *_Clusters; {
		int nClusters = MaxTilePals; if(MaxPalSize > nClusters) nClusters = MaxPalSize;
		_Clusters = malloc(DATA_ALIGNMENT-1 + nClusters*sizeof(struct QuantCluster_t));
		if(!_Clusters) return 0;
		Clusters = (struct QuantCluster_t*)DATA_ALIGN(_Clusters);
	}

	//! Categorize tiles by palette
	QuantCluster_Quantize(Clusters, MaxTilePals, TilesData->TileValue, nTiles, TilesData->TilePalIdx, MAX_PALETTE_INDICES_PASSES);

	//! Quantize tile palettes
	for(i=0;i<MaxTilePals;i++) {
		struct BGRAf_t *PxTemp = TilesData->PxTemp;

		//! Get all pixels of all tiles falling into this palette
		int PxCnt; {
			struct BGRAf_t *Dst = PxTemp;
			for(j=0;j<nTiles;j++) if(TilesData->TilePalIdx[j] == i) {
				const struct BGRAf_t *Src = TilesData->TilePxPtr[j].PxBGRAf;
				for(k=0;k<nPxTile;k++) *Dst++ = *Src++;
			}
			PxCnt = Dst - PxTemp;
		}
		if(!PxCnt) continue;

		//! Perform quantization
		QuantCluster_Quantize(Clusters, MaxPalSize, PxTemp, PxCnt, TilesData->PxTempIdx, MAX_PALETTE_QUANTIZATION_PASSES);

		//! Extract palette from cluster centroids
		for(j=0;j<PalUnusedEntries;j++) *Palette++ = (struct BGRAf_t){0,0,0,0};
		for(j=0;j<MaxPalSize;      j++) *Palette++ = Clusters[j].Centroid;
	}

	//! Clean up, return
	free(_Clusters);
	return 1;
}

/**************************************/
//! EOF
/**************************************/
