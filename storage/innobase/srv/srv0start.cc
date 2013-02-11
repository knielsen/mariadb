/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All rights reserved.
Copyright (c) 2008, Google Inc.
Copyright (c) 2009, Percona Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

Portions of this file contain modifications contributed and copyrighted
by Percona Inc.. Those modifications are
gratefully acknowledged and are described briefly in the InnoDB
documentation. The contributions by Percona Inc. are incorporated with
their permission, and subject to the conditions contained in the file
COPYING.Percona.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/********************************************************************//**
@file srv/srv0start.cc
Starts the InnoDB database server

Created 2/16/1996 Heikki Tuuri
*************************************************************************/

#include "ut0mem.h"
#include "mem0mem.h"
#include "data0data.h"
#include "data0type.h"
#include "dict0dict.h"
#include "buf0buf.h"
#include "buf0dump.h"
#include "os0file.h"
#include "os0thread.h"
#include "fil0fil.h"
#include "fsp0fsp.h"
#include "rem0rec.h"
#include "mtr0mtr.h"
#include "log0log.h"
#include "log0recv.h"
#include "page0page.h"
#include "page0cur.h"
#include "trx0trx.h"
#include "trx0sys.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "rem0rec.h"
#include "ibuf0ibuf.h"
#include "srv0start.h"
#include "srv0srv.h"
#ifndef UNIV_HOTBACKUP
# include "trx0rseg.h"
# include "os0proc.h"
# include "sync0sync.h"
# include "buf0flu.h"
# include "buf0rea.h"
# include "dict0boot.h"
# include "dict0load.h"
# include "que0que.h"
# include "usr0sess.h"
# include "lock0lock.h"
# include "trx0roll.h"
# include "trx0purge.h"
# include "lock0lock.h"
# include "pars0pars.h"
# include "btr0sea.h"
# include "rem0cmp.h"
# include "dict0crea.h"
# include "row0ins.h"
# include "row0sel.h"
# include "row0upd.h"
# include "row0row.h"
# include "row0mysql.h"
# include "btr0pcur.h"
# include "os0sync.h" /* for INNODB_RW_LOCKS_USE_ATOMICS */
# include "zlib.h" /* for ZLIB_VERSION */
# include "buf0dblwr.h"

/** Log sequence number immediately after startup */
UNIV_INTERN lsn_t	srv_start_lsn;
/** Log sequence number at shutdown */
UNIV_INTERN lsn_t	srv_shutdown_lsn;

#ifdef HAVE_DARWIN_THREADS
# include <sys/utsname.h>
/** TRUE if the F_FULLFSYNC option is available */
UNIV_INTERN ibool	srv_have_fullfsync = FALSE;
#endif

/** TRUE if a raw partition is in use */
UNIV_INTERN ibool	srv_start_raw_disk_in_use = FALSE;

/** TRUE if the server is being started, before rolling back any
incomplete transactions */
UNIV_INTERN ibool	srv_startup_is_before_trx_rollback_phase = FALSE;
/** TRUE if the server is being started */
UNIV_INTERN ibool	srv_is_being_started = FALSE;
/** TRUE if the server was successfully started */
UNIV_INTERN ibool	srv_was_started = FALSE;
/** TRUE if innobase_start_or_create_for_mysql() has been called */
static ibool		srv_start_has_been_called = FALSE;

/** At a shutdown this value climbs from SRV_SHUTDOWN_NONE to
SRV_SHUTDOWN_CLEANUP and then to SRV_SHUTDOWN_LAST_PHASE, and so on */
UNIV_INTERN enum srv_shutdown_state	srv_shutdown_state = SRV_SHUTDOWN_NONE;

/** Files comprising the system tablespace */
static os_file_t	files[1000];

/** io_handler_thread parameters for thread identification */
static ulint		n[SRV_MAX_N_IO_THREADS + 6];
/** io_handler_thread identifiers, 32 is the maximum number of purge threads  */
static os_thread_id_t	thread_ids[SRV_MAX_N_IO_THREADS + 6 + 32];

/** We use this mutex to test the return value of pthread_mutex_trylock
   on successful locking. HP-UX does NOT return 0, though Linux et al do. */
static os_fast_mutex_t	srv_os_test_mutex;

/** Name of srv_monitor_file */
static char*	srv_monitor_file_name;
#endif /* !UNIV_HOTBACKUP */

/** Default undo tablespace size in UNIV_PAGEs count (10MB). */
static const ulint SRV_UNDO_TABLESPACE_SIZE_IN_PAGES =
	((1024 * 1024) * 10) / UNIV_PAGE_SIZE_DEF;

/** */
#define SRV_N_PENDING_IOS_PER_THREAD	OS_AIO_N_PENDING_IOS_PER_THREAD
#define SRV_MAX_N_PENDING_SYNC_IOS	100

#ifdef UNIV_PFS_THREAD
/* Keys to register InnoDB threads with performance schema */
UNIV_INTERN mysql_pfs_key_t	io_handler_thread_key;
UNIV_INTERN mysql_pfs_key_t	srv_lock_timeout_thread_key;
UNIV_INTERN mysql_pfs_key_t	srv_error_monitor_thread_key;
UNIV_INTERN mysql_pfs_key_t	srv_monitor_thread_key;
UNIV_INTERN mysql_pfs_key_t	srv_master_thread_key;
UNIV_INTERN mysql_pfs_key_t	srv_purge_thread_key;
#endif /* UNIV_PFS_THREAD */

/*********************************************************************//**
Convert a numeric string that optionally ends in G or M, to a number
containing megabytes.
@return	next character in string */
static
char*
srv_parse_megabytes(
/*================*/
	char*	str,	/*!< in: string containing a quantity in bytes */
	ulint*	megs)	/*!< out: the number in megabytes */
{
	char*	endp;
	ulint	size;

	size = strtoul(str, &endp, 10);

	str = endp;

	switch (*str) {
	case 'G': case 'g':
		size *= 1024;
		/* fall through */
	case 'M': case 'm':
		str++;
		break;
	default:
		size /= 1024 * 1024;
		break;
	}

	*megs = size;
	return(str);
}

/*********************************************************************//**
Reads the data files and their sizes from a character string given in
the .cnf file.
@return	TRUE if ok, FALSE on parse error */
UNIV_INTERN
ibool
srv_parse_data_file_paths_and_sizes(
/*================================*/
	char*	str)	/*!< in/out: the data file path string */
{
	char*	input_str;
	char*	path;
	ulint	size;
	ulint	i	= 0;

	srv_auto_extend_last_data_file = FALSE;
	srv_last_file_size_max = 0;
	srv_data_file_names = NULL;
	srv_data_file_sizes = NULL;
	srv_data_file_is_raw_partition = NULL;

	input_str = str;

	/* First calculate the number of data files and check syntax:
	path:size[M | G];path:size[M | G]... . Note that a Windows path may
	contain a drive name and a ':'. */

	while (*str != '\0') {
		path = str;

		while ((*str != ':' && *str != '\0')
		       || (*str == ':'
			   && (*(str + 1) == '\\' || *(str + 1) == '/'
			       || *(str + 1) == ':'))) {
			str++;
		}

		if (*str == '\0') {
			return(FALSE);
		}

		str++;

		str = srv_parse_megabytes(str, &size);

		if (0 == strncmp(str, ":autoextend",
				 (sizeof ":autoextend") - 1)) {

			str += (sizeof ":autoextend") - 1;

			if (0 == strncmp(str, ":max:",
					 (sizeof ":max:") - 1)) {

				str += (sizeof ":max:") - 1;

				str = srv_parse_megabytes(str, &size);
			}

			if (*str != '\0') {

				return(FALSE);
			}
		}

		if (strlen(str) >= 6
		    && *str == 'n'
		    && *(str + 1) == 'e'
		    && *(str + 2) == 'w') {
			str += 3;
		}

		if (*str == 'r' && *(str + 1) == 'a' && *(str + 2) == 'w') {
			str += 3;
		}

		if (size == 0) {
			return(FALSE);
		}

		i++;

		if (*str == ';') {
			str++;
		} else if (*str != '\0') {

			return(FALSE);
		}
	}

	if (i == 0) {
		/* If innodb_data_file_path was defined it must contain
		at least one data file definition */

		return(FALSE);
	}

	srv_data_file_names = static_cast<char**>(
		malloc(i * sizeof *srv_data_file_names));

	srv_data_file_sizes = static_cast<ulint*>(
		malloc(i * sizeof *srv_data_file_sizes));

	srv_data_file_is_raw_partition = static_cast<ulint*>(
		malloc(i * sizeof *srv_data_file_is_raw_partition));

	srv_n_data_files = i;

	/* Then store the actual values to our arrays */

	str = input_str;
	i = 0;

	while (*str != '\0') {
		path = str;

		/* Note that we must step over the ':' in a Windows path;
		a Windows path normally looks like C:\ibdata\ibdata1:1G, but
		a Windows raw partition may have a specification like
		\\.\C::1Gnewraw or \\.\PHYSICALDRIVE2:1Gnewraw */

		while ((*str != ':' && *str != '\0')
		       || (*str == ':'
			   && (*(str + 1) == '\\' || *(str + 1) == '/'
			       || *(str + 1) == ':'))) {
			str++;
		}

		if (*str == ':') {
			/* Make path a null-terminated string */
			*str = '\0';
			str++;
		}

		str = srv_parse_megabytes(str, &size);

		srv_data_file_names[i] = path;
		srv_data_file_sizes[i] = size;

		if (0 == strncmp(str, ":autoextend",
				 (sizeof ":autoextend") - 1)) {

			srv_auto_extend_last_data_file = TRUE;

			str += (sizeof ":autoextend") - 1;

			if (0 == strncmp(str, ":max:",
					 (sizeof ":max:") - 1)) {

				str += (sizeof ":max:") - 1;

				str = srv_parse_megabytes(
					str, &srv_last_file_size_max);
			}

			if (*str != '\0') {

				return(FALSE);
			}
		}

		(srv_data_file_is_raw_partition)[i] = 0;

		if (strlen(str) >= 6
		    && *str == 'n'
		    && *(str + 1) == 'e'
		    && *(str + 2) == 'w') {
			str += 3;
			(srv_data_file_is_raw_partition)[i] = SRV_NEW_RAW;
		}

		if (*str == 'r' && *(str + 1) == 'a' && *(str + 2) == 'w') {
			str += 3;

			if ((srv_data_file_is_raw_partition)[i] == 0) {
				(srv_data_file_is_raw_partition)[i] = SRV_OLD_RAW;
			}
		}

		i++;

		if (*str == ';') {
			str++;
		}
	}

	return(TRUE);
}

