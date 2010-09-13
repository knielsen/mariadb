/*
  This file contains declarations for Disk-Sweep MultiRangeRead (DS-MRR) 
  implementation
*/

/**
  A Disk-Sweep implementation of MRR Interface (DS-MRR for short)

  This is a "plugin"(*) for storage engines that allows make index scans 
  read table rows in rowid order. For disk-based storage engines, this is
  faster than reading table rows in whatever-SQL-layer-makes-calls-in order.

  (*) - only conceptually. No dynamic loading or binary compatibility of any
        kind.

  General scheme of things:
   
      SQL Layer code
       |   |   |
      -v---v---v---- handler->multi_range_read_XXX() function calls
       |   |   |
      ____________________________________
     / DS-MRR module                      \
     |  (scan indexes, order rowids, do    |
     |   full record reads in rowid order) |
     \____________________________________/
       |   |   |
      -|---|---|----- handler->read_range_first()/read_range_next(), 
       |   |   |      handler->index_read(), handler->rnd_pos() calls.
       |   |   |
       v   v   v
      Storage engine internals
   
  Currently DS-MRR is used by MyISAM, InnoDB/XtraDB and Maria storage engines.
  Potentially it can be used with any table handler that has disk-based data
  storage and has better performance when reading data in rowid order.
*/


/*
  A simple memory buffer for reading and writing.

  when writing, there is no user-visible "current" position, although
  internally 'pos' points to just after the end of used area  (or at the 
  start of it for reverse buffer).

  When reading, there is current position pointing at start (for reverse
  buffer, end) of the element that will be read next.
   ^^ why end for reverse? it's more logical to point at start 
*/

class SimpleBuffer
{
  uchar *start;
  uchar *end;
  uchar *read_pos;
  uchar *write_pos;
  
  /*
     1 <=> buffer grows/is filled/is read from start to end
    -1 <=> everthing is done from end to start instead.
  */
  int direction;
  
  /* Pointers to read data from */
  uchar **write_ptr1;
  size_t write_size1;
  /* Same as above, but may be NULL */
  uchar **write_ptr2;
  size_t write_size2;

  /* Pointers to write data to */
  uchar **read_ptr1;
  size_t read_size1;
  /* Same as above, but may be NULL */
  uchar **read_ptr2;
  size_t read_size2;

  bool have_space_for(size_t bytes);
  uchar *used_area() { return (direction == 1)? read_pos : write_pos; }
  size_t used_size();

  void write(const uchar *data, size_t bytes);
  uchar *read(size_t bytes);

public:
  /* Set up writing*/
  void setup_writing(uchar **data1, size_t len1, 
                     uchar **data2, size_t len2);

  void sort(qsort2_cmp cmp_func, void *cmp_func_arg);

  /* Write-mode functions */
  void reset_for_writing();
  void write();
  bool can_write();

  bool is_empty() { return used_size() == 0; }

  /* Read-mode functions */
  void reset_for_reading();
  // todo: join with setup-writing? (but what for?)
  void setup_reading(uchar **data1, size_t len1, 
                     uchar **data2, size_t len2);
  bool read();

  bool have_data(size_t bytes);
  uchar *end_of_space();

  /* Control functions */
  void set_buffer_space(uchar *start_arg, uchar *end_arg, int direction_arg) 
  {
    start= start_arg;
    end= end_arg;
    direction= direction_arg;
    TRASH(start, end - start);
    reset_for_writing();
  }
  
  /*
    Stop/return the unneded space (the one that we have wrote to and have read
    from.
  */
  void remove_unused_space(uchar **unused_start, uchar **unused_end)
  {
    if (direction == 1)
    {
      *unused_start= start;
      *unused_end= read_pos;
      start= read_pos;
    }
    else
    {
      *unused_start= read_pos;
      *unused_end= end;
      end= read_pos;
    }
  }

  void flip()
  {
    uchar *tmp= read_pos;
    read_pos= write_pos;
    write_pos= tmp;
    direction= -direction;
  }
  bool is_reverse() { return direction == -1; }

