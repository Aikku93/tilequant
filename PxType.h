/**************************************/
#pragma once
/**************************************/
#include <math.h>
#include <stdint.h>
/**************************************/
#define PXTYPE_CLIP(x, Min, Max) ((x) < (Min) ? (Min) : (x) > (Max) ? (Max) : (x))
/**************************************/

struct BGRA8_t { uint8_t b, g, r, a; };
struct YUVAf_t { float   y, u, v, a; };

/**************************************/

static inline struct BGRA8_t BGRA8_FromYUVAf_Ranged(const struct YUVAf_t *x, const struct BGRA8_t *Range) {
	int b = lrintf((x->y - 0.5f*(x->u + x->v)) * Range->b);
	int g = lrintf((x->y + 0.5f*(x->u       )) * Range->g);
	int r = lrintf((x->y - 0.5f*(x->u - x->v)) * Range->r);
	int a = lrintf((x->a                     ) * Range->a);

	struct BGRA8_t Out;
	Out.b = (uint8_t)PXTYPE_CLIP(b, 0, Range->b);
	Out.g = (uint8_t)PXTYPE_CLIP(g, 0, Range->g);
	Out.r = (uint8_t)PXTYPE_CLIP(r, 0, Range->r);
	Out.a = (uint8_t)PXTYPE_CLIP(a, 0, Range->a);
	return Out;
}

static inline struct BGRA8_t BGRA8_FromYUVAf(const struct YUVAf_t *x) {
	return BGRA8_FromYUVAf_Ranged(x, &(struct BGRA8_t){255,255,255,255});
}

/**************************************/

static inline struct YUVAf_t YUVAf_FromBGRA8_Ranged(const struct BGRA8_t *x, const struct BGRA8_t *Range) {
	float pb = x->b / (float)Range->b;
	float pg = x->g / (float)Range->g;
	float pr = x->r / (float)Range->r;
	float pa = x->a / (float)Range->a;
	float y = ( pr + 2*pg + pb) * 0.25f;
	float u = (-pr + 2*pg - pb) * 0.50f;
	float v = ( pr        - pb) * 1.00f;
	float a = ( pa            ) * 1.00f;

	struct YUVAf_t Out;
	Out.y = y;
	Out.u = u;
	Out.v = v;
	Out.a = a;
	return Out;
}

static inline struct YUVAf_t YUVAf_FromBGRA8(const struct BGRA8_t *x) {
	return YUVAf_FromBGRA8_Ranged(x, &(struct BGRA8_t){255,255,255,255});
}

/**************************************/

static inline struct YUVAf_t YUVAf_Add(const struct YUVAf_t *a, const struct YUVAf_t *b) {
	struct YUVAf_t Out;
	Out.y = a->y + b->y;
	Out.u = a->u + b->u;
	Out.v = a->v + b->v;
	Out.a = a->a + b->a;
	return Out;
}

static inline struct YUVAf_t YUVAf_Addi(const struct YUVAf_t *a, float b) {
	struct YUVAf_t Out;
	Out.y = a->y + b;
	Out.u = a->u + b;
	Out.v = a->v + b;
	Out.a = a->a + b;
	return Out;
}

/**************************************/

static inline struct YUVAf_t YUVAf_Sub(const struct YUVAf_t *a, const struct YUVAf_t *b) {
	struct YUVAf_t Out;
	Out.y = a->y - b->y;
	Out.u = a->u - b->u;
	Out.v = a->v - b->v;
	Out.a = a->a - b->a;
	return Out;
}

static inline struct YUVAf_t YUVAf_Subi(const struct YUVAf_t *a, float b) {
	struct YUVAf_t Out;
	Out.y = a->y - b;
	Out.u = a->u - b;
	Out.v = a->v - b;
	Out.a = a->a - b;
	return Out;
}

/**************************************/

static inline struct YUVAf_t YUVAf_Mul(const struct YUVAf_t *a, const struct YUVAf_t *b) {
	struct YUVAf_t Out;
	Out.y = a->y * b->y;
	Out.u = a->u * b->u;
	Out.v = a->v * b->v;
	Out.a = a->a * b->a;
	return Out;
}

