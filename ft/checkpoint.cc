/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2009-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

/***********
 * The purpose of this file is to implement the high-level logic for 
 * taking a checkpoint.
 *
 * There are three locks used for taking a checkpoint.  They are listed below.
 *
 * NOTE: The reader-writer locks may be held by either multiple clients 
 *       or the checkpoint function.  (The checkpoint function has the role
 *       of the writer, the clients have the reader roles.)
 *
 *  - multi_operation_lock
 *    This is a new reader-writer lock.
 *    This lock is held by the checkpoint function only for as long as is required to 
 *    to set all the "pending" bits and to create the checkpoint-in-progress versions
 *    of the header and translation table (btt).
 *    The following operations must take the multi_operation_lock:
 *     - any set of operations that must be atomic with respect to begin checkpoint
 *
 *  - checkpoint_safe_lock
 *    This is a new reader-writer lock.
 *    This lock is held for the entire duration of the checkpoint.
 *    It is used to prevent more than one checkpoint from happening at a time
 *    (the checkpoint function is non-re-entrant), and to prevent certain operations
 *    that should not happen during a checkpoint.  
 *    The following operations must take the checkpoint_safe lock:
 *       - delete a dictionary
 *       - rename a dictionary
 *    The application can use this lock to disable checkpointing during other sensitive
 *    operations, such as making a backup copy of the database.
 *
 * Once the "pending" bits are set and the snapshots are taken of the header and btt,
 * most normal database operations are permitted to resume.
 *
 *
 *
 *****/

#include <toku_portability.h>
#include <time.h>

#include "fttypes.h"
#include "cachetable.h"
#include "log-internal.h"
#include "logger.h"
#include "checkpoint.h"
#include <portability/toku_atomic.h>

///////////////////////////////////////////////////////////////////////////////////
// Engine status
//
// Status is intended for display to humans to help understand system behavior.
// It does not need to be perfectly thread-safe.

static CHECKPOINT_STATUS_S cp_status;

#define STATUS_INIT(k,t,l) { \
        cp_status.status[k].keyname = #k; \
        cp_status.status[k].type    = t;  \
        cp_status.status[k].legend  = "checkpoint: " l; \
    }

static void
status_init(void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.

    STATUS_INIT(CP_PERIOD,                              UINT64,   "period");
    STATUS_INIT(CP_FOOTPRINT,                           UINT64,   "footprint");
    STATUS_INIT(CP_TIME_LAST_CHECKPOINT_BEGIN,          UNIXTIME, "last checkpoint began ");
    STATUS_INIT(CP_TIME_LAST_CHECKPOINT_BEGIN_COMPLETE, UNIXTIME, "last complete checkpoint began ");
    STATUS_INIT(CP_TIME_LAST_CHECKPOINT_END,            UNIXTIME, "last complete checkpoint ended");
    STATUS_INIT(CP_LAST_LSN,                            UINT64,   "last complete checkpoint LSN");
    STATUS_INIT(CP_CHECKPOINT_COUNT,                    UINT64,   "checkpoints taken ");
    STATUS_INIT(CP_CHECKPOINT_COUNT_FAIL,               UINT64,   "checkpoints failed");
    STATUS_INIT(CP_WAITERS_NOW,                         UINT64,   "waiters now");
    STATUS_INIT(CP_WAITERS_MAX,                         UINT64,   "waiters max");
    STATUS_INIT(CP_CLIENT_WAIT_ON_MO,                   UINT64,   "non-checkpoint client wait on mo lock");
    STATUS_INIT(CP_CLIENT_WAIT_ON_CS,                   UINT64,   "non-checkpoint client wait on cs lock");
    cp_status.initialized = true;
}
#undef STATUS_INIT

#define STATUS_VALUE(x) cp_status.status[x].value.num

void
toku_checkpoint_get_status(CACHETABLE ct, CHECKPOINT_STATUS statp) {
    if (!cp_status.initialized)
        status_init();
    STATUS_VALUE(CP_PERIOD) = toku_get_checkpoint_period_unlocked(ct);
    *statp = cp_status;
}



static LSN last_completed_checkpoint_lsn;

static toku_pthread_rwlock_t checkpoint_safe_lock;
static toku_pthread_rwlock_t multi_operation_lock;

static bool initialized = false;     // sanity check
static volatile bool locked_mo = false;       // true when the multi_operation write lock is held (by checkpoint)
static volatile bool locked_cs = false;       // true when the checkpoint_safe write lock is held (by checkpoint)


// Note following static functions are called from checkpoint internal logic only,
// and use the "writer" calls for locking and unlocking.


static void
multi_operation_lock_init(void) {
    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
#if defined(HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP)
    pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#else
    // TODO: need to figure out how to make writer-preferential rwlocks
    // happen on osx
#endif
    toku_pthread_rwlock_init(&multi_operation_lock, &attr); 
    pthread_rwlockattr_destroy(&attr);
    locked_mo = false;
}

static void
multi_operation_lock_destroy(void) {
    toku_pthread_rwlock_destroy(&multi_operation_lock);
}

static void 
multi_operation_checkpoint_lock(void) {
    toku_pthread_rwlock_wrlock(&multi_operation_lock);   
    locked_mo = true;
}

static void 
multi_operation_checkpoint_unlock(void) {
    locked_mo = false;
    toku_pthread_rwlock_wrunlock(&multi_operation_lock); 
}

