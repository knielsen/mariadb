/*****************************************************************************

Copyright (c) 1996, 2011, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file dict/dict0load.cc
Loads to the memory cache database object definitions
from dictionary tables

Created 4/24/1996 Heikki Tuuri
*******************************************************/

#include "dict0load.h"
#include "mysql_version.h"

#ifdef UNIV_NONINL
#include "dict0load.ic"
#endif

#include "btr0pcur.h"
#include "btr0btr.h"
#include "page0page.h"
#include "mach0data.h"
#include "dict0dict.h"
#include "dict0boot.h"
#include "dict0stats.h"
#include "rem0cmp.h"
#include "srv0start.h"
#include "srv0srv.h"
#include "dict0priv.h"
#include "ha_prototypes.h" /* innobase_casedn_str() */
#include "fts0priv.h"

/** Following are six InnoDB system tables */
static const char* SYSTEM_TABLE_NAME[] = {
	"SYS_TABLES",
	"SYS_INDEXES",
	"SYS_COLUMNS",
	"SYS_FIELDS",
	"SYS_FOREIGN",
	"SYS_FOREIGN_COLS"
};

/* If this flag is TRUE, then we will load the cluster index's (and tables')
metadata even if it is marked as "corrupted". */
UNIV_INTERN my_bool     srv_load_corrupted = FALSE;

#ifdef UNIV_DEBUG
/****************************************************************//**
Compare the name of an index column.
@return	TRUE if the i'th column of index is 'name'. */
static
ibool
name_of_col_is(
/*===========*/
	const dict_table_t*	table,	/*!< in: table */
	const dict_index_t*	index,	/*!< in: index */
	ulint			i,	/*!< in: index field offset */
	const char*		name)	/*!< in: name to compare to */
{
	ulint	tmp = dict_col_get_no(dict_field_get_col(
					      dict_index_get_nth_field(
						      index, i)));

	return(strcmp(name, dict_table_get_col_name(table, tmp)) == 0);
}
#endif /* UNIV_DEBUG */

/********************************************************************//**
Finds the first table name in the given database.
@return own: table name, NULL if does not exist; the caller must free
the memory in the string! */
UNIV_INTERN
char*
dict_get_first_table_name_in_db(
/*============================*/
	const char*	name)	/*!< in: database name which ends in '/' */
{
	dict_table_t*	sys_tables;
	btr_pcur_t	pcur;
	dict_index_t*	sys_index;
	dtuple_t*	tuple;
	mem_heap_t*	heap;
	dfield_t*	dfield;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	mtr_t		mtr;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	heap = mem_heap_create(1000);

	mtr_start(&mtr);

	sys_tables = dict_table_get_low("SYS_TABLES");
	sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);
	ut_ad(!dict_table_is_comp(sys_tables));

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, name, ut_strlen(name));
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
loop:
	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur)) {
		/* Not found */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);

		return(NULL);
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__NAME, &len);

	if (len < strlen(name)
	    || ut_memcmp(name, field, strlen(name)) != 0) {
		/* Not found */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);

		return(NULL);
	}

	if (!rec_get_deleted_flag(rec, 0)) {

		/* We found one */

		char*	table_name = mem_strdupl((char*) field, len);

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);

		return(table_name);
	}

	btr_pcur_move_to_next_user_rec(&pcur, &mtr);

	goto loop;
}

/********************************************************************//**
Prints to the standard output information on all tables found in the data
dictionary system table. */
UNIV_INTERN
void
dict_print(void)
/*============*/
{
	dict_table_t*	table;
	btr_pcur_t	pcur;
	const rec_t*	rec;
	mem_heap_t*	heap;
	mtr_t		mtr;

	/* Enlarge the fatal semaphore wait timeout during the InnoDB table
	monitor printout */

	os_increment_counter_by_amount(
		server_mutex,
		srv_fatal_semaphore_wait_threshold, 7200/*2 hours*/);

	heap = mem_heap_create(1000);
	mutex_enter(&(dict_sys->mutex));
	mtr_start(&mtr);

	rec = dict_startscan_system(&pcur, &mtr, SYS_TABLES);

	while (rec) {
		const char* err_msg;

		err_msg = static_cast<const char*>(
			dict_process_sys_tables_rec_and_mtr_commit(
			heap, rec, &table,
			static_cast<dict_table_info_t>(
				DICT_TABLE_LOAD_FROM_CACHE
				| DICT_TABLE_UPDATE_STATS), &mtr));

		if (!err_msg) {
			dict_table_print_low(table);
		} else {
			ut_print_timestamp(stderr);
			fprintf(stderr, "  InnoDB: %s\n", err_msg);
		}

		mem_heap_empty(heap);

		mtr_start(&mtr);
		rec = dict_getnext_system(&pcur, &mtr);
	}

	mtr_commit(&mtr);
	mutex_exit(&(dict_sys->mutex));
	mem_heap_free(heap);

	/* Restore the fatal semaphore wait timeout */
	os_decrement_counter_by_amount(
		server_mutex,
		srv_fatal_semaphore_wait_threshold, 7200/*2 hours*/);
}

/********************************************************************//**
This function gets the next system table record as it scans the table.
@return	the next record if found, NULL if end of scan */
static
const rec_t*
dict_getnext_system_low(
/*====================*/
	btr_pcur_t*	pcur,		/*!< in/out: persistent cursor to the
					record*/
	mtr_t*		mtr)		/*!< in: the mini-transaction */
{
	rec_t*	rec = NULL;

	while (!rec || rec_get_deleted_flag(rec, 0)) {
		btr_pcur_move_to_next_user_rec(pcur, mtr);

		rec = btr_pcur_get_rec(pcur);

		if (!btr_pcur_is_on_user_rec(pcur)) {
			/* end of index */
			btr_pcur_close(pcur);

			return(NULL);
		}
	}

	/* Get a record, let's save the position */
	btr_pcur_store_position(pcur, mtr);

	return(rec);
}

/********************************************************************//**
This function opens a system table, and returns the first record.
@return	first record of the system table */
UNIV_INTERN
const rec_t*
dict_startscan_system(
/*==================*/
	btr_pcur_t*	pcur,		/*!< out: persistent cursor to
					the record */
	mtr_t*		mtr,		/*!< in: the mini-transaction */
	dict_system_id_t system_id)	/*!< in: which system table to open */
{
	dict_table_t*	system_table;
	dict_index_t*	clust_index;
	const rec_t*	rec;

	ut_a(system_id < SYS_NUM_SYSTEM_TABLES);

	system_table = dict_table_get_low(SYSTEM_TABLE_NAME[system_id]);

	clust_index = UT_LIST_GET_FIRST(system_table->indexes);

	btr_pcur_open_at_index_side(TRUE, clust_index, BTR_SEARCH_LEAF, pcur,
				    TRUE, mtr);

	rec = dict_getnext_system_low(pcur, mtr);

	return(rec);
}

/********************************************************************//**
This function gets the next system table record as it scans the table.
@return	the next record if found, NULL if end of scan */
UNIV_INTERN
const rec_t*
dict_getnext_system(
/*================*/
	btr_pcur_t*	pcur,		/*!< in/out: persistent cursor
					to the record */
	mtr_t*		mtr)		/*!< in: the mini-transaction */
{
	const rec_t*	rec;

	/* Restore the position */
	btr_pcur_restore_position(BTR_SEARCH_LEAF, pcur, mtr);

	/* Get the next record */
	rec = dict_getnext_system_low(pcur, mtr);

	return(rec);
}
/********************************************************************//**
This function processes one SYS_TABLES record and populate the dict_table_t
struct for the table. Extracted out of dict_print() to be used by
both monitor table output and information schema innodb_sys_tables output.
@return error message, or NULL on success */
UNIV_INTERN
const char*
dict_process_sys_tables_rec_and_mtr_commit(
/*=======================================*/
	mem_heap_t*	heap,		/*!< in/out: temporary memory heap */
	const rec_t*	rec,		/*!< in: SYS_TABLES record */
	dict_table_t**	table,		/*!< out: dict_table_t to fill */
	dict_table_info_t status,	/*!< in: status bit controls
					options such as whether we shall
					look for dict_table_t from cache
					first */
	mtr_t*		mtr)		/*!< in/out: mini-transaction,
					will be committed */
{
	ulint		len;
	const char*	field;
	const char*	err_msg = NULL;
	char*		table_name;

	field = (const char*) rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__NAME, &len);

	ut_a(!rec_get_deleted_flag(rec, 0));

	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));

	/* Get the table name */
	table_name = mem_heap_strdupl(heap, field, len);

	/* If DICT_TABLE_LOAD_FROM_CACHE is set, first check
	whether there is cached dict_table_t struct */
	if (status & DICT_TABLE_LOAD_FROM_CACHE) {

		/* Commit before load the table again */
		mtr_commit(mtr);

		*table = dict_table_get_low(table_name);

		if (!(*table)) {
			err_msg = "Table not found in cache";
		}
	} else {
		err_msg = dict_load_table_low(table_name, rec, table);
		mtr_commit(mtr);
	}

	if (err_msg) {
		return(err_msg);
	}

	if ((status & DICT_TABLE_UPDATE_STATS)
	    && dict_table_get_first_index(*table)) {

		/* Update statistics member fields in *table if
		DICT_TABLE_UPDATE_STATS is set */
		ut_ad(mutex_own(&dict_sys->mutex));
		dict_stats_update(*table, DICT_STATS_FETCH, TRUE);
	}

	return(NULL);
}