/*********************************************************************//**
Reads log group home directories from a character string given in
the .cnf file.
@return	TRUE if ok, FALSE on parse error */
UNIV_INTERN
ibool
srv_parse_log_group_home_dirs(
/*==========================*/
	char*	str)	/*!< in/out: character string */
{
	char*	input_str;
	char*	path;
	ulint	i	= 0;

	srv_log_group_home_dirs = NULL;

	input_str = str;

	/* First calculate the number of directories and check syntax:
	path;path;... */

	while (*str != '\0') {
		path = str;

		while (*str != ';' && *str != '\0') {
			str++;
		}

		i++;

		if (*str == ';') {
			str++;
		} else if (*str != '\0') {

			return(FALSE);
		}
	}

	if (i != 1) {
		/* If innodb_log_group_home_dir was defined it must
		contain exactly one path definition under current MySQL */

		return(FALSE);
	}

	srv_log_group_home_dirs = static_cast<char**>(
		malloc(i * sizeof *srv_log_group_home_dirs));

	/* Then store the actual values to our array */

	str = input_str;
	i = 0;

	while (*str != '\0') {
		path = str;

		while (*str != ';' && *str != '\0') {
			str++;
		}

		if (*str == ';') {
			*str = '\0';
			str++;
		}

		srv_log_group_home_dirs[i] = path;

		i++;
	}

	return(TRUE);
}

/*********************************************************************//**
Frees the memory allocated by srv_parse_data_file_paths_and_sizes()
and srv_parse_log_group_home_dirs(). */
UNIV_INTERN
void
srv_free_paths_and_sizes(void)
/*==========================*/
{
	free(srv_data_file_names);
	srv_data_file_names = NULL;
	free(srv_data_file_sizes);
	srv_data_file_sizes = NULL;
	free(srv_data_file_is_raw_partition);
	srv_data_file_is_raw_partition = NULL;
	free(srv_log_group_home_dirs);
	srv_log_group_home_dirs = NULL;
}

#ifndef UNIV_HOTBACKUP
/********************************************************************//**
I/o-handler thread function.
@return	OS_THREAD_DUMMY_RETURN */
extern "C" UNIV_INTERN
os_thread_ret_t
DECLARE_THREAD(io_handler_thread)(
/*==============================*/
	void*	arg)	/*!< in: pointer to the number of the segment in
			the aio array */
{
	ulint	segment;

	segment = *((ulint*) arg);

#ifdef UNIV_DEBUG_THREAD_CREATION
	fprintf(stderr, "Io handler thread %lu starts, id %lu\n", segment,
		os_thread_pf(os_thread_get_curr_id()));
#endif

#ifdef UNIV_PFS_THREAD
	pfs_register_thread(io_handler_thread_key);
#endif /* UNIV_PFS_THREAD */

	while (srv_shutdown_state != SRV_SHUTDOWN_EXIT_THREADS) {
		fil_aio_wait(segment);
	}

	/* We count the number of threads in os_thread_exit(). A created
	thread should always use that to exit and not use return() to exit.
	The thread actually never comes here because it is exited in an
	os_event_wait(). */

	os_thread_exit(NULL);

	OS_THREAD_DUMMY_RETURN;
}
#endif /* !UNIV_HOTBACKUP */

/*********************************************************************//**
Normalizes a directory path for Windows: converts slashes to backslashes. */
UNIV_INTERN
void
srv_normalize_path_for_win(
/*=======================*/
	char*	str __attribute__((unused)))	/*!< in/out: null-terminated
						character string */
{
#ifdef __WIN__
	for (; *str; str++) {

		if (*str == '/') {
			*str = '\\';
		}
	}
#endif
}

#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
Creates or opens the log files and closes them.
@return	DB_SUCCESS or error code */
static
ulint
open_or_create_log_file(
/*====================*/
	ibool	create_new_db,		/*!< in: TRUE if we should create a
					new database */
	ibool*	log_file_created,	/*!< out: TRUE if new log file
					created */
	ibool	log_file_has_been_opened,/*!< in: TRUE if a log file has been
					opened before: then it is an error
					to try to create another log file */
	ulint	k,			/*!< in: log group number */
	ulint	i)			/*!< in: log file number in group */
{
	ibool		ret;
	os_offset_t	size;
	char		name[10000];
	ulint		dirnamelen;

	UT_NOT_USED(create_new_db);

	*log_file_created = FALSE;

	srv_normalize_path_for_win(srv_log_group_home_dirs[k]);

	dirnamelen = strlen(srv_log_group_home_dirs[k]);
	ut_a(dirnamelen < (sizeof name) - 10 - sizeof "ib_logfile");
	memcpy(name, srv_log_group_home_dirs[k], dirnamelen);

	/* Add a path separator if needed. */
	if (dirnamelen && name[dirnamelen - 1] != SRV_PATH_SEPARATOR) {
		name[dirnamelen++] = SRV_PATH_SEPARATOR;
	}

	sprintf(name + dirnamelen, "%s%lu", "ib_logfile", (ulong) i);

	files[i] = os_file_create(innodb_file_log_key, name,
				  OS_FILE_CREATE, OS_FILE_NORMAL,
				  OS_LOG_FILE, &ret);
	if (ret == FALSE) {
		if (os_file_get_last_error(FALSE) != OS_FILE_ALREADY_EXISTS
#ifdef UNIV_AIX
		    /* AIX 5.1 after security patch ML7 may have errno set
		    to 0 here, which causes our function to return 100;
		    work around that AIX problem */
		    && os_file_get_last_error(FALSE) != 100
#endif
		    ) {
			fprintf(stderr,
				"InnoDB: Error in creating"
				" or opening %s\n", name);

			return(DB_ERROR);
		}

		files[i] = os_file_create(innodb_file_log_key, name,
					  OS_FILE_OPEN, OS_FILE_AIO,
					  OS_LOG_FILE, &ret);
		if (!ret) {
			fprintf(stderr,
				"InnoDB: Error in opening %s\n", name);

			return(DB_ERROR);
		}

		size = os_file_get_size(files[i]);
		ut_a(size != (os_offset_t) -1);

		if (UNIV_UNLIKELY(size != (os_offset_t) srv_log_file_size
				  << UNIV_PAGE_SIZE_SHIFT)) {

			fprintf(stderr,
				"InnoDB: Error: log file %s is"
				" of different size "UINT64PF" bytes\n"
				"InnoDB: than specified in the .cnf"
				" file "UINT64PF" bytes!\n",
				name, size,
				(os_offset_t) srv_log_file_size
				<< UNIV_PAGE_SIZE_SHIFT);

			return(DB_ERROR);
		}
	} else {
		*log_file_created = TRUE;

		ut_print_timestamp(stderr);

		fprintf(stderr,
			" InnoDB: Log file %s did not exist:"
			" new to be created\n",
			name);
		if (log_file_has_been_opened) {

			return(DB_ERROR);
		}

		fprintf(stderr, "InnoDB: Setting log file %s size to %lu MB\n",
			name, (ulong) srv_log_file_size
			>> (20 - UNIV_PAGE_SIZE_SHIFT));

		fprintf(stderr,
			"InnoDB: Database physically writes the file"
			" full: wait...\n");

		ret = os_file_set_size(name, files[i],
				       (os_offset_t) srv_log_file_size
				       << UNIV_PAGE_SIZE_SHIFT);
		if (!ret) {
			fprintf(stderr,
				"InnoDB: Error in creating %s:"
				" probably out of disk space\n",
				name);

			return(DB_ERROR);
		}
	}

	ret = os_file_close(files[i]);
	ut_a(ret);

	if (i == 0) {
		/* Create in memory the file space object
		which is for this log group */

		fil_space_create(name,
				 2 * k + SRV_LOG_SPACE_FIRST_ID,
				 fsp_flags_set_page_size(0, UNIV_PAGE_SIZE),
				 FIL_LOG);
	}

	ut_a(fil_validate());

	/* srv_log_file_size is measured in pages; if page size is 16KB,
	then we have a limit of 64TB on 32 bit systems */
	ut_a(srv_log_file_size <= ULINT_MAX);

	fil_node_create(name, (ulint) srv_log_file_size,
			2 * k + SRV_LOG_SPACE_FIRST_ID, FALSE);
#ifdef UNIV_LOG_ARCHIVE
	/* If this is the first log group, create the file space object
	for archived logs.
	Under MySQL, no archiving ever done. */

	if (k == 0 && i == 0) {
		arch_space_id = 2 * k + 1 + SRV_LOG_SPACE_FIRST_ID;

		fil_space_create("arch_log_space", arch_space_id, 0, FIL_LOG);
	} else {
		arch_space_id = ULINT_UNDEFINED;
	}
#endif /* UNIV_LOG_ARCHIVE */
	if (i == 0) {
		log_group_init(k, srv_n_log_files,
			       srv_log_file_size * UNIV_PAGE_SIZE,
			       2 * k + SRV_LOG_SPACE_FIRST_ID,
			       SRV_LOG_SPACE_FIRST_ID + 1); /* dummy arch
							    space id */
	}

	return(DB_SUCCESS);
}

