/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "utils.h"

void report (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stdout, fmt, ap);
  va_end (ap);
}

void error (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  fprintf (stderr, "\n");
  va_end (ap);
}

void xerror (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  fprintf (stderr, ": %s\n", strerror (errno));
  va_end (ap);
}


void fatal (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  fprintf (stderr, "fatal: ");
  vfprintf (stderr, fmt, ap);
  fprintf (stderr, "\n");
  va_end (ap);
  exit (1);
}

void xfatal (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  fprintf (stderr, "fatal: ");
  vfprintf (stderr, fmt, ap);
  fprintf (stderr, ": %s\n", strerror (errno));
  va_end (ap);
  exit (1);
}

void *xmalloc (size_t size)
{
  void *ptr = malloc (size);
  if (!ptr) fatal (__FILE__ ": memory exhausted");
  return ptr;
}

void *xrealloc (void *ptr, size_t size)
{
  void *nptr = realloc (ptr, size);
  if (!nptr) fatal (__FILE__ ": can't realloc");
  return nptr;
}


static
int _file_size (FILE *fp, const char *path, size_t *size)
{
  long r;

  if (fseek (fp, 0L, SEEK_END)) {
    xerror (__FILE__ ": can't seek file `%s'", path);
    fclose (fp);
    return 0;
  }

  r = ftell (fp);
  if (r == -1) {
    xerror (__FILE__ ": can't get file size of `%s'", path);
    return 0;
  }

  if (size) *size = (size_t) r;
  return 1;
}

void *read_file (const char *path, size_t *size)
{
  FILE *fp;
  void *buffer;
  size_t file_size;
  size_t read_return;

  fp = fopen (path, "rb");
  if (!fp) {
    xerror (__FILE__ ": can't open file `%s'", path);
    return NULL;
  }

  if (!_file_size (fp, path, &file_size)) {
    return NULL;
  }

  buffer = xmalloc (file_size);
  rewind (fp);

  read_return = fread (buffer, 1, file_size, fp);
  fclose (fp);

  if (read_return != file_size) {
    error (__FILE__ ": can't fully read file `%s'", path);
    free (buffer);
    return NULL;
  }

  if (size) *size = file_size;
  return buffer;
}
