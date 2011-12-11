/* Copyright (C) 2011 Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mysys_priv.h"
#include <m_string.h>
#include <my_sys.h>
#include <my_stacktrace.h>

/**
  strip the path, leave the file name and the last dirname
*/
static const char *strip_path(const char *s)
{
  const char *prev, *last;
  for(prev= last= s; *s; s++)
    if (*s == '/' || *s == '\\')
    {
      prev= last;
      last= s + 1;
    }
  return prev;
}

/*
  The following is very much single-threaded code and it's only supposed
  to be used on shutdown or for a crash report
  Or the caller should take care and use mutexes.

  Also it does not free any its memory. For the same reason -
  it's only used for crash reports or on shutdown when we already
  have a memory leak.
*/

#ifdef HAVE_BFD_H
#include <bfd.h>
static bfd *bfdh= 0;
static asymbol **symtable= 0;

/**
  finds a file name, a line number, and a function name corresponding to addr.

  the function name is demangled.
  the file name is stripped of its path, only the two last components are kept
  the resolving logic is mostly based on addr2line of binutils-2.17

  @return 0 on success, 1 on failure
*/
int my_addr_resolve(void *ptr, my_addr_loc *loc)
{
  bfd_vma addr= (intptr)ptr;
  asection *sec;

  for (sec= bfdh->sections; sec; sec= sec->next)
  {
    bfd_vma start;

    if ((bfd_get_section_flags(bfdh, sec) & SEC_ALLOC) == 0)
      continue;

    start = bfd_get_section_vma(bfdh, sec);
    if (addr < start || addr >= start + bfd_get_section_size(sec))
      continue;

    if (bfd_find_nearest_line(bfdh, sec, symtable, addr - start,
                              &loc->file, &loc->func, &loc->line))
    {
      if (loc->file)
        loc->file= strip_path(loc->file);
      else
        loc->file= "";

      if (loc->func)
      {
        const char *str= bfd_demangle(bfdh, loc->func, 3);
        if (str)
          loc->func= str;
      }

      return 0;
    }
  }
  
  return 1;
}

const char *my_addr_resolve_init()
{
  if (!bfdh)
  {
    uint unused;
    char **matching;

    bfdh= bfd_openr(my_progname, NULL);
    if (!bfdh)
      goto err;

    if (bfd_check_format(bfdh, bfd_archive))
      goto err;
    if (!bfd_check_format_matches (bfdh, bfd_object, &matching))
      goto err;

    if (bfd_read_minisymbols(bfdh, FALSE, (void *)&symtable, &unused) < 0)
      goto err;
  }
  return 0;

err:
  return bfd_errmsg(bfd_get_error());
}
#elif defined(HAVE_LIBELF_H)
/*
  another possible implementation.
*/
#elif defined(MY_ADDR_RESOLVE_FORK)
/*
  yet another - just execute addr2line or eu-addr2line, whatever available,
  pipe the addresses to it, and parse the output
*/
#endif