/********************************************************************//**
This function parses a SYS_INDEXES record and populate a dict_index_t
structure with the information from the record. For detail information
about SYS_INDEXES fields, please refer to dict_boot() function.
@return error message, or NULL on success */
UNIV_INTERN
const char*
dict_process_sys_indexes_rec(
/*=========================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_INDEXES rec */
	dict_index_t*	index,		/*!< out: index to be filled */
	table_id_t*	table_id)	/*!< out: index table id */
{
	const char*	err_msg;
	byte*		buf;

	buf = static_cast<byte*>(mem_heap_alloc(heap, 8));

	/* Parse the record, and get "dict_index_t" struct filled */
	err_msg = dict_load_index_low(buf, NULL,
				      heap, rec, FALSE, &index);

	*table_id = mach_read_from_8(buf);

	return(err_msg);
}
/********************************************************************//**
This function parses a SYS_COLUMNS record and populate a dict_column_t
structure with the information from the record.
@return error message, or NULL on success */
UNIV_INTERN
const char*
dict_process_sys_columns_rec(
/*=========================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_COLUMNS rec */
	dict_col_t*	column,		/*!< out: dict_col_t to be filled */
	table_id_t*	table_id,	/*!< out: table id */
	const char**	col_name)	/*!< out: column name */
{
	const char*	err_msg;

	/* Parse the record, and get "dict_col_t" struct filled */
	err_msg = dict_load_column_low(NULL, heap, column,
				       table_id, col_name, rec);

	return(err_msg);
}
/********************************************************************//**
This function parses a SYS_FIELDS record and populates a dict_field_t
structure with the information from the record.
@return error message, or NULL on success */
UNIV_INTERN
const char*
dict_process_sys_fields_rec(
/*========================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_FIELDS rec */
	dict_field_t*	sys_field,	/*!< out: dict_field_t to be
					filled */
	ulint*		pos,		/*!< out: Field position */
	index_id_t*	index_id,	/*!< out: current index id */
	index_id_t	last_id)	/*!< in: previous index id */
{
	byte*		buf;
	byte*		last_index_id;
	const char*	err_msg;

	buf = static_cast<byte*>(mem_heap_alloc(heap, 8));

	last_index_id = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(last_index_id, last_id);

	err_msg = dict_load_field_low(buf, NULL, sys_field,
				      pos, last_index_id, heap, rec);

	*index_id = mach_read_from_8(buf);

	return(err_msg);

}

/********************************************************************//**
This function parses a SYS_FOREIGN record and populate a dict_foreign_t
structure with the information from the record. For detail information
about SYS_FOREIGN fields, please refer to dict_load_foreign() function.
@return error message, or NULL on success */
UNIV_INTERN
const char*
dict_process_sys_foreign_rec(
/*=========================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_FOREIGN rec */
	dict_foreign_t*	foreign)	/*!< out: dict_foreign_t struct
					to be filled */
{
	ulint		len;
	const byte*	field;
	ulint		n_fields_and_type;

	if (UNIV_UNLIKELY(rec_get_deleted_flag(rec, 0))) {
		return("delete-marked record in SYS_FOREIGN");
	}

	if (rec_get_n_fields_old(rec) != DICT_NUM_FIELDS__SYS_FOREIGN) {
		return("wrong number of columns in SYS_FOREIGN record");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN__ID, &len);
	if (UNIV_UNLIKELY(len < 1 || len == UNIV_SQL_NULL)) {
err_len:
		return("incorrect column length in SYS_FOREIGN");
	}

	/* This recieves a dict_foreign_t* that points to a stack variable.
	So mem_heap_free(foreign->heap) is not used as elsewhere.
	Since the heap used here is freed elsewhere, foreign->heap
	is not assigned. */
	foreign->id = mem_heap_strdupl(heap, (const char*) field, len);

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_FOREIGN__DB_TRX_ID, &len);
	if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}
	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_FOREIGN__DB_ROLL_PTR, &len);
	if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	/* The _lookup versions of the referenced and foreign table names
	 are not assigned since they are not used in this dict_foreign_t */

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN__FOR_NAME, &len);
	if (len < 1 || len == UNIV_SQL_NULL) {
		goto err_len;
	}
	foreign->foreign_table_name = mem_heap_strdupl(
		heap, (const char*) field, len);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN__REF_NAME, &len);
	if (len < 1 || len == UNIV_SQL_NULL) {
		goto err_len;
	}
	foreign->referenced_table_name = mem_heap_strdupl(
		heap, (const char*) field, len);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN__N_COLS, &len);
	if (len != 4) {
		goto err_len;
	}
	n_fields_and_type = mach_read_from_4(field);

	foreign->type = (unsigned int) (n_fields_and_type >> 24);
	foreign->n_fields = (unsigned int) (n_fields_and_type & 0x3FFUL);

	return(NULL);
}

/********************************************************************//**
This function parses a SYS_FOREIGN_COLS record and extract necessary
information from the record and return to caller.
@return error message, or NULL on success */
UNIV_INTERN
const char*
dict_process_sys_foreign_col_rec(
/*=============================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_FOREIGN_COLS rec */
	const char**	name,		/*!< out: foreign key constraint name */
	const char**	for_col_name,	/*!< out: referencing column name */
	const char**	ref_col_name,	/*!< out: referenced column name
					in referenced table */
	ulint*		pos)		/*!< out: column position */
{
	ulint		len;
	const byte*	field;

	if (rec_get_deleted_flag(rec, 0)) {
		return("delete-marked record in SYS_FOREIGN_COLS");
	}

	if (rec_get_n_fields_old(rec) != DICT_NUM_FIELDS__SYS_FOREIGN_COLS) {
		return("wrong number of columns in SYS_FOREIGN_COLS record");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN_COLS__ID, &len);
	if (len < 1 || len == UNIV_SQL_NULL) {
err_len:
		return("incorrect column length in SYS_FOREIGN_COLS");
	}
	*name = mem_heap_strdupl(heap, (char*) field, len);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN_COLS__POS, &len);
	if (len != 4) {
		goto err_len;
	}
	*pos = mach_read_from_4(field);

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_FOREIGN_COLS__DB_TRX_ID, &len);
	if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}
	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_FOREIGN_COLS__DB_ROLL_PTR, &len);
	if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN_COLS__FOR_COL_NAME, &len);
	if (len < 1 || len == UNIV_SQL_NULL) {
		goto err_len;
	}
	*for_col_name = mem_heap_strdupl(heap, (char*) field, len);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN_COLS__REF_COL_NAME, &len);
	if (len < 1 || len == UNIV_SQL_NULL) {
		goto err_len;
	}
	*ref_col_name = mem_heap_strdupl(heap, (char*) field, len);

	return(NULL);
}

/********************************************************************//**
Determine the flags of a table as stored in SYS_TABLES.TYPE and N_COLS.
@return  ULINT_UNDEFINED if error, else a valid dict_table_t::flags. */
static
ulint
dict_sys_tables_get_flags(
/*======================*/
	const rec_t*	rec)	/*!< in: a record of SYS_TABLES */
{
	const byte*	field;
	ulint		len;
	ulint		type;
	ulint		n_cols;

	/* read the 4 byte flags from the TYPE field */
	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__TYPE, &len);
	ut_a(len == 4);
	type = mach_read_from_4(field);

	/* The low order bit of SYS_TABLES.TYPE is always set to 1. If no
	other bits are used, that is defined as SYS_TABLE_TYPE_ANTELOPE.
	But in dict_table_t::flags the low order bit is used to determine
	if the row format is Redundant or Compact when the format is
	Antelope.
	Read the 4 byte N_COLS field and look at the high order bit.  It
	should be set for COMPACT and later.  It should not be set for
	REDUNDANT. */
	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__N_COLS, &len);
	ut_a(len == 4);
	n_cols = mach_read_from_4(field);

	/* This validation function also combines the DICT_N_COLS_COMPACT
	flag in n_cols into the type field to effectively make it a
	dict_table_t::flags. */
	return(dict_sys_tables_type_validate(type, n_cols));
}