/*********************************************************************//**
Creates or opens database data files and closes them.
@return	DB_SUCCESS or error code */
static
ulint
open_or_create_data_files(
/*======================*/
	ibool*		create_new_db,	/*!< out: TRUE if new database should be
					created */
#ifdef UNIV_LOG_ARCHIVE
	ulint*		min_arch_log_no,/*!< out: min of archived log
					numbers in data files */
	ulint*		max_arch_log_no,/*!< out: max of archived log
					numbers in data files */
#endif /* UNIV_LOG_ARCHIVE */
	lsn_t*		min_flushed_lsn,/*!< out: min of flushed lsn
					values in data files */
	lsn_t*		max_flushed_lsn,/*!< out: max of flushed lsn
					values in data files */
	ulint*		sum_of_new_sizes)/*!< out: sum of sizes of the
					new files added */
{
	ibool		ret;
	ulint		i;
	ibool		one_opened	= FALSE;
	ibool		one_created	= FALSE;
	os_offset_t	size;
	ulint		flags;
	ulint		rounded_size_pages;
	char		name[10000];

	if (srv_n_data_files >= 1000) {
		fprintf(stderr, "InnoDB: can only have < 1000 data files\n"
			"InnoDB: you have defined %lu\n",
			(ulong) srv_n_data_files);
		return(DB_ERROR);
	}

	*sum_of_new_sizes = 0;

	*create_new_db = FALSE;

	srv_normalize_path_for_win(srv_data_home);

	for (i = 0; i < srv_n_data_files; i++) {
		ulint	dirnamelen;

		srv_normalize_path_for_win(srv_data_file_names[i]);
		dirnamelen = strlen(srv_data_home);

		ut_a(dirnamelen + strlen(srv_data_file_names[i])
		     < (sizeof name) - 1);
		memcpy(name, srv_data_home, dirnamelen);
		/* Add a path separator if needed. */
		if (dirnamelen && name[dirnamelen - 1] != SRV_PATH_SEPARATOR) {
			name[dirnamelen++] = SRV_PATH_SEPARATOR;
		}

		strcpy(name + dirnamelen, srv_data_file_names[i]);

		if (srv_data_file_is_raw_partition[i] == 0) {

			/* First we try to create the file: if it already
			exists, ret will get value FALSE */

			files[i] = os_file_create(innodb_file_data_key,
						  name, OS_FILE_CREATE,
						  OS_FILE_NORMAL,
						  OS_DATA_FILE, &ret);

			if (ret == FALSE && os_file_get_last_error(FALSE)
			    != OS_FILE_ALREADY_EXISTS
#ifdef UNIV_AIX
			    /* AIX 5.1 after security patch ML7 may have
			    errno set to 0 here, which causes our function
			    to return 100; work around that AIX problem */
			    && os_file_get_last_error(FALSE) != 100
#endif
			    ) {
				fprintf(stderr,
					"InnoDB: Error in creating"
					" or opening %s\n",
					name);

				return(DB_ERROR);
			}
		} else if (srv_data_file_is_raw_partition[i] == SRV_NEW_RAW) {
			/* The partition is opened, not created; then it is
			written over */

			srv_start_raw_disk_in_use = TRUE;
			srv_created_new_raw = TRUE;

			files[i] = os_file_create(innodb_file_data_key,
						  name, OS_FILE_OPEN_RAW,
						  OS_FILE_NORMAL,
						  OS_DATA_FILE, &ret);
			if (!ret) {
				fprintf(stderr,
					"InnoDB: Error in opening %s\n", name);

				return(DB_ERROR);
			}
		} else if (srv_data_file_is_raw_partition[i] == SRV_OLD_RAW) {
			srv_start_raw_disk_in_use = TRUE;

			ret = FALSE;
		} else {
			ut_a(0);
		}

		if (ret == FALSE) {
			/* We open the data file */

			if (one_created) {
				fprintf(stderr,
					"InnoDB: Error: data files can only"
					" be added at the end\n");
				fprintf(stderr,
					"InnoDB: of a tablespace, but"
					" data file %s existed beforehand.\n",
					name);
				return(DB_ERROR);
			}

			if (srv_data_file_is_raw_partition[i] == SRV_OLD_RAW) {
				files[i] = os_file_create(
					innodb_file_data_key,
					name, OS_FILE_OPEN_RAW,
					OS_FILE_NORMAL, OS_DATA_FILE, &ret);
			} else if (i == 0) {
				files[i] = os_file_create(
					innodb_file_data_key,
					name, OS_FILE_OPEN_RETRY,
					OS_FILE_NORMAL, OS_DATA_FILE, &ret);
			} else {
				files[i] = os_file_create(
					innodb_file_data_key,
					name, OS_FILE_OPEN, OS_FILE_NORMAL,
					OS_DATA_FILE, &ret);
			}

			if (!ret) {
				fprintf(stderr,
					"InnoDB: Error in opening %s\n", name);
				os_file_get_last_error(TRUE);

				return(DB_ERROR);
			}

			if (srv_data_file_is_raw_partition[i] == SRV_OLD_RAW) {

				goto skip_size_check;
			}

			size = os_file_get_size(files[i]);
			ut_a(size != (os_offset_t) -1);
			/* Round size downward to megabytes */

			rounded_size_pages = (ulint)
				(size >> UNIV_PAGE_SIZE_SHIFT);

			if (i == srv_n_data_files - 1
			    && srv_auto_extend_last_data_file) {

				if (srv_data_file_sizes[i] > rounded_size_pages
				    || (srv_last_file_size_max > 0
					&& srv_last_file_size_max
					< rounded_size_pages)) {

					fprintf(stderr,
						"InnoDB: Error: auto-extending"
						" data file %s is"
						" of a different size\n"
						"InnoDB: %lu pages (rounded"
						" down to MB) than specified"
						" in the .cnf file:\n"
						"InnoDB: initial %lu pages,"
						" max %lu (relevant if"
						" non-zero) pages!\n",
						name,
						(ulong) rounded_size_pages,
						(ulong) srv_data_file_sizes[i],
						(ulong)
						srv_last_file_size_max);

					return(DB_ERROR);
				}

				srv_data_file_sizes[i] = rounded_size_pages;
			}

			if (rounded_size_pages != srv_data_file_sizes[i]) {

				fprintf(stderr,
					"InnoDB: Error: data file %s"
					" is of a different size\n"
					"InnoDB: %lu pages"
					" (rounded down to MB)\n"
					"InnoDB: than specified"
					" in the .cnf file %lu pages!\n",
					name,
					(ulong) rounded_size_pages,
					(ulong) srv_data_file_sizes[i]);

				return(DB_ERROR);
			}
skip_size_check:
			fil_read_first_page(
				files[i], one_opened, &flags,
#ifdef UNIV_LOG_ARCHIVE
				min_arch_log_no, max_arch_log_no,
#endif /* UNIV_LOG_ARCHIVE */
				min_flushed_lsn, max_flushed_lsn);

			if (!one_opened
			    && UNIV_PAGE_SIZE
			       != fsp_flags_get_page_size(flags)) {

				ut_print_timestamp(stderr);
				fprintf(stderr,
					" InnoDB: Error: data file %s"
					" uses page size %lu,\n",
					name,
					fsp_flags_get_page_size(flags));
				ut_print_timestamp(stderr);
				fprintf(stderr,
					" InnoDB: but the start-up parameter"
					" is innodb-page-size=%lu\n",
					UNIV_PAGE_SIZE);

				return(DB_ERROR);
			}

			one_opened = TRUE;
		} else {
			/* We created the data file and now write it full of
			zeros */

			one_created = TRUE;

			if (i > 0) {
				ut_print_timestamp(stderr);
				fprintf(stderr,
					" InnoDB: Data file %s did not"
					" exist: new to be created\n",
					name);
			} else {
				fprintf(stderr,
					"InnoDB: The first specified"
					" data file %s did not exist:\n"
					"InnoDB: a new database"
					" to be created!\n", name);
				*create_new_db = TRUE;
			}

			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: Setting file %s size to %lu MB\n",
				name,
				(ulong) (srv_data_file_sizes[i]
					 >> (20 - UNIV_PAGE_SIZE_SHIFT)));

			fprintf(stderr,
				"InnoDB: Database physically writes the"
				" file full: wait...\n");

			ret = os_file_set_size(
				name, files[i],
				(os_offset_t) srv_data_file_sizes[i]
				<< UNIV_PAGE_SIZE_SHIFT);

			if (!ret) {
				fprintf(stderr,
					"InnoDB: Error in creating %s:"
					" probably out of disk space\n", name);

				return(DB_ERROR);
			}

			*sum_of_new_sizes += srv_data_file_sizes[i];
		}

		ret = os_file_close(files[i]);
		ut_a(ret);

		if (i == 0) {
			flags = fsp_flags_set_page_size(0, UNIV_PAGE_SIZE);
			fil_space_create(name, 0, flags, FIL_TABLESPACE);
		}

		ut_a(fil_validate());

		fil_node_create(name, srv_data_file_sizes[i], 0,
				srv_data_file_is_raw_partition[i] != 0);
	}

	return(DB_SUCCESS);
}

