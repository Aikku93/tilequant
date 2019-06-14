/**************************************/
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
/**************************************/
#include "Quantize.h"
#include "Tiles.h"
/**************************************/
#define MAX_PALETTE_INDICES_PASSES      8
#define MAX_PALETTE_QUANTIZATION_PASSES 8
/**************************************/
#define ALIGN2N(x,N) (((x) + (N)-1) &~ ((N)-1))
#define DATA_ALIGNMENT 32
#define DATA_ALIGN(x) ALIGN2N((uintptr_t)(x), DATA_ALIGNMENT) //! NOTE: Cast to uintptr_t
/**************************************/

//! 3rd-order Smoothstep
static inline float SmoothStep1(float x) {
	return x*x*(3.0f - 2.0f*x);
}

//! 5th-order Smoothstep
static inline float SmoothStep2(float x) {
	return x*x*x*(3.0f*x*(2.0f*x - 5.0f) + 10.0f);
}

//! 7th-order Smoothstep
static inline float SmoothStep3(float x) {
	float v = x*x;
	return v*v*(-2.0f*x*(5.0f*x*(2.0f*x - 7.0f) + 42.0f) + 35.0f);
}

/**************************************/

//! Fill out the tile data
//! Separated out from the main function as there is two
//! paths (palettized, and direct) with identical code
//! except for how the pixels are fetched.
static inline __attribute__((always_inline)) void ConvertToTiles(
	struct TilesData_t *TilesData,
	const struct BGRA8_t *PxBGR,
	const        uint8_t *PxIdx,
	int TileW,
	int TileH,
	int nTileX,
	int nTileY,
	const struct BGRA8_t *BitRange
) {
	int tx, ty, px, py;
	union TilePx_t *TilePxPtr = TilesData->TilePxPtr;
	struct YUVAf_t *TileValue = TilesData->TileValue;
	struct YUVAf_t *PxData    = TilesData->PxData;
	for(ty=0;ty<nTileY;ty++) for(tx=0;tx<nTileX;tx++) {
		//! Copy pixels and get RMS
		struct YUVAf_t RMS = {0,0,0,0};
		for(py=0;py<TileH;py++) for(px=0;px<TileW;px++) {
			//! Get original BGR pixel
			struct BGRA8_t pBGR;
			if(PxIdx) pBGR = PxBGR[PxIdx[(ty*TileH+py)*(nTileX*TileW) + (tx*TileW+px)]];
			else      pBGR = PxBGR[      (ty*TileH+py)*(nTileX*TileW) + (tx*TileW+px) ];

			//! Convert range
			pBGR.b = pBGR.b*BitRange->b/255;
			pBGR.g = pBGR.g*BitRange->g/255;
			pBGR.r = pBGR.r*BitRange->r/255;
			pBGR.a = pBGR.a*BitRange->a/255;

			//! Convert to YUV, add to RMS
			struct YUVAf_t p = YUVAf_FromBGRA8_Ranged(&pBGR, BitRange);
			PxData[py*TileW+px] = p;
			p = YUVAf_SignedSquare(&p);
			RMS = YUVAf_Add(&RMS, &p);
		}
		RMS = YUVAf_Divi(&RMS, TileW*TileH);
		RMS = YUVAf_SignedSqrt(&RMS);

		//! Get deviation from RMS
		struct YUVAf_t Dev = {0,0,0,0};
		for(py=0;py<TileH;py++) for(px=0;px<TileW;px++) {
			struct YUVAf_t p = PxData[py*TileW+px];
			p   = YUVAf_Sub(&p, &RMS);
			p   = YUVAf_Mul(&p, &p);
			Dev = YUVAf_Add(&Dev, &p);
		}
		Dev = YUVAf_Divi(&Dev, TileW*TileH);
		Dev = YUVAf_Sqrt(&Dev);

		//! Get tile value from RMS and deviation
		struct YUVAf_t Val;
		Val = YUVAf_Mul(&Dev, &(struct YUVAf_t){2,1,1,2});
		Val = YUVAf_Addi(&Val, 1.0f);
		Val = YUVAf_Mul(&RMS, &Val);

		//! Apply equalization to luma
		//! This basically increases contrast
		Val.y = SmoothStep1(Val.y);

		//! Move to next tile
		(TilePxPtr++)->PxYUVA = PxData;
		*TileValue++ = Val;
		PxData += TileW*TileH;
	}
}

/**************************************/