/********************************************************************//**
In a crash recovery we already have all the tablespace objects created.
This function compares the space id information in the InnoDB data dictionary
to what we already read with fil_load_single_table_tablespaces().

In a normal startup, we create the tablespace objects for every table in
InnoDB's data dictionary, if the corresponding .ibd file exists.
We also scan the biggest space id, and store it to fil_system. */
UNIV_INTERN
void
dict_check_tablespaces_and_store_max_id(
/*====================================*/
	ibool	in_crash_recovery)	/*!< in: are we doing a crash recovery */
{
	dict_table_t*	sys_tables;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	const rec_t*	rec;
	ulint		max_space_id;
	mtr_t		mtr;

	mutex_enter(&(dict_sys->mutex));

	mtr_start(&mtr);

	sys_tables = dict_table_get_low("SYS_TABLES");
	sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);
	ut_ad(!dict_table_is_comp(sys_tables));

	max_space_id = mtr_read_ulint(dict_hdr_get(&mtr)
				      + DICT_HDR_MAX_SPACE_ID,
				      MLOG_4BYTES, &mtr);
	fil_set_max_space_id_if_bigger(max_space_id);

	btr_pcur_open_at_index_side(TRUE, sys_index, BTR_SEARCH_LEAF, &pcur,
				    TRUE, &mtr);
loop:
	btr_pcur_move_to_next_user_rec(&pcur, &mtr);

	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur)) {
		/* end of index */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);

		/* We must make the tablespace cache aware of the biggest
		known space id */

		/* printf("Biggest space id in data dictionary %lu\n",
		max_space_id); */
		fil_set_max_space_id_if_bigger(max_space_id);

		mutex_exit(&(dict_sys->mutex));

		return;
	}

	if (!rec_get_deleted_flag(rec, 0)) {

		/* We found one */
		const byte*	field;
		ulint		len;
		ulint		space_id;
		ulint		flags;
		char*		name;

		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_TABLES__NAME, &len);
		name = mem_strdupl((char*) field, len);

		flags = dict_sys_tables_get_flags(rec);
		if (UNIV_UNLIKELY(flags == ULINT_UNDEFINED)) {
			/* Read again the 4 bytes from rec. */
			field = rec_get_nth_field_old(
				rec, DICT_FLD__SYS_TABLES__TYPE, &len);
			ut_ad(len == 4); /* this was checked earlier */
			flags = mach_read_from_4(field);

			ut_print_timestamp(stderr);
			fputs("  InnoDB: Error: table ", stderr);
			ut_print_filename(stderr, name);
			fprintf(stderr, "\n"
				"InnoDB: in InnoDB data dictionary"
				" has unknown type %lx.\n",
				(ulong) flags);

			goto loop;
		}

		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_TABLES__SPACE, &len);
		ut_a(len == 4);

		space_id = mach_read_from_4(field);

		btr_pcur_store_position(&pcur, &mtr);

		mtr_commit(&mtr);

		if (space_id == 0) {
			/* The system tablespace always exists. */
		} else if (in_crash_recovery) {
			/* Check that the tablespace (the .ibd file) really
			exists; print a warning to the .err log if not.
			Do not print warnings for temporary tables. */
			ibool	is_temp;

			field = rec_get_nth_field_old(
				rec, DICT_FLD__SYS_TABLES__N_COLS, &len);
			if (mach_read_from_4(field) & DICT_N_COLS_COMPACT) {
				/* ROW_FORMAT=COMPACT: read the is_temp
				flag from SYS_TABLES.MIX_LEN. */
				field = rec_get_nth_field_old(
					rec, 7/*MIX_LEN*/, &len);
				is_temp = !!(mach_read_from_4(field)
					     & DICT_TF2_TEMPORARY);
			} else {
				/* For tables created with old versions
				of InnoDB, SYS_TABLES.MIX_LEN may contain
				garbage.  Such tables would always be
				in ROW_FORMAT=REDUNDANT.  Pretend that
				all such tables are non-temporary.  That is,
				do not suppress error printouts about
				temporary tables not being found. */
				is_temp = FALSE;
			}

			fil_space_for_table_exists_in_mem(
				space_id, name, TRUE, !is_temp);
		} else {
			/* It is a normal database startup: create the space
			object and check that the .ibd file exists. */

			fil_open_single_table_tablespace(
				FALSE, space_id,
				dict_tf_to_fsp_flags(flags), name);
		}

		mem_free(name);

		if (space_id > max_space_id) {
			max_space_id = space_id;
		}

		mtr_start(&mtr);

		btr_pcur_restore_position(BTR_SEARCH_LEAF, &pcur, &mtr);
	}

	goto loop;
}

/********************************************************************//**
Loads a table column definition from a SYS_COLUMNS record to
dict_table_t.
@return error message, or NULL on success */
UNIV_INTERN
const char*
dict_load_column_low(
/*=================*/
	dict_table_t*	table,		/*!< in/out: table, could be NULL
					if we just populate a dict_column_t
					struct with information from
					a SYS_COLUMNS record */
	mem_heap_t*	heap,		/*!< in/out: memory heap
					for temporary storage */
	dict_col_t*	column,		/*!< out: dict_column_t to fill,
					or NULL if table != NULL */
	table_id_t*	table_id,	/*!< out: table id */
	const char**	col_name,	/*!< out: column name */
	const rec_t*	rec)		/*!< in: SYS_COLUMNS record */
{
	char*		name;
	const byte*	field;
	ulint		len;
	ulint		mtype;
	ulint		prtype;
	ulint		col_len;
	ulint		pos;

	ut_ad(table || column);

	if (rec_get_deleted_flag(rec, 0)) {
		return("delete-marked record in SYS_COLUMNS");
	}

	if (rec_get_n_fields_old(rec) != DICT_NUM_FIELDS__SYS_COLUMNS) {
		return("wrong number of columns in SYS_COLUMNS record");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_COLUMNS__TABLE_ID, &len);
	if (len != 8) {
err_len:
		return("incorrect column length in SYS_COLUMNS");
	}

	if (table_id) {
		*table_id = mach_read_from_8(field);
	} else if (table->id != mach_read_from_8(field)) {
		return("SYS_COLUMNS.TABLE_ID mismatch");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_COLUMNS__POS, &len);
	if (len != 4) {

		goto err_len;
	}

	pos = mach_read_from_4(field);

	if (table && table->n_def != pos) {
		return("SYS_COLUMNS.POS mismatch");
	}

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_COLUMNS__DB_TRX_ID, &len);
	if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}
	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_COLUMNS__DB_ROLL_PTR, &len);
	if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_COLUMNS__NAME, &len);
	if (len < 1 || len == UNIV_SQL_NULL) {
		goto err_len;
	}

	name = mem_heap_strdupl(heap, (const char*) field, len);

	if (col_name) {
		*col_name = name;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_COLUMNS__MTYPE, &len);
	if (len != 4) {
		goto err_len;
	}

	mtype = mach_read_from_4(field);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_COLUMNS__PRTYPE, &len);
	if (len != 4) {
		goto err_len;
	}
	prtype = mach_read_from_4(field);

	if (dtype_get_charset_coll(prtype) == 0
	    && dtype_is_string_type(mtype)) {
		/* The table was created with < 4.1.2. */

		if (dtype_is_binary_string_type(mtype, prtype)) {
			/* Use the binary collation for
			string columns of binary type. */

			prtype = dtype_form_prtype(
				prtype,
				DATA_MYSQL_BINARY_CHARSET_COLL);
		} else {
			/* Use the default charset for
			other than binary columns. */

			prtype = dtype_form_prtype(
				prtype,
				data_mysql_default_charset_coll);
		}
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_COLUMNS__LEN, &len);
	if (len != 4) {
		goto err_len;
	}
	col_len = mach_read_from_4(field);
	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_COLUMNS__PREC, &len);
	if (len != 4) {
		goto err_len;
	}

	if (!column) {
		dict_mem_table_add_col(table, heap, name, mtype,
				       prtype, col_len);
	} else {
		dict_mem_fill_column_struct(column, pos, mtype,
					    prtype, col_len);
	}

	return(NULL);
}

