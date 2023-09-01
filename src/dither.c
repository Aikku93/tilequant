/**************************************/
#include <math.h>
#include <stdlib.h>
/**************************************/
#include "colourspace.h"
#include "dither.h"
#include "qualetize.h"
/**************************************/

//! Palette entry matching
static int FindPaletteEntry(const struct BGRAf_t *Px, const struct BGRAf_t *Pal, int MaxPalSize, int PalUnused)
{
    int   i;
    int   MinIdx = 0;
    float MinDst = INFINITY;
    struct BGRAf_t PxYUV = BGRAf_AsYUV(Px), PalYUV;
    for(i=PalUnused-1; i<MaxPalSize; i++)
    {
        PalYUV = BGRAf_AsYUV(&Pal[i]);
        float Dst = BGRAf_ColDistance(&PxYUV, &PalYUV);
        if(Dst < MinDst) MinIdx = i, MinDst = Dst;
    }
    return MinIdx;
}

/**************************************/

//! Handle conversion of image with given palette, return RMS error
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
)
{
    int i;

    //! Get parameters, pointers, etc.
    int x, y;
    int ImgW = Image->Width;
    int ImgH = Image->Height;
#if MEASURE_PSNR
    struct BGRAf_t  RMSE     = (struct BGRAf_t)
    {
        0,0,0,0
    };
#endif
    const        uint8_t *PxSrcIdx = Image->ColPal ? Image->PxIdx  : NULL;
    const struct BGRA8_t *PxSrcBGR = Image->ColPal ? Image->ColPal : Image->PxBGR;

    //! Initialize dither patterns
    //! For Floyd-Steinberg dithering, we only keep track of two scanlines
    //! of diffusion error (the current line and the next), and just swap
    //! back-and-forth between them to avoid a memcpy(). We also append an
    //! extra 2 pixels at the end of each line to avoid extra comparisons.
    union
    {
        struct BGRAf_t *DiffuseError;  //! DITHER_FLOYDSTEINBERG only
        struct BGRAf_t *PaletteSpread; //! DITHER_ORDERED only
        void *DataPtr;
    } Dither;
    Dither.DataPtr = DiffusionBuffer;
    if(DitherType != DITHER_NONE)
    {
        if(DitherType == DITHER_FLOYDSTEINBERG)
        {
            //! Error diffusion dithering
            for(i=0; i<(ImgW+2)*2; i++) Dither.DiffuseError[i] = (struct BGRAf_t)
            {
                0,0,0,0
            };
        }
        else if(TilePxOutput)
        {
            //! Ordered dithering (with tile palettes)
            for(i=0; i<MaxTilePals; i++)
            {
                //! Find the mean values of this palette
                int n;
                struct BGRAf_t Mean = (struct BGRAf_t)
                {
                    0,0,0,0
                };
                for(n=PalUnused; n<MaxPalSize; n++) Mean = BGRAf_Add(&Mean, &TilePalettes[i*MaxPalSize+n]);
                Mean = BGRAf_Divi(&Mean, MaxPalSize-PalUnused);

                //! Compute slopes and store to the palette spread
                //! NOTE: For some reason, it works better to use the square root as a weight.
                //! This probably gives a value somewhere between the arithmetic mean and
                //! the smooth-max, which should result in better quality.
                //! NOTE: Pre-multiply by DitherLevel to remove a multiply from the main loop.
                struct BGRAf_t Spread = {0,0,0,0}, SpreadW = {0,0,0,0};
                for(n=PalUnused; n<MaxPalSize; n++)
                {
                    struct BGRAf_t d = BGRAf_Sub(&TilePalettes[i*MaxPalSize+n], &Mean);
                    d = BGRAf_Abs(&d);
                    struct BGRAf_t w = BGRAf_Sqrt(&d);
                    d = BGRAf_Mul(&d, &w);
                    Spread  = BGRAf_Add(&Spread,  &d);
                    SpreadW = BGRAf_Add(&SpreadW, &w);
                }
                Spread = BGRAf_DivSafe(&Spread, &SpreadW, NULL);
#ifdef DITHER_NO_ALPHA
                Spread.a = 0.0f;
#endif
                Dither.PaletteSpread[i] = BGRAf_Muli(&Spread, DitherLevel);
            }
        }
        else
        {
            //! "Real" ordered dithering (without tile palettes)
            static const struct BGRA8_t MinValue = {1,1,1,1};
            struct BGRAf_t Spread = BGRAf_FromBGRA(&MinValue, BitRange);
            Dither.PaletteSpread[0] = BGRAf_Muli(&Spread, DitherLevel);
        }
    }

    //! Begin processing of pixels
    int TileHeightCounter = TileH;
    struct BGRAf_t *DiffuseThisLine = Dither.DiffuseError + 1;    //! <- 1px padding on left
    struct BGRAf_t *DiffuseNextLine = DiffuseThisLine + (ImgW+1); //! <- 1px padding on right
    struct BGRAf_t RMSE = (struct BGRAf_t)
    {
        0,0,0,0
    };
    for(y=0; y<ImgH; y++)
    {
        int TilePalIdx = 0;
        int TileWidthCounter = 0;
        for(x=0; x<ImgW; x++)
        {
            //! Advance tile palette index
            if(TilePxOutput && --TileWidthCounter <= 0)
            {
                TilePalIdx = *TilePalIndices++;
                TileWidthCounter = TileW;
            }

            //! Get pixel and apply dithering
            struct BGRAf_t Px, Px_Original;
            {
                //! Read original pixel data
                struct BGRA8_t p;
                if(PxSrcIdx) p = PxSrcBGR[*PxSrcIdx++];
                else         p = *PxSrcBGR++;
                Px = Px_Original = BGRAf_FromBGRA8(&p);
            }
            if(DitherType != DITHER_NONE)
            {
                if(DitherType == DITHER_FLOYDSTEINBERG)
                {
                    //! Adjust for diffusion error
                    struct BGRAf_t t = DiffuseThisLine[x];
#ifdef DITHER_NO_ALPHA
                    t.a = 0.0f;
#endif
                    t  = BGRAf_Muli(&t, DitherLevel);
                    Px = BGRAf_Add (&Px, &t);
                }
                else
                {
                    //! Adjust for dither matrix
                    int Threshold = 0, xKey = x, yKey = x^y;
                    int Bit = DitherType-1;
                    do
                    {
                        Threshold = Threshold*2 + (yKey & 1), yKey >>= 1; //! <- Hopefully turned into "SHR, ADC"
                        Threshold = Threshold*2 + (xKey & 1), xKey >>= 1;
                    }
                    while(--Bit >= 0);
                    float fThres = Threshold * (1.0f / (1 << (2*DitherType))) - 0.5f;
                    struct BGRAf_t DitherVal = BGRAf_Muli(&Dither.PaletteSpread[TilePalIdx], fThres);
                    Px = BGRAf_Add(&Px, &DitherVal);
                }
            }

            //! Find matching palette entry, store to output, and get error
            if(TilePxOutput)
            {
                int PalIdx  = FindPaletteEntry(&Px, TilePalettes + TilePalIdx*MaxPalSize, MaxPalSize, PalUnused);
                PalIdx += TilePalIdx*MaxPalSize;
                *TilePxOutput++ = PalIdx;
                Px = TilePalettes[PalIdx];
            }
            else
            {
                //! Reduce range when not using tile output
                struct BGRA8_t t = BGRA_FromBGRAf(&Px, BitRange);
                Px = BGRAf_FromBGRA(&t, BitRange);
            }
            if(RawPxOutput)
            {
                *RawPxOutput++ = Px;
            }
            struct BGRAf_t Error = BGRAf_Sub(&Px_Original, &Px);

            //! Add to error diffusion
            if(DitherType == DITHER_FLOYDSTEINBERG)
            {
                struct BGRAf_t t;

                //! {x+1,y} @ 7/16
                t = BGRAf_Muli(&Error, 7.0f/16);
                DiffuseThisLine[x+1] = BGRAf_Add(&DiffuseThisLine[x+1], &t);

                //! {x-1,y+1} @ 3/16
                t = BGRAf_Muli(&Error, 3.0f/16);
                DiffuseNextLine[x-1] = BGRAf_Add(&DiffuseNextLine[x-1], &t);

                //! {x+0,y+1} @ 5/16
                t = BGRAf_Muli(&Error, 5.0f/16);
                DiffuseNextLine[x+0] = BGRAf_Add(&DiffuseNextLine[x+0], &t);

                //! {x+1,y+1} @ 1/16
                t = BGRAf_Muli(&Error, 1.0f/16);
                DiffuseNextLine[x+1] = BGRAf_Add(&DiffuseNextLine[x+1], &t);
            }

            //! Accumulate error for RMS calculation
            Error = BGRAf_Mul(&Error, &Error);
            RMSE  = BGRAf_Add(&RMSE, &Error);
        }

        //! Advance tile palette index pointer
        //! At this point, we're already pointing to the next row of tiles,
        //! so we only need to either update the counter, or rewind the pointer.
        if(TilePxOutput)
        {
            if(--TileHeightCounter <= 0)
            {
                TileHeightCounter = TileH;
            }
            else TilePalIndices -= (ImgW/TileW);
        }

        //! Swap diffusion dithering pointers and clear buffer for next line
        if(DitherType == DITHER_FLOYDSTEINBERG)
        {
            struct BGRAf_t *t = DiffuseThisLine;
            DiffuseThisLine = DiffuseNextLine;
            DiffuseNextLine = t;
            for(x=0; x<ImgW; x++) DiffuseNextLine[x] = (struct BGRAf_t)
            {
                0,0,0,0
            };
        }
    }

    //! Return error
    RMSE = BGRAf_Divi(&RMSE, ImgW*ImgH);
    RMSE = BGRAf_Sqrt(&RMSE);
    return RMSE;
}

/**************************************/
//! EOF
/**************************************/
