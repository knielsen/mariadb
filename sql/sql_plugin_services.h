/* Copyright (C) 2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* support for Services */
#include <service_versions.h>

struct st_service_ref {
  const char *name;
  uint version;
  void *service;
};

#ifdef HAVE_DLOPEN

static struct my_snprintf_service_st my_snprintf_handler = {
  my_snprintf,
  my_vsnprintf
};

static struct thd_alloc_service_st thd_alloc_handler= {
  thd_alloc,
  thd_calloc,
  thd_strdup,
  thd_strmake,
  thd_memdup,
  thd_make_lex_string
};

static struct progress_report_service_st progress_report_handler= {
  thd_progress_init,
  thd_progress_report,
  thd_progress_next_stage,
  thd_progress_end,
  set_thd_proc_info
};

static struct st_service_ref list_of_services[] __attribute__((unused)) =
{
  { "my_snprintf_service", VERSION_my_snprintf, &my_snprintf_handler },
  { "thd_alloc_service",   VERSION_thd_alloc,   &thd_alloc_handler },
  { "progress_report_service", VERSION_progress_report, &progress_report_handler }
};
#endif