/********************************************************************//**
Loads definitions for table columns. */
static
void
dict_load_columns(
/*==============*/
	dict_table_t*	table,	/*!< in/out: table */
	mem_heap_t*	heap)	/*!< in/out: memory heap
				for temporary storage */
{
	dict_table_t*	sys_columns;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	const rec_t*	rec;
	byte*		buf;
	ulint		i;
	mtr_t		mtr;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	mtr_start(&mtr);

	sys_columns = dict_table_get_low("SYS_COLUMNS");
	sys_index = UT_LIST_GET_FIRST(sys_columns->indexes);
	ut_ad(!dict_table_is_comp(sys_columns));

	ut_ad(name_of_col_is(sys_columns, sys_index,
			     DICT_FLD__SYS_COLUMNS__NAME, "NAME"));
	ut_ad(name_of_col_is(sys_columns, sys_index,
			     DICT_FLD__SYS_COLUMNS__PREC, "PREC"));

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	buf = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(buf, table->id);

	dfield_set_data(dfield, buf, 8);
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
	for (i = 0; i + DATA_N_SYS_COLS < (ulint) table->n_cols; i++) {
		const char*	err_msg;
		const char*	name;

		rec = btr_pcur_get_rec(&pcur);

		ut_a(btr_pcur_is_on_user_rec(&pcur));

		err_msg = dict_load_column_low(table, heap, NULL, NULL,
					       &name, rec);

		/* Note: Currently we have one DOC_ID column that is
		shared by all FTS indexes on a table. */
		if (innobase_strcasecmp(name,
					FTS_DOC_ID_COL_NAME) == 0) {
			dict_col_t*	col;
			/* As part of normal loading of tables the
			table->flag is not set for tables with FTS
			till after the FTS indexes are loaded. So we
			create the fts_t instance here if there isn't
			one already created.

			This case does not arise for table create as
			the flag is set before the table is created. */
			if (table->fts == NULL) {
				table->fts = fts_create(table);
				fts_optimize_add_table(table);
			}

			ut_a(table->fts->doc_col == ULINT_UNDEFINED);

			col = dict_table_get_nth_col(table, i);

			ut_ad(col->len == sizeof(doc_id_t));

			if (col->prtype & DATA_FTS_DOC_ID) {
				DICT_TF2_FLAG_SET(
					table, DICT_TF2_FTS_HAS_DOC_ID);
				DICT_TF2_FLAG_UNSET(
					table, DICT_TF2_FTS_ADD_DOC_ID);
			}

			table->fts->doc_col = i;
		}

		if (err_msg) {
			fprintf(stderr, "InnoDB: %s\n", err_msg);
			ut_error;
		}

		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
}

/** Error message for a delete-marked record in dict_load_field_low() */
static const char* dict_load_field_del = "delete-marked record in SYS_FIELDS";

/********************************************************************//**
Loads an index field definition from a SYS_FIELDS record to
dict_index_t.
@return error message, or NULL on success */
UNIV_INTERN
const char*
dict_load_field_low(
/*================*/
	byte*		index_id,	/*!< in/out: index id (8 bytes)
					an "in" value if index != NULL
					and "out" if index == NULL */
	dict_index_t*	index,		/*!< in/out: index, could be NULL
					if we just populate a dict_field_t
					struct with information from
					a SYS_FIELDS record */
	dict_field_t*	sys_field,	/*!< out: dict_field_t to be
					filled */
	ulint*		pos,		/*!< out: Field position */
	byte*		last_index_id,	/*!< in: last index id */
	mem_heap_t*	heap,		/*!< in/out: memory heap
					for temporary storage */
	const rec_t*	rec)		/*!< in: SYS_FIELDS record */
{
	const byte*	field;
	ulint		len;
	ulint		pos_and_prefix_len;
	ulint		prefix_len;
	ibool		first_field;
	ulint		position;

	/* Either index or sys_field is supplied, not both */
	ut_a((!index) || (!sys_field));

	if (rec_get_deleted_flag(rec, 0)) {
		return(dict_load_field_del);
	}

	if (rec_get_n_fields_old(rec) != DICT_NUM_FIELDS__SYS_FIELDS) {
		return("wrong number of columns in SYS_FIELDS record");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FIELDS__INDEX_ID, &len);
	if (len != 8) {
err_len:
		return("incorrect column length in SYS_FIELDS");
	}

	if (!index) {
		ut_a(last_index_id);
		memcpy(index_id, (const char*) field, 8);
		first_field = memcmp(index_id, last_index_id, 8);
	} else {
		first_field = (index->n_def == 0);
		if (memcmp(field, index_id, 8)) {
			return("SYS_FIELDS.INDEX_ID mismatch");
		}
	}

	/* The next field stores the field position in the index and a
	possible column prefix length if the index field does not
	contain the whole column. The storage format is like this: if
	there is at least one prefix field in the index, then the HIGH
	2 bytes contain the field number (index->n_def) and the low 2
	bytes the prefix length for the field. Otherwise the field
	number (index->n_def) is contained in the 2 LOW bytes. */

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FIELDS__POS, &len);
	if (len != 4) {
		goto err_len;
	}

	pos_and_prefix_len = mach_read_from_4(field);

	if (index && UNIV_UNLIKELY
	    ((pos_and_prefix_len & 0xFFFFUL) != index->n_def
	     && (pos_and_prefix_len >> 16 & 0xFFFF) != index->n_def)) {
		return("SYS_FIELDS.POS mismatch");
	}

	if (first_field || pos_and_prefix_len > 0xFFFFUL) {
		prefix_len = pos_and_prefix_len & 0xFFFFUL;
		position = (pos_and_prefix_len & 0xFFFF0000UL)  >> 16;
	} else {
		prefix_len = 0;
		position = pos_and_prefix_len & 0xFFFFUL;
	}

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_FIELDS__DB_TRX_ID, &len);
	if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}
	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_FIELDS__DB_ROLL_PTR, &len);
	if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FIELDS__COL_NAME, &len);
	if (len < 1 || len == UNIV_SQL_NULL) {
		goto err_len;
	}

	if (index) {
		dict_mem_index_add_field(
			index, mem_heap_strdupl(heap, (const char*) field, len),
			prefix_len);
	} else {
		ut_a(sys_field);
		ut_a(pos);

		sys_field->name = mem_heap_strdupl(
			heap, (const char*) field, len);
		sys_field->prefix_len = prefix_len;
		*pos = position;
	}

	return(NULL);
}

/********************************************************************//**
Loads definitions for index fields.
@return DB_SUCCESS if ok, DB_CORRUPTION if corruption */
static
ulint
dict_load_fields(
/*=============*/
	dict_index_t*	index,	/*!< in/out: index whose fields to load */
	mem_heap_t*	heap)	/*!< in: memory heap for temporary storage */
{
	dict_table_t*	sys_fields;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	const rec_t*	rec;
	byte*		buf;
	ulint		i;
	mtr_t		mtr;
	ulint		error;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	mtr_start(&mtr);

	sys_fields = dict_table_get_low("SYS_FIELDS");
	sys_index = UT_LIST_GET_FIRST(sys_fields->indexes);
	ut_ad(!dict_table_is_comp(sys_fields));
	ut_ad(name_of_col_is(sys_fields, sys_index,
			     DICT_FLD__SYS_FIELDS__COL_NAME, "COL_NAME"));

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	buf = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(buf, index->id);

	dfield_set_data(dfield, buf, 8);
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
	for (i = 0; i < index->n_fields; i++) {
		const char* err_msg;

		rec = btr_pcur_get_rec(&pcur);

		ut_a(btr_pcur_is_on_user_rec(&pcur));

		err_msg = dict_load_field_low(buf, index, NULL, NULL, NULL,
					      heap, rec);

		if (err_msg == dict_load_field_del) {
			/* There could be delete marked records in
			SYS_FIELDS because SYS_FIELDS.INDEX_ID can be
			updated by ALTER TABLE ADD INDEX. */

			goto next_rec;
		} else if (err_msg) {
			fprintf(stderr, "InnoDB: %s\n", err_msg);
			error = DB_CORRUPTION;
			goto func_exit;
		}
next_rec:
		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	}

	error = DB_SUCCESS;
func_exit:
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	return(error);
}

/** Error message for a delete-marked record in dict_load_index_low() */
static const char* dict_load_index_del = "delete-marked record in SYS_INDEXES";
/** Error message for table->id mismatch in dict_load_index_low() */
static const char* dict_load_index_id_err = "SYS_INDEXES.TABLE_ID mismatch";

