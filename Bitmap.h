/**************************************/
#pragma once
/**************************************/
#include <stdint.h>
/**************************************/
#include "PxType.h"
/**************************************/
#define BMP_PALETTE_COLOURS 256
/**************************************/

struct BmpCtx_t {
	int Width, Height;
	struct BGRA8_t *ColPal;
	union {
		       uint8_t *PxIdx; //! Palettized
		struct BGRA8_t *PxBGR; //! Direct
	};
};

/**************************************/

//! Create context
//! Pass PalCol=0 for BGRA
int BmpCtx_Create(struct BmpCtx_t *Ctx, int w, int h, int PalCol);

//! Destroy context
void BmpCtx_Destroy(struct BmpCtx_t *Ctx);

//! Load from file
//! NOTE: Image is vertically inverted
//! NOTE: This internally creates the context
int BmpCtx_FromFile(struct BmpCtx_t *Ctx, const char *Filename);

//! Write to file
//! To write a BGRA image, set ColPal=nullptr
//! NOTE: Always 32bit BGRA; 24bit BGR is never used for output
int BmpCtx_ToFile(const struct BmpCtx_t *Ctx, const char *Filename);

/**************************************/
//! EOF
/**************************************/