static inline struct YUVAf_t YUVAf_Muli(const struct YUVAf_t *a, float b) {
	struct YUVAf_t Out;
	Out.y = a->y * b;
	Out.u = a->u * b;
	Out.v = a->v * b;
	Out.a = a->a * b;
	return Out;
}

/**************************************/

static inline struct YUVAf_t YUVAf_Div(const struct YUVAf_t *a, const struct YUVAf_t *b) {
	struct YUVAf_t Out;
	Out.y = a->y / b->y;
	Out.u = a->u / b->u;
	Out.v = a->v / b->v;
	Out.a = a->a / b->a;
	return Out;
}

static inline struct YUVAf_t YUVAf_Divi(const struct YUVAf_t *a, float b) {
	struct YUVAf_t Out;
	Out.y = a->y / b;
	Out.u = a->u / b;
	Out.v = a->v / b;
	Out.a = a->a / b;
	return Out;
}

/**************************************/

static inline float YUVAf_Dot(const struct YUVAf_t *a, const struct YUVAf_t *b) {
	return a->y*b->y +
	       a->u*b->u +
	       a->v*b->v +
	       a->a*b->a ;
}

static inline struct YUVAf_t YUVAf_Sqrt(const struct YUVAf_t *x) {
	struct YUVAf_t Out;
	Out.y = sqrtf(x->y);
	Out.u = sqrtf(x->u);
	Out.v = sqrtf(x->v);
	Out.a = sqrtf(x->a);
	return Out;
}

static inline struct YUVAf_t YUVAf_Dist2(const struct YUVAf_t *a, const struct YUVAf_t *b) {
	struct YUVAf_t Out;
	Out = YUVAf_Sub(a, b);
	Out = YUVAf_Mul(&Out, &Out);
	return Out;
}

static inline struct YUVAf_t YUVAf_Dist(const struct YUVAf_t *a, const struct YUVAf_t *b) {
	struct YUVAf_t Out;
	Out = YUVAf_Dist2(a, b);
	Out = YUVAf_Sqrt(&Out);
	return Out;
}

static inline float YUVAf_AbsDist2(const struct YUVAf_t *a, const struct YUVAf_t *b) {
	struct YUVAf_t Out;
	Out = YUVAf_Sub(a, b);
	return YUVAf_Dot(&Out, &Out);
}

static inline float YUVAf_AbsDist(const struct YUVAf_t *a, const struct YUVAf_t *b) {
	return sqrt(YUVAf_AbsDist2(a, b));
}

static inline float YUVAf_Len2(const struct YUVAf_t *x) {
	return YUVAf_Dot(x, x);
}

static inline float YUVAf_Len(const struct YUVAf_t *x) {
	return sqrtf(YUVAf_Len2(x));
}

/**************************************/

static inline struct YUVAf_t YUVAf_Sign(const struct YUVAf_t *x) {
	struct YUVAf_t Out;
	Out.y = (x->y < 0) ? (-1) : (+1);
	Out.u = (x->u < 0) ? (-1) : (+1);
	Out.v = (x->v < 0) ? (-1) : (+1);
	Out.a = (x->a < 0) ? (-1) : (+1);
	return Out;
}

static inline struct YUVAf_t YUVAf_SignedSquare(const struct YUVAf_t *x) {
	struct YUVAf_t Out, Sign = YUVAf_Sign(x);
	Out = YUVAf_Mul(x, x);
	Out = YUVAf_Mul(&Out, &Sign);
	return Out;
}

static inline struct YUVAf_t YUVAf_SignedSqrt(const struct YUVAf_t *x) {
	struct YUVAf_t Out, Sign = YUVAf_Sign(x);
	Out = YUVAf_Mul(x, &Sign);
	Out = YUVAf_Sqrt(&Out);
	Out = YUVAf_Mul(&Out, &Sign);
	return Out;
}

/**************************************/

//! Colour distance function
//! Attempted to tune this as much as posssible
static inline float YUVAf_ColDistance(const struct YUVAf_t *a, const struct YUVAf_t *b) {
	struct YUVAf_t d = YUVAf_Sub(a, b);
	d = YUVAf_Muli(&d, (1.0f + fabs(d.y)) * (1.0f + fabs(d.a)));
	return YUVAf_Dot(&d, &d);
}

/**************************************/
//! EOF
/**************************************/