/********************************************************************//**
Loads an index definition from a SYS_INDEXES record to dict_index_t.
If allocate=TRUE, we will create a dict_index_t structure and fill it
accordingly. If allocated=FALSE, the dict_index_t will be supplied by
the caller and filled with information read from the record.  @return
error message, or NULL on success */
UNIV_INTERN
const char*
dict_load_index_low(
/*================*/
	byte*		table_id,	/*!< in/out: table id (8 bytes),
					an "in" value if allocate=TRUE
					and "out" when allocate=FALSE */
	const char*	table_name,	/*!< in: table name */
	mem_heap_t*	heap,		/*!< in/out: temporary memory heap */
	const rec_t*	rec,		/*!< in: SYS_INDEXES record */
	ibool		allocate,	/*!< in: TRUE=allocate *index,
					FALSE=fill in a pre-allocated
					*index */
	dict_index_t**	index)		/*!< out,own: index, or NULL */
{
	const byte*	field;
	ulint		len;
	ulint		name_len;
	char*		name_buf;
	index_id_t	id;
	ulint		n_fields;
	ulint		type;
	ulint		space;

	if (allocate) {
		/* If allocate=TRUE, no dict_index_t will
		be supplied. Initialize "*index" to NULL */
		*index = NULL;
	}

	if (rec_get_deleted_flag(rec, 0)) {
		return(dict_load_index_del);
	}

	if (rec_get_n_fields_old(rec) != DICT_NUM_FIELDS__SYS_INDEXES) {
		return("wrong number of columns in SYS_INDEXES record");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__TABLE_ID, &len);
	if (len != 8) {
err_len:
		return("incorrect column length in SYS_INDEXES");
	}

	if (!allocate) {
		/* We are reading a SYS_INDEXES record. Copy the table_id */
		memcpy(table_id, (const char*) field, 8);
	} else if (memcmp(field, table_id, 8)) {
		/* Caller supplied table_id, verify it is the same
		id as on the index record */
		return(dict_load_index_id_err);
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__ID, &len);
	if (len != 8) {
		goto err_len;
	}

	id = mach_read_from_8(field);

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_INDEXES__DB_TRX_ID, &len);
	if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}
	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_INDEXES__DB_ROLL_PTR, &len);
	if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__NAME, &name_len);
	if (name_len == UNIV_SQL_NULL) {
		goto err_len;
	}

	name_buf = mem_heap_strdupl(heap, (const char*) field,
				    name_len);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__N_FIELDS, &len);
	if (len != 4) {
		goto err_len;
	}
	n_fields = mach_read_from_4(field);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__TYPE, &len);
	if (len != 4) {
		goto err_len;
	}
	type = mach_read_from_4(field);
	if (type & (~0 << DICT_IT_BITS)) {
		return("unknown SYS_INDEXES.TYPE bits");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__SPACE, &len);
	if (len != 4) {
		goto err_len;
	}
	space = mach_read_from_4(field);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__PAGE_NO, &len);
	if (len != 4) {
		goto err_len;
	}

	if (allocate) {
		*index = dict_mem_index_create(table_name, name_buf,
					       space, type, n_fields);
	} else {
		ut_a(*index);

		dict_mem_fill_index_struct(*index, NULL, NULL, name_buf,
					   space, type, n_fields);
	}

	(*index)->id = id;
	(*index)->page = mach_read_from_4(field);
	ut_ad((*index)->page);

	return(NULL);
}

/********************************************************************//**
Loads definitions for table indexes. Adds them to the data dictionary
cache.
@return DB_SUCCESS if ok, DB_CORRUPTION if corruption of dictionary
table or DB_UNSUPPORTED if table has unknown index type */
static
ulint
dict_load_indexes(
/*==============*/
	dict_table_t*	table,	/*!< in/out: table */
	mem_heap_t*	heap,	/*!< in: memory heap for temporary storage */
	dict_err_ignore_t ignore_err)
				/*!< in: error to be ignored when
				loading the index definition */
{
	dict_table_t*	sys_indexes;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	const rec_t*	rec;
	byte*		buf;
	mtr_t		mtr;
	ulint		error = DB_SUCCESS;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	mtr_start(&mtr);

	sys_indexes = dict_table_get_low("SYS_INDEXES");
	sys_index = UT_LIST_GET_FIRST(sys_indexes->indexes);
	ut_ad(!dict_table_is_comp(sys_indexes));
	ut_ad(name_of_col_is(sys_indexes, sys_index,
			     DICT_FLD__SYS_INDEXES__NAME, "NAME"));
	ut_ad(name_of_col_is(sys_indexes, sys_index,
			     DICT_FLD__SYS_INDEXES__PAGE_NO, "PAGE_NO"));

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	buf = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(buf, table->id);

	dfield_set_data(dfield, buf, 8);
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
	for (;;) {
		dict_index_t*	index = NULL;
		const char*	err_msg;

		if (!btr_pcur_is_on_user_rec(&pcur)) {

			break;
		}

		rec = btr_pcur_get_rec(&pcur);

		err_msg = dict_load_index_low(buf, table->name, heap, rec,
					      TRUE, &index);
		ut_ad((index == NULL && err_msg != NULL)
		      || (index != NULL && err_msg == NULL));

		if (err_msg == dict_load_index_id_err) {
			/* TABLE_ID mismatch means that we have
			run out of index definitions for the table. */
			break;
		} else if (err_msg == dict_load_index_del) {
			/* Skip delete-marked records. */
			goto next_rec;
		} else if (err_msg) {
			fprintf(stderr, "InnoDB: %s\n", err_msg);
			if (ignore_err & DICT_ERR_IGNORE_CORRUPT) {
				goto next_rec;
			}
			error = DB_CORRUPTION;
			goto func_exit;
		}

		ut_ad(index);

		/* Check whether the index is corrupted */
		if (dict_index_is_corrupted(index)) {
			ut_print_timestamp(stderr);
			fputs("  InnoDB: ", stderr);
			dict_index_name_print(stderr, NULL, index);
			fputs(" is corrupted\n", stderr);

			if (!srv_load_corrupted
			    && !(ignore_err & DICT_ERR_IGNORE_CORRUPT)
			    && dict_index_is_clust(index)) {
				dict_mem_index_free(index);

				error = DB_INDEX_CORRUPT;
				goto func_exit;
			} else {
				/* We will load the index if
				1) srv_load_corrupted is TRUE
				2) ignore_err is set with
				DICT_ERR_IGNORE_CORRUPT
				3) if the index corrupted is a secondary
				index */
				ut_print_timestamp(stderr);
				fputs("  InnoDB: load corrupted index ", stderr);
				dict_index_name_print(stderr, NULL, index);
				putc('\n', stderr);
			}
		}

		if (index->type & DICT_FTS
		    && !DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS)) {
			/* This should have been created by now. */
			ut_a(table->fts != NULL);
			DICT_TF2_FLAG_SET(table, DICT_TF2_FTS);
		}

		/* We check for unsupported types first, so that the
		subsequent checks are relevant for the supported types. */
		if (index->type & ~(DICT_CLUSTERED | DICT_UNIQUE
				    | DICT_CORRUPT | DICT_FTS)) {
			fprintf(stderr,
				"InnoDB: Error: unknown type %lu"
				" of index %s of table %s\n",
				(ulong) index->type, index->name, table->name);

			error = DB_UNSUPPORTED;
			dict_mem_index_free(index);
			goto func_exit;
		} else if (index->page == FIL_NULL
			   && (!(index->type & DICT_FTS))) {

			fprintf(stderr,
				"InnoDB: Error: trying to load index %s"
				" for table %s\n"
				"InnoDB: but the index tree has been freed!\n",
				index->name, table->name);

			if (ignore_err & DICT_ERR_IGNORE_INDEX_ROOT) {
				/* If caller can tolerate this error,
				we will continue to load the index and
				let caller deal with this error. However
				mark the index and table corrupted. We
				only need to mark such in the index
				dictionary cache for such metadata corruption,
				since we would always be able to set it
				when loading the dictionary cache */
				dict_set_corrupted_index_cache_only(
					index, table);

				fprintf(stderr,
					"InnoDB: Index is corrupt but forcing"
					" load into data dictionary\n");
			} else {
corrupted:
				dict_mem_index_free(index);
				error = DB_CORRUPTION;
				goto func_exit;
			}
		} else if (!dict_index_is_clust(index)
			   && NULL == dict_table_get_first_index(table)) {

			fputs("InnoDB: Error: trying to load index ",
			      stderr);
			ut_print_name(stderr, NULL, FALSE, index->name);
			fputs(" for table ", stderr);
			ut_print_name(stderr, NULL, TRUE, table->name);
			fputs("\nInnoDB: but the first index"
			      " is not clustered!\n", stderr);

			goto corrupted;
		} else if (table->id < DICT_HDR_FIRST_ID
			   && (dict_index_is_clust(index)
			       || ((table == dict_sys->sys_tables)
				   && !strcmp("ID_IND", index->name)))) {

			/* The index was created in memory already at booting
			of the database server */
			dict_mem_index_free(index);
		} else {
			dict_load_fields(index, heap);
			error = dict_index_add_to_cache(table, index,
							index->page, FALSE);
			/* The data dictionary tables should never contain
			invalid index definitions.  If we ignored this error
			and simply did not load this index definition, the
			.frm file would disagree with the index definitions
			inside InnoDB. */
			if (UNIV_UNLIKELY(error != DB_SUCCESS)) {

				goto func_exit;
			}
		}
next_rec:
		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	}

	/* If the table contains FTS indexes, populate table->fts->indexes */
	if (DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS)) {
		/* table->fts->indexes should have been created. */
		ut_a(table->fts->indexes != NULL);
		dict_table_get_all_fts_indexes(table, table->fts->indexes);
	}

