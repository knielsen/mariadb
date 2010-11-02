#include "mysql_priv.h"
#include <my_bit.h>
#include "sql_select.h"

/****************************************************************************
 * Default MRR implementation (MRR to non-MRR converter)
 ***************************************************************************/

/**
  Get cost and other information about MRR scan over a known list of ranges

  Calculate estimated cost and other information about an MRR scan for given
  sequence of ranges.

  @param keyno           Index number
  @param seq             Range sequence to be traversed
  @param seq_init_param  First parameter for seq->init()
  @param n_ranges_arg    Number of ranges in the sequence, or 0 if the caller
                         can't efficiently determine it
  @param bufsz    INOUT  IN:  Size of the buffer available for use
                         OUT: Size of the buffer that is expected to be actually
                              used, or 0 if buffer is not needed.
  @param flags    INOUT  A combination of HA_MRR_* flags
  @param cost     OUT    Estimated cost of MRR access

  @note
    This method (or an overriding one in a derived class) must check for
    thd->killed and return HA_POS_ERROR if it is not zero. This is required
    for a user to be able to interrupt the calculation by killing the
    connection/query.

  @retval
    HA_POS_ERROR  Error or the engine is unable to perform the requested
                  scan. Values of OUT parameters are undefined.
  @retval
    other         OK, *cost contains cost of the scan, *bufsz and *flags
                  contain scan parameters.
*/

ha_rows 
handler::multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                     void *seq_init_param, uint n_ranges_arg,
                                     uint *bufsz, uint *flags, COST_VECT *cost)
{
  KEY_MULTI_RANGE range;
  range_seq_t seq_it;
  ha_rows rows, total_rows= 0;
  uint n_ranges=0;
  THD *thd= current_thd;
  
  /* Default MRR implementation doesn't need buffer */
  *bufsz= 0;

  seq_it= seq->init(seq_init_param, n_ranges, *flags);
  while (!seq->next(seq_it, &range))
  {
    if (unlikely(thd->killed != 0))
      return HA_POS_ERROR;
    
    n_ranges++;
    key_range *min_endp, *max_endp;
    if (range.range_flag & GEOM_FLAG)
    {
      /* In this case tmp_min_flag contains the handler-read-function */
      range.start_key.flag= (ha_rkey_function) (range.range_flag ^ GEOM_FLAG);
      min_endp= &range.start_key;
      max_endp= NULL;
    }
    else
    {
      min_endp= range.start_key.length? &range.start_key : NULL;
      max_endp= range.end_key.length? &range.end_key : NULL;
    }
    if ((range.range_flag & UNIQUE_RANGE) && !(range.range_flag & NULL_RANGE))
      rows= 1; /* there can be at most one row */
    else
    {
      if (HA_POS_ERROR == (rows= this->records_in_range(keyno, min_endp, 
                                                        max_endp)))
      {
        /* Can't scan one range => can't do MRR scan at all */
        total_rows= HA_POS_ERROR;
        break;
      }
    }
    total_rows += rows;
  }
  
  if (total_rows != HA_POS_ERROR)
  {
    /* The following calculation is the same as in multi_range_read_info(): */
    *flags |= HA_MRR_USE_DEFAULT_IMPL;
    cost->zero();
    cost->avg_io_cost= 1; /* assume random seeks */
    if ((*flags & HA_MRR_INDEX_ONLY) && total_rows > 2)
      cost->io_count= keyread_time(keyno, n_ranges, (uint)total_rows);
    else
      cost->io_count= read_time(keyno, n_ranges, total_rows);
    cost->cpu_cost= (double) total_rows / TIME_FOR_COMPARE + 0.01;
  }
  return total_rows;
}


/**
  Get cost and other information about MRR scan over some sequence of ranges

  Calculate estimated cost and other information about an MRR scan for some
  sequence of ranges.

  The ranges themselves will be known only at execution phase. When this
  function is called we only know number of ranges and a (rough) E(#records)
  within those ranges.

  Currently this function is only called for "n-keypart singlepoint" ranges,
  i.e. each range is "keypart1=someconst1 AND ... AND keypartN=someconstN"

  The flags parameter is a combination of those flags: HA_MRR_SORTED,
  HA_MRR_INDEX_ONLY, HA_MRR_NO_ASSOCIATION, HA_MRR_LIMITS.

  @param keyno           Index number
  @param n_ranges        Estimated number of ranges (i.e. intervals) in the
                         range sequence.
  @param n_rows          Estimated total number of records contained within all
                         of the ranges
  @param bufsz    INOUT  IN:  Size of the buffer available for use
                         OUT: Size of the buffer that will be actually used, or
                              0 if buffer is not needed.
  @param flags    INOUT  A combination of HA_MRR_* flags
  @param cost     OUT    Estimated cost of MRR access

  @retval
    0     OK, *cost contains cost of the scan, *bufsz and *flags contain scan
          parameters.
  @retval
    other Error or can't perform the requested scan
*/

ha_rows handler::multi_range_read_info(uint keyno, uint n_ranges, uint n_rows,
                                       uint key_parts, uint *bufsz, 
                                       uint *flags, COST_VECT *cost)
{
  /* 
    Currently we expect this function to be called only in preparation of scan
    with HA_MRR_SINGLE_POINT property.
  */
  DBUG_ASSERT(*flags | HA_MRR_SINGLE_POINT);

  *bufsz= 0; /* Default implementation doesn't need a buffer */
  *flags |= HA_MRR_USE_DEFAULT_IMPL;

  cost->zero();
  cost->avg_io_cost= 1; /* assume random seeks */

  /* Produce the same cost as non-MRR code does */
  if (*flags & HA_MRR_INDEX_ONLY)
    cost->io_count= keyread_time(keyno, n_ranges, n_rows);
  else
    cost->io_count= read_time(keyno, n_ranges, n_rows);
  return 0;
}


/**
  Initialize the MRR scan

  Initialize the MRR scan. This function may do heavyweight scan 
  initialization like row prefetching/sorting/etc (NOTE: but better not do
  it here as we may not need it, e.g. if we never satisfy WHERE clause on
  previous tables. For many implementations it would be natural to do such
  initializations in the first multi_read_range_next() call)

  mode is a combination of the following flags: HA_MRR_SORTED,
  HA_MRR_INDEX_ONLY, HA_MRR_NO_ASSOCIATION 

  @param seq             Range sequence to be traversed
  @param seq_init_param  First parameter for seq->init()
  @param n_ranges        Number of ranges in the sequence
  @param mode            Flags, see the description section for the details
  @param buf             INOUT: memory buffer to be used

  @note
    One must have called index_init() before calling this function. Several
    multi_range_read_init() calls may be made in course of one query.

    Until WL#2623 is done (see its text, section 3.2), the following will 
    also hold:
    The caller will guarantee that if "seq->init == mrr_ranges_array_init"
    then seq_init_param is an array of n_ranges KEY_MULTI_RANGE structures.
    This property will only be used by NDB handler until WL#2623 is done.
     
    Buffer memory management is done according to the following scenario:
    The caller allocates the buffer and provides it to the callee by filling
    the members of HANDLER_BUFFER structure.
    The callee consumes all or some fraction of the provided buffer space, and
    sets the HANDLER_BUFFER members accordingly.
    The callee may use the buffer memory until the next multi_range_read_init()
    call is made, all records have been read, or until index_end() call is
    made, whichever comes first.

  @retval 0  OK
  @retval 1  Error
*/

