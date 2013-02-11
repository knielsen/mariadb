/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file storage/perfschema/pfs_global.cc
  Miscellaneous global dependencies (implementation).
*/

#include "my_global.h"
#include "my_sys.h"
#include "pfs_global.h"
#include "my_net.h"

#include <stdlib.h>
#include <string.h>

#ifdef __WIN__
  #include <winsock2.h>
#else
  #include <arpa/inet.h>
#endif

bool pfs_initialized= false;
ulonglong pfs_allocated_memory= 0;

/**
  Memory allocation for the performance schema.
  The memory used internally in the performance schema implementation
  is allocated once during startup, and considered static thereafter.
*/
void *pfs_malloc(size_t size, myf flags)
{
  DBUG_ASSERT(! pfs_initialized);
  DBUG_ASSERT(size > 0);

  void *ptr= malloc(size);
  if (likely(ptr != NULL))
    pfs_allocated_memory+= size;
  if (likely((ptr != NULL) && (flags & MY_ZEROFILL)))
    memset(ptr, 0, size);
  return ptr;
}

void pfs_free(void *ptr)
{
  if (ptr != NULL)
    free(ptr);
}

void pfs_print_error(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  /*
    Printing to anything else, like the error log, would generate even more
    recursive calls to the performance schema implementation
    (file io is instrumented), so that could lead to catastrophic results.
    Printing to something safe, and low level: stderr only.
  */
  vfprintf(stderr, format, args);
  va_end(args);
  fflush(stderr);
}

/** Convert raw ip address into readable format. Do not do a reverse DNS lookup. */

uint pfs_get_socket_address(char *host,
                            uint host_len,
                            uint *port,
                            const struct sockaddr_storage *src_addr,
                            socklen_t src_len)
{
  DBUG_ASSERT(host);
  DBUG_ASSERT(src_addr);
  DBUG_ASSERT(port);

  memset(host, 0, host_len);
  *port= 0;

  switch (src_addr->ss_family)
  {
    case AF_INET:
    {
      if (host_len < INET_ADDRSTRLEN+1)
        return 0;
      struct sockaddr_in *sa4= (struct sockaddr_in *)(src_addr);
    #ifdef __WIN__
      /* Older versions of Windows do not support inet_ntop() */
      getnameinfo((struct sockaddr *)sa4, sizeof(struct sockaddr_in),
                  host, host_len, NULL, 0, NI_NUMERICHOST);
    #else
      inet_ntop(AF_INET, &(sa4->sin_addr), host, INET_ADDRSTRLEN);
    #endif
      *port= ntohs(sa4->sin_port);
    }
    break;

#ifdef HAVE_IPV6
    case AF_INET6:
    {
      if (host_len < INET6_ADDRSTRLEN+1)
        return 0;
      struct sockaddr_in6 *sa6= (struct sockaddr_in6 *)(src_addr);
    #ifdef __WIN__
      /* Older versions of Windows do not support inet_ntop() */
      getnameinfo((struct sockaddr *)sa6, sizeof(struct sockaddr_in6),
                  host, host_len, NULL, 0, NI_NUMERICHOST);
    #else
      inet_ntop(AF_INET6, &(sa6->sin6_addr), host, INET6_ADDRSTRLEN);
    #endif
      *port= ntohs(sa6->sin6_port);
    }
    break;
#endif

    default:
      break;
  }

  /* Return actual IP address string length */
  return (strlen((const char*)host));
}