static void
checkpoint_safe_lock_init(void) {
    toku_pthread_rwlock_init(&checkpoint_safe_lock, NULL); 
    locked_cs = false;
}

static void
checkpoint_safe_lock_destroy(void) {
    toku_pthread_rwlock_destroy(&checkpoint_safe_lock); 
}

static void 
checkpoint_safe_checkpoint_lock(void) {
    toku_pthread_rwlock_wrlock(&checkpoint_safe_lock);   
    locked_cs = true;
}

static void 
checkpoint_safe_checkpoint_unlock(void) {
    locked_cs = false;
    toku_pthread_rwlock_wrunlock(&checkpoint_safe_lock); 
}


// toku_xxx_client_(un)lock() functions are only called from client code,
// never from checkpoint code, and use the "reader" interface to the lock functions.

void 
toku_multi_operation_client_lock(void) {
    if (locked_mo)
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(CP_CLIENT_WAIT_ON_MO), 1);
    toku_pthread_rwlock_rdlock(&multi_operation_lock);   
}

void 
toku_multi_operation_client_unlock(void) {
    toku_pthread_rwlock_rdunlock(&multi_operation_lock); 
}

void 
toku_checkpoint_safe_client_lock(void) {
    if (locked_cs)
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(CP_CLIENT_WAIT_ON_CS), 1);
    toku_pthread_rwlock_rdlock(&checkpoint_safe_lock);  
    toku_multi_operation_client_lock();
}

void 
toku_checkpoint_safe_client_unlock(void) {
    toku_pthread_rwlock_rdunlock(&checkpoint_safe_lock); 
    toku_multi_operation_client_unlock();
}



// Initialize the checkpoint mechanism, must be called before any client operations.
void
toku_checkpoint_init(void) {
    multi_operation_lock_init();
    checkpoint_safe_lock_init();
    initialized = true;
}

void
toku_checkpoint_destroy(void) {
    multi_operation_lock_destroy();
    checkpoint_safe_lock_destroy();
    initialized = false;
}

#define SET_CHECKPOINT_FOOTPRINT(x) STATUS_VALUE(CP_FOOTPRINT) = footprint_offset + x


// Take a checkpoint of all currently open dictionaries
int 
toku_checkpoint(CHECKPOINTER cp, TOKULOGGER logger,
                void (*callback_f)(void*),  void * extra,
                void (*callback2_f)(void*), void * extra2,
                checkpoint_caller_t caller_id) {
    int r = 0;
    int footprint_offset = (int) caller_id * 1000;

    assert(initialized);

    (void) toku_sync_fetch_and_add(&STATUS_VALUE(CP_WAITERS_NOW), 1);
    checkpoint_safe_checkpoint_lock();
    (void) toku_sync_fetch_and_sub(&STATUS_VALUE(CP_WAITERS_NOW), 1);

    if (STATUS_VALUE(CP_WAITERS_NOW) > STATUS_VALUE(CP_WAITERS_MAX))
        STATUS_VALUE(CP_WAITERS_MAX) = STATUS_VALUE(CP_WAITERS_NOW);  // threadsafe, within checkpoint_safe lock

    SET_CHECKPOINT_FOOTPRINT(10);
    multi_operation_checkpoint_lock();
    SET_CHECKPOINT_FOOTPRINT(20);
    toku_ft_open_close_lock();
    
    SET_CHECKPOINT_FOOTPRINT(30);
    STATUS_VALUE(CP_TIME_LAST_CHECKPOINT_BEGIN) = time(NULL);
    toku_cachetable_begin_checkpoint(cp, logger);

    toku_ft_open_close_unlock();
    multi_operation_checkpoint_unlock();

    SET_CHECKPOINT_FOOTPRINT(40);
    if (r==0) {
        if (callback_f) 
            callback_f(extra);      // callback is called with checkpoint_safe_lock still held
        toku_cachetable_end_checkpoint(cp, logger, callback2_f, extra2);
    }
    SET_CHECKPOINT_FOOTPRINT(50);
    if (r==0 && logger) {
        last_completed_checkpoint_lsn = logger->last_completed_checkpoint_lsn;
        r = toku_logger_maybe_trim_log(logger, last_completed_checkpoint_lsn);
        STATUS_VALUE(CP_LAST_LSN) = last_completed_checkpoint_lsn.lsn;
    }

    SET_CHECKPOINT_FOOTPRINT(60);
    STATUS_VALUE(CP_TIME_LAST_CHECKPOINT_END) = time(NULL);
    STATUS_VALUE(CP_TIME_LAST_CHECKPOINT_BEGIN_COMPLETE) = STATUS_VALUE(CP_TIME_LAST_CHECKPOINT_BEGIN);

    if (r == 0)
        STATUS_VALUE(CP_CHECKPOINT_COUNT)++;
    else
        STATUS_VALUE(CP_CHECKPOINT_COUNT_FAIL)++;

    STATUS_VALUE(CP_FOOTPRINT) = 0;
    checkpoint_safe_checkpoint_unlock();
    return r;
}

#include <toku_race_tools.h>
void __attribute__((__constructor__)) toku_checkpoint_helgrind_ignore(void);
void
toku_checkpoint_helgrind_ignore(void) {
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&cp_status, sizeof cp_status);
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&locked_mo, sizeof locked_mo);
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&locked_cs, sizeof locked_cs);
}

#undef SET_CHECKPOINT_FOOTPRINT
#undef STATUS_VALUE