int
handler::multi_range_read_init(RANGE_SEQ_IF *seq_funcs, void *seq_init_param,
                               uint n_ranges, uint mode, HANDLER_BUFFER *buf)
{
  DBUG_ENTER("handler::multi_range_read_init");
  mrr_iter= seq_funcs->init(seq_init_param, n_ranges, mode);
  mrr_funcs= *seq_funcs;
  mrr_is_output_sorted= test(mode & HA_MRR_SORTED);
  mrr_have_range= FALSE;
  DBUG_RETURN(0);
}

/**
  Get next record in MRR scan

  Default MRR implementation: read the next record

  @param range_info  OUT  Undefined if HA_MRR_NO_ASSOCIATION flag is in effect
                          Otherwise, the opaque value associated with the range
                          that contains the returned record.

  @retval 0      OK
  @retval other  Error code
*/

int handler::multi_range_read_next(char **range_info)
{
  int result= HA_ERR_END_OF_FILE;
  int range_res;
  DBUG_ENTER("handler::multi_range_read_next");

  if (!mrr_have_range)
  {
    mrr_have_range= TRUE;
    goto start;
  }

  do
  {
    /* Save a call if there can be only one row in range. */
    if (mrr_cur_range.range_flag != (UNIQUE_RANGE | EQ_RANGE))
    {
      result= read_range_next();
      /* On success or non-EOF errors jump to the end. */
      if (result != HA_ERR_END_OF_FILE)
        break;
    }
    else
    {
      if (was_semi_consistent_read())
        goto scan_it_again;
      /*
        We need to set this for the last range only, but checking this
        condition is more expensive than just setting the result code.
      */
      result= HA_ERR_END_OF_FILE;
    }

start:
    /* Try the next range(s) until one matches a record. */
    while (!(range_res= mrr_funcs.next(mrr_iter, &mrr_cur_range)))
    {
scan_it_again:
      result= read_range_first(mrr_cur_range.start_key.keypart_map ?
                                 &mrr_cur_range.start_key : 0,
                               mrr_cur_range.end_key.keypart_map ?
                                 &mrr_cur_range.end_key : 0,
                               test(mrr_cur_range.range_flag & EQ_RANGE),
                               mrr_is_output_sorted);
      if (result != HA_ERR_END_OF_FILE)
        break;
    }
  }
  while ((result == HA_ERR_END_OF_FILE) && !range_res);

  *range_info= mrr_cur_range.ptr;
  DBUG_PRINT("exit",("handler::multi_range_read_next result %d", result));
  DBUG_RETURN(result);
}


/***** MRR_impl classes ****************************************************/

int Mrr_simple_index_reader::get_next(char **range_info)
{
  while (!(res= h->handler::multi_range_read_next(range_info)))
  {
    KEY_MULTI_RANGE *curr_range= &h->handler::mrr_cur_range;
    if (!h->mrr_funcs.skip_index_tuple ||
        !h->mrr_funcs.skip_index_tuple(h->mrr_iter, curr_range->ptr))
      break;
  }
  return res;
}

int Mrr_simple_index_reader::init(handler *h_arg, RANGE_SEQ_IF *seq_funcs, 
                                  void *seq_init_param, uint n_ranges,
                                  uint mode, Buffer_manager *buf_manager_arg)
{
  HANDLER_BUFFER no_buffer = {NULL, NULL, NULL};
  h= h_arg;
  res= 0;
  return h->handler::multi_range_read_init(seq_funcs, seq_init_param, n_ranges,
                                           mode, &no_buffer);
}

/**
  DS-MRR/CPK: multi_range_read_next() function
  
  @param range_info  OUT  identifier of range that the returned record belongs to
  
  @note
    This function walks over key buffer and does index reads, i.e. it produces
    {current_record, range_id} pairs.

    The function has the same call contract like multi_range_read_next()'s.

    We actually iterate over nested sequences:
    - a disjoint sequence of index ranges
      - each range has multiple records
        - each record goes into multiple identical ranges.

  @retval 0                   OK, next record was successfully read
  @retval HA_ERR_END_OF_FILE  End of records
  @retval Other               Some other error
*/

int Mrr_ordered_index_reader::get_next(char **range_info_arg)
{
  DBUG_ENTER("Mrr_ordered_index_reader::get_next");
  
  if (!know_key_tuple_params) /* We're in startup phase */
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  while (1)
  {
    bool have_record= FALSE;
    if (scanning_key_val_iter)
    {
      if (kv_it.get_next())
      {
        kv_it.close();
        scanning_key_val_iter= FALSE;
      }
      else
        have_record= TRUE;
    }
    else
    {
      while (kv_it.init(this))
      {
        if (key_buffer->is_empty())
        {
          /*if (auto_refill)
          {
            int res;
            if ((res= refill_buffer()))
              DBUG_RETURN(res);
            if (key_buffer->is_empty())
            {
              index_scan_eof= TRUE;
              DBUG_RETURN(HA_ERR_END_OF_FILE);
            }
          }
          else
          */
          {
            /* Buffer refills are managed by somebody else for us */
            index_scan_eof= TRUE;
            DBUG_RETURN(HA_ERR_END_OF_FILE);
          }
        }
      }
      scanning_key_val_iter= TRUE;
    }

    if (have_record &&
        (!mrr_funcs.skip_index_tuple ||
         !mrr_funcs.skip_index_tuple(mrr_iter, *(char**)cur_range_info))
        && 
        (!mrr_funcs.skip_record ||
         !mrr_funcs.skip_record(mrr_iter, *(char**)cur_range_info, NULL)))
    {
      break;
    }
    /* Go get another (record, range_id) combination */
  } /* while */

  memcpy(range_info_arg, cur_range_info, sizeof(void*));
  DBUG_RETURN(0);
}


/**
  DS-MRR/CPK: Fill the buffer with (lookup_tuple, range_id) pairs and sort
  
  Enumerate the input range (=key) sequence, fill the key buffer with 
  (lookup_key, range_id) pairs and sort it.

  When this function returns, either
   - key buffer is non-empty, or
   - key buffer is empty and source range sequence is exhausted
  
  @note
    dsmrr_eof is set to indicate whether we've exhausted the list of ranges 
    we're scanning.
*/