  void grow(uchar *unused_start, uchar *unused_end)
  {
    /*
      Passed memory area can be meaningfully used for growing the buffer if:
      - it is adjacent to buffer space we're using
      - it is on the end towards which we grow.
    */
    DBUG_ASSERT(unused_end >= unused_start);
    TRASH(unused_start, unused_end - unused_start);
    if (direction == 1 && end == unused_start)
    {
      end= unused_end;
    }
    else if (direction == -1 && start == unused_end)
    {
      start= unused_start;
    }
    else
      DBUG_ASSERT(0); /* Attempt to grow buffer in wrong direction */
  }
  
  /*
    An iterator to do look at what we're about to read from the buffer without
    actually reading it.
  */
  class PeekIterator
  {
    // if direction==1 : pointer to what to return next
    // if direction==-1: pointer to the end of what is to be returned next
    uchar *pos;
    SimpleBuffer *sb;
  public:
    void init(SimpleBuffer *sb_arg)
    {
      sb= sb_arg;
      pos= sb->read_pos;
    }
    
    /*
      If the buffer stores tuples, this call will return pointer to the first
      component.
    */
    bool read_next()
    {
      // Always read the first component first? (because we do inverted-writes
      // if needed, so no measures need to be taken here).
      uchar *res;
      if ((res= get_next(sb->read_size1)))
      {
        *(sb->read_ptr1)= res;
        if (sb->read_ptr2)
          *sb->read_ptr2= get_next(sb->read_size2);
        return FALSE;
      }
      return TRUE; /* EOF */
    }
  private:
    /* Return pointer to next chunk of nbytes bytes and avance over it */
    uchar *get_next(size_t nbytes)
    {
      if (sb->direction == 1)
      {
        if (pos + nbytes > sb->write_pos)
          return NULL;
        uchar *res= pos;
        pos += nbytes;
        return res;
      }
      else
      {
        if (pos - nbytes < sb->write_pos)
          return NULL;
        pos -= nbytes;
        return pos;
      }
    }
  };
};


/*
  DS-MRR implementation for one table. Create/use one object of this class for
  each ha_{myisam/innobase/etc} object. That object will be further referred to
  as "the handler"

  There are actually three strategies
   S1. Bypass DS-MRR, pass all calls to default implementation (i.e. to
      MRR-to-non-MRR calls converter)
   S2. Regular DS-MRR 
   S3. DS-MRR/CPK for doing scans on clustered primary keys.

  S1 is used for cases which DS-MRR is unable to handle for some reason.

  S2 is the actual DS-MRR. The basic algorithm is as follows:
    1. Scan the index (and only index, that is, with HA_EXTRA_KEYREAD on) and 
        fill the buffer with {rowid, range_id} pairs
    2. Sort the buffer by rowid
    3. for each {rowid, range_id} pair in the buffer
         get record by rowid and return the {record, range_id} pair
    4. Repeat the above steps until we've exhausted the list of ranges we're
       scanning.

  S3 is the variant of DS-MRR for use with clustered primary keys (or any
  clustered index). The idea is that in clustered index it is sufficient to 
  access the index in index order, and we don't need an intermediate steps to
  get rowid (like step #1 in S2).

   DS-MRR/CPK's basic algorithm is as follows:
    1. Collect a number of ranges (=lookup keys)
    2. Sort them so that they follow in index order.
    3. for each {lookup_key, range_id} pair in the buffer 
       get record(s) matching the lookup key and return {record, range_id} pairs
    4. Repeat the above steps until we've exhausted the list of ranges we're
       scanning.
*/

class DsMrr_impl
{
public:
  typedef void (handler::*range_check_toggle_func_t)(bool on);

  DsMrr_impl()
    : h2(NULL) {};
  
  void init(handler *h_arg, TABLE *table_arg)
  {
    h= h_arg; 
    table= table_arg;
  }
  int dsmrr_init(handler *h, RANGE_SEQ_IF *seq_funcs, void *seq_init_param, 
                 uint n_ranges, uint mode, HANDLER_BUFFER *buf);
  void dsmrr_close();
  int dsmrr_next(char **range_info);

