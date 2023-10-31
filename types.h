#ifndef XASM_TYPES_H_
#define XASM_TYPES_H_

#include <stdint.h>
#include "macros.h"

#if CHAR_BIT != 8
#error Need 8 bit char type
#endif

// Based on https://nullprogram.com/blog/2023/10/08/

typedef char c8;

typedef int32_t b32;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef char byte;

typedef ptrdiff_t size;

#endif /* XASM_TYPES_H_ */
