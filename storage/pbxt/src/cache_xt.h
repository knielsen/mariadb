/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2005-05-24	Paul McCullagh
 *
 * H&G2JCtL
 */
#ifndef __xt_cache_h__
#define __xt_cache_h__

//#define XT_USE_MYSYS

#include "filesys_xt.h"
#include "index_xt.h"

struct XTOpenTable;
struct XTIdxReadBuffer;

#ifdef DEBUG
//#define XT_USE_CACHE_DEBUG_SIZES
#endif

#ifdef XT_USE_CACHE_DEBUG_SIZES
#define XT_INDEX_CACHE_SEGMENT_SHIFTS	1
#else
#define XT_INDEX_CACHE_SEGMENT_SHIFTS	3
#endif

#define IDX_CAC_BLOCK_FREE				0
#define IDX_CAC_BLOCK_CLEAN				1
#define IDX_CAC_BLOCK_DIRTY				2

typedef enum XTPageLockType { XT_LOCK_READ, XT_LOCK_WRITE, XT_XLOCK_LEAF };
typedef enum XTPageUnlockType { XT_UNLOCK_NONE, XT_UNLOCK_READ, XT_UNLOCK_WRITE, XT_UNLOCK_R_UPDATE, XT_UNLOCK_W_UPDATE };

/* A block is X locked if it is being changed or freed.
 * A block is S locked if it is being read.
 */
typedef struct XTIndBlock {
	xtIndexNodeID		cb_address;						/* The block address. */
	u_int				cb_file_id;						/* The file id of the block. */
	/* This is protected by cs_lock */
	struct XTIndBlock	*cb_next;						/* Pointer to next block on hash list, or next free block on free list. */
	/* This is protected by mi_dirty_lock */
	struct XTIndBlock	*cb_dirty_next;					/* Double link for dirty blocks, next pointer. */
	struct XTIndBlock	*cb_dirty_prev;					/* Double link for dirty blocks, previous pointer. */
	/* This is protected by cg_lock */
	xtWord4				cb_ru_time;						/* If this is in the top 1/4 don't change position in MRU list. */
	struct XTIndBlock	*cb_mr_used;					/* More recently used blocks. */
	struct XTIndBlock	*cb_lr_used;					/* Less recently used blocks. */
	/* Protected by cb_lock: */
	XTAtomicRWLockRec	cb_lock;
	xtWord1				cb_state;						/* Block status. */
	xtWord2				cb_handle_count;				/* TRUE if this page is referenced by a handle. */
	xtWord2				cp_flush_seq;
#ifdef XT_USE_DIRECT_IO_ON_INDEX
	xtWord1				*cb_data;
#else
	xtWord1				cb_data[XT_INDEX_PAGE_SIZE];
#endif
} XTIndBlockRec, *XTIndBlockPtr;

typedef struct XTIndReference {
	XTPageUnlockType		ir_ulock;
	XTIndBlockPtr			ir_block;
	XTIdxBranchDPtr			ir_branch;
} XTIndReferenceRec, *XTIndReferencePtr;

typedef struct XTIndFreeBlock {
	XTDiskValue1			if_status_1;
	XTDiskValue1			if_unused1_1;
	XTDiskValue2			if_unused2_2;
	XTDiskValue4			if_unused3_4;
	XTDiskValue8			if_next_block_8;
} XTIndFreeBlockRec, *XTIndFreeBlockPtr;

typedef struct XTIndHandleBlock {
	xtWord4					hb_ref_count;
	struct XTIndHandleBlock	*hb_next;
	XTIdxBranchDRec			hb_branch;
} XTIndHandleBlockRec, *XTIndHandleBlockPtr;

typedef struct XTIndHandle {
	struct XTIndHandle		*ih_next;
	struct XTIndHandle		*ih_prev;
	XTSpinLockRec			ih_lock;
	xtIndexNodeID			ih_address;
	xtBool					ih_cache_reference;		/* True if this handle references the cache. */
	union {
		XTIndBlockPtr		ih_cache_block;
		XTIndHandleBlockPtr	ih_handle_block;
	} x;
	XTIdxBranchDPtr			ih_branch;
} XTIndHandleRec, *XTIndHandlePtr;

void			xt_ind_init(XTThreadPtr self, size_t cache_size);
void			xt_ind_exit(XTThreadPtr self);

xtInt8			xt_ind_get_usage();
xtInt8			xt_ind_get_size();
xtBool			xt_ind_write(struct XTOpenTable *ot, XTIndexPtr ind, xtIndexNodeID offset, size_t size, xtWord1 *data);
xtBool			xt_ind_write_cache(struct XTOpenTable *ot, xtIndexNodeID offset, size_t size, xtWord1 *data);
xtBool			xt_ind_clean(struct XTOpenTable *ot, XTIndexPtr ind, xtIndexNodeID offset);
xtBool			xt_ind_read_bytes(struct XTOpenTable *ot, xtIndexNodeID offset, size_t size, xtWord1 *data);
void			xt_ind_check_cache(XTIndexPtr ind);
xtBool			xt_ind_reserve(struct XTOpenTable *ot, u_int count, XTIdxBranchDPtr not_this);
void			xt_ind_free_reserved(struct XTOpenTable *ot);
void			xt_ind_unreserve(struct XTOpenTable *ot);
void			xt_load_indices(XTThreadPtr self, struct XTOpenTable *ot);

xtBool			xt_ind_fetch(struct XTOpenTable *ot, xtIndexNodeID node, XTPageLockType ltype, XTIndReferencePtr iref);
xtBool			xt_ind_release(struct XTOpenTable *ot, XTIndexPtr ind, XTPageUnlockType utype, XTIndReferencePtr iref);

void			xt_ind_lock_handle(XTIndHandlePtr handle);
void			xt_ind_unlock_handle(XTIndHandlePtr handle);
xtBool			xt_ind_copy_on_write(XTIndReferencePtr iref);

XTIndHandlePtr	xt_ind_get_handle(struct XTOpenTable *ot, XTIndexPtr ind, XTIndReferencePtr iref);
void			xt_ind_release_handle(XTIndHandlePtr handle, xtBool have_lock, XTThreadPtr thread);

#ifdef DEBUG
//#define DEBUG_CHECK_IND_CACHE
#endif

//#define XT_TRACE_INDEX

#ifdef XT_TRACE_INDEX
#define IDX_TRACE(x, y, z)		xt_trace(x, y, z)
#else
#define IDX_TRACE(x, y, z)
#endif

#endif