func_exit:
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	return(error);
}

/********************************************************************//**
Loads a table definition from a SYS_TABLES record to dict_table_t.
Does not load any columns or indexes.
@return error message, or NULL on success */
UNIV_INTERN
const char*
dict_load_table_low(
/*================*/
	const char*	name,		/*!< in: table name */
	const rec_t*	rec,		/*!< in: SYS_TABLES record */
	dict_table_t**	table)		/*!< out,own: table, or NULL */
{
	const byte*	field;
	ulint		len;
	ulint		space;
	ulint		n_cols;
	ulint		flags = 0;
	ulint		flags2;

	if (rec_get_deleted_flag(rec, 0)) {
		return("delete-marked record in SYS_TABLES");
	}

	if (rec_get_n_fields_old(rec) != DICT_NUM_FIELDS__SYS_TABLES) {
		return("wrong number of columns in SYS_TABLES record");
	}

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_TABLES__NAME, &len);
	if (len < 1 || len == UNIV_SQL_NULL) {
err_len:
		return("incorrect column length in SYS_TABLES");
	}
	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_TABLES__DB_TRX_ID, &len);
	if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}
	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_TABLES__DB_ROLL_PTR, &len);
	if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	rec_get_nth_field_offs_old(rec, DICT_FLD__SYS_TABLES__ID, &len);
	if (len != 8) {
		goto err_len;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__N_COLS, &len);
	if (len != 4) {
		goto err_len;
	}

	n_cols = mach_read_from_4(field);

	rec_get_nth_field_offs_old(rec, DICT_FLD__SYS_TABLES__TYPE, &len);
	if (len != 4) {
		goto err_len;
	}

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_TABLES__MIX_ID, &len);
	if (len != 8) {
		goto err_len;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__MIX_LEN, &len);
	if (len != 4) {
		goto err_len;
	}

	/* MIX_LEN may hold additional flags in post-antelope file formats. */
	flags2 = mach_read_from_4(field);

	/* DICT_TF2_FTS will be set when indexes is being loaded */
	flags2 &= ~DICT_TF2_FTS;

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_TABLES__CLUSTER_ID, &len);
	if (len != UNIV_SQL_NULL) {
		goto err_len;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__SPACE, &len);
	if (len != 4) {
		goto err_len;
	}

	space = mach_read_from_4(field);

	/* Check if the tablespace exists and has the right name */
	flags = dict_sys_tables_get_flags(rec);

	if (UNIV_UNLIKELY(flags == ULINT_UNDEFINED)) {
		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_TABLES__TYPE, &len);
		ut_ad(len == 4); /* this was checked earlier */
		flags = mach_read_from_4(field);

		ut_print_timestamp(stderr);
		fputs("  InnoDB: Error: table ", stderr);
		ut_print_filename(stderr, name);
		fprintf(stderr, "\n"
			"InnoDB: in InnoDB data dictionary"
			" has unknown type %lx.\n",
			(ulong) flags);
		return("incorrect flags in SYS_TABLES");
	}

	/* The high-order bit of N_COLS is the "compact format" flag.
	For tables in that format, MIX_LEN may hold additional flags. */
	if (n_cols & DICT_N_COLS_COMPACT) {
		ut_ad(flags & DICT_TF_COMPACT);

		if (flags2 & ~DICT_TF2_BIT_MASK) {
			ut_print_timestamp(stderr);
			fputs("  InnoDB: Warning: table ", stderr);
			ut_print_filename(stderr, name);
			fprintf(stderr, "\n"
				"InnoDB: in InnoDB data dictionary"
				" has unknown flags %lx.\n",
				(ulong) flags2);

			/* Clean it up and keep going */
			flags2 &= DICT_TF2_BIT_MASK;
		}
	} else {
		/* Do not trust the MIX_LEN field when the
		row format is Redundant. */
		flags2 = 0;
	}

	/* See if the tablespace is available. */
	*table = dict_mem_table_create(
		name, space, n_cols & ~DICT_N_COLS_COMPACT, flags, flags2);

	field = rec_get_nth_field_old(rec, DICT_FLD__SYS_TABLES__ID, &len);
	ut_ad(len == 8); /* this was checked earlier */

	(*table)->id = mach_read_from_8(field);

	(*table)->ibd_file_missing = FALSE;

	return(NULL);
}

/********************************************************************//**
Loads a table definition and also all its index definitions, and also
the cluster definition if the table is a member in a cluster. Also loads
all foreign key constraints where the foreign key is in the table or where
a foreign key references columns in this table. Adds all these to the data
dictionary cache.
@return table, NULL if does not exist; if the table is stored in an
.ibd file, but the file does not exist, then we set the
ibd_file_missing flag TRUE in the table object we return */
UNIV_INTERN
dict_table_t*
dict_load_table(
/*============*/
	const char*	name,	/*!< in: table name in the
				databasename/tablename format */
	ibool		cached,	/*!< in: TRUE=add to cache, FALSE=do not */
	dict_err_ignore_t ignore_err)
				/*!< in: error to be ignored when loading
				table and its indexes' definition */
{
	dict_table_t*	table;
	dict_table_t*	sys_tables;
	btr_pcur_t	pcur;
	dict_index_t*	sys_index;
	dtuple_t*	tuple;
	mem_heap_t*	heap;
	dfield_t*	dfield;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	ulint		err;
	const char*	err_msg;
	mtr_t		mtr;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	heap = mem_heap_create(32000);

	mtr_start(&mtr);

	sys_tables = dict_table_get_low("SYS_TABLES");
	sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);
	ut_ad(!dict_table_is_comp(sys_tables));
	ut_ad(name_of_col_is(sys_tables, sys_index,
			     DICT_FLD__SYS_TABLES__ID, "ID"));
	ut_ad(name_of_col_is(sys_tables, sys_index,
			     DICT_FLD__SYS_TABLES__N_COLS, "N_COLS"));
	ut_ad(name_of_col_is(sys_tables, sys_index,
			     DICT_FLD__SYS_TABLES__TYPE, "TYPE"));
	ut_ad(name_of_col_is(sys_tables, sys_index,
			     DICT_FLD__SYS_TABLES__MIX_LEN, "MIX_LEN"));
	ut_ad(name_of_col_is(sys_tables, sys_index,
			     DICT_FLD__SYS_TABLES__SPACE, "SPACE"));

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, name, ut_strlen(name));
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur)
	    || rec_get_deleted_flag(rec, 0)) {
		/* Not found */
err_exit:
		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);

		return(NULL);
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__NAME, &len);

	/* Check if the table name in record is the searched one */
	if (len != ut_strlen(name) || ut_memcmp(name, field, len) != 0) {

		goto err_exit;
	}

	err_msg = dict_load_table_low(name, rec, &table);

	if (err_msg) {

		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB: %s\n", err_msg);
		goto err_exit;
	}

	if (table->space == 0) {
		/* The system tablespace is always available. */
	} else if (!fil_space_for_table_exists_in_mem(
			table->space, name, FALSE, FALSE)) {

		if (table->flags2 & DICT_TF2_TEMPORARY) {
			/* Do not bother to retry opening temporary tables. */
			table->ibd_file_missing = TRUE;
		} else {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: error: space object of table ");
			ut_print_filename(stderr, name);
			fprintf(stderr, ",\n"
				"InnoDB: space id %lu did not exist in memory."
				" Retrying an open.\n",
				(ulong) table->space);
			/* Try to open the tablespace */
			if (!fil_open_single_table_tablespace(
				TRUE, table->space,
				dict_tf_to_fsp_flags(table->flags),
				name)) {
				/* We failed to find a sensible
				tablespace file */

				table->ibd_file_missing = TRUE;
			}
		}
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	dict_load_columns(table, heap);

	if (cached) {
		dict_table_add_to_cache(table, TRUE, heap);
	} else {
		dict_table_add_system_columns(table, heap);
	}

	mem_heap_empty(heap);

	err = dict_load_indexes(table, heap, ignore_err);

	if (err == DB_INDEX_CORRUPT) {
		/* Refuse to load the table if the table has a corrupted
		cluster index */
		if (!srv_load_corrupted) {
			fprintf(stderr, "InnoDB: Error: Load table ");
			ut_print_name(stderr, NULL, TRUE, table->name);
			fprintf(stderr, " failed, the table has corrupted"
					" clustered indexes. Turn on"
					" 'innodb_force_load_corrupted'"
					" to drop it\n");

			dict_table_remove_from_cache(table);
			table = NULL;
			goto func_exit;
		} else {
			dict_index_t*	clust_index;
			clust_index = dict_table_get_first_index(table);

			if (dict_index_is_corrupted(clust_index)) {
				table->corrupted = TRUE;
			}
		}
	}

	/* Initialize table foreign_child value. Its value could be
	changed when dict_load_foreigns() is called below */
	table->fk_max_recusive_level = 0;

	/* If the force recovery flag is set, we open the table irrespective
	of the error condition, since the user may want to dump data from the
	clustered index. However we load the foreign key information only if
	all indexes were loaded. */
	if (!cached) {
	} else if (err == DB_SUCCESS) {
		err = dict_load_foreigns(table->name, TRUE, TRUE);

		if (err != DB_SUCCESS) {
			dict_table_remove_from_cache(table);
			table = NULL;
		} else {
			table->fk_max_recusive_level = 0;
		}
	} else {
		dict_index_t*   index;

		/* Make sure that at least the clustered index was loaded.
		Otherwise refuse to load the table */
		index = dict_table_get_first_index(table);

		if (!srv_force_recovery || !index
		    || !dict_index_is_clust(index)) {
			dict_table_remove_from_cache(table);
			table = NULL;
		} else if (dict_index_is_corrupted(index)) {

			/* It is possible we force to load a corrupted
			clustered index if srv_load_corrupted is set.
			Mark the table as corrupted in this case */
			table->corrupted = TRUE;
		}
	}
#if 0
	if (err != DB_SUCCESS && table != NULL) {

		mutex_enter(&dict_foreign_err_mutex);

		ut_print_timestamp(stderr);

		fprintf(stderr,
			"  InnoDB: Error: could not make a foreign key"
			" definition to match\n"
			"InnoDB: the foreign key table"
			" or the referenced table!\n"
			"InnoDB: The data dictionary of InnoDB is corrupt."
			" You may need to drop\n"
			"InnoDB: and recreate the foreign key table"
			" or the referenced table.\n"
			"InnoDB: Submit a detailed bug report"
			" to http://bugs.mysql.com\n"
			"InnoDB: Latest foreign key error printout:\n%s\n",
			dict_foreign_err_buf);

		mutex_exit(&dict_foreign_err_mutex);
	}
#endif /* 0 */
func_exit:
	mem_heap_free(heap);

	ut_ad(!table || ignore_err != DICT_ERR_IGNORE_NONE
	      || !table->corrupted);

	return(table);
}