int Mrr_ordered_index_reader::refill_buffer()
{
  int res;
  KEY_MULTI_RANGE cur_range;
  uchar **range_info_ptr= (uchar**)&cur_range.ptr;
  uchar *key_ptr;
  DBUG_ENTER("Mrr_ordered_index_reader::refill_buffer");

  DBUG_ASSERT(!know_key_tuple_params || key_buffer->is_empty());
  if (know_key_tuple_params)
  {
    buf_manager->reset_buffer_sizes();
    key_buffer->reset();
    key_buffer->setup_writing(&key_ptr, keypar.key_size_in_keybuf,
                              is_mrr_assoc? (uchar**)&range_info_ptr : NULL,
                              sizeof(uchar*));
  }
#if 0

  if (know_key_tuple_params)
  {
    if (do_rndpos_scan && rowid_buffer.is_empty())
    {
      /*
        We're using two buffers and both of them are empty now. Restore the
        original sizes
      */
      rowid_buffer.set_buffer_space(full_buf, rowid_buffer_end);
      key_buffer= &backward_key_buf;
      key_buffer->set_buffer_space(rowid_buffer_end, full_buf_end);
    }
  }
  is all of the ifdef-ed stuff is handled above?
#endif
  while ((!know_key_tuple_params || key_buffer->can_write()) && 
         !(res= mrr_funcs.next(mrr_iter, &cur_range)))
  {
    DBUG_ASSERT(cur_range.range_flag & EQ_RANGE);

    if (!know_key_tuple_params)
    {
      /* This only happens when we've just started filling the buffer */
      key_range *sample_key= &cur_range.start_key;
      know_key_tuple_params= TRUE;
      keypar.key_tuple_length= sample_key->length;
      keypar.key_tuple_map= sample_key->keypart_map;
      keypar.key_size_in_keybuf= keypar.use_key_pointers ? sizeof(char*) : keypar.key_tuple_length;
      KEY *key_info= &h->get_table()->key_info[h->active_index];
      keypar.index_ranges_unique= test(key_info->flags & HA_NOSAME && 
                                       key_info->key_parts == 
                                       my_count_bits(sample_key->keypart_map));
      buf_manager->setup_buffer_sizes(keypar.key_size_in_keybuf, keypar.key_tuple_map);
      key_buffer= buf_manager->get_key_buffer();
      key_buffer->setup_writing(&key_ptr, keypar.key_size_in_keybuf,
                               is_mrr_assoc? (uchar**)&range_info_ptr : NULL,
                               sizeof(uchar*));
      DBUG_ASSERT(key_buffer->can_write());
    }
    
    /* Put key, or {key, range_id} pair into the buffer */
    if (keypar.use_key_pointers)
      key_ptr=(uchar*) &cur_range.start_key.key;
    else
      key_ptr=(uchar*) cur_range.start_key.key;

    key_buffer->write();
  }

  no_more_keys= test(res);
  scanning_key_val_iter= FALSE;
  index_scan_eof= FALSE;

  if (no_more_keys && (!know_key_tuple_params || key_buffer->is_empty()))
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  key_buffer->sort((key_buffer->type() == Lifo_buffer::FORWARD)? 
                     (qsort2_cmp)Mrr_ordered_index_reader::key_tuple_cmp_reverse : 
                     (qsort2_cmp)Mrr_ordered_index_reader::key_tuple_cmp, 
                   (void*)this);
  DBUG_RETURN(0);
}


int Mrr_ordered_index_reader::init(handler *h_arg, RANGE_SEQ_IF *seq_funcs,
                                   void *seq_init_param, uint n_ranges,
                                   uint mode, Buffer_manager *buf_manager_arg)
{
  h= h_arg;
  mrr_iter= seq_funcs->init(seq_init_param, n_ranges, mode);
  keypar.use_key_pointers= test(mode & HA_MRR_MATERIALIZED_KEYS);
  is_mrr_assoc=    !test(mode & HA_MRR_NO_ASSOCIATION);
  mrr_funcs= *seq_funcs;
  know_key_tuple_params= FALSE;
  buf_manager= buf_manager_arg;
  return 0;
}


static int rowid_cmp_reverse(void *h, uchar *a, uchar *b)
{
  return - ((handler*)h)->cmp_ref(a, b);
}


int Mrr_ordered_rndpos_reader::init(handler *h_arg, 
                                    Mrr_index_reader *index_reader_arg,
                                    uint mode,
                                    Lifo_buffer *buf)
{
  h= h_arg;
  index_reader= index_reader_arg;
  rowid_buffer= buf;
  is_mrr_assoc=    !test(mode & HA_MRR_NO_ASSOCIATION);
  //rowid_buff_elem_size= h->ref_length;
  //if (!(mode & HA_MRR_NO_ASSOCIATION))
  //  rowid_buff_elem_size += sizeof(char*);

  index_reader_exhausted= FALSE;
  ///int res= index_reader->refill_buffer();
  ///if (res && res!=HA_ERR_END_OF_FILE)
  ///  return res;
  return 0;
}


/**
  DS-MRR: Fill and sort the rowid buffer

  Scan the MRR ranges and collect ROWIDs (or {ROWID, range_id} pairs) into 
  buffer. When the buffer is full or scan is completed, sort the buffer by 
  rowid and return.

  When this function returns, either rowid buffer is not empty, or the source
  of lookup keys (i.e. ranges) is exhaused.
  
  dsmrr_eof is set to indicate whether we've exhausted the list of ranges we're
  scanning. This function never returns HA_ERR_END_OF_FILE.

  @retval 0      OK, the next portion of rowids is in the buffer,
                 properly ordered
  @retval other  Error
*/


int Mrr_ordered_rndpos_reader::refill_buffer()
{
  int res;
  DBUG_ENTER("Mrr_ordered_rndpos_reader::refill_buffer");

  if (index_reader_exhausted)
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  while ((res= refill2() == HA_ERR_END_OF_FILE))
  {
    if ((res= index_reader->refill_buffer()))
    {
      if (res == HA_ERR_END_OF_FILE)
        index_reader_exhausted= TRUE;
      break;
    }
  }
  DBUG_RETURN(res);
}


