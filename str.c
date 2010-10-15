/*
  $Id: str.c,v 1.8 2006/12/26 18:15:51 francesco Exp $
 */

#define _GNU_SOURCE 1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "common.h"
#include "str.h"

static inline size_t
str_sz_round (size_t sz)
{
#define MINSIZE (1 << 4)
  size_t res = MINSIZE;

  while (res < sz)
    {
      res *= 2;
      /* integer overflow ? */
      assert (res > 0);
    }

  return res;
}

static inline void
str_init_raw (str_ptr s, size_t len)
{
  s->size = str_sz_round (len+1);
  s->heap = emalloc (s->size);
}

void
str_init (str_ptr s, int len)
{
  if (len < 0)
    len = 0;

  str_init_raw (s, (size_t)len);
  s->heap[0] = 0;
  s->length = 0;
}

str_ptr
str_new (void)
{
  str_ptr s = emalloc (sizeof(str_t));
  str_init_raw (s, 16);
  s->heap[0] = 0;
  s->length = 0;
  return s;
}

void
str_free (str_ptr s)
{
  if (s->size > 0)
    free (s->heap);
  s->heap = 0;
}

static void
str_init_from_c_raw (str_ptr s, const char *sf, size_t len)
{
  str_init_raw (s, len);
  memcpy (s->heap, sf, len+1);
  s->length = len;
}

void
str_init_from_c (str_ptr s, const char *sf)
{
  size_t len = strlen (sf);
  str_init_from_c_raw (s, sf, len);
}

void
str_init_from_str (str_ptr s, const str_t sf)
{
  str_init_from_c_raw (s, CSTR(sf), STR_LENGTH(sf));
}

void
str_size_check (str_t s, size_t reqlen)
{
  char *old_heap;

  if (reqlen+1 < s->size)
    return;

  old_heap = (s->size > 0 ? s->heap : NULL);
  str_init_raw (s, reqlen);

  if (old_heap)
    {
      memcpy (s->heap, old_heap, s->length+1);
      free (old_heap);
    }
}

void
str_copy (str_t d, str_t s)
{
  const size_t len = s->length;
  str_size_check (d, len);
  memcpy (d->heap, s->heap, len+1);
  d->length = len;
}

void
str_copy_c (str_t d, const char *s)
{
  const size_t len = strlen (s);
  str_size_check (d, len);
  memcpy (d->heap, s, len+1);
  d->length = len;
}

void
str_copy_c_substr (str_t d, const char *s, int len)
{
  len = (len >= 0 ? len : 0);
  str_size_check (d, (size_t)len);
  memcpy (d->heap, s, (size_t)len);
  d->heap[len] = 0;
  d->length = len;
}

void
str_append_c (str_t to, const char *from, int sep)
{
  size_t flen = strlen (from);
  size_t newlen = STR_LENGTH(to) + flen + (sep == 0 ? 0 : 1);
  int idx = STR_LENGTH(to);

  str_size_check (to, newlen);

  if (sep != 0)
    to->heap[idx++] = sep;

  memcpy (to->heap + idx, from, flen+1);
  to->length = newlen;
}

void
str_append (str_t to, const str_t from, int sep)
{
  str_append_c (to, CSTR(from), sep);
}

void
str_trunc (str_t s, int len)
{
  if (len < 0 || (size_t)len >= STR_LENGTH(s))
    return;

  s->heap[len] = 0;
  s->length = len;
}

void
str_get_basename (str_t to, const str_t from, int dirsep)
{
  const char * ptr = strrchr (CSTR(from), dirsep);

  if (ptr == NULL)
    str_copy_c (to, CSTR(from));
  else
    str_copy_c (to, ptr + 1);
}

void
str_dirname (str_t to, const str_t from, int dirsep)
{
  const char * ptr = strrchr (CSTR(from), dirsep);

  if (ptr == NULL)
    str_copy_c (to, CSTR(from));
  else
    str_copy_c_substr (to, CSTR(from), ptr - CSTR(from));
}

int
str_getline (str_t d, FILE *f)
{
  char *res, *ptr;
  int szres, pending = 0;

  str_size_check (d, 0);
  str_trunc (d, 0);

  for (;;)
    {
      int j;

      ptr = d->heap + d->length;
      szres = d->size - d->length;

      res = fgets (ptr, szres, f);

      if (res == NULL)
	{
	  if (feof (f) && pending)
	    return 0;
	  return (-1);
	}

      for (j = 0; j < szres; j++)
	{
	  if (ptr[j] == '\n' || ptr[j] == 0)
	    {
	      ptr[j] = 0;
	      d->length += j;
	      break;
	    }
	}

      if (j < szres - 1)
	break;

      pending = 1;

      str_size_check (d, 2 * d->length);
    }

  if (d->length > 0)
    {
      if (d->heap[d->length - 1] == '\015')
	{
	  d->heap[d->length - 1] = 0;
	  d->length --;
	}
    }

  return 0;
}

void
str_vprintf (str_t d, const char *fmt, int append, va_list ap)
{
  char *buffer;
  int ns;

  ns = vasprintf (&buffer, fmt, ap);
  assert (ns >= 0);

  if (append)
    str_append_c (d, buffer, 0);
  else
    {
      free (d->heap);
      d->heap = buffer;
      d->size = ns;
      d->length = strlen (buffer);
    }
#if 0
  static char buffer[1024];
  int ns;
  ns = vsnprintf (buffer, 1024, fmt, ap);
  buffer[1024 - 1] = 0;
  if (ns >= 0)
    {
      if (append)
	str_append_c (d, buffer, 0);
      else
	str_copy_c (d, buffer);
    }
#endif
}

void
str_printf (str_t d, const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  str_vprintf (d, fmt, 0, ap);
  va_end (ap);
}

void
str_printf_add (str_t d, const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  str_vprintf (d, fmt, 1, ap);
  va_end (ap);
}

void
str_pad (str_t s, int len, char sep)
{
  int diff = len - s->length;

  if (diff <= 0)
    return;

  str_size_check (s, (size_t)len-1);
  
  memmove (s->heap + diff, s->heap, (s->length + 1) * sizeof(char));
  memset (s->heap, sep, diff * sizeof(char));

  s->length += diff;
}