/***********************************************************************//**
Loads a table object based on the table id.
@return	table; NULL if table does not exist */
UNIV_INTERN
dict_table_t*
dict_load_table_on_id(
/*==================*/
	table_id_t	table_id)	/*!< in: table id */
{
	byte		id_buf[8];
	btr_pcur_t	pcur;
	mem_heap_t*	heap;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	dict_index_t*	sys_table_ids;
	dict_table_t*	sys_tables;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	dict_table_t*	table;
	mtr_t		mtr;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	table = NULL;

	/* NOTE that the operation of this function is protected by
	the dictionary mutex, and therefore no deadlocks can occur
	with other dictionary operations. */

	mtr_start(&mtr);
	/*---------------------------------------------------*/
	/* Get the secondary index based on ID for table SYS_TABLES */
	sys_tables = dict_sys->sys_tables;
	sys_table_ids = dict_table_get_next_index(
		dict_table_get_first_index(sys_tables));
	ut_ad(!dict_table_is_comp(sys_tables));
	heap = mem_heap_create(256);

	tuple  = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	/* Write the table id in byte format to id_buf */
	mach_write_to_8(id_buf, table_id);

	dfield_set_data(dfield, id_buf, 8);
	dict_index_copy_types(tuple, sys_table_ids, 1);

	btr_pcur_open_on_user_rec(sys_table_ids, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);

check_rec:
	rec = btr_pcur_get_rec(&pcur);

	if (page_rec_is_user_rec(rec)) {
		/*---------------------------------------------------*/
		/* Now we have the record in the secondary index
		containing the table ID and NAME */

		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_TABLE_IDS__ID, &len);
		ut_ad(len == 8);

		/* Check if the table id in record is the one searched for */
		if (table_id == mach_read_from_8(field)) {
			if (rec_get_deleted_flag(rec, 0)) {
				/* Until purge has completed, there
				may be delete-marked duplicate records
				for the same SYS_TABLES.ID.
				Due to Bug #60049, some delete-marked
				records may survive the purge forever. */
				if (btr_pcur_move_to_next(&pcur, &mtr)) {

					goto check_rec;
				}
			} else {
				/* Now we get the table name from the record */
				field = rec_get_nth_field_old(rec,
					DICT_FLD__SYS_TABLE_IDS__NAME, &len);
				/* Load the table definition to memory */
				table = dict_load_table(
					mem_heap_strdupl(
						heap, (char*) field, len),
					TRUE, DICT_ERR_IGNORE_NONE);
			}
		}
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	mem_heap_free(heap);

	return(table);
}

/********************************************************************//**
This function is called when the database is booted. Loads system table
index definitions except for the clustered index which is added to the
dictionary cache at booting before calling this function. */
UNIV_INTERN
void
dict_load_sys_table(
/*================*/
	dict_table_t*	table)	/*!< in: system table */
{
	mem_heap_t*	heap;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	heap = mem_heap_create(1000);

	dict_load_indexes(table, heap, DICT_ERR_IGNORE_NONE);

	mem_heap_free(heap);
}

/********************************************************************//**
Loads foreign key constraint col names (also for the referenced table). */
static
void
dict_load_foreign_cols(
/*===================*/
	const char*	id,	/*!< in: foreign constraint id, not
				necessary '\0'-terminated */
	ulint		id_len,	/*!< in: id length */
	dict_foreign_t*	foreign)/*!< in: foreign constraint object */
{
	dict_table_t*	sys_foreign_cols;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	ulint		i;
	mtr_t		mtr;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	foreign->foreign_col_names = static_cast<const char**>(
		mem_heap_alloc(foreign->heap,
			       foreign->n_fields * sizeof(void*)));

	foreign->referenced_col_names = static_cast<const char**>(
		mem_heap_alloc(foreign->heap,
			       foreign->n_fields * sizeof(void*)));

	mtr_start(&mtr);

	sys_foreign_cols = dict_table_get_low("SYS_FOREIGN_COLS");

	sys_index = UT_LIST_GET_FIRST(sys_foreign_cols->indexes);
	ut_ad(!dict_table_is_comp(sys_foreign_cols));

	tuple = dtuple_create(foreign->heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, id, id_len);
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
	for (i = 0; i < foreign->n_fields; i++) {

		rec = btr_pcur_get_rec(&pcur);

		ut_a(btr_pcur_is_on_user_rec(&pcur));
		ut_a(!rec_get_deleted_flag(rec, 0));

		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_FOREIGN_COLS__ID, &len);
		ut_a(len == id_len);
		ut_a(ut_memcmp(id, field, len) == 0);

		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_FOREIGN_COLS__POS, &len);
		ut_a(len == 4);
		ut_a(i == mach_read_from_4(field));

		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_FOREIGN_COLS__FOR_COL_NAME, &len);
		foreign->foreign_col_names[i] = mem_heap_strdupl(
			foreign->heap, (char*) field, len);

		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_FOREIGN_COLS__REF_COL_NAME, &len);
		foreign->referenced_col_names[i] = mem_heap_strdupl(
			foreign->heap, (char*) field, len);

		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
}

