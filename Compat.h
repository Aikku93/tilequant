/**************************************/
#pragma once
/**************************************/
#ifdef _WIN32
	#include <malloc.h>
	#define aligned_alloc(alignment,size) _aligned_malloc (size, alignment)
	#define free_aligned(x) _aligned_free (x)
#else
	#define free_aligned(x) free (x)
#endif
/**************************************/