/* This one refills without calling index_reader->refill_buffer(). */
int Mrr_ordered_rndpos_reader::refill2()
{
  char *range_info;
  uchar **range_info_ptr= (uchar**)&range_info;
  int res;
  DBUG_ENTER("Mrr_ordered_rndpos_reader::refill2");

  DBUG_ASSERT(rowid_buffer->is_empty());
  index_rowid= index_reader->get_rowid_ptr();
  rowid_buffer->reset();
  rowid_buffer->setup_writing(&index_rowid, h->ref_length,
                              is_mrr_assoc? (uchar**)&range_info_ptr: NULL,
                              sizeof(void*));

  last_identical_rowid= NULL;

  while (rowid_buffer->can_write())
  {
    res= index_reader->get_next(&range_info);

    if (res)
      break;

    /* Put rowid, or {rowid, range_id} pair into the buffer */
    index_reader->h->position(index_reader->h->get_table()->record[0]);

    rowid_buffer->write();
  }
   
  /* Sort the buffer contents by rowid */
  rowid_buffer->sort((qsort2_cmp)rowid_cmp_reverse, (void*)h);

  rowid_buffer->setup_reading(&rowid, h->ref_length,
                              is_mrr_assoc? (uchar**)&rowids_range_id: NULL,
                              sizeof(void*));
  DBUG_RETURN(rowid_buffer->is_empty()? HA_ERR_END_OF_FILE : 0);
}


/**
  DS-MRR implementation: multi_range_read_next() function.

  Calling convention is like multi_range_read_next() has.
*/

int Mrr_ordered_rndpos_reader::get_next(char **range_info)
{
  int res;

  while (last_identical_rowid)
  {
    /*
      Current record (the one we've returned in previous call) was obtained
      from a rowid that matched multiple range_ids. Return this record again,
      with next matching range_id.
    */
    bool UNINIT_VAR(bres);
    bres= rowid_buffer->read();
    DBUG_ASSERT(!bres);

    if (is_mrr_assoc)
      memcpy(range_info, rowids_range_id, sizeof(uchar*));

    if (rowid == last_identical_rowid)
    {
      last_identical_rowid= NULL; /* reached the last of identical rowids */
    }

    if (!index_reader->skip_record((char*)*range_info, rowid))
    {
      return 0;
    }
  }

  while (1)
  {
#if 0      
    if (rowid_buffer->is_empty()) /* We're out of rowids */
    {
      /* First, finish off the sorted keys we have */ 
      if (!index_reader->eof())
      {
        res= refill_buffer();
        if (res && res != HA_ERR_END_OF_FILE)
          return res;
      }

      if (rowid_buffer->is_empty())
      {
        /*
          Ok neither index_reader nor us have any records. Refill index
          reader, then refill us.
        */
        // TODO: if key buffer is empty, too, redistribute the buffer space.
        if ((res= index_reader->refill_buffer()) ||
            (res= refill_buffer()))
          return res;
      }
    }
#endif
   
    last_identical_rowid= NULL;

    /* Return eof if there are no rowids in the buffer after re-fill attempt */
    if (rowid_buffer->read())
      return HA_ERR_END_OF_FILE;

    if (is_mrr_assoc)
    {
      memcpy(range_info, rowids_range_id, sizeof(uchar*));
    }

    if (index_reader->skip_record(*range_info, rowid))
      continue;

    res= h->ha_rnd_pos(h->get_table()->record[0], rowid);

    if (res == HA_ERR_RECORD_DELETED)
      continue;
    
    /* 
      Check if subsequent buffer elements have the same rowid value as this
      one. If yes, remember this fact so that we don't make any more rnd_pos()
      calls with this value.
    */
    if (!res)
    {
      uchar *cur_rowid= rowid;
      /* 
        Note: this implies that SQL layer doesn't touch table->record[0]
        between calls.
      */
      Lifo_buffer_iterator it;
      it.init(rowid_buffer);
      while (!it.read()) // reads to (rowid, ...)
      {
        if (h->cmp_ref(rowid, cur_rowid))
          break;
        last_identical_rowid= rowid;
      }
    }
    return 0;
  }

  return res;
}




/************ MRR_impl classes end *********************************************/


/****************************************************************************
 * DS-MRR implementation 
 ***************************************************************************/

/**
  DS-MRR: Initialize and start MRR scan

  Initialize and start the MRR scan. Depending on the mode parameter, this
  may use default or DS-MRR implementation.

  @param h               Table handler to be used
  @param key             Index to be used
  @param seq_funcs       Interval sequence enumeration functions
  @param seq_init_param  Interval sequence enumeration parameter
  @param n_ranges        Number of ranges in the sequence.
  @param mode            HA_MRR_* modes to use
  @param buf             INOUT Buffer to use

  @retval 0     Ok, Scan started.
  @retval other Error
*/

int DsMrr_impl::dsmrr_init(handler *h_arg, RANGE_SEQ_IF *seq_funcs, 
                           void *seq_init_param, uint n_ranges, uint mode,
                           HANDLER_BUFFER *buf)
{
  THD *thd= current_thd;
  int res;
  DBUG_ENTER("DsMrr_impl::dsmrr_init");

  /*
    index_merge may invoke a scan on an object for which dsmrr_info[_const]
    has not been called, so set the owner handler here as well.
  */
  h= h_arg;
  is_mrr_assoc=    !test(mode & HA_MRR_NO_ASSOCIATION);

  if ((mode & HA_MRR_USE_DEFAULT_IMPL) || (mode & HA_MRR_SORTED))
  {
    DBUG_ASSERT(h->inited == handler::INDEX);
    Mrr_simple_index_reader *s= &strategy_factory.simple_index_reader;
    res= s->init(h, seq_funcs, seq_init_param, n_ranges, mode, this);
    strategy= s;
    DBUG_RETURN(res);
  }
  
  /* Neither of strategies used below can handle sorting */
  DBUG_ASSERT(!(mode & HA_MRR_SORTED));

  /*
    Determine whether we'll need to do key sorting and/or rnd_pos() scan
  */
  index_strategy= NULL;
  Mrr_ordered_index_reader *ordered_idx_reader= NULL;
  if ((mode & HA_MRR_SINGLE_POINT) &&
      optimizer_flag(thd, OPTIMIZER_SWITCH_MRR_SORT_KEYS))
  {
    index_strategy= ordered_idx_reader= &strategy_factory.ordered_index_reader;
  }
  else
    index_strategy= &strategy_factory.simple_index_reader;

  strategy= index_strategy;
  /*
    We don't need a rowid-to-rndpos step if
     - We're doing a scan on clustered primary key
     - [In the future] We're doing an index_only read
  */
  DBUG_ASSERT(h->inited == handler::INDEX || 
              (h->inited == handler::RND && h2 && 
               h2->inited == handler::INDEX));

  handler *h_idx= (h->inited == handler::INDEX)? h: h2;
  keyno= h_idx->active_index;

  Mrr_ordered_rndpos_reader *disk_strategy= NULL;
  if (!(keyno == table->s->primary_key && h_idx->primary_key_is_clustered()))
  {
    strategy= disk_strategy= &strategy_factory.ordered_rndpos_reader;
  }

  if (is_mrr_assoc)
    status_var_increment(thd->status_var.ha_multi_range_read_init_count);

  full_buf= buf->buffer;
  full_buf_end= buf->buffer_end;
  
  if (strategy == index_strategy)
  {
    ///if (ordered_idx_reader)
    //  ordered_idx_reader->auto_refill= TRUE;
    /* Index strategy serves it all. We don't need two handlers, etc */
    /* Give the buffer to index strategy */
    if ((res= index_strategy->init(h, seq_funcs, seq_init_param, n_ranges,
                                   mode, this)))
      goto error;
  }
  else
  {
    /*
      If we got here the request is served by both index and rndpos strategies
      working together.

    */
    rowid_buffer.set_buffer_space(buf->buffer, buf->buffer_end);

    if ((res= setup_two_handlers()))
      DBUG_RETURN(res);

    ///if (ordered_idx_reader)
    ///  ordered_idx_reader->auto_refill= FALSE;

    if ((res= index_strategy->init(h2, seq_funcs, seq_init_param, n_ranges, 
                                   mode, this)) || 
        (res= disk_strategy->init(h, index_strategy, mode, &rowid_buffer)))
    {
      goto error;
    }
  }
  
  res= strategy->refill_buffer();
  if (res && res != HA_ERR_END_OF_FILE) //psergey-todo: remove EOF check here
    goto error;

  /*
    If we have scanned through all intervals in *seq, then adjust *buf to 
    indicate that the remaining buffer space will not be used.
  */
//  if (dsmrr_eof) 
//    buf->end_of_used_area= rowid_buffer.end_of_space();

  
  DBUG_RETURN(0);
error:
  close_second_handler();
  strategy= NULL;
  DBUG_RETURN(1);
}


