/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#ifndef __TYPES_H
#define __TYPES_H

#include <stddef.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef signed char    int8;
typedef signed short   int16;
typedef signed int     int32;
typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32;

#endif /* __TYPES_H */