/*********************************************************************//**
Create undo tablespace.
@return	DB_SUCCESS or error code */
static
enum db_err
srv_undo_tablespace_create(
/*=======================*/
	const char*	name,		/*!< in: tablespace name */
	ulint		size)		/*!< in: tablespace size in pages */
{
	os_file_t	fh;
	ibool		ret;
	enum db_err	err = DB_SUCCESS;

	os_file_create_subdirs_if_needed(name);

	fh = os_file_create(
		innodb_file_data_key, name, OS_FILE_CREATE,
		OS_FILE_NORMAL, OS_DATA_FILE, &ret);

	if (ret == FALSE
	    && os_file_get_last_error(FALSE) != OS_FILE_ALREADY_EXISTS
#ifdef UNIV_AIX
	    /* AIX 5.1 after security patch ML7 may have
	    errno set to 0 here, which causes our function
	    to return 100; work around that AIX problem */
	    && os_file_get_last_error(FALSE) != 100
#endif
		) {

		fprintf(stderr, "InnoDB: Error in creating %s\n", name);

		err = DB_ERROR;
	} else {
		/* We created the data file and now write it full of zeros */

		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: Data file %s did not"
				" exist: new to be created\n", name);

		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: Setting file %s size to %lu MB\n",
				name, size >> (20 - UNIV_PAGE_SIZE_SHIFT));

		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: Database physically writes the"
				" file full: wait...\n");

		ret = os_file_set_size(name, fh, size << UNIV_PAGE_SIZE_SHIFT);

		if (!ret) {
			ut_print_timestamp(stderr);
			fprintf(stderr, " InnoDB: Error in creating %s:"
					" probably out of disk space\n", name);

			err = DB_ERROR;
		}

		os_file_close(fh);
	}

	return(err);
}

/*********************************************************************//**
Open an undo tablespace.
@return	DB_SUCCESS or error code */
static
enum db_err
srv_undo_tablespace_open(
/*=====================*/
	const char*	name,		/*!< in: tablespace name */
	ulint		space)		/*!< in: tablespace id */
{
	os_file_t	fh;
	enum db_err	err;
	ibool		ret;
	ulint		flags;

	fh = os_file_create(
		innodb_file_data_key, name,
		OS_FILE_OPEN_RETRY
		| OS_FILE_ON_ERROR_NO_EXIT
		| OS_FILE_ON_ERROR_SILENT,
		OS_FILE_NORMAL,
		OS_DATA_FILE,
		&ret);

	/* If the file open was successful then load the tablespace. */

	if (ret) {
		os_offset_t	size;
		os_offset_t	n_pages;

		size = os_file_get_size(fh);
		ut_a(size != (os_offset_t) -1);

		ret = os_file_close(fh);
		ut_a(ret);

		/* Load the tablespace into InnoDB's internal
		data structures. */

		/* We set the biggest space id to the undo tablespace
		because InnoDB hasn't opened any other tablespace apart
		from the system tablespace. */

		fil_set_max_space_id_if_bigger(space);

		/* Set the compressed page size to 0 (non-compressed) */
		flags = fsp_flags_set_page_size(0, UNIV_PAGE_SIZE);
		fil_space_create(name, space, flags, FIL_TABLESPACE);

		ut_a(fil_validate());

		n_pages = size / UNIV_PAGE_SIZE;

		/* On 64 bit Windows ulint can be 32 bit and os_offset_t
		is 64 bit. It is OK to cast the n_pages to ulint because
		the unit has been scaled to pages and they are always
		32 bit. */
		fil_node_create(name, (ulint) n_pages, space, FALSE);

		err = DB_SUCCESS;
	} else {
		err = DB_ERROR;
	}

	return(err);
}

/********************************************************************
Opens the configured number of undo tablespaces.
@return	DB_SUCCESS or error code */
static
enum db_err
srv_undo_tablespaces_init(
/*======================*/
	ibool		create_new_db,		/*!< in: TRUE if new db being
						created */
	const ulint	n_conf_tablespaces)	/*!< in: configured undo
						tablespaces */
{
	ulint		i;
	enum db_err	err = DB_SUCCESS;
	ulint		prev_space_id = 0;
	ulint		n_undo_tablespaces;
	ulint		undo_tablespace_ids[TRX_SYS_N_RSEGS + 1];

	ut_a(n_conf_tablespaces <= TRX_SYS_N_RSEGS);

	memset(undo_tablespace_ids, 0x0, sizeof(undo_tablespace_ids));

	/* Create the undo spaces only if we are creating a new
	instance. We don't allow creating of new undo tablespaces
	in an existing instance (yet).  This restriction exists because
	we check in several places for SYSTEM tablespaces to be less than
	the min of user defined tablespace ids. Once we implement saving
	the location of the undo tablespaces and their space ids this
	restriction will/should be lifted. */

	for (i = 0; create_new_db && i < n_conf_tablespaces; ++i) {
		char	name[OS_FILE_MAX_PATH];

		ut_snprintf(
			name, sizeof(name),
			"%s%cundo%03lu",
			srv_undo_dir, SRV_PATH_SEPARATOR, i + 1);

		/* Undo space ids start from 1. */
		err = srv_undo_tablespace_create(
			name, SRV_UNDO_TABLESPACE_SIZE_IN_PAGES);

		if (err != DB_SUCCESS) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: Could not create "
				"undo tablespace '%s'.\n", name);

			return(err);
		}
	}

	/* Get the tablespace ids of all the undo segments excluding
	the system tablespace (0). If we are creating a new instance then
	we build the undo_tablespace_ids ourselves since they don't
	already exist. */

	if (!create_new_db) {
		n_undo_tablespaces = trx_rseg_get_n_undo_tablespaces(
			undo_tablespace_ids);
	} else {
		n_undo_tablespaces = n_conf_tablespaces;

		for (i = 1; i <= n_undo_tablespaces; ++i) {
			undo_tablespace_ids[i - 1] = i;
		}

		undo_tablespace_ids[i] = ULINT_UNDEFINED;
	}

	/* Open all the undo tablespaces that are currently in use. If we
	fail to open any of these it is a fatal error. The tablespace ids
	should be contiguous. It is a fatal error because they are required
	for recovery and are referenced by the UNDO logs (a.k.a RBS). */

	for (i = 0; i < n_undo_tablespaces; ++i) {
		char	name[OS_FILE_MAX_PATH];

		ut_snprintf(
			name, sizeof(name),
			"%s%cundo%03lu",
			srv_undo_dir, SRV_PATH_SEPARATOR,
			undo_tablespace_ids[i]);

		/* Should be no gaps in undo tablespace ids. */
		ut_a(prev_space_id + 1 == undo_tablespace_ids[i]);

		/* The system space id should not be in this array. */
		ut_a(undo_tablespace_ids[i] != 0);
		ut_a(undo_tablespace_ids[i] != ULINT_UNDEFINED);

		/* Undo space ids start from 1. */

		err = srv_undo_tablespace_open(name, undo_tablespace_ids[i]);

		if (err != DB_SUCCESS) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: Error opening undo "
				"tablespace %s.\n", name);

			return(err);
		}

		prev_space_id = undo_tablespace_ids[i];
	}

	/* Open any extra unused undo tablespaces. These must be contiguous.
	We stop at the first failure. These are undo tablespaces that are
	not in use and therefore not required by recovery. We only check
	that there are no gaps. */

	for (i = prev_space_id + 1; i < TRX_SYS_N_RSEGS; ++i) {
		char	name[OS_FILE_MAX_PATH];

		ut_snprintf(
			name, sizeof(name),
			"%s%cundo%03lu", srv_undo_dir, SRV_PATH_SEPARATOR, i);

		/* Undo space ids start from 1. */
		err = srv_undo_tablespace_open(name, i);

		if (err != DB_SUCCESS) {
			break;
		}

		++n_undo_tablespaces;
	}

	/* If the user says that there are fewer than what we find we
	tolerate that discrepancy but not the inverse. Because there could
	be unused undo tablespaces for future use. */

	if (n_conf_tablespaces > n_undo_tablespaces) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Expected to open %lu undo "
			"tablespaces but was able\n",
			n_conf_tablespaces);
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: to find only %lu undo "
			"tablespaces.\n", n_undo_tablespaces);
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Set the "
			"innodb_undo_tablespaces parameter to "
			"the\n");
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: correct value and retry. Suggested "
			"value is %lu\n", n_undo_tablespaces);

		return(err != DB_SUCCESS ? err : DB_ERROR);
	}

	if (n_undo_tablespaces > 0) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Opened %lu undo tablespaces\n",
			n_conf_tablespaces);
	}

	if (create_new_db) {
		mtr_t	mtr;

		mtr_start(&mtr);

		/* The undo log tablespace */
		for (i = 1; i <= n_undo_tablespaces; ++i) {

			fsp_header_init(
				i, SRV_UNDO_TABLESPACE_SIZE_IN_PAGES, &mtr);
		}

		mtr_commit(&mtr);
	}

	return(DB_SUCCESS);
}

/********************************************************************
Starts InnoDB and creates a new database if database files
are not found and the user wants.
@return	DB_SUCCESS or error code */
UNIV_INTERN
int
innobase_start_or_create_for_mysql(void)
/*====================================*/
{
	ibool		create_new_db;
	ibool		log_file_created;
	ibool		log_created	= FALSE;
	ibool		log_opened	= FALSE;
	lsn_t		min_flushed_lsn;
	lsn_t		max_flushed_lsn;
#ifdef UNIV_LOG_ARCHIVE
	ulint		min_arch_log_no;
	ulint		max_arch_log_no;
#endif /* UNIV_LOG_ARCHIVE */
	ulint		sum_of_new_sizes;
	ulint		sum_of_data_file_sizes;
	ulint		tablespace_size_in_header;
	ulint		err;
	ulint		i;
	ulint		io_limit;
	mtr_t		mtr;
	ib_bh_t*	ib_bh;

#ifdef HAVE_DARWIN_THREADS
# ifdef F_FULLFSYNC
	/* This executable has been compiled on Mac OS X 10.3 or later.
	Assume that F_FULLFSYNC is available at run-time. */
	srv_have_fullfsync = TRUE;
# else /* F_FULLFSYNC */
	/* This executable has been compiled on Mac OS X 10.2
	or earlier.  Determine if the executable is running
	on Mac OS X 10.3 or later. */
	struct utsname utsname;
	if (uname(&utsname)) {
		ut_print_timestamp(stderr);
		fputs(" InnoDB: cannot determine Mac OS X version!\n", stderr);
	} else {
		srv_have_fullfsync = strcmp(utsname.release, "7.") >= 0;
	}
	if (!srv_have_fullfsync) {
		ut_print_timestamp(stderr);
		fputs(" InnoDB: On Mac OS X, fsync() may be "
		      "broken on internal drives,\n", stderr);
		ut_print_timestamp(stderr);
		fputs(" InnoDB: making transactions unsafe!\n", stderr);
	}
# endif /* F_FULLFSYNC */
#endif /* HAVE_DARWIN_THREADS */

	if (sizeof(ulint) != sizeof(void*)) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error: size of InnoDB's ulint is %lu, "
			"but size of void*\n", (ulong) sizeof(ulint));
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: is %lu. The sizes should be the same "
			"so that on a 64-bit\n",
			(ulong) sizeof(void*));
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: platforms you can allocate more than 4 GB "
			"of memory.\n");
	}

