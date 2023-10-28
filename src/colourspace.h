/**************************************/
#pragma once
/**************************************/
#include <math.h>
#include <stdint.h>
/**************************************/
#define COLOURSPACE_CLIP(x, Min, Max) ((x) < (Min) ? (Min) : (x) > (Max) ? (Max) : (x))
/**************************************/

struct BGRA8_t
{
    uint8_t b, g, r, a;
};
struct BGRAf_t
{
    float   b, g, r, a;
};

/**************************************/

static inline struct BGRA8_t BGRA_FromBGRAf(const struct BGRAf_t *x, const struct BGRA8_t *Range)
{
    struct BGRA8_t Out;
    Out.b = (uint8_t)COLOURSPACE_CLIP((x->b*Range->b + 0.5f), 0, Range->b);
    Out.g = (uint8_t)COLOURSPACE_CLIP((x->g*Range->g + 0.5f), 0, Range->g);
    Out.r = (uint8_t)COLOURSPACE_CLIP((x->r*Range->r + 0.5f), 0, Range->r);
    Out.a = (uint8_t)COLOURSPACE_CLIP((x->a*Range->a + 0.5f), 0, Range->a);
    return Out;
}

static inline struct BGRA8_t BGRA8_FromBGRAf(const struct BGRAf_t *x)
{
    return BGRA_FromBGRAf(x, &(struct BGRA8_t)
    {
        255,255,255,255
    });
}

/**************************************/

static inline struct BGRAf_t BGRAf_FromBGRA(const struct BGRA8_t *x, const struct BGRA8_t *Range)
{
    struct BGRAf_t Out =
    {
        x->b / (float)Range->b,
        x->g / (float)Range->g,
        x->r / (float)Range->r,
        x->a / (float)Range->a
    };
    return Out;
}

static inline struct BGRAf_t BGRAf_FromBGRA8(const struct BGRA8_t *x)
{
    return BGRAf_FromBGRA(x, &(struct BGRA8_t)
    {
        255,255,255,255
    });
}

/**************************************/

//! x->b = Y
//! x->g = Cb
//! x->r = Cr
//! Using ITU-R BT.709 constants
static inline struct BGRAf_t BGRAf_AsYUV(const struct BGRAf_t *x)
{
    return (struct BGRAf_t)
    {
         0.2126f*x->r + 0.71520f*x->g + 0.0722f*x->b,
        -0.1146f*x->r - 0.38540f*x->g + 0.5000f*x->b,
         0.5f   *x->r - 0.45420f*x->g - 0.0458f*x->b,
        x->a
    };
}
static inline struct BGRAf_t BGRAf_FromYUV(const struct BGRAf_t *x)
{
    return (struct BGRAf_t)
    {
        x->b + 1.855609686f*x->g + 0.000105740f*x->b,
        x->b - 0.187280216f*x->g - 0.468124625f*x->r,
        x->b - 0.000151501f*x->g + 1.574765276f*x->r,
        x->a
    };
}

/**************************************/

static inline struct BGRAf_t BGRAf_Add(const struct BGRAf_t *a, const struct BGRAf_t *b)
{
    struct BGRAf_t Out;
    Out.b = a->b + b->b;
    Out.g = a->g + b->g;
    Out.r = a->r + b->r;
    Out.a = a->a + b->a;
    return Out;
}

static inline struct BGRAf_t BGRAf_Addi(const struct BGRAf_t *a, float b)
{
    struct BGRAf_t Out;
    Out.b = a->b + b;
    Out.g = a->g + b;
    Out.r = a->r + b;
    Out.a = a->a + b;
    return Out;
}

/**************************************/

static inline struct BGRAf_t BGRAf_Sub(const struct BGRAf_t *a, const struct BGRAf_t *b)
{
    struct BGRAf_t Out;
    Out.b = a->b - b->b;
    Out.g = a->g - b->g;
    Out.r = a->r - b->r;
    Out.a = a->a - b->a;
    return Out;
}

static inline struct BGRAf_t BGRAf_Subi(const struct BGRAf_t *a, float b)
{
    struct BGRAf_t Out;
    Out.b = a->b - b;
    Out.g = a->g - b;
    Out.r = a->r - b;
    Out.a = a->a - b;
    return Out;
}

/**************************************/

static inline struct BGRAf_t BGRAf_Mul(const struct BGRAf_t *a, const struct BGRAf_t *b)
{
    struct BGRAf_t Out;
    Out.b = a->b * b->b;
    Out.g = a->g * b->g;
    Out.r = a->r * b->r;
    Out.a = a->a * b->a;
    return Out;
}

