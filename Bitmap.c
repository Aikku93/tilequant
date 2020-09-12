/**************************************/
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/**************************************/
#include "Bitmap.h"
#include "Colourspace.h"
/**************************************/

//! Clear context data
#define CLEAR_CONTEXT(Ctx)  \
	Ctx->Width  = 0,    \
	Ctx->Height = 0,    \
	Ctx->ColPal = NULL, \
	Ctx->PxBGR  = NULL

//! Destroy context and return 0
#define DESTROY_AND_RETURN(Ctx, ...) \
	do {                         \
		BmpCtx_Destroy(Ctx); \
		return __VA_ARGS__;  \
	} while(0)

/**************************************/

//! BMP file header
struct __attribute__((packed)) BMFH_t {
	uint16_t Type;
	uint32_t Size;
	uint16_t r1[2];
	uint32_t Offs;
};

//! BMP image header
struct __attribute__((packed)) BMIH_t {
	uint32_t Size;
	uint32_t Width;
	uint32_t Height;
	uint16_t nPlanes;
	uint16_t BitCnt;
	uint32_t CompType;
	uint32_t ImgSize;
	uint32_t XpM;
	uint32_t YpM;
	uint32_t ColUsed;
	uint32_t ColImportant;
};

/**************************************/

//! Create context
int BmpCtx_Create(struct BmpCtx_t *Ctx, int w, int h, int PalCol) {
	Ctx->Width  = w;
	Ctx->Height = h;
	if(PalCol) {
		Ctx->ColPal = calloc(PalCol, sizeof(struct BGRA8_t));
		Ctx->PxIdx  = calloc(w * h,  sizeof(uint8_t));
		if(!Ctx->ColPal || !Ctx->PxIdx) DESTROY_AND_RETURN(Ctx, 0);
	} else {
		Ctx->ColPal = NULL;
		Ctx->PxBGR  = calloc(w * h, sizeof(struct BGRA8_t));
		if(!Ctx->PxBGR) DESTROY_AND_RETURN(Ctx, 0);
	}
	return 1;
}

/**************************************/

//! Destroy context
void BmpCtx_Destroy(struct BmpCtx_t *Ctx) {
	free(Ctx->ColPal);
	free(Ctx->PxBGR);
	CLEAR_CONTEXT(Ctx);
}

/**************************************/

//! Load from file
int BmpCtx_FromFile(struct BmpCtx_t *Ctx, const char *Filename) {
	CLEAR_CONTEXT(Ctx);

	//! Open file, read headers
	FILE *File = fopen(Filename, "rb"); if(!File) return 0;
	struct BMFH_t bmFH; fread(&bmFH, 1, sizeof(bmFH), File);
	struct BMIH_t bmIH; fread(&bmIH, 1, sizeof(bmIH), File);
	Ctx->Width  = bmIH.Width;
	Ctx->Height = bmIH.Height;

	//! Read pixels
	int nPx = Ctx->Width*Ctx->Height;
	if(bmFH.Type == ('B'|'M'<<8)) switch(bmIH.BitCnt) {
		//! 8bit palettized
		case 8: {
			//! Read palette
			Ctx->ColPal = malloc(BMP_PALETTE_COLOURS * sizeof(struct BGRA8_t));
			if(!Ctx->ColPal) break;
			fread(Ctx->ColPal, BMP_PALETTE_COLOURS, sizeof(struct BGRA8_t), File);

			//! Read pixels
			fseek(File, bmFH.Offs, SEEK_SET);
			Ctx->PxIdx = malloc(nPx * sizeof(uint8_t));
			if(!Ctx->PxIdx) break;
			fread(Ctx->PxIdx, sizeof(uint8_t), nPx, File);
		} break;

		//! BGR
		case 24: {
			struct BGRA8_t *PxBGR = Ctx->PxBGR = malloc(nPx * sizeof(struct BGRA8_t));
			if(!PxBGR) break;

			//! Convert pixels
			int i;
			for(i=0;i<nPx;i++) {
				//! Read BGR
				struct __attribute__((packed)) {
					uint8_t b, g, r;
				} Col;
				fread(&Col, 1, sizeof(Col), File);

				//! Store BGRA
				PxBGR[i].b = Col.b;
				PxBGR[i].g = Col.g;
				PxBGR[i].r = Col.r;
				PxBGR[i].a = 255;
			}
		} break;

		//! BGRA
		case 32: {
			//! Everything is prepared already, so straight read
			Ctx->PxBGR = malloc(nPx * sizeof(struct BGRA8_t));
			if(!Ctx->PxBGR) break;
			fread(Ctx->PxBGR, sizeof(struct BGRA8_t), nPx, File);
		} break;
	}

	//! Close file, check success
	fclose(File);
	if(Ctx->PxBGR || (Ctx->ColPal && Ctx->PxIdx)) return 1;
	else DESTROY_AND_RETURN(Ctx, 0);
}
/**************************************/

//! Write to file
int BmpCtx_ToFile(const struct BmpCtx_t *Ctx, const char *Filename) {
	//! Check image is valid
	int nPx = Ctx->Width*Ctx->Height;
	if(!nPx || (!Ctx->PxBGR && !(Ctx->ColPal && Ctx->PxIdx))) return 0;

	//! Open file, write headers
	FILE *File = fopen(Filename, "wb"); if(!File) return 0;
	struct BMFH_t bmFH; memset(&bmFH, 0, sizeof(bmFH));
	struct BMIH_t bmIH; memset(&bmIH, 0, sizeof(bmIH));
	bmFH.Type     = 'B'|'M'<<8;
	bmFH.Size     = sizeof(struct BMFH_t) + sizeof(struct BMIH_t) + BMP_PALETTE_COLOURS*(Ctx->ColPal ? sizeof(struct BGRA8_t) : 0) + nPx*(Ctx->ColPal ? sizeof(uint8_t) : sizeof(struct BGRA8_t));
	bmFH.Offs     = sizeof(struct BMFH_t) + sizeof(struct BMIH_t) + BMP_PALETTE_COLOURS*(Ctx->ColPal ? sizeof(struct BGRA8_t) : 0);
	bmIH.Size     = sizeof(struct BMIH_t);
	bmIH.Width    = Ctx->Width;
	bmIH.Height   = Ctx->Height;
	bmIH.nPlanes  = 1;
	bmIH.BitCnt   = Ctx->ColPal ? 8 : 32;
	fwrite(&bmFH, 1, sizeof(bmFH), File);
	fwrite(&bmIH, 1, sizeof(bmIH), File);

	//! Write palette
	if(Ctx->ColPal) fwrite(Ctx->ColPal, BMP_PALETTE_COLOURS, sizeof(struct BGRA8_t), File);

	//! Write pixels
	if(Ctx->ColPal) fwrite(Ctx->PxIdx, nPx, sizeof(uint8_t), File);
	else            fwrite(Ctx->PxBGR, nPx, sizeof(struct BGRA8_t), File);

	//! Done
	fclose(File);
	return 1;
}

/**************************************/
//! EOF
/**************************************/