/*
  Whatever the current state is, make it so that we have two handler objects:
  - h (the primary)    -  initialized for rnd_pos() scan
  - h2 (the secondary) -  initialized for scanning the index specified in
                          this->keyno
  RETURN 
    0        OK
    HA_XXX   Error code
*/

int DsMrr_impl::setup_two_handlers()
{
  int res;
  THD *thd= current_thd;
  DBUG_ENTER("DsMrr_impl::setup_two_handlers");
  if (!h2)
  {
    handler *new_h2;
    Item *pushed_cond= NULL;
    DBUG_ASSERT(h->inited == handler::INDEX);
    /* Create a separate handler object to do rnd_pos() calls. */
    /*
      ::clone() takes up a lot of stack, especially on 64 bit platforms.
      The constant 5 is an empiric result.
    */
    if (check_stack_overrun(thd, 5*STACK_MIN_SIZE, (uchar*) &new_h2))
      DBUG_RETURN(1);

    /* Create a separate handler object to do rnd_pos() calls. */
    if (!(new_h2= h->clone(thd->mem_root)) || 
        new_h2->ha_external_lock(thd, F_RDLCK))
    {
      delete new_h2;
      DBUG_RETURN(1);
    }

    if (keyno == h->pushed_idx_cond_keyno)
      pushed_cond= h->pushed_idx_cond;
    
    Mrr_reader *save_strategy= strategy;
    strategy= NULL;
    /*
      Caution: this call will invoke this->dsmrr_close(). Do not put the
      created secondary table handler new_h2 into this->h2 or it will delete 
      it. Also, save the picked strategy
    */
    res= h->ha_index_end();

    strategy= save_strategy;
    h2= new_h2; /* Ok, now can put it into h2 */

    if (res || (res= (h->ha_rnd_init(FALSE))))
      goto error;

    table->prepare_for_position();
    h2->extra(HA_EXTRA_KEYREAD);
    h2->mrr_iter= h->mrr_iter;

    if ((res= h2->ha_index_init(keyno, FALSE)))
      goto error;

    if (pushed_cond)
      h2->idx_cond_push(keyno, pushed_cond);
  }
  else
  {
    DBUG_ASSERT(h2 && h2->inited==handler::INDEX);
    /* 
      We get here when the access alternates betwen MRR scan(s) and non-MRR
      scans.

      Calling h->index_end() will invoke dsmrr_close() for this object,
      which will delete h2. We need to keep it, so put it away and dont
      let it be deleted:
    */
    if (h->inited == handler::INDEX)
    {
      handler *save_h2= h2;
      Mrr_reader *save_strategy= strategy;
      h2= NULL;
      strategy= NULL;
      res= h->ha_index_end();
      h2= save_h2;
      strategy= save_strategy;
      if (res)
        goto error;
    }
    if ((h->inited != handler::RND) && h->ha_rnd_init(FALSE))
      goto error;
  }
  DBUG_RETURN(0);
error:
  //close_second_handler(); -- caller does that
  DBUG_RETURN(res);
}


void DsMrr_impl::close_second_handler()
{
  if (h2)
  {
    h2->ha_index_or_rnd_end();
    h2->ha_external_lock(current_thd, F_UNLCK);
    h2->close();
    delete h2;
    h2= NULL;
  }
}


void DsMrr_impl::dsmrr_close()
{
  DBUG_ENTER("DsMrr_impl::dsmrr_close");
  close_second_handler();
  strategy= NULL;
  DBUG_VOID_RETURN;
}


/* 
  my_qsort2-compatible function to compare key tuples 
*/

int Mrr_ordered_index_reader::key_tuple_cmp(void* arg, uchar* key1, uchar* key2)
{
  Mrr_ordered_index_reader *this_= (Mrr_ordered_index_reader*)arg;
  TABLE *table= this_->h->get_table();
  int res;
  KEY_PART_INFO *part= table->key_info[this_->h->active_index].key_part;
  
  if (this_->keypar.use_key_pointers)
  {
    /* the buffer stores pointers to keys, get to the keys */
    key1= *((uchar**)key1);
    key2= *((uchar**)key2);  // todo is this alignment-safe?
  }

  uchar *key1_end= key1 + this_->keypar.key_tuple_length;

  while (key1 < key1_end)
  {
    Field* f = part->field;
    int len = part->store_length;
    if (part->null_bit)
    {
      if (*key1) // key1 == NULL
      {
        if (!*key2) // key1(NULL) < key2(notNULL)
          return -1;
        goto equals;
      }
      else if (*key2) // key1(notNULL) > key2 (NULL)
        return 1;
      // Step over NULL byte for f->cmp().
      key1++;
      key2++;
      len--;
    }
    
    if ((res= f->key_cmp(key1, key2)))
      return res;
equals:
    key1 += len;
    key2 += len;
    part++;
  }
  return 0;
}


