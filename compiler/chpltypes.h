#ifndef _chpltypes_H_
#define _chpltypes_H_

typedef char int8;
typedef unsigned char uint8;
typedef int int32;
typedef unsigned int uint32;
typedef long long int64;
typedef unsigned long long uint64;
typedef short int16;
typedef unsigned short uint16;
/* typedef uint32 uint; * already part of most systems */
typedef float float32;
typedef double float64;
typedef struct { float64 real; float64 imag; } complex64;

#endif