//! Convert bitmap to tiles
struct TilesData_t *TilesData_FromBitmap(const struct BmpCtx_t *Ctx, int TileW, int TileH, const struct BGRA8_t *BitRange) {
	//! Allocate memory for tiles
	int nPx    = Ctx->Width * Ctx->Height;
	int nTileX = (Ctx->Width  / TileW);
	int nTileY = (Ctx->Height / TileH);
	int nTiles = nTileX * nTileY;
	struct TilesData_t *TilesData = aligned_alloc(DATA_ALIGNMENT,
		DATA_ALIGN(sizeof(struct TilesData_t))    +
		DATA_ALIGN(nTiles*sizeof(union TilePx_t)) +
		DATA_ALIGN(nTiles*sizeof(struct YUVAf_t)) +
		DATA_ALIGN(nPx   *sizeof(struct YUVAf_t)) +
		DATA_ALIGN(nPx   *sizeof(struct YUVAf_t)) +
		DATA_ALIGN(nPx   *sizeof(int32_t)       ) +
		DATA_ALIGN(nTiles*sizeof(int32_t)       )
	);
	if(!TilesData) return NULL;

	//! Setup structure
	TilesData->TileW      = TileW;
	TilesData->TileH      = TileH;
	TilesData->TilesX     = nTileX;
	TilesData->TilesY     = nTileY;
	TilesData->TilePxPtr  = (union TilePx_t*)DATA_ALIGN(TilesData + 1);
	TilesData->TileValue  = (struct YUVAf_t*)DATA_ALIGN(TilesData->TilePxPtr  + nTiles);
	TilesData->PxData     = (struct YUVAf_t*)DATA_ALIGN(TilesData->TileValue  + nTiles);
	TilesData->PxTemp     = (struct YUVAf_t*)DATA_ALIGN(TilesData->PxData     + nPx);
	TilesData->PxTempIdx  = (int32_t       *)DATA_ALIGN(TilesData->PxTemp     + nPx);
	TilesData->TilePalIdx = (int32_t       *)DATA_ALIGN(TilesData->PxTempIdx  + nPx);

	//! Fill tiles
	if(Ctx->ColPal) ConvertToTiles(TilesData, Ctx->ColPal, Ctx->PxIdx, TileW, TileH, nTileX, nTileY, BitRange);
	else            ConvertToTiles(TilesData, Ctx->PxBGR,  NULL,       TileW, TileH, nTileX, nTileY, BitRange);

	//! Return tiles array
	return TilesData;
}

/**************************************/

//! Create quantized palette
int TilesData_QuantizePalettes(struct TilesData_t *TilesData, struct YUVAf_t *Palette, int MaxTilePals, int MaxPalSize, int PalUnusedEntries) {
	int i, j, k;
	int nPxTile = TilesData->TileW  * TilesData->TileH;
	int nTiles  = TilesData->TilesX * TilesData->TilesY;

	//! Unused entries should not count towards
	//! the maximum palette size
	MaxPalSize -= PalUnusedEntries;

	//! Allocate clusters
	struct QuantCluster_t *Clusters; {
		int nClusters = MaxTilePals; if(MaxPalSize > nClusters) nClusters = MaxPalSize;
		Clusters = aligned_alloc(DATA_ALIGNMENT, nClusters*sizeof(struct QuantCluster_t));
		if(!Clusters) return 0;
	}

	//! Categorize tiles by palette
	QuantCluster_Quantize(Clusters, MaxTilePals, TilesData->TileValue, nTiles, TilesData->TilePalIdx, MAX_PALETTE_INDICES_PASSES);

	//! Quantize tile palettes
	for(i=0;i<MaxTilePals;i++) {
		struct YUVAf_t *PxTemp = TilesData->PxTemp;

		//! Get all pixels of all tiles falling into this palette
		int PxCnt; {
			struct YUVAf_t *Dst = PxTemp;
			for(j=0;j<nTiles;j++) if(TilesData->TilePalIdx[j] == i) {
				const struct YUVAf_t *Src = TilesData->TilePxPtr[j].PxYUVA;
				for(k=0;k<nPxTile;k++) *Dst++ = *Src++;
			}
			PxCnt = Dst - PxTemp;
		}
		if(!PxCnt) continue;

		//! Perform quantization
		QuantCluster_Quantize(Clusters, MaxPalSize, PxTemp, PxCnt, TilesData->PxTempIdx, MAX_PALETTE_QUANTIZATION_PASSES);

		//! Extract palette from cluster centroids
		for(j=0;j<PalUnusedEntries;j++) *Palette++ = (struct YUVAf_t){0,0,0,0};
		for(j=0;j<MaxPalSize;      j++) *Palette++ = Clusters[j].Centroid;
	}

	//! Clean up, return
	free(Clusters);
	return 1;
}

/**************************************/
//! EOF
/**************************************/