int Mrr_ordered_index_reader::key_tuple_cmp_reverse(void* arg, uchar* key1, uchar* key2)
{
  return -key_tuple_cmp(arg, key1, key2);
}


/**
  Setup key/rowid buffer sizes based on sample_key and its length.
  
  @param
    sample_key  A lookup key to use as a sample. It is assumed that
                all other keys will have the same length/etc.
  @note
    This function must be called when all buffers are empty
*/

void DsMrr_impl::setup_buffer_sizes(uint key_size_in_keybuf, 
                                    key_part_map key_tuple_map)
{
  uint key_buff_elem_size= key_size_in_keybuf + 
                           (int)is_mrr_assoc * sizeof(void*);
  
  KEY *key_info= &h->get_table()->key_info[keyno];
  if (strategy == index_strategy)
  {
    /* Give all space to the key buffer, key buffer must be forward */
    key_buffer= &forward_key_buf;
    key_buffer->set_buffer_space(full_buf, full_buf_end);

    /* Just in case, tell rowid buffer that it has zero size: */
    rowid_buffer.set_buffer_space(full_buf_end, full_buf_end);
    return;
  }
  
  /* 
    Ok if we got here we need to allocate one part of the buffer 
    for keys and another part for rowids.
  */
  uint rowid_buf_elem_size= h->ref_length + 
                            (int)is_mrr_assoc * sizeof(char*);
  
  /*
    Use rec_per_key statistics as a basis to find out how many rowids 
    we'll get for each key value.
     TODO: are we guaranteed to get r_p_c==1 for unique keys?
     TODO: what should be the default value to use when there is no 
           statistics?
  */
  uint parts= my_count_bits(key_tuple_map);
  ulong rpc;
  if ((rpc= key_info->rec_per_key[parts - 1]))
  {
    rowid_buf_elem_size *= rpc;
  }

  double fraction_for_rowids=
    ((double) rowid_buf_elem_size / 
         ((double)rowid_buf_elem_size + key_buff_elem_size));

  size_t bytes_for_rowids= 
    round(fraction_for_rowids * (full_buf_end - full_buf));
  
  uint bytes_for_keys= (full_buf_end - full_buf) - bytes_for_rowids;

  if (bytes_for_keys < key_buff_elem_size + 1)
  {
    uint add= key_buff_elem_size + 1 - bytes_for_keys;
    bytes_for_rowids -= add;
    DBUG_ASSERT(bytes_for_rowids >= 
                (h->ref_length + (int)is_mrr_assoc * sizeof(char*) + 1));
  }

  rowid_buffer_end= full_buf + bytes_for_rowids;
  rowid_buffer.set_buffer_space(full_buf, rowid_buffer_end);
  key_buffer= &backward_key_buf;
  key_buffer->set_buffer_space(rowid_buffer_end, full_buf_end); 
}


void DsMrr_impl::reset_buffer_sizes()
{
  if (strategy != index_strategy)
  {
    /*
      Ok we have both ordered index reader and there is a disk rearder. 
      Redistribute the buffer space.
    */
    rowid_buffer.set_buffer_space(full_buf, rowid_buffer_end);
    key_buffer= &backward_key_buf;
    key_buffer->set_buffer_space(rowid_buffer_end, full_buf_end);
  }
}

/**
  Take unused space from the key buffer and give it to the rowid buffer
*/
//psergey-todo: do invoke this function.
void DsMrr_impl::reallocate_buffer_space()
{
  uchar *unused_start, *unused_end;
  key_buffer->remove_unused_space(&unused_start, &unused_end);
  rowid_buffer.grow(unused_start, unused_end);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

bool Key_value_records_iterator::init(Mrr_ordered_index_reader *owner_arg)
{
  int res;
  //h= h_arg;
  //param= param_arg;
  owner= owner_arg;

  identical_key_it.init(owner->key_buffer);
  /* Get the first pair into (cur_index_tuple, cur_range_info) */ 
  owner->key_buffer->setup_reading(&cur_index_tuple, owner->keypar.key_size_in_keybuf,
                            owner->is_mrr_assoc? (uchar**)&owner->cur_range_info: NULL,
                            sizeof(void*));

  if (identical_key_it.read())
    return TRUE;

  uchar *key_in_buf= cur_index_tuple;

  last_identical_key_ptr= cur_index_tuple;
  if (owner->keypar.use_key_pointers)
    cur_index_tuple= *((uchar**)cur_index_tuple);
  
  /* Check out how many more identical keys are following */
  uchar *save_cur_index_tuple= cur_index_tuple;
  while (!identical_key_it.read())
  {
    if (Mrr_ordered_index_reader::key_tuple_cmp(owner, key_in_buf, cur_index_tuple))
      break;
    last_identical_key_ptr= cur_index_tuple;
  }
  identical_key_it.init(owner->key_buffer);
  cur_index_tuple= save_cur_index_tuple;
  res= owner->h->ha_index_read_map(owner->h->get_table()->record[0], 
                            cur_index_tuple, 
                            owner->keypar.key_tuple_map, 
                            HA_READ_KEY_EXACT);

  if (res)
  {
    close();
    return res; /* Fatal error */
  }
  get_next_row= FALSE;
  return 0;
}


int Key_value_records_iterator::get_next()
{
  int res;

  if (get_next_row)
  {
    if (owner->keypar.index_ranges_unique)
      return HA_ERR_END_OF_FILE;  /* Max one match */
    
    handler *h= owner->h;
    if ((res= h->ha_index_next_same(h->get_table()->record[0], 
                                    cur_index_tuple, 
                                    owner->keypar.key_tuple_length)))
    {
      /* EOF is EOF for iterator, also, any error means EOF on the iterator */
      return res;
    }
    identical_key_it.init(owner->key_buffer);
    get_next_row= FALSE;
  }

  identical_key_it.read(); // This gets us next range_id.
  if (!last_identical_key_ptr || (cur_index_tuple == last_identical_key_ptr))
  {
    get_next_row= TRUE;
  }
  return 0;
}

void Key_value_records_iterator::close()
{
  while (!owner->key_buffer->read() && 
         (cur_index_tuple != last_identical_key_ptr)) {}
}


/**
  DS-MRR implementation: multi_range_read_next() function.

  Calling convention is like multi_range_read_next() has.
*/

int DsMrr_impl::dsmrr_next(char **range_info)
{
  int res;
  while ((res= strategy->get_next(range_info)) == HA_ERR_END_OF_FILE)
  {
    if ((res= strategy->refill_buffer()))
      break; /* EOF or error */
  }
  return res;
  //return strategy->get_next(range_info);
}


/**
  DS-MRR implementation: multi_range_read_info() function
*/
ha_rows DsMrr_impl::dsmrr_info(uint keyno, uint n_ranges, uint rows, 
                               uint key_parts,
                               uint *bufsz, uint *flags, COST_VECT *cost)
{  
  ha_rows res;
  uint def_flags= *flags;
  uint def_bufsz= *bufsz;

  /* Get cost/flags/mem_usage of default MRR implementation */
  res= h->handler::multi_range_read_info(keyno, n_ranges, rows, key_parts, 
                                         &def_bufsz, &def_flags, cost);
  DBUG_ASSERT(!res);

  if ((*flags & HA_MRR_USE_DEFAULT_IMPL) || 
      choose_mrr_impl(keyno, rows, &def_flags, &def_bufsz, cost))
  {
    /* Default implementation is choosen */
    DBUG_PRINT("info", ("Default MRR implementation choosen"));
    *flags= def_flags;
    *bufsz= def_bufsz;
  }
  else
  {
    /* *flags and *bufsz were set by choose_mrr_impl */
    DBUG_PRINT("info", ("DS-MRR implementation choosen"));
  }
  return 0;
}


/**
  DS-MRR Implementation: multi_range_read_info_const() function
*/

ha_rows DsMrr_impl::dsmrr_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                 void *seq_init_param, uint n_ranges, 
                                 uint *bufsz, uint *flags, COST_VECT *cost)
{
  ha_rows rows;
  uint def_flags= *flags;
  uint def_bufsz= *bufsz;
  /* Get cost/flags/mem_usage of default MRR implementation */
  rows= h->handler::multi_range_read_info_const(keyno, seq, seq_init_param,
                                                n_ranges, &def_bufsz, 
                                                &def_flags, cost);
  if (rows == HA_POS_ERROR)
  {
    /* Default implementation can't perform MRR scan => we can't either */
    return rows;
  }

  /*
    If HA_MRR_USE_DEFAULT_IMPL has been passed to us, that is an order to
    use the default MRR implementation (we need it for UPDATE/DELETE).
    Otherwise, make a choice based on cost and @@optimizer_use_mrr.
  */
  if ((*flags & HA_MRR_USE_DEFAULT_IMPL) ||
      choose_mrr_impl(keyno, rows, flags, bufsz, cost))
  {
    DBUG_PRINT("info", ("Default MRR implementation choosen"));
    *flags= def_flags;
    *bufsz= def_bufsz;
  }
  else
  {
    /* *flags and *bufsz were set by choose_mrr_impl */
    DBUG_PRINT("info", ("DS-MRR implementation choosen"));
  }
  return rows;
}