#ifdef UNIV_DEBUG
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: !!!!!!!! UNIV_DEBUG switched on !!!!!!!!!\n");
#endif

#ifdef UNIV_IBUF_DEBUG
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: !!!!!!!! UNIV_IBUF_DEBUG switched on !!!!!!!!!\n");
# ifdef UNIV_IBUF_COUNT_DEBUG
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: !!!!!!!! UNIV_IBUF_COUNT_DEBUG switched on "
		"!!!!!!!!!\n");
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: Crash recovery will fail with UNIV_IBUF_COUNT_DEBUG\n");
# endif
#endif

#ifdef UNIV_BLOB_DEBUG
	fprintf(stderr,
		"InnoDB: !!!!!!!! UNIV_BLOB_DEBUG switched on !!!!!!!!!\n"
		"InnoDB: Server restart may fail with UNIV_BLOB_DEBUG\n");
#endif /* UNIV_BLOB_DEBUG */

#ifdef UNIV_SYNC_DEBUG
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: !!!!!!!! UNIV_SYNC_DEBUG switched on !!!!!!!!!\n");
#endif

#ifdef UNIV_SEARCH_DEBUG
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: !!!!!!!! UNIV_SEARCH_DEBUG switched on !!!!!!!!!\n");
#endif

#ifdef UNIV_LOG_LSN_DEBUG
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: !!!!!!!! UNIV_LOG_LSN_DEBUG switched on !!!!!!!!!\n");
#endif /* UNIV_LOG_LSN_DEBUG */
#ifdef UNIV_MEM_DEBUG
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: !!!!!!!! UNIV_MEM_DEBUG switched on !!!!!!!!!\n");
#endif

	if (UNIV_LIKELY(srv_use_sys_malloc)) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: The InnoDB memory heap is disabled\n");
	}

#if defined(COMPILER_HINTS_ENABLED)
	ut_print_timestamp(stderr);
	fprintf(stderr, " InnoDB: Compiler hints enabled.\n");
#endif /* defined(COMPILER_HINTS_ENABLED) */

	ut_print_timestamp(stderr);
	fputs(" InnoDB: " IB_ATOMICS_STARTUP_MSG "\n", stderr);

	ut_print_timestamp(stderr);
	fputs(" InnoDB: Compressed tables use zlib " ZLIB_VERSION
#ifdef UNIV_ZIP_DEBUG
	      " with validation"
#endif /* UNIV_ZIP_DEBUG */
	      "\n" , stderr);
#ifdef UNIV_ZIP_COPY
	ut_print_timestamp(stderr);
	fputs(" InnoDB: and extra copying\n", stderr);
#endif /* UNIV_ZIP_COPY */

	/* Since InnoDB does not currently clean up all its internal data
	structures in MySQL Embedded Server Library server_end(), we
	print an error message if someone tries to start up InnoDB a
	second time during the process lifetime. */

	if (srv_start_has_been_called) {
		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: Error: startup called second time "
			"during the process\n");
		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: lifetime. In the MySQL Embedded "
			"Server Library you\n");
		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: cannot call server_init() more "
			"than once during the\n");
		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: process lifetime.\n");
	}

	srv_start_has_been_called = TRUE;

#ifdef UNIV_DEBUG
	log_do_write = TRUE;
#endif /* UNIV_DEBUG */
	/*	yydebug = TRUE; */

	srv_is_being_started = TRUE;
	srv_startup_is_before_trx_rollback_phase = TRUE;

#ifdef __WIN__
	switch (os_get_os_version()) {
	case OS_WIN95:
	case OS_WIN31:
	case OS_WINNT:
		/* On Win 95, 98, ME, Win32 subsystem for Windows 3.1,
		and NT use simulated aio. In NT Windows provides async i/o,
		but when run in conjunction with InnoDB Hot Backup, it seemed
		to corrupt the data files. */

		srv_use_native_aio = FALSE;
		break;

	case OS_WIN2000:
	case OS_WINXP:
		/* On 2000 and XP, async IO is available. */
		srv_use_native_aio = TRUE;
		break;

	default:
		/* Vista and later have both async IO and condition variables */
		srv_use_native_aio = TRUE;
		srv_use_native_conditions = TRUE;
		break;
	}

#elif defined(LINUX_NATIVE_AIO)

	if (srv_use_native_aio) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Using Linux native AIO\n");
	}
#else
	/* Currently native AIO is supported only on windows and linux
	and that also when the support is compiled in. In all other
	cases, we ignore the setting of innodb_use_native_aio. */
	srv_use_native_aio = FALSE;

#endif

	if (srv_file_flush_method_str == NULL) {
		/* These are the default options */

		srv_unix_file_flush_method = SRV_UNIX_FSYNC;

		srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;
#ifndef __WIN__
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "fsync")) {
		srv_unix_file_flush_method = SRV_UNIX_FSYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "O_DSYNC")) {
		srv_unix_file_flush_method = SRV_UNIX_O_DSYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "O_DIRECT")) {
		srv_unix_file_flush_method = SRV_UNIX_O_DIRECT;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "littlesync")) {
		srv_unix_file_flush_method = SRV_UNIX_LITTLESYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "nosync")) {
		srv_unix_file_flush_method = SRV_UNIX_NOSYNC;
#else
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "normal")) {
		srv_win_file_flush_method = SRV_WIN_IO_NORMAL;
		srv_use_native_aio = FALSE;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "unbuffered")) {
		srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;
		srv_use_native_aio = FALSE;

	} else if (0 == ut_strcmp(srv_file_flush_method_str,
				  "async_unbuffered")) {
		srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;
#endif
	} else {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Unrecognized value %s for"
			" innodb_flush_method\n",
			srv_file_flush_method_str);
		return(DB_ERROR);
	}

	/* Note that the call srv_boot() also changes the values of
	some variables to the units used by InnoDB internally */

	/* Set the maximum number of threads which can wait for a semaphore
	inside InnoDB: this is the 'sync wait array' size, as well as the
	maximum number of threads that can wait in the 'srv_conc array' for
	their time to enter InnoDB. */

	if (srv_buf_pool_size >= 1000 * 1024 * 1024) {
		/* If buffer pool is less than 1000 MB,
		assume fewer threads. Also use only one
		buffer pool instance */
		srv_max_n_threads = 50000;

	} else if (srv_buf_pool_size >= 8 * 1024 * 1024) {

		srv_buf_pool_instances = 1;
		srv_max_n_threads = 10000;
	} else {
		srv_buf_pool_instances = 1;
		srv_max_n_threads = 1000;	/* saves several MB of memory,
						especially in 64-bit
						computers */
	}

	err = srv_boot();

	if (err != DB_SUCCESS) {

		return((int) err);
	}

	mutex_create(srv_monitor_file_mutex_key,
		     &srv_monitor_file_mutex, SYNC_NO_ORDER_CHECK);

	if (srv_innodb_status) {

		srv_monitor_file_name = static_cast<char*>(
			mem_alloc(
				strlen(fil_path_to_mysql_datadir)
				+ 20 + sizeof "/innodb_status."));

		sprintf(srv_monitor_file_name, "%s/innodb_status.%lu",
			fil_path_to_mysql_datadir, os_proc_get_number());
		srv_monitor_file = fopen(srv_monitor_file_name, "w+");
		if (!srv_monitor_file) {
			fprintf(stderr, "InnoDB: unable to create %s: %s\n",
				srv_monitor_file_name, strerror(errno));
			return(DB_ERROR);
		}
	} else {
		srv_monitor_file_name = NULL;
		srv_monitor_file = os_file_create_tmpfile();
		if (!srv_monitor_file) {
			return(DB_ERROR);
		}
	}

	mutex_create(srv_dict_tmpfile_mutex_key,
		     &srv_dict_tmpfile_mutex, SYNC_DICT_OPERATION);

	srv_dict_tmpfile = os_file_create_tmpfile();
	if (!srv_dict_tmpfile) {
		return(DB_ERROR);
	}

	mutex_create(srv_misc_tmpfile_mutex_key,
		     &srv_misc_tmpfile_mutex, SYNC_ANY_LATCH);

	srv_misc_tmpfile = os_file_create_tmpfile();
	if (!srv_misc_tmpfile) {
		return(DB_ERROR);
	}

	/* If user has set the value of innodb_file_io_threads then
	we'll emit a message telling the user that this parameter
	is now deprecated. */
	if (srv_n_file_io_threads != 4) {
		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: Warning:"
			" innodb_file_io_threads is deprecated."
			" Please use innodb_read_io_threads and"
			" innodb_write_io_threads instead\n");
	}

	/* Now overwrite the value on srv_n_file_io_threads */
	srv_n_file_io_threads = 2 + srv_n_read_io_threads
				+ srv_n_write_io_threads;

	ut_a(srv_n_file_io_threads <= SRV_MAX_N_IO_THREADS);

	io_limit = 8 * SRV_N_PENDING_IOS_PER_THREAD;

	/* On Windows when using native aio the number of aio requests
	that a thread can handle at a given time is limited to 32
	i.e.: SRV_N_PENDING_IOS_PER_THREAD */