/***********************************************************************//**
Loads a foreign key constraint to the dictionary cache.
@return	DB_SUCCESS or error code */
static
ulint
dict_load_foreign(
/*==============*/
	const char*	id,	/*!< in: foreign constraint id, not
				necessary '\0'-terminated */
	ulint		id_len,	/*!< in: id length */
	ibool		check_charsets,
				/*!< in: TRUE=check charset compatibility */
	ibool		check_recursive)
				/*!< in: Whether to record the foreign table
				parent count to avoid unlimited recursive
				load of chained foreign tables */
{
	dict_foreign_t*	foreign;
	dict_table_t*	sys_foreign;
	btr_pcur_t	pcur;
	dict_index_t*	sys_index;
	dtuple_t*	tuple;
	mem_heap_t*	heap2;
	dfield_t*	dfield;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	ulint		n_fields_and_type;
	mtr_t		mtr;
	dict_table_t*	for_table;
	dict_table_t*	ref_table;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	heap2 = mem_heap_create(1000);

	mtr_start(&mtr);

	sys_foreign = dict_table_get_low("SYS_FOREIGN");

	sys_index = UT_LIST_GET_FIRST(sys_foreign->indexes);
	ut_ad(!dict_table_is_comp(sys_foreign));

	tuple = dtuple_create(heap2, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, id, id_len);
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur)
	    || rec_get_deleted_flag(rec, 0)) {
		/* Not found */

		fprintf(stderr,
			"InnoDB: Error: cannot load foreign constraint "
			"%.*s: could not find the relevant record in "
			"SYS_FOREIGN\n", (int) id_len, id);

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap2);

		return(DB_ERROR);
	}

	field = rec_get_nth_field_old(rec, DICT_FLD__SYS_FOREIGN__ID, &len);

	/* Check if the id in record is the searched one */
	if (len != id_len || ut_memcmp(id, field, len) != 0) {

		fprintf(stderr,
			"InnoDB: Error: cannot load foreign constraint "
			"%.*s: found %.*s instead in SYS_FOREIGN\n",
			(int) id_len, id, (int) len, field);

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap2);

		return(DB_ERROR);
	}

	/* Read the table names and the number of columns associated
	with the constraint */

	mem_heap_free(heap2);

	foreign = dict_mem_foreign_create();

	n_fields_and_type = mach_read_from_4(
		rec_get_nth_field_old(
			rec, DICT_FLD__SYS_FOREIGN__N_COLS, &len));

	ut_a(len == 4);

	/* We store the type in the bits 24..29 of n_fields_and_type. */

	foreign->type = (unsigned int) (n_fields_and_type >> 24);
	foreign->n_fields = (unsigned int) (n_fields_and_type & 0x3FFUL);

	foreign->id = mem_heap_strdupl(foreign->heap, id, id_len);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN__FOR_NAME, &len);

	foreign->foreign_table_name = mem_heap_strdupl(
		foreign->heap, (char*) field, len);
	dict_mem_foreign_table_name_lookup_set(foreign, TRUE);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN__REF_NAME, &len);
	foreign->referenced_table_name = mem_heap_strdupl(
		foreign->heap, (char*) field, len);
	dict_mem_referenced_table_name_lookup_set(foreign, TRUE);

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	dict_load_foreign_cols(id, id_len, foreign);

	ref_table = dict_table_check_if_in_cache_low(
			foreign->referenced_table_name_lookup);

	/* We could possibly wind up in a deep recursive calls if
	we call dict_table_get_low() again here if there
	is a chain of tables concatenated together with
	foreign constraints. In such case, each table is
	both a parent and child of the other tables, and
	act as a "link" in such table chains.
	To avoid such scenario, we would need to check the
	number of ancesters the current table has. If that
	exceeds DICT_FK_MAX_CHAIN_LEN, we will stop loading
	the child table.
	Foreign constraints are loaded in a Breath First fashion,
	that is, the index on FOR_NAME is scanned first, and then
	index on REF_NAME. So foreign constrains in which
	current table is a child (foreign table) are loaded first,
	and then those constraints where current table is a
	parent (referenced) table.
	Thus we could check the parent (ref_table) table's
	reference count (fk_max_recusive_level) to know how deep the
	recursive call is. If the parent table (ref_table) is already
	loaded, and its fk_max_recusive_level is larger than
	DICT_FK_MAX_CHAIN_LEN, we will stop the recursive loading
	by skipping loading the child table. It will not affect foreign
	constraint check for DMLs since child table will be loaded
	at that time for the constraint check. */
	if (!ref_table
	    || ref_table->fk_max_recusive_level < DICT_FK_MAX_RECURSIVE_LOAD) {

		/* If the foreign table is not yet in the dictionary cache, we
		have to load it so that we are able to make type comparisons
		in the next function call. */

		for_table = dict_table_get_low(foreign->foreign_table_name_lookup);

		if (for_table && ref_table && check_recursive) {
			/* This is to record the longest chain of ancesters
			this table has, if the parent has more ancesters
			than this table has, record it after add 1 (for this
			parent */
			if (ref_table->fk_max_recusive_level
			    >= for_table->fk_max_recusive_level) {
				for_table->fk_max_recusive_level =
					 ref_table->fk_max_recusive_level + 1;
			}
		}
	}

	/* Note that there may already be a foreign constraint object in
	the dictionary cache for this constraint: then the following
	call only sets the pointers in it to point to the appropriate table
	and index objects and frees the newly created object foreign.
	Adding to the cache should always succeed since we are not creating
	a new foreign key constraint but loading one from the data
	dictionary. */

	return(dict_foreign_add_to_cache(foreign, check_charsets));
}

/***********************************************************************//**
Loads foreign key constraints where the table is either the foreign key
holder or where the table is referenced by a foreign key. Adds these
constraints to the data dictionary. Note that we know that the dictionary
cache already contains all constraints where the other relevant table is
already in the dictionary cache.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ulint
dict_load_foreigns(
/*===============*/
	const char*	table_name,	/*!< in: table name */
	ibool		check_recursive,/*!< in: Whether to check recursive
					load of tables chained by FK */
	ibool		check_charsets)	/*!< in: TRUE=check charset
					compatibility */
{
	ulint		tuple_buf[(DTUPLE_EST_ALLOC(1) + sizeof(ulint) - 1)
				/ sizeof(ulint)];
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	dict_index_t*	sec_index;
	dict_table_t*	sys_foreign;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	ulint		err;
	mtr_t		mtr;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	sys_foreign = dict_table_get_low("SYS_FOREIGN");

	if (sys_foreign == NULL) {
		/* No foreign keys defined yet in this database */

		fprintf(stderr,
			"InnoDB: Error: no foreign key system tables"
			" in the database\n");

		return(DB_ERROR);
	}

	ut_ad(!dict_table_is_comp(sys_foreign));
	mtr_start(&mtr);

	/* Get the secondary index based on FOR_NAME from table
	SYS_FOREIGN */

	sec_index = dict_table_get_next_index(
		dict_table_get_first_index(sys_foreign));
start_load:

	tuple = dtuple_create_from_mem(tuple_buf, sizeof(tuple_buf), 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, table_name, ut_strlen(table_name));
	dict_index_copy_types(tuple, sec_index, 1);

	btr_pcur_open_on_user_rec(sec_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
loop:
	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur)) {
		/* End of index */

		goto load_next_index;
	}

	/* Now we have the record in the secondary index containing a table
	name and a foreign constraint ID */

	rec = btr_pcur_get_rec(&pcur);
	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN_FOR_NAME__NAME, &len);

	/* Check if the table name in the record is the one searched for; the
	following call does the comparison in the latin1_swedish_ci
	charset-collation, in a case-insensitive way. */

	if (0 != cmp_data_data(dfield_get_type(dfield)->mtype,
			       dfield_get_type(dfield)->prtype,
			       static_cast<const byte*>(
				       dfield_get_data(dfield)),
			       dfield_get_len(dfield),
			       field, len)) {

		goto load_next_index;
	}

	/* Since table names in SYS_FOREIGN are stored in a case-insensitive
	order, we have to check that the table name matches also in a binary
	string comparison. On Unix, MySQL allows table names that only differ
	in character case.  If lower_case_table_names=2 then what is stored
	may not be the same case, but the previous comparison showed that they
	match with no-case.  */

	if ((innobase_get_lower_case_table_names() != 2)
	    && (0 != ut_memcmp(field, table_name, len))) {
		goto next_rec;
	}

	if (rec_get_deleted_flag(rec, 0)) {

		goto next_rec;
	}

	/* Now we get a foreign key constraint id */
	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN_FOR_NAME__ID, &len);

	btr_pcur_store_position(&pcur, &mtr);

	mtr_commit(&mtr);

	/* Load the foreign constraint definition to the dictionary cache */

	err = dict_load_foreign((char*) field, len, check_charsets,
				check_recursive);

	if (err != DB_SUCCESS) {
		btr_pcur_close(&pcur);

		return(err);
	}

	mtr_start(&mtr);

	btr_pcur_restore_position(BTR_SEARCH_LEAF, &pcur, &mtr);
next_rec:
	btr_pcur_move_to_next_user_rec(&pcur, &mtr);

	goto loop;

load_next_index:
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	sec_index = dict_table_get_next_index(sec_index);

	if (sec_index != NULL) {

		mtr_start(&mtr);

		/* Switch to scan index on REF_NAME, fk_max_recusive_level
		already been updated when scanning FOR_NAME index, no need to
		update again */
		check_recursive = FALSE;

		goto start_load;
	}

	return(DB_SUCCESS);
}