/**
  Check if key has partially-covered columns

  We can't use DS-MRR to perform range scans when the ranges are over
  partially-covered keys, because we'll not have full key part values
  (we'll have their prefixes from the index) and will not be able to check
  if we've reached the end the range.

  @param keyno  Key to check

  @todo
    Allow use of DS-MRR in cases where the index has partially-covered
    components but they are not used for scanning.

  @retval TRUE   Yes
  @retval FALSE  No
*/

bool key_uses_partial_cols(TABLE *table, uint keyno)
{
  KEY_PART_INFO *kp= table->key_info[keyno].key_part;
  KEY_PART_INFO *kp_end= kp + table->key_info[keyno].key_parts;
  for (; kp != kp_end; kp++)
  {
    if (!kp->field->part_of_key.is_set(keyno))
      return TRUE;
  }
  return FALSE;
}


/*
  Check if key/flags allow DS-MRR/CPK strategy to be used
  
  @param thd
  @param keyno      Index that will be used
  @param  mrr_flags  
  
  @retval TRUE   DS-MRR/CPK should be used
  @retval FALSE  Otherwise
*/

bool DsMrr_impl::check_cpk_scan(THD *thd, uint keyno, uint mrr_flags)
{
  return test((mrr_flags & HA_MRR_SINGLE_POINT) &&  // check
        //      !(mrr_flags & HA_MRR_SORTED) && 
              keyno == table->s->primary_key && 
              h->primary_key_is_clustered() && 
              optimizer_flag(thd, OPTIMIZER_SWITCH_MRR_SORT_KEYS)); //check
}


/*
  DS-MRR Internals: Choose between Default MRR implementation and DS-MRR

  Make the choice between using Default MRR implementation and DS-MRR.
  This function contains common functionality factored out of dsmrr_info()
  and dsmrr_info_const(). The function assumes that the default MRR
  implementation's applicability requirements are satisfied.

  @param keyno       Index number
  @param rows        E(full rows to be retrieved)
  @param flags  IN   MRR flags provided by the MRR user
                OUT  If DS-MRR is choosen, flags of DS-MRR implementation
                     else the value is not modified
  @param bufsz  IN   If DS-MRR is choosen, buffer use of DS-MRR implementation
                     else the value is not modified
  @param cost   IN   Cost of default MRR implementation
                OUT  If DS-MRR is choosen, cost of DS-MRR scan
                     else the value is not modified

  @retval TRUE   Default MRR implementation should be used
  @retval FALSE  DS-MRR implementation should be used
*/


bool DsMrr_impl::choose_mrr_impl(uint keyno, ha_rows rows, uint *flags,
                                 uint *bufsz, COST_VECT *cost)
{
  COST_VECT dsmrr_cost;
  bool res;
  THD *thd= current_thd;

  bool doing_cpk_scan= check_cpk_scan(thd, keyno, *flags); 
  bool using_cpk= test(keyno == table->s->primary_key &&
                       h->primary_key_is_clustered());
  if (thd->variables.optimizer_use_mrr == 2 || *flags & HA_MRR_INDEX_ONLY ||
      (using_cpk && !doing_cpk_scan) || key_uses_partial_cols(table, keyno))
  {
    /* Use the default implementation */
    *flags |= HA_MRR_USE_DEFAULT_IMPL;
    return TRUE;
  }

  uint add_len= table->key_info[keyno].key_length + h->ref_length; 
  *bufsz -= add_len;
  if (get_disk_sweep_mrr_cost(keyno, rows, *flags, bufsz, &dsmrr_cost))
    return TRUE;
  *bufsz += add_len;
  
  bool force_dsmrr;
  /* 
    If @@optimizer_use_mrr==force, then set cost of DS-MRR to be minimum of
    DS-MRR and Default implementations cost. This allows one to force use of
    DS-MRR whenever it is applicable without affecting other cost-based
    choices.
  */
  if ((force_dsmrr= (thd->variables.optimizer_use_mrr == 1)) &&
      dsmrr_cost.total_cost() > cost->total_cost())
    dsmrr_cost= *cost;

  if (force_dsmrr || dsmrr_cost.total_cost() <= cost->total_cost())
  {
    *flags &= ~HA_MRR_USE_DEFAULT_IMPL;  /* Use the DS-MRR implementation */
    *flags &= ~HA_MRR_SORTED;          /* We will return unordered output */
    *cost= dsmrr_cost;
    res= FALSE;

    if ((*flags & HA_MRR_SINGLE_POINT) && 
         optimizer_flag(thd, OPTIMIZER_SWITCH_MRR_SORT_KEYS))
      *flags |= HA_MRR_MATERIALIZED_KEYS;
  }
  else
  {
    /* Use the default MRR implementation */
    res= TRUE;
  }
  return res;
}