# ifdef __WIN__
	if (srv_use_native_aio) {
		io_limit = SRV_N_PENDING_IOS_PER_THREAD;
	}
# endif /* __WIN__ */

	os_aio_init(io_limit,
		    srv_n_read_io_threads,
		    srv_n_write_io_threads,
		    SRV_MAX_N_PENDING_SYNC_IOS);

	fil_init(srv_file_per_table ? 50000 : 5000, srv_max_n_open_files);

	/* Print time to initialize the buffer pool */
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: Initializing buffer pool, size =");

	if (srv_buf_pool_size >= 1024 * 1024 * 1024) {
		fprintf(stderr,
			" %.1fG\n",
			((double) srv_buf_pool_size) / (1024 * 1024 * 1024));
	} else {
		fprintf(stderr,
			" %.1fM\n",
			((double) srv_buf_pool_size) / (1024 * 1024));
	}

	err = buf_pool_init(srv_buf_pool_size, srv_buf_pool_instances);

	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: Completed initialization of buffer pool\n");

	if (err != DB_SUCCESS) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Fatal error: cannot allocate memory"
			" for the buffer pool\n");

		return(DB_ERROR);
	}

#ifdef UNIV_DEBUG
	/* We have observed deadlocks with a 5MB buffer pool but
	the actual lower limit could very well be a little higher. */

	if (srv_buf_pool_size <= 5 * 1024 * 1024) {

		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: Warning: Small buffer pool size "
			"(%luM), the flst_validate() debug function "
			"can cause a deadlock if the buffer pool fills up.\n",
			srv_buf_pool_size / 1024 / 1024);
	}
#endif

	fsp_init();
	log_init();

	lock_sys_create(srv_lock_table_size);

	/* Create i/o-handler threads: */

	for (i = 0; i < srv_n_file_io_threads; i++) {
		n[i] = i;

		os_thread_create(io_handler_thread, n + i, thread_ids + i);
	}

#ifdef UNIV_LOG_ARCHIVE
	if (0 != ut_strcmp(srv_log_group_home_dirs[0], srv_arch_dir)) {
		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: Error: you must set the log group home dir in my.cnf\n");
		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: the same as log arch dir.\n");

		return(DB_ERROR);
	}
#endif /* UNIV_LOG_ARCHIVE */

	if (srv_n_log_files * srv_log_file_size * UNIV_PAGE_SIZE
	    >= 549755813888ULL /* 512G */) {
		/* log_block_convert_lsn_to_no() limits the returned block
		number to 1G and given that OS_FILE_LOG_BLOCK_SIZE is 512
		bytes, then we have a limit of 512 GB. If that limit is to
		be raised, then log_block_convert_lsn_to_no() must be
		modified. */
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error: combined size of log files"
			" must be < 512 GB\n");

		return(DB_ERROR);
	}

	if (srv_n_log_files * srv_log_file_size >= ULINT_MAX) {
		/* fil_io() takes ulint as an argument and we are passing
		(next_offset / UNIV_PAGE_SIZE) to it in log_group_write_buf().
		So (next_offset / UNIV_PAGE_SIZE) must be less than ULINT_MAX.
		So next_offset must be < ULINT_MAX * UNIV_PAGE_SIZE. This
		means that we are limited to ULINT_MAX * UNIV_PAGE_SIZE which
		is 64 TB on 32 bit systems. */
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error: combined size of log files"
			" must be < %lu GB\n",
			ULINT_MAX / 1073741824 * UNIV_PAGE_SIZE);

		return(DB_ERROR);
	}

	sum_of_new_sizes = 0;

	for (i = 0; i < srv_n_data_files; i++) {
#ifndef __WIN__
		if (sizeof(off_t) < 5
		    && srv_data_file_sizes[i]
		    >= (ulint) (1 << (32 - UNIV_PAGE_SIZE_SHIFT))) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: Error: file size must be < 4 GB"
				" with this MySQL binary\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: and operating system combination,"
				" in some OS's < 2 GB\n");

			return(DB_ERROR);
		}
#endif
		sum_of_new_sizes += srv_data_file_sizes[i];
	}

	if (sum_of_new_sizes < 10485760 / UNIV_PAGE_SIZE) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error: tablespace size must be"
			" at least 10 MB\n");

		return(DB_ERROR);
	}

	err = open_or_create_data_files(&create_new_db,
#ifdef UNIV_LOG_ARCHIVE
					&min_arch_log_no, &max_arch_log_no,
#endif /* UNIV_LOG_ARCHIVE */
					&min_flushed_lsn, &max_flushed_lsn,
					&sum_of_new_sizes);
	if (err != DB_SUCCESS) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Could not open or create data files.\n");
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: If you tried to add new data files,"
			" and it failed here,\n");
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: you should now edit innodb_data_file_path"
			" in my.cnf back\n");
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: to what it was, and remove the"
			" new ibdata files InnoDB created\n");
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: in this failed attempt. InnoDB only wrote"
			" those files full of\n");
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: zeros, but did not yet use them in any way."
			" But be careful: do not\n");
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: remove old data files"
			" which contain your precious data!\n");

		return((int) err);
	}

#ifdef UNIV_LOG_ARCHIVE
	srv_normalize_path_for_win(srv_arch_dir);
	srv_arch_dir = srv_add_path_separator_if_needed(srv_arch_dir);
#endif /* UNIV_LOG_ARCHIVE */

	for (i = 0; i < srv_n_log_files; i++) {
		err = open_or_create_log_file(create_new_db, &log_file_created,
					      log_opened, 0, i);
		if (err != DB_SUCCESS) {

			return((int) err);
		}

		if (log_file_created) {
			log_created = TRUE;
		} else {
			log_opened = TRUE;
		}
		if ((log_opened && create_new_db)
		    || (log_opened && log_created)) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: Error: all log files must be"
				" created at the same time.\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: All log files must be"
				" created also in database creation.\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: If you want bigger or smaller"
				" log files, shut down the\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: database and make sure there"
				" were no errors in shutdown.\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: Then delete the existing log files."
				" Edit the .cnf file\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: and start the database again.\n");

			return(DB_ERROR);
		}
	}

	/* Open all log files and data files in the system tablespace: we
	keep them open until database shutdown */

	fil_open_log_and_system_tablespace_files();

	err = srv_undo_tablespaces_init(create_new_db, srv_undo_tablespaces);

	/* If the force recovery is set very high then we carry on regardless
	of all errors. Basically this is fingers crossed mode. */

	if (err != DB_SUCCESS
	    && srv_force_recovery < SRV_FORCE_NO_UNDO_LOG_SCAN) {

		return((int) err);
	}

	if (log_created && !create_new_db
#ifdef UNIV_LOG_ARCHIVE
	    && !srv_archive_recovery
#endif /* UNIV_LOG_ARCHIVE */
	    ) {
		if (max_flushed_lsn != min_flushed_lsn
#ifdef UNIV_LOG_ARCHIVE
		    || max_arch_log_no != min_arch_log_no
#endif /* UNIV_LOG_ARCHIVE */
		    ) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: Cannot initialize created"
				" log files because\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: data files were not in sync"
				" with each other\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: or the data files are corrupt.\n");

			return(DB_ERROR);
		}

		if (max_flushed_lsn < (lsn_t) 1000) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: Cannot initialize created"
				" log files because\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: data files are corrupt,"
				" or new data files were\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: created when the database"
				" was started previous\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: time but the database"
				" was not shut down\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: normally after that.\n");

			return(DB_ERROR);
		}

		mutex_enter(&(log_sys->mutex));

#ifdef UNIV_LOG_ARCHIVE
		/* Do not + 1 arch_log_no because we do not use log
		archiving */
		recv_reset_logs(max_flushed_lsn, max_arch_log_no, TRUE);
#else
		recv_reset_logs(max_flushed_lsn, TRUE);
#endif /* UNIV_LOG_ARCHIVE */

		mutex_exit(&(log_sys->mutex));
	}

	trx_sys_file_format_init();

	trx_sys_create();

	if (create_new_db) {
		mtr_start(&mtr);

		fsp_header_init(0, sum_of_new_sizes, &mtr);

		mtr_commit(&mtr);

		/* To maintain backward compatibility we create only
		the first rollback segment before the double write buffer.
		All the remaining rollback segments will be created later,
		after the double write buffer has been created. */
		trx_sys_create_sys_pages();

		ib_bh = trx_sys_init_at_db_start();

		/* The purge system needs to create the purge view and
		therefore requires that the trx_sys is inited. */

		trx_purge_sys_create(srv_n_purge_threads, ib_bh);

		dict_create();

		srv_startup_is_before_trx_rollback_phase = FALSE;

#ifdef UNIV_LOG_ARCHIVE
	} else if (srv_archive_recovery) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Starting archive"
			" recovery from a backup...\n");
		err = recv_recovery_from_archive_start(
			min_flushed_lsn, srv_archive_recovery_limit_lsn,
			min_arch_log_no);
		if (err != DB_SUCCESS) {

			return(DB_ERROR);
		}
		/* Since ibuf init is in dict_boot, and ibuf is needed
		in any disk i/o, first call dict_boot */

		dict_boot();

		ib_bh = trx_sys_init_at_db_start();

		/* The purge system needs to create the purge view and
		therefore requires that the trx_sys is inited. */

		trx_purge_sys_create(srv_n_purge_threads, ib_bh);

		srv_startup_is_before_trx_rollback_phase = FALSE;

		recv_recovery_from_archive_finish();