  ha_rows dsmrr_info(uint keyno, uint n_ranges, uint keys, uint key_parts, 
                     uint *bufsz, uint *flags, COST_VECT *cost);

  ha_rows dsmrr_info_const(uint keyno, RANGE_SEQ_IF *seq, 
                            void *seq_init_param, uint n_ranges, uint *bufsz,
                            uint *flags, COST_VECT *cost);
private:
  /*
    The "owner" handler object (the one that calls dsmrr_XXX functions.
    It is used to retrieve full table rows by calling rnd_pos().
  */
  handler *h;
  TABLE *table; /* Always equal to h->table */

  /* Secondary handler object.  It is used for scanning the index */
  handler *h2;

  uchar *full_buf;
  uchar *full_buf_end;
  
  /* Valid when using both rowid and key buffer: the original bound between them */
  uchar *rowid_buffer_end;

  /* Buffer to store rowids, or (rowid, range_id) pairs */
  SimpleBuffer rowid_buffer;
  
  /*  Reads from rowid buffer go to here: */
  uchar *rowid;
  uchar *rowids_range_id;
  
  /*
    not-NULL: we're traversing a group of (rowid, range_id) pairs with
              identical rowid values, and this is the pointer to the last one.
    NULL: we're not in the group of indentical rowids.
  */
  uchar *last_identical_rowid;
  
  /* Identical keys */
  bool in_identical_keys_range;
  uchar *last_identical_key_ptr;
  SimpleBuffer::PeekIterator identical_key_it;

  SimpleBuffer key_buffer;
  
  uint keyno;

  /* Execution control */
  bool do_sort_keys;
  bool use_key_pointers;
  bool do_rowid_fetch;

  bool dsmrr_eof; /* TRUE <=> We have reached EOF when reading index tuples */
  
  /* 
    TRUE <=> key buffer is exhausted (we need this because we may have a situation
    where we've read everything from the key buffer but haven't finished with
    scanning the last range)
  */
  bool key_eof;

  /* TRUE <=> need range association, buffer holds {rowid, range_id} pairs */
  bool is_mrr_assoc;

  bool use_default_impl; /* TRUE <=> shortcut all calls to default MRR impl */

  bool doing_cpk_scan; /* TRUE <=> DS-MRR/CPK variant is used */

  
  /* Initially FALSE, becomes TRUE when we've set key_tuple_xxx members */
  bool know_key_tuple_params;
  /* Length of lookup tuple being used, in bytes */
  uint key_tuple_length;
  key_part_map key_tuple_map; 
  /*
    This is 
      = key_tuple_length   if we copy keys to buffer
      = sizeof(void*)      if we're using pointers to materialized keys.
  */
  uint key_size_in_keybuf;
  
  /* = key_size_in_keybuf [ + sizeof(range_assoc_info) ] */
  uint key_buff_elem_size;
  
  /* = h->ref_length  [ + sizeof(range_assoc_info) ] */
  uint rowid_buff_elem_size;
  
  /*
    TRUE <=> We're scanning on a full primary key (and not on prefix), and so 
    can get max. one match for each key 
  */
  bool index_ranges_unique;
  /* TRUE<=> we're in a middle of enumerating records from a range */ 
  bool in_index_range;
  uchar *cur_index_tuple;

  /* if in_index_range==TRUE: range_id of the range we're enumerating */
  char *cur_range_info;

  char *first_identical_range_info;

  bool choose_mrr_impl(uint keyno, ha_rows rows, uint *flags, uint *bufsz, 
                       COST_VECT *cost);
  bool get_disk_sweep_mrr_cost(uint keynr, ha_rows rows, uint flags, 
                               uint *buffer_size, COST_VECT *cost);
  bool check_cpk_scan(uint keyno, uint mrr_flags);
  static int key_tuple_cmp(void* arg, uchar* key1, uchar* key2);
  int dsmrr_fill_rowid_buffer();
  void dsmrr_fill_key_buffer();
  int dsmrr_next_from_index(char **range_info);

  void setup_buffer_sizes(key_range *sample_key);

  static range_seq_t key_buf_seq_init(void *init_param, uint n_ranges, uint flags);
  static uint key_buf_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range);
};