static void get_sort_and_sweep_cost(TABLE *table, ha_rows nrows, COST_VECT *cost);


/**
  Get cost of DS-MRR scan

  @param keynr              Index to be used
  @param rows               E(Number of rows to be scanned)
  @param flags              Scan parameters (HA_MRR_* flags)
  @param buffer_size INOUT  Buffer size
  @param cost        OUT    The cost

  @retval FALSE  OK
  @retval TRUE   Error, DS-MRR cannot be used (the buffer is too small
                 for even 1 rowid)
*/

bool DsMrr_impl::get_disk_sweep_mrr_cost(uint keynr, ha_rows rows, uint flags,
                                         uint *buffer_size, COST_VECT *cost)
{
  ulong max_buff_entries, elem_size;
  ha_rows rows_in_full_step, rows_in_last_step;
  uint n_full_steps;
  double index_read_cost;

  elem_size= h->ref_length + sizeof(void*) * (!test(flags & HA_MRR_NO_ASSOCIATION));
  max_buff_entries = *buffer_size / elem_size;

  if (!max_buff_entries)
    return TRUE; /* Buffer has not enough space for even 1 rowid */

  /* Number of iterations we'll make with full buffer */
  n_full_steps= (uint)floor(rows2double(rows) / max_buff_entries);
  
  /* 
    Get numbers of rows we'll be processing in 
     - non-last sweep, with full buffer 
     - last iteration, with non-full buffer
  */
  rows_in_full_step= max_buff_entries;
  rows_in_last_step= rows % max_buff_entries;
  
  /* Adjust buffer size if we expect to use only part of the buffer */
  if (n_full_steps)
  {
    get_sort_and_sweep_cost(table, rows, cost);
    cost->multiply(n_full_steps);
  }
  else
  {
    cost->zero();
    *buffer_size= max(*buffer_size, 
                      (size_t)(1.2*rows_in_last_step) * elem_size + 
                      h->ref_length + table->key_info[keynr].key_length);
  }
  
  COST_VECT last_step_cost;
  get_sort_and_sweep_cost(table, rows_in_last_step, &last_step_cost);
  cost->add(&last_step_cost);
 
  if (n_full_steps != 0)
    cost->mem_cost= *buffer_size;
  else
    cost->mem_cost= (double)rows_in_last_step * elem_size;
  
  /* Total cost of all index accesses */
  index_read_cost= h->keyread_time(keynr, 1, (double)rows);
  cost->add_io(index_read_cost, 1 /* Random seeks */);
  return FALSE;
}


/* 
  Get cost of one sort-and-sweep step
  
  It consists of two parts:
   - sort an array of #nrows ROWIDs using qsort
   - read #nrows records from table in a sweep.

  @param table       Table being accessed
  @param nrows       Number of rows to be sorted and retrieved
  @param cost   OUT  The cost of scan
*/

static 
void get_sort_and_sweep_cost(TABLE *table, ha_rows nrows, COST_VECT *cost)
{
  if (nrows)
  {
    get_sweep_read_cost(table, nrows, FALSE, cost);
    /* Add cost of qsort call: n * log2(n) * cost(rowid_comparison) */
    double cmp_op= rows2double(nrows) * (1.0 / TIME_FOR_COMPARE_ROWID);
    if (cmp_op < 3)
      cmp_op= 3;
    cost->cpu_cost += cmp_op * log2(cmp_op);
  }
  else
    cost->zero();
}


/**
  Get cost of reading nrows table records in a "disk sweep"

  A disk sweep read is a sequence of handler->rnd_pos(rowid) calls that made
  for an ordered sequence of rowids.

  We assume hard disk IO. The read is performed as follows:

   1. The disk head is moved to the needed cylinder
   2. The controller waits for the plate to rotate
   3. The data is transferred

  Time to do #3 is insignificant compared to #2+#1.

  Time to move the disk head is proportional to head travel distance.

  Time to wait for the plate to rotate depends on whether the disk head
  was moved or not. 

  If disk head wasn't moved, the wait time is proportional to distance
  between the previous block and the block we're reading.

  If the head was moved, we don't know how much we'll need to wait for the
  plate to rotate. We assume the wait time to be a variate with a mean of
  0.5 of full rotation time.

  Our cost units are "random disk seeks". The cost of random disk seek is
  actually not a constant, it depends one range of cylinders we're going
  to access. We make it constant by introducing a fuzzy concept of "typical 
  datafile length" (it's fuzzy as it's hard to tell whether it should
  include index file, temp.tables etc). Then random seek cost is:

    1 = half_rotation_cost + move_cost * 1/3 * typical_data_file_length

  We define half_rotation_cost as DISK_SEEK_BASE_COST=0.9.

  @param table             Table to be accessed
  @param nrows             Number of rows to retrieve
  @param interrupted       TRUE <=> Assume that the disk sweep will be
                           interrupted by other disk IO. FALSE - otherwise.
  @param cost         OUT  The cost.
*/

void get_sweep_read_cost(TABLE *table, ha_rows nrows, bool interrupted, 
                         COST_VECT *cost)
{
  DBUG_ENTER("get_sweep_read_cost");

  cost->zero();
  if (table->file->primary_key_is_clustered())
  {
    cost->io_count= table->file->read_time(table->s->primary_key,
                                           (uint) nrows, nrows);
  }
  else
  {
    double n_blocks=
      ceil(ulonglong2double(table->file->stats.data_file_length) / IO_SIZE);
    double busy_blocks=
      n_blocks * (1.0 - pow(1.0 - 1.0/n_blocks, rows2double(nrows)));
    if (busy_blocks < 1.0)
      busy_blocks= 1.0;

    DBUG_PRINT("info",("sweep: nblocks=%g, busy_blocks=%g", n_blocks,
                       busy_blocks));
    cost->io_count= busy_blocks;

    if (!interrupted)
    {
      /* Assume reading is done in one 'sweep' */
      cost->avg_io_cost= (DISK_SEEK_BASE_COST +
                          DISK_SEEK_PROP_COST*n_blocks/busy_blocks);
    }
  }
  DBUG_PRINT("info",("returning cost=%g", cost->total_cost()));
  DBUG_VOID_RETURN;
}


/* **************************************************************************
 * DS-MRR implementation ends
 ***************************************************************************/


