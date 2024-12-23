#ifndef __MAGDEBUG_HDR
#define __MAGDEBUG_HDR

#ifdef __DEBUG
#define MemoryAlloc(x,y) AllocVec(x,y); printf("AllocVec: In Func %s, File %s, Line %s, Size %u\n", __FUNC__, __FILE__, __LINE__, x)
#define D(x) x
#else
#define MemoryAlloc(x,y) AllocVec(x,y)
#define D(x)
#endif



#endif