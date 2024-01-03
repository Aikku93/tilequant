/**************************************/
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
/**************************************/
#include "dither.h"
#include "quantize.h"
#include "tiles.h"
/**************************************/

#define DEFAULT_TILECLUSTER_PASSES   16
#define DEFAULT_COLOURCLUSTER_PASSES 16

/**************************************/
#define ALIGN2N(x,N) (((x) + (N)-1) &~ ((N)-1))
#define DATA_ALIGNMENT 32
#define DATA_ALIGN(x) ALIGN2N((uintptr_t)(x), DATA_ALIGNMENT) //! NOTE: Cast to uintptr_t
/**************************************/

//! Fill out the tile data
static inline void ConvertToTiles(
    struct TilesData_t *TilesData,
    const struct BGRAf_t *PxBGRA,
    int TileW,
    int TileH,
    int nTileX,
    int nTileY
)
{
    int tx, ty, px, py;
    union TilePx_t *TilePxPtr = TilesData->TilePxPtr;
    struct BGRAf_t *TileValue = TilesData->TileValue;
    struct BGRAf_t *PxData    = TilesData->PxData;
    for(ty=0; ty<nTileY; ty++) for(tx=0; tx<nTileX; tx++)
        {
            //! Copy pixels as YUV, and get mean
            struct BGRAf_t Mean = {0,0,0,0};
            for(py=0; py<TileH; py++) for(px=0; px<TileW; px++)
                {
                    //! Convert and store pixel
                    struct BGRAf_t Px = BGRAf_AsYUV(&PxBGRA[(ty*TileH+py)*(nTileX*TileW) + (tx*TileW+px)]);
                    *PxData++ = Px;
                    Mean = BGRAf_Add(&Mean, &Px);
                }

            //! Now normalize the luma and alpha, but leave chroma alone.
            //! The idea here is that the importance of the variance in
            //! chroma is proportional to the number of pixels in the tile,
            //! while luma and alpha are independent dimensions to optimize.
            Mean.b /= (float)(TileW*TileH);
            Mean.a /= (float)(TileW*TileH);

            //! Store value and move to next tile
            (TilePxPtr++)->PxBGRAf = PxData - TileW*TileH;
            *TileValue++ = Mean;
        }
}

/**************************************/

//! Convert bitmap to tiles
struct TilesData_t *TilesData_FromBitmap(
    const struct BmpCtx_t *Ctx,
    int TileW,
    int TileH,
    const struct BGRA8_t *BitRange,
    int   DitherType,
    float DitherLevel
)
{
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

    //! Apply first-pass dithering into PxTemp[] and fill tiles using this data
    DitherImage(
        Ctx,
        BitRange,
        TilesData->PxTemp,
        0,
        0,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
        DitherType,
        DitherLevel,
        TilesData->PxData //! <- This is unused until after ConvertToTiles(), so we can use it here
    );
    ConvertToTiles(TilesData, TilesData->PxTemp, TileW, TileH, nTileX, nTileY);

    //! Return tiles array
    return TilesData;
}

/**************************************/

//! Create quantized palette
int TilesData_QuantizePalettes(
    struct TilesData_t *TilesData,
    struct BGRAf_t *Palette,
    int MaxTilePals,
    int MaxPalSize,
    int PalUnusedEntries,
    int nTileClusterPasses,
    int nColourClusterPasses
)
{
    int i, j, k;
    int nPxTile = TilesData->TileW  * TilesData->TileH;
    int nTiles  = TilesData->TilesX * TilesData->TilesY;

    //! Set default passes as needed
    if(nTileClusterPasses   == 0) nTileClusterPasses   = DEFAULT_TILECLUSTER_PASSES;
    if(nColourClusterPasses == 0) nColourClusterPasses = DEFAULT_COLOURCLUSTER_PASSES;

    //! Unused entries should not count towards
    //! the maximum palette size
    MaxPalSize -= PalUnusedEntries;

    //! Allocate clusters
    struct QuantCluster_t *Clusters, *_Clusters;
    {
        int nClusters = MaxTilePals;
        if(MaxPalSize > nClusters) nClusters = MaxPalSize;
        _Clusters = malloc(DATA_ALIGNMENT-1 + nClusters*sizeof(struct QuantCluster_t));
        if(!_Clusters) return 0;
        Clusters = (struct QuantCluster_t*)DATA_ALIGN(_Clusters);
    }

    //! Categorize tiles by palette
    QuantCluster_Quantize(Clusters, MaxTilePals, TilesData->TileValue, nTiles, TilesData->TilePalIdx, nTileClusterPasses);

    //! Quantize tile palettes
    for(i=0; i<MaxTilePals; i++)
    {
        struct BGRAf_t *PxTemp = TilesData->PxTemp;

        //! Get all pixels of all tiles falling into this palette
        int PxCnt;
        {
            struct BGRAf_t *Dst = PxTemp;
            for(j=0; j<nTiles; j++) if(TilesData->TilePalIdx[j] == i)
                {
                    const struct BGRAf_t *Src = TilesData->TilePxPtr[j].PxBGRAf;
                    for(k=0; k<nPxTile; k++)
                        {
                            //! NOTE: Do not add alpha=0 pixels, as this is a separate
                            //! thing altogether when PalUnusedEntries != 0.
                            struct BGRAf_t x = *Src++;
                            if(PalUnusedEntries != 0 && x.a != 0) *Dst++ = x;
                        }
                }
            PxCnt = Dst - PxTemp;
        }
        if(!PxCnt) continue;

        //! Perform quantization
        QuantCluster_Quantize(Clusters, MaxPalSize, PxTemp, PxCnt, TilesData->PxTempIdx, nColourClusterPasses);

        //! Extract palette from cluster centroids
        for(j=0; j<PalUnusedEntries; j++) *Palette++ = (struct BGRAf_t)
        {
            0,0,0,0
        };
        for(j=0; j<MaxPalSize;       j++) *Palette++ = Clusters[j].Centroid;
    }

    //! Clean up, return
    free(_Clusters);
    return 1;
}

/**************************************/
//! EOF
/**************************************/
