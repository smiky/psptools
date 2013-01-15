/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#ifndef __UTILS_H
#define __UTILS_H

#include <stddef.h>
#include <stdarg.h>

void report (const char *fmt, ...);

void error (const char *fmt, ...);
void xerror (const char *fmt, ...);

void fatal (const char *fmt, ...);
void xfatal (const char *fmt, ...);

void *xmalloc (size_t size);
void *xrealloc (void *ptr, size_t size);

void *read_file (const char *path, size_t *size);

#endif /* __UTILS_H */