#endif /* UNIV_LOG_ARCHIVE */
	} else {

		/* Check if we support the max format that is stamped
		on the system tablespace.
		Note:  We are NOT allowed to make any modifications to
		the TRX_SYS_PAGE_NO page before recovery  because this
		page also contains the max_trx_id etc. important system
		variables that are required for recovery.  We need to
		ensure that we return the system to a state where normal
		recovery is guaranteed to work. We do this by
		invalidating the buffer cache, this will force the
		reread of the page and restoration to its last known
		consistent state, this is REQUIRED for the recovery
		process to work. */
		err = trx_sys_file_format_max_check(
			srv_max_file_format_at_startup);

		if (err != DB_SUCCESS) {
			return(err);
		}

		/* Invalidate the buffer pool to ensure that we reread
		the page that we read above, during recovery.
		Note that this is not as heavy weight as it seems. At
		this point there will be only ONE page in the buf_LRU
		and there must be no page in the buf_flush list. */
		buf_pool_invalidate();

		/* We always try to do a recovery, even if the database had
		been shut down normally: this is the normal startup path */

		err = recv_recovery_from_checkpoint_start(LOG_CHECKPOINT,
							  IB_ULONGLONG_MAX,
							  min_flushed_lsn,
							  max_flushed_lsn);
		if (err != DB_SUCCESS) {

			return(DB_ERROR);
		}

		/* Since the insert buffer init is in dict_boot, and the
		insert buffer is needed in any disk i/o, first we call
		dict_boot(). Note that trx_sys_init_at_db_start() only needs
		to access space 0, and the insert buffer at this stage already
		works for space 0. */

		dict_boot();

		ib_bh = trx_sys_init_at_db_start();

		/* The purge system needs to create the purge view and
		therefore requires that the trx_sys is inited. */

		trx_purge_sys_create(srv_n_purge_threads, ib_bh);

		/* recv_recovery_from_checkpoint_finish needs trx lists which
		are initialized in trx_sys_init_at_db_start(). */

		recv_recovery_from_checkpoint_finish();
		if (srv_force_recovery < SRV_FORCE_NO_IBUF_MERGE) {
			/* The following call is necessary for the insert
			buffer to work with multiple tablespaces. We must
			know the mapping between space id's and .ibd file
			names.

			In a crash recovery, we check that the info in data
			dictionary is consistent with what we already know
			about space id's from the call of
			fil_load_single_table_tablespaces().

			In a normal startup, we create the space objects for
			every table in the InnoDB data dictionary that has
			an .ibd file.

			We also determine the maximum tablespace id used. */

			dict_check_tablespaces_and_store_max_id(
				recv_needed_recovery);
		}

		srv_startup_is_before_trx_rollback_phase = FALSE;
		recv_recovery_rollback_active();

		/* It is possible that file_format tag has never
		been set. In this case we initialize it to minimum
		value.  Important to note that we can do it ONLY after
		we have finished the recovery process so that the
		image of TRX_SYS_PAGE_NO is not stale. */
		trx_sys_file_format_tag_init();
	}

	if (!create_new_db && sum_of_new_sizes > 0) {
		/* New data file(s) were added */
		mtr_start(&mtr);

		fsp_header_inc_size(0, sum_of_new_sizes, &mtr);

		mtr_commit(&mtr);

		/* Immediately write the log record about increased tablespace
		size to disk, so that it is durable even if mysqld would crash
		quickly */

		log_buffer_flush_to_disk();
	}

#ifdef UNIV_LOG_ARCHIVE
	/* Archiving is always off under MySQL */
	if (!srv_log_archive_on) {
		ut_a(DB_SUCCESS == log_archive_noarchivelog());
	} else {
		mutex_enter(&(log_sys->mutex));

		start_archive = FALSE;

		if (log_sys->archiving_state == LOG_ARCH_OFF) {
			start_archive = TRUE;
		}

		mutex_exit(&(log_sys->mutex));

		if (start_archive) {
			ut_a(DB_SUCCESS == log_archive_archivelog());
		}
	}
#endif /* UNIV_LOG_ARCHIVE */

	/* fprintf(stderr, "Max allowed record size %lu\n",
	page_get_free_space_of_empty() / 2); */

	if (buf_dblwr == NULL) {
		/* Create the doublewrite buffer to a new tablespace */

		buf_dblwr_create();
	}

	/* Here the double write buffer has already been created and so
	any new rollback segments will be allocated after the double
	write buffer. The default segment should already exist.
	We create the new segments only if it's a new database or
	the database was shutdown cleanly. */

	/* Note: When creating the extra rollback segments during an upgrade
	we violate the latching order, even if the change buffer is empty.
	We make an exception in sync0sync.cc and check srv_is_being_started
	for that violation. It cannot create a deadlock because we are still
	running in single threaded mode essentially. Only the IO threads
	should be running at this stage. */

	ut_a(srv_undo_logs > 0);
	ut_a(srv_undo_logs <= TRX_SYS_N_RSEGS);

	/* The number of rsegs that exist in InnoDB is given by status
	variable srv_available_undo_logs. The number of rsegs to use can
	be set using the dynamic global variable srv_undo_logs. */

	srv_available_undo_logs = trx_sys_create_rsegs(
		srv_undo_tablespaces, srv_undo_logs);

	if (srv_available_undo_logs == ULINT_UNDEFINED) {
		/* Can only happen if force recovery is set. */
		ut_a(srv_force_recovery >= SRV_FORCE_NO_TRX_UNDO);
		srv_undo_logs = ULONG_UNDEFINED;
	}

	/* Create the thread which watches the timeouts for lock waits */
	os_thread_create(
		lock_wait_timeout_thread,
		NULL, thread_ids + 2 + SRV_MAX_N_IO_THREADS);

	/* Create the thread which warns of long semaphore waits */
	os_thread_create(
		srv_error_monitor_thread,
		NULL, thread_ids + 3 + SRV_MAX_N_IO_THREADS);

	/* Create the thread which prints InnoDB monitor info */
	os_thread_create(
		srv_monitor_thread,
		NULL, thread_ids + 4 + SRV_MAX_N_IO_THREADS);

	srv_is_being_started = FALSE;

	/* Create the SYS_FOREIGN and SYS_FOREIGN_COLS system tables */
	err = dict_create_or_check_foreign_constraint_tables();
	if (err != DB_SUCCESS) {
		return((int)DB_ERROR);
	}

	srv_is_being_started = FALSE;

	ut_a(trx_purge_state() == PURGE_STATE_INIT);

	/* Create the master thread which does purge and other utility
	operations */

	os_thread_create(
		srv_master_thread,
		NULL, thread_ids + (1 + SRV_MAX_N_IO_THREADS));

	if (srv_force_recovery < SRV_FORCE_NO_BACKGROUND) {

		os_thread_create(
			srv_purge_coordinator_thread,
			NULL, thread_ids + 5 + SRV_MAX_N_IO_THREADS);

		ut_a(UT_ARR_SIZE(thread_ids)
		     > 5 + srv_n_purge_threads + SRV_MAX_N_IO_THREADS);

		/* We've already created the purge coordinator thread above. */
		for (i = 1; i < srv_n_purge_threads; ++i) {
			os_thread_create(
				srv_worker_thread, NULL,
				thread_ids + 5 + i + SRV_MAX_N_IO_THREADS);
		}
	}

	os_thread_create(buf_flush_page_cleaner_thread, NULL, NULL);

	/* Wait for the purge coordinator and master thread to startup. */

	purge_state_t	state = trx_purge_state();

	while (srv_shutdown_state == SRV_SHUTDOWN_NONE
	       && srv_force_recovery < SRV_FORCE_NO_BACKGROUND
	       && state == PURGE_STATE_INIT) {

		switch (state = trx_purge_state()) {
		case PURGE_STATE_RUN:
		case PURGE_STATE_STOP:
			break;

		case PURGE_STATE_INIT:
			ut_print_timestamp(stderr);
			fprintf(stderr, " InnoDB: "
				"Waiting for the background threads to "
				"start\n");

			os_thread_sleep(50000);
			break;

		case PURGE_STATE_EXIT:
			ut_error;
		}
	}

#ifdef UNIV_DEBUG
	/* buf_debug_prints = TRUE; */
#endif /* UNIV_DEBUG */
	sum_of_data_file_sizes = 0;

	for (i = 0; i < srv_n_data_files; i++) {
		sum_of_data_file_sizes += srv_data_file_sizes[i];
	}

	tablespace_size_in_header = fsp_header_get_tablespace_size();

	if (!srv_auto_extend_last_data_file
	    && sum_of_data_file_sizes != tablespace_size_in_header) {

		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error: tablespace size"
			" stored in header is %lu pages, but\n",
			(ulong) tablespace_size_in_header);
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"InnoDB: the sum of data file sizes is %lu pages\n",
			(ulong) sum_of_data_file_sizes);

		if (srv_force_recovery == 0
		    && sum_of_data_file_sizes < tablespace_size_in_header) {
			/* This is a fatal error, the tail of a tablespace is
			missing */

			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: Cannot start InnoDB."
				" The tail of the system tablespace is\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: missing. Have you edited"
				" innodb_data_file_path in my.cnf in an\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: inappropriate way, removing"
				" ibdata files from there?\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: You can set innodb_force_recovery=1"
				" in my.cnf to force\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: a startup if you are trying"
				" to recover a badly corrupt database.\n");

			return(DB_ERROR);
		}
	}

	if (srv_auto_extend_last_data_file
	    && sum_of_data_file_sizes < tablespace_size_in_header) {

		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error: tablespace size stored in header"
			" is %lu pages, but\n",
			(ulong) tablespace_size_in_header);
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: the sum of data file sizes"
			" is only %lu pages\n",
			(ulong) sum_of_data_file_sizes);

		if (srv_force_recovery == 0) {

			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: Cannot start InnoDB. The tail of"
				" the system tablespace is\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: missing. Have you edited"
				" innodb_data_file_path in my.cnf in an\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: inappropriate way, removing"
				" ibdata files from there?\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: You can set innodb_force_recovery=1"
				" in my.cnf to force\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: a startup if you are trying to"
				" recover a badly corrupt database.\n");

			return(DB_ERROR);
		}
	}

	/* Check that os_fast_mutexes work as expected */
	os_fast_mutex_init(PFS_NOT_INSTRUMENTED, &srv_os_test_mutex);

	if (0 != os_fast_mutex_trylock(&srv_os_test_mutex)) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error: pthread_mutex_trylock returns"
			" an unexpected value on\n");
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: success! Cannot continue.\n");
		exit(1);
	}

	os_fast_mutex_unlock(&srv_os_test_mutex);

	os_fast_mutex_lock(&srv_os_test_mutex);

	os_fast_mutex_unlock(&srv_os_test_mutex);

	os_fast_mutex_free(&srv_os_test_mutex);

	if (srv_print_verbose_log) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: %s started; "
			"log sequence number " LSN_PF "\n",
			INNODB_VERSION_STR, srv_start_lsn);
	}

	if (srv_force_recovery > 0) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: !!! innodb_force_recovery"
			" is set to %lu !!!\n",
			(ulong) srv_force_recovery);
	}

	fflush(stderr);

	if (srv_force_recovery == 0) {
		/* In the insert buffer we may have even bigger tablespace
		id's, because we may have dropped those tablespaces, but
		insert buffer merge has not had time to clean the records from
		the ibuf tree. */

		ibuf_update_max_tablespace_id();
	}

	/* Create the buffer pool dump/load thread */
	os_thread_create(buf_dump_thread, NULL, NULL);

	srv_was_started = TRUE;

	/* Create the thread that will optimize the FTS sub-system
	in a separate background thread. */
	fts_optimize_init();

	return((int) DB_SUCCESS);
}