static inline struct BGRAf_t BGRAf_Muli(const struct BGRAf_t *a, float b)
{
    struct BGRAf_t Out;
    Out.b = a->b * b;
    Out.g = a->g * b;
    Out.r = a->r * b;
    Out.a = a->a * b;
    return Out;
}

/**************************************/

static inline struct BGRAf_t BGRAf_Div(const struct BGRAf_t *a, const struct BGRAf_t *b)
{
    struct BGRAf_t Out;
    Out.b = a->b / b->b;
    Out.g = a->g / b->g;
    Out.r = a->r / b->r;
    Out.a = a->a / b->a;
    return Out;
}

static inline struct BGRAf_t BGRAf_DivSafe(const struct BGRAf_t *a, const struct BGRAf_t *b, const struct BGRAf_t *DivByZeroValue)
{
    static const struct BGRAf_t Zero = {0,0,0,0};
    if(!DivByZeroValue) DivByZeroValue = &Zero;

    struct BGRAf_t Out;
    Out.b = (b->b == 0.0f) ? DivByZeroValue->b : (a->b / b->b);
    Out.g = (b->g == 0.0f) ? DivByZeroValue->g : (a->g / b->g);
    Out.r = (b->r == 0.0f) ? DivByZeroValue->r : (a->r / b->r);
    Out.a = (b->a == 0.0f) ? DivByZeroValue->a : (a->a / b->a);
    return Out;
}

static inline struct BGRAf_t BGRAf_Divi(const struct BGRAf_t *a, float b)
{
#if 0
    struct BGRAf_t Out;
    Out.b = a->b / b;
    Out.g = a->g / b;
    Out.r = a->r / b;
    Out.a = a->a / b;
    return Out;
#else
    return BGRAf_Muli(a, 1.0f / b);
#endif
}

static inline struct BGRAf_t BGRAf_InvDivi(const struct BGRAf_t *a, float b)
{
    struct BGRAf_t Out;
    Out.b = b / a->b;
    Out.g = b / a->g;
    Out.r = b / a->r;
    Out.a = b / a->a;
    return Out;
}

/**************************************/

static inline float BGRAf_Dot(const struct BGRAf_t *a, const struct BGRAf_t *b)
{
    return a->b*b->b +
           a->g*b->g +
           a->r*b->r +
           a->a*b->a ;
}

static inline struct BGRAf_t BGRAf_Sqrt(const struct BGRAf_t *x)
{
    struct BGRAf_t Out;
    Out.b = sqrtf(x->b);
    Out.g = sqrtf(x->g);
    Out.r = sqrtf(x->r);
    Out.a = sqrtf(x->a);
    return Out;
}

static inline struct BGRAf_t BGRAf_Dist2(const struct BGRAf_t *a, const struct BGRAf_t *b)
{
    struct BGRAf_t Out;
    Out = BGRAf_Sub(a, b);
    Out = BGRAf_Mul(&Out, &Out);
    return Out;
}

static inline struct BGRAf_t BGRAf_Dist(const struct BGRAf_t *a, const struct BGRAf_t *b)
{
    struct BGRAf_t Out;
    Out = BGRAf_Dist2(a, b);
    Out = BGRAf_Sqrt(&Out);
    return Out;
}

static inline float BGRAf_AbsDist2(const struct BGRAf_t *a, const struct BGRAf_t *b)
{
    struct BGRAf_t Out;
    Out = BGRAf_Sub(a, b);
    return BGRAf_Dot(&Out, &Out);
}

static inline float BGRAf_AbsDist(const struct BGRAf_t *a, const struct BGRAf_t *b)
{
    return sqrtf(BGRAf_AbsDist2(a, b));
}

static inline float BGRAf_Len2(const struct BGRAf_t *x)
{
    return BGRAf_Dot(x, x);
}

static inline float BGRAf_Len(const struct BGRAf_t *x)
{
    return sqrtf(BGRAf_Len2(x));
}

/**************************************/

static inline struct BGRAf_t BGRAf_Abs(const struct BGRAf_t *x)
{
    struct BGRAf_t Out;
    Out.b = (x->b < 0) ? (-x->b) : (x->b);
    Out.g = (x->g < 0) ? (-x->g) : (x->g);
    Out.r = (x->r < 0) ? (-x->r) : (x->r);
    Out.a = (x->a < 0) ? (-x->a) : (x->a);
    return Out;
}

/**************************************/

//! Colour distance function
static inline float BGRAf_ColDistance(const struct BGRAf_t *a, const struct BGRAf_t *b)
{
    struct BGRAf_t d = BGRAf_Sub(a, b);
    return BGRAf_Len2(&d);
}

/**************************************/
//! EOF
/**************************************/
