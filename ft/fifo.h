/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef FIFO_H
#define FIFO_H
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "fttypes.h"
#include "xids-internal.h"
#include "xids.h"


// If the fifo_entry is unpacked, the compiler aligns the xids array and we waste a lot of space
#if TOKU_WINDOWS
#pragma pack(push, 1)
#endif

struct __attribute__((__packed__)) fifo_entry {
    unsigned int keylen;
    unsigned int vallen;
    unsigned char type;
    bool          is_fresh;
    MSN           msn;
    XIDS_S        xids_s;
};

// get and set the brt message type for a fifo entry.
// it is internally stored as a single unsigned char.
static inline enum ft_msg_type 
fifo_entry_get_msg_type(const struct fifo_entry * entry)
{
    enum ft_msg_type msg_type;
    msg_type = (enum ft_msg_type) entry->type;
    return msg_type;
}

static inline void
fifo_entry_set_msg_type(struct fifo_entry * entry,
        enum ft_msg_type msg_type)
{
    unsigned char type = (unsigned char) msg_type;
    entry->type = type;
}

#if TOKU_WINDOWS
#pragma pack(pop)
#endif

typedef struct fifo *FIFO;

int toku_fifo_create(FIFO *);

void toku_fifo_free(FIFO *);

int toku_fifo_n_entries(FIFO);

int toku_fifo_enq (FIFO, const void *key, ITEMLEN keylen, const void *data, ITEMLEN datalen, enum ft_msg_type type, MSN msn, XIDS xids, bool is_fresh, int32_t *dest);

unsigned int toku_fifo_buffer_size_in_use (FIFO fifo);
unsigned long toku_fifo_memory_size_in_use(FIFO fifo);  // return how much memory in the fifo holds useful data

unsigned long toku_fifo_memory_footprint(FIFO fifo);  // return how much memory the fifo occupies

//These two are problematic, since I don't want to malloc() the bytevecs, but dequeueing the fifo frees the memory.
//int toku_fifo_peek_deq (FIFO, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen, uint32_t *type, TXNID *xid);
//int toku_fifo_peek_deq_cmdstruct (FIFO, FT_MSG, DBT*, DBT*); // fill in the FT_MSG, using the two DBTs for the DBT part.
void toku_fifo_iterate(FIFO, void(*f)(bytevec key,ITEMLEN keylen,bytevec data,ITEMLEN datalen, enum ft_msg_type type, MSN msn, XIDS xids, bool is_fresh, void*), void*);

#define FIFO_ITERATE(fifo,keyvar,keylenvar,datavar,datalenvar,typevar,msnvar,xidsvar,is_freshvar,body) ({ \
  for (int fifo_iterate_off = toku_fifo_iterate_internal_start(fifo);                                     \
       toku_fifo_iterate_internal_has_more(fifo, fifo_iterate_off);                                       \
       fifo_iterate_off = toku_fifo_iterate_internal_next(fifo, fifo_iterate_off)) {                      \
      struct fifo_entry *e = toku_fifo_iterate_internal_get_entry(fifo, fifo_iterate_off);                \
      ITEMLEN keylenvar = e->keylen;                                                                      \
      ITEMLEN datalenvar = e->vallen;                                                                     \
      enum ft_msg_type typevar = fifo_entry_get_msg_type(e);                                              \
      MSN     msnvar  = e->msn;                                                                           \
      XIDS    xidsvar = &e->xids_s;                                                                       \
      bytevec keyvar  = xids_get_end_of_array(xidsvar);                                                   \
      bytevec datavar = (const uint8_t*)keyvar + e->keylen;                                               \
      bool is_freshvar = e->is_fresh;                                                                     \
      body;                                                                                               \
  } })

#define FIFO_CURRENT_ENTRY_MEMSIZE toku_fifo_internal_entry_memsize(e)

// Internal functions for the iterator.
int toku_fifo_iterate_internal_start(FIFO fifo);
int toku_fifo_iterate_internal_has_more(FIFO fifo, int off);
int toku_fifo_iterate_internal_next(FIFO fifo, int off);
struct fifo_entry * toku_fifo_iterate_internal_get_entry(FIFO fifo, int off);
size_t toku_fifo_internal_entry_memsize(struct fifo_entry *e) __attribute__((const,nonnull));
size_t toku_ft_msg_memsize_in_fifo(FT_MSG cmd) __attribute__((const,nonnull));

DBT *fill_dbt_for_fifo_entry(DBT *dbt, const struct fifo_entry *entry);
struct fifo_entry *toku_fifo_get_entry(FIFO fifo, int off);

void toku_fifo_clone(FIFO orig_fifo, FIFO* cloned_fifo);

bool toku_are_fifos_same(FIFO fifo1, FIFO fifo2);




#endif