#if 0
/********************************************************************
Sync all FTS cache before shutdown */
static
void
srv_fts_close(void)
/*===============*/
{
	dict_table_t*	table;

	for (table = UT_LIST_GET_FIRST(dict_sys->table_LRU);
	     table; table = UT_LIST_GET_NEXT(table_LRU, table)) {
		fts_t*          fts = table->fts;

		if (fts != NULL) {
			fts_sync_table(table);
		}
	}

	for (table = UT_LIST_GET_FIRST(dict_sys->table_non_LRU);
	     table; table = UT_LIST_GET_NEXT(table_LRU, table)) {
		fts_t*          fts = table->fts;

		if (fts != NULL) {
			fts_sync_table(table);
		}
	}
}
#endif

/****************************************************************//**
Shuts down the InnoDB database.
@return	DB_SUCCESS or error code */
UNIV_INTERN
int
innobase_shutdown_for_mysql(void)
/*=============================*/
{
	ulint	i;
	if (!srv_was_started) {
		if (srv_is_being_started) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: Warning: shutting down"
				" a not properly started\n"
				"InnoDB: or created database!\n");
		}

		return(DB_SUCCESS);
	}

	/* Shutdown the FTS optimize sub system. */
	fts_optimize_start_shutdown();

	fts_optimize_end();

	/* 1. Flush the buffer pool to disk, write the current lsn to
	the tablespace header(s), and copy all log data to archive.
	The step 1 is the real InnoDB shutdown. The remaining steps 2 - ...
	just free data structures after the shutdown. */

	logs_empty_and_mark_files_at_shutdown();

	if (srv_conc_get_active_threads() != 0) {
		fprintf(stderr,
			"InnoDB: Warning: query counter shows %ld queries"
			" still\n"
			"InnoDB: inside InnoDB at shutdown\n",
			srv_conc_get_active_threads());
	}

	/* This functionality will be used by WL#5522. */
	ut_a(trx_purge_state() == PURGE_STATE_RUN
	     || trx_purge_state() == PURGE_STATE_EXIT
	     || srv_force_recovery >= SRV_FORCE_NO_BACKGROUND);

	/* 2. Make all threads created by InnoDB to exit */

	srv_shutdown_state = SRV_SHUTDOWN_EXIT_THREADS;

	/* All threads end up waiting for certain events. Put those events
	to the signaled state. Then the threads will exit themselves after
	os_event_wait(). */

	for (i = 0; i < 1000; i++) {
		/* NOTE: IF YOU CREATE THREADS IN INNODB, YOU MUST EXIT THEM
		HERE OR EARLIER */

		/* a. Let the lock timeout thread exit */
		os_event_set(srv_timeout_event);

		/* b. srv error monitor thread exits automatically, no need
		to do anything here */

		/* c. We wake the master thread so that it exits */
		srv_wake_master_thread();

		/* d. Wakeup purge threads. */
		srv_purge_wakeup();

		/* e. Exit the i/o threads */

		os_aio_wake_all_threads_at_shutdown();

		os_mutex_enter(os_sync_mutex);

		if (os_thread_count == 0) {
			/* All the threads have exited or are just exiting;
			NOTE that the threads may not have completed their
			exit yet. Should we use pthread_join() to make sure
			they have exited? If we did, we would have to
			remove the pthread_detach() from
			os_thread_exit().  Now we just sleep 0.1
			seconds and hope that is enough! */

			os_mutex_exit(os_sync_mutex);

			os_thread_sleep(100000);

			break;
		}

		os_mutex_exit(os_sync_mutex);

		os_thread_sleep(100000);
	}

	if (i == 1000) {
		fprintf(stderr,
			"InnoDB: Warning: %lu threads created by InnoDB"
			" had not exited at shutdown!\n",
			(ulong) os_thread_count);
	}

	if (srv_monitor_file) {
		fclose(srv_monitor_file);
		srv_monitor_file = 0;
		if (srv_monitor_file_name) {
			unlink(srv_monitor_file_name);
			mem_free(srv_monitor_file_name);
		}
	}
	if (srv_dict_tmpfile) {
		fclose(srv_dict_tmpfile);
		srv_dict_tmpfile = 0;
	}

	if (srv_misc_tmpfile) {
		fclose(srv_misc_tmpfile);
		srv_misc_tmpfile = 0;
	}

	/* This must be disabled before closing the buffer pool
	and closing the data dictionary.  */
	btr_search_disable();

	ibuf_close();
	log_shutdown();
	lock_sys_close();
	trx_sys_file_format_close();
	trx_sys_close();

	mutex_free(&srv_monitor_file_mutex);
	mutex_free(&srv_dict_tmpfile_mutex);
	mutex_free(&srv_misc_tmpfile_mutex);
	dict_close();
	btr_search_sys_free();

	/* 3. Free all InnoDB's own mutexes and the os_fast_mutexes inside
	them */
	os_aio_free();
	que_close();
	row_mysql_close();
	sync_close();
	srv_free();
	fil_close();

	/* 4. Free the os_conc_mutex and all os_events and os_mutexes */

	os_sync_free();

	/* 5. Free all allocated memory */

	pars_lexer_close();
	log_mem_free();
	buf_pool_free(srv_buf_pool_instances);
	mem_close();

	/* ut_free_all_mem() frees all allocated memory not freed yet
	in shutdown, and it will also free the ut_list_mutex, so it
	should be the last one for all operation */
	ut_free_all_mem();

	if (os_thread_count != 0
	    || os_event_count != 0
	    || os_mutex_count != 0
	    || os_fast_mutex_count != 0) {
		fprintf(stderr,
			"InnoDB: Warning: some resources were not"
			" cleaned up in shutdown:\n"
			"InnoDB: threads %lu, events %lu,"
			" os_mutexes %lu, os_fast_mutexes %lu\n",
			(ulong) os_thread_count, (ulong) os_event_count,
			(ulong) os_mutex_count, (ulong) os_fast_mutex_count);
	}

	if (dict_foreign_err_file) {
		fclose(dict_foreign_err_file);
	}

	if (srv_print_verbose_log) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Shutdown completed;"
			" log sequence number " LSN_PF "\n",
			srv_shutdown_lsn);
	}

	srv_was_started = FALSE;
	srv_start_has_been_called = FALSE;

	return((int) DB_SUCCESS);
}
#endif /* !UNIV_HOTBACKUP */


/********************************************************************
Signal all per-table background threads to shutdown, and wait for them to do
so. */

void
srv_shutdown_table_bg_threads(void)
/*===============================*/
{
	dict_table_t*	table;
	dict_table_t*	first;
	dict_table_t*	last = NULL;

	mutex_enter(&dict_sys->mutex);

	/* Signal all threads that they should stop. */
	table = UT_LIST_GET_FIRST(dict_sys->table_LRU);
	first = table;
	while (table) {
		dict_table_t*	next;
		fts_t*		fts = table->fts;

		if (fts != NULL) {
			fts_start_shutdown(table, fts);
		}

		next = UT_LIST_GET_NEXT(table_LRU, table);

		if (!next) {
			last = table;
		}

		table = next;
	}

	/* We must release dict_sys->mutex here; if we hold on to it in the
	loop below, we will deadlock if any of the background threads try to
	acquire it (for example, the FTS thread by calling que_eval_sql).

	Releasing it here and going through dict_sys->table_LRU without
	holding it is safe because:

	 a) MySQL only starts the shutdown procedure after all client
	 threads have been disconnected and no new ones are accepted, so no
	 new tables are added or old ones dropped.

	 b) Despite its name, the list is not LRU, and the order stays
	 fixed.

	To safeguard against the above assumptions ever changing, we store
	the first and last items in the list above, and then check that
	they've stayed the same below. */

	mutex_exit(&dict_sys->mutex);

	/* Wait for the threads of each table to stop. This is not inside
	the above loop, because by signaling all the threads first we can
	overlap their shutting down delays. */
	table = UT_LIST_GET_FIRST(dict_sys->table_LRU);
	ut_a(first == table);
	while (table) {
		dict_table_t*	next;
		fts_t*		fts = table->fts;

		if (fts != NULL) {
			fts_shutdown(table, fts);
		}

		next = UT_LIST_GET_NEXT(table_LRU, table);

		if (table == last) {
			ut_a(!next);
		}

		table = next;
	}
}
