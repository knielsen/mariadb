/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_digest.h
  Statement Digest data structures (implementation).
*/

/*
  This code needs extra visibility in the lexer structures
*/

#include "my_global.h"
#include "my_sys.h"
#include "pfs_instr.h"
#include "pfs_digest.h"
#include "pfs_global.h"
#include "table_helper.h"
#include "my_md5.h"
#include "sql_lex.h"
#include <string.h>

/* Generated code */
#include "../sql/sql_yacc.h"
#include "../storage/perfschema/pfs_lex_token.h"

/* Name pollution from sql/sql_lex.h */
#ifdef LEX_YYSTYPE
#undef LEX_YYSTYPE
#endif

#define LEX_YYSTYPE YYSTYPE

/**
  Token array : 
  Token array is an array of bytes to store tokens recieved during parsing.
  Following is the way token array is formed.
     
      ...<non-id-token><non-id-token><id-token><id_len><id_text>...

  For Ex:
  SELECT * FROM T1;
  <SELECT_TOKEN><*><FROM_TOKEN><ID_TOKEN><2><T1>
*/

ulong digest_max= 0;
ulong digest_lost= 0;


/** EVENTS_STATEMENTS_HISTORY_LONG circular buffer. */
PFS_statements_digest_stat *statements_digest_stat_array= NULL;
/** Consumer flag for table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST. */
bool flag_statements_digest= true;
/** 
  Current index in Stat array where new record is to be inserted.
  index 0 is reserved for "all else" case when entire array is full.
*/
volatile uint32 digest_index= 1;

static LF_HASH digest_hash;
static bool digest_hash_inited= false;

/**
  Initialize table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST.
  @param digest_sizing      
*/
int init_digest(const PFS_global_param *param)
{
  unsigned int index;

  /*
    Allocate memory for statements_digest_stat_array based on
    performance_schema_digests_size values
  */
  digest_max= param->m_digest_sizing;
  digest_lost= 0;

  if (digest_max == 0)
    return 0;

  statements_digest_stat_array=
    PFS_MALLOC_ARRAY(digest_max, PFS_statements_digest_stat,
                     MYF(MY_ZEROFILL));
  if (unlikely(statements_digest_stat_array == NULL))
    return 1;

  for (index= 0; index < digest_max; index++)
  {
    statements_digest_stat_array[index].reset_data();
  }

  return 0;
}

/** Cleanup table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST. */
void cleanup_digest(void)
{
  /*  Free memory allocated to statements_digest_stat_array. */
  pfs_free(statements_digest_stat_array);
  statements_digest_stat_array= NULL;
}

C_MODE_START
static uchar *digest_hash_get_key(const uchar *entry, size_t *length,
                                  my_bool)
{
  const PFS_statements_digest_stat * const *typed_entry;
  const PFS_statements_digest_stat *digest;
  const void *result;
  typed_entry= reinterpret_cast<const PFS_statements_digest_stat*const*>(entry);
  DBUG_ASSERT(typed_entry != NULL);
  digest= *typed_entry;
  DBUG_ASSERT(digest != NULL);
  *length= PFS_MD5_SIZE; 
  result= digest->m_digest_hash.m_md5;
  return const_cast<uchar*> (reinterpret_cast<const uchar*> (result));
}
C_MODE_END


/**
  Initialize the digest hash.
  @return 0 on success
*/
int init_digest_hash(void)
{
  if (! digest_hash_inited)
  {
    lf_hash_init(&digest_hash, sizeof(PFS_statements_digest_stat*),
                 LF_HASH_UNIQUE, 0, 0, digest_hash_get_key,
                 &my_charset_bin);
    digest_hash_inited= true;
  }
  return 0;
}

void cleanup_digest_hash(void)
{
  if (digest_hash_inited)
  {
    lf_hash_destroy(&digest_hash);
    digest_hash_inited= false;
  }
}

static LF_PINS* get_digest_hash_pins(PFS_thread *thread)
{
  if (unlikely(thread->m_digest_hash_pins == NULL))
  {
    if (!digest_hash_inited)
      return NULL;
    thread->m_digest_hash_pins= lf_hash_get_pins(&digest_hash);
  }
  return thread->m_digest_hash_pins;
}

PFS_statement_stat*
find_or_create_digest(PFS_thread* thread,
                      PSI_digest_storage* digest_storage)
{
  if (statements_digest_stat_array == NULL)
    return NULL;

  if (digest_storage->m_byte_count <= 0)
    return NULL;

  LF_PINS *pins= get_digest_hash_pins(thread);
  if (unlikely(pins == NULL))
    return NULL;

  /* Compute MD5 Hash of the tokens received. */
  PFS_digest_hash md5;
  compute_md5_hash((char *) md5.m_md5,
                   (char *) digest_storage->m_token_array,
                   digest_storage->m_byte_count);

  unsigned char* hash_key= md5.m_md5;

  int res;
  ulong safe_index;
  uint retry_count= 0;
  const uint retry_max= 3;
  PFS_statements_digest_stat **entry;
  PFS_statements_digest_stat *pfs= NULL;

  ulonglong now= microsecond_interval_timer();

search:

  /* Lookup LF_HASH using this new key. */
  entry= reinterpret_cast<PFS_statements_digest_stat**>
    (lf_hash_search(&digest_hash, pins,
                    hash_key, PFS_MD5_SIZE));

  if (entry && (entry != MY_ERRPTR))
  {
    /* If digest already exists, update stats and return. */
    pfs= *entry;
    pfs->m_last_seen= now;
    lf_hash_search_unpin(pins);
    return & pfs->m_stat;
  }

  lf_hash_search_unpin(pins);

  /* Dirty read of digest_index */
  if (digest_index == 0)
  {
    /*  digest_stat array is full. Add stat at index 0 and return. */
    pfs= &statements_digest_stat_array[0];

    if (pfs->m_first_seen == 0)
      pfs->m_first_seen= now;
    pfs->m_last_seen= now;
    return & pfs->m_stat;
  }

  safe_index= PFS_atomic::add_u32(& digest_index, 1);
  if (safe_index >= digest_max)
  {
    /* The digest array is now full. */
    digest_index= 0;
    pfs= &statements_digest_stat_array[0];

    if (pfs->m_first_seen == 0)
      pfs->m_first_seen= now;
    pfs->m_last_seen= now;
    return & pfs->m_stat;
  }

  /* Add a new record in digest stat array. */
  pfs= &statements_digest_stat_array[safe_index];

  /* Copy digest hash/LF Hash search key. */
  memcpy(pfs->m_digest_hash.m_md5, md5.m_md5, PFS_MD5_SIZE);

  /*
    Copy digest storage to statement_digest_stat_array so that it could be
    used later to generate digest text.
  */
  digest_copy(& pfs->m_digest_storage, digest_storage);

  pfs->m_first_seen= now;
  pfs->m_last_seen= now;

  res= lf_hash_insert(&digest_hash, pins, &pfs);
  if (likely(res == 0))
  {
    return & pfs->m_stat;
  }

  if (res > 0)
  {
    /* Duplicate insert by another thread */
    if (++retry_count > retry_max)
    {
      /* Avoid infinite loops */
      digest_lost++;
      return NULL;
    }
    goto search;
  }

  /* OOM in lf_hash_insert */
  digest_lost++;
  return NULL;
}

void purge_digest(PFS_thread* thread, unsigned char* hash_key)
{
  LF_PINS *pins= get_digest_hash_pins(thread);
  if (unlikely(pins == NULL))
    return;

  PFS_statements_digest_stat **entry;

  /* Lookup LF_HASH using this new key. */
  entry= reinterpret_cast<PFS_statements_digest_stat**>
    (lf_hash_search(&digest_hash, pins,
                    hash_key, PFS_MD5_SIZE));

  if (entry && (entry != MY_ERRPTR))
  { 
    lf_hash_delete(&digest_hash, pins,
                   hash_key, PFS_MD5_SIZE);
  }
  lf_hash_search_unpin(pins);
  return;
}

void PFS_statements_digest_stat::reset_data()
{
  digest_reset(& m_digest_storage);
  m_stat.reset();
  m_first_seen= 0;
  m_last_seen= 0;
}

void PFS_statements_digest_stat::reset_index(PFS_thread *thread)
{
  /* Only remove entries that exists in the HASH index. */
  if (m_digest_storage.m_byte_count > 0)
  {
    purge_digest(thread, m_digest_hash.m_md5);
  }
}

void reset_esms_by_digest()
{
  uint index;

  if (statements_digest_stat_array == NULL)
    return;

  PFS_thread *thread= PFS_thread::get_current_thread();
  if (unlikely(thread == NULL))
    return;

  /* Reset statements_digest_stat_array. */
  for (index= 0; index < digest_max; index++)
  {
    statements_digest_stat_array[index].reset_index(thread);
    statements_digest_stat_array[index].reset_data();
  }

  /* 
    Reset index which indicates where the next calculated digest information
    to be inserted in statements_digest_stat_array.
  */
  digest_index= 1;
}

/*
  Iterate token array and updates digest_text.
*/
void get_digest_text(char* digest_text, PSI_digest_storage* digest_storage)
{
  bool truncated= false;
  int byte_count= digest_storage->m_byte_count;
  int need_bytes;
  uint tok= 0;
  char *id_string;
  int id_length;
  int current_byte= 0;
  lex_token_string *tok_data;
  /* -4 is to make sure extra space for '...' and a '\0' at the end. */
  int available_bytes_to_write= COL_DIGEST_TEXT_SIZE - 4;

  DBUG_ASSERT(byte_count <= PSI_MAX_DIGEST_STORAGE_SIZE);

  while ((current_byte < byte_count) &&
         (available_bytes_to_write > 0) &&
         (! truncated))
  {
    current_byte= read_token(digest_storage, current_byte, &tok);
    tok_data= & lex_token_array[tok];
    
    switch (tok)
    {
    /* All identifiers are printed with their name. */
    case IDENT:
      current_byte= read_identifier(digest_storage, current_byte,
                                    & id_string, & id_length);
      need_bytes= id_length + 1; /* <id> space */
      if (need_bytes <= available_bytes_to_write)
      {
        if (id_length > 0)
        {
          strncpy(digest_text, id_string, id_length);
          digest_text+= id_length;
        }
        *digest_text= ' ';
        digest_text++;
        available_bytes_to_write-= need_bytes;
      }
      else
      {
        truncated= true;
      }
      break;
    case IDENT_QUOTED:
      current_byte= read_identifier(digest_storage, current_byte,
                                    & id_string, & id_length);
      need_bytes= id_length + 3; /* quote <id> quote space  */
      if (need_bytes <= available_bytes_to_write)
      {
        *digest_text= '`';
        digest_text++;
        if (id_length > 0)
        {
          strncpy(digest_text, id_string, id_length);
          digest_text+= id_length;
        }
        *digest_text= '`';
        digest_text++;
        *digest_text= ' ';
        digest_text++;
        available_bytes_to_write-= need_bytes;
      }
      else
      {
        truncated= true;
      }
      break;

    /* Everything else is printed as is. */
    default:
      /* 
        Make sure not to overflow digest_text buffer while writing
        this token string.
        +1 is to make sure extra space for ' '.
      */
      int tok_length= tok_data->m_token_length;
      need_bytes= tok_length + 1;

      if (need_bytes <= available_bytes_to_write)
      {
        strncpy(digest_text,
                tok_data->m_token_string,
                tok_length);
        digest_text+= tok_length;
        *digest_text= ' ';
        digest_text++;
        available_bytes_to_write-= need_bytes;
      }
      else
      {
        truncated= true;
      }
    }
  }

  /* Truncate digest text in case of long queries. */
  if (digest_storage->m_full || truncated)
  {
    strcpy(digest_text, "...");
    digest_text+= 3;
  }

  *digest_text= '\0';
}

static inline uint peek_token(const PSI_digest_storage *digest, int index)
{
  uint token;
  DBUG_ASSERT(index >= 0);
  DBUG_ASSERT(index + PFS_SIZE_OF_A_TOKEN <= digest->m_byte_count);
  DBUG_ASSERT(digest->m_byte_count <=  PSI_MAX_DIGEST_STORAGE_SIZE);

  token= ((digest->m_token_array[index + 1])<<8) | digest->m_token_array[index];
  return token;
}

/**
  Function to read last two tokens from token array. If an identifier
  is found, do not look for token after that.
*/
static inline void peek_last_two_tokens(const PSI_digest_storage* digest_storage,
                                        int last_id_index, uint *t1, uint *t2)
{
  int byte_count= digest_storage->m_byte_count;

  if (last_id_index <= byte_count - PFS_SIZE_OF_A_TOKEN)
  {
    /* Take last token. */
    *t1= peek_token(digest_storage, byte_count - PFS_SIZE_OF_A_TOKEN);
  }
  else
  {
    *t1= TOK_PFS_UNUSED;
  }

  if (last_id_index <= byte_count - 2*PFS_SIZE_OF_A_TOKEN)
  {
    /* Take 2nd token from last. */
    *t2= peek_token(digest_storage, byte_count - 2 * PFS_SIZE_OF_A_TOKEN);
  }
  else
  {
    *t2= TOK_PFS_UNUSED;
  }
}

struct PSI_digest_locker* pfs_digest_start_v1(PSI_statement_locker *locker)
{
  PSI_statement_locker_state *statement_state;
  statement_state= reinterpret_cast<PSI_statement_locker_state*> (locker);
  DBUG_ASSERT(statement_state != NULL);

  if (statement_state->m_discarded)
    return NULL;

  if (statement_state->m_flags & STATE_FLAG_DIGEST)
  {
    PSI_digest_locker_state *digest_state;
    digest_state= &statement_state->m_digest_state;
    return reinterpret_cast<PSI_digest_locker*> (digest_state);
  }

  return NULL;
}

PSI_digest_locker* pfs_digest_add_token_v1(PSI_digest_locker *locker,
                                           uint token,
                                           OPAQUE_LEX_YYSTYPE *yylval)
{
  PSI_digest_locker_state *state= NULL;
  PSI_digest_storage      *digest_storage= NULL;

  state= reinterpret_cast<PSI_digest_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  digest_storage= &state->m_digest_storage;

  if (digest_storage->m_full)
    return NULL;

  /* 
    Take last_token 2 tokens collected till now. These tokens will be used
    in reduce for normalisation. Make sure not to consider ID tokens in reduce.
  */
  uint last_token;
  uint last_token2;
  
  peek_last_two_tokens(digest_storage, state->m_last_id_index,
                       &last_token, &last_token2);

  switch (token)
  {
    case BIN_NUM:
    case DECIMAL_NUM:
    case FLOAT_NUM:
    case HEX_NUM:
    case LEX_HOSTNAME:
    case LONG_NUM:
    case NUM:
    case TEXT_STRING:
    case NCHAR_STRING:
    case ULONGLONG_NUM:
    {
      /*
        REDUCE:
        TOK_PFS_GENERIC_VALUE := BIN_NUM | DECIMAL_NUM | ... | ULONGLONG_NUM
      */
      token= TOK_PFS_GENERIC_VALUE;

      if ((last_token2 == TOK_PFS_GENERIC_VALUE ||
           last_token2 == TOK_PFS_GENERIC_VALUE_LIST) &&
          (last_token == ','))
      {
        /*
          REDUCE:
          TOK_PFS_GENERIC_VALUE_LIST :=
            TOK_PFS_GENERIC_VALUE ',' TOK_PFS_GENERIC_VALUE
          
          REDUCE:
          TOK_PFS_GENERIC_VALUE_LIST :=
            TOK_PFS_GENERIC_VALUE_LIST ',' TOK_PFS_GENERIC_VALUE
        */
        digest_storage->m_byte_count-= 2*PFS_SIZE_OF_A_TOKEN;
        token= TOK_PFS_GENERIC_VALUE_LIST;
      }
      /*
        Add this token or the resulting reduce to digest storage.
      */
      store_token(digest_storage, token);
      break;
    }
    case ')':
    {
      if (last_token == TOK_PFS_GENERIC_VALUE &&
          last_token2 == '(') 
      { 
        /*
          REDUCE:
          TOK_PFS_ROW_SINGLE_VALUE :=
            '(' TOK_PFS_GENERIC_VALUE ')' 
        */
        digest_storage->m_byte_count-= 2*PFS_SIZE_OF_A_TOKEN;
        token= TOK_PFS_ROW_SINGLE_VALUE;
      
        /* Read last two tokens again */
        peek_last_two_tokens(digest_storage, state->m_last_id_index,
                             &last_token, &last_token2);

        if ((last_token2 == TOK_PFS_ROW_SINGLE_VALUE ||
             last_token2 == TOK_PFS_ROW_SINGLE_VALUE_LIST) &&
            (last_token == ','))
        {
          /*
            REDUCE:
            TOK_PFS_ROW_SINGLE_VALUE_LIST := 
              TOK_PFS_ROW_SINGLE_VALUE ',' TOK_PFS_ROW_SINGLE_VALUE

            REDUCE:
            TOK_PFS_ROW_SINGLE_VALUE_LIST := 
              TOK_PFS_ROW_SINGLE_VALUE_LIST ',' TOK_PFS_ROW_SINGLE_VALUE
          */
          digest_storage->m_byte_count-= 2*PFS_SIZE_OF_A_TOKEN;
          token= TOK_PFS_ROW_SINGLE_VALUE_LIST;
        }
      }
      else if (last_token == TOK_PFS_GENERIC_VALUE_LIST &&
               last_token2 == '(') 
      {
        /*
          REDUCE:
          TOK_PFS_ROW_MULTIPLE_VALUE :=
            '(' TOK_PFS_GENERIC_VALUE_LIST ')'
        */
        digest_storage->m_byte_count-= 2*PFS_SIZE_OF_A_TOKEN;
        token= TOK_PFS_ROW_MULTIPLE_VALUE;

        /* Read last two tokens again */
        peek_last_two_tokens(digest_storage, state->m_last_id_index,
                             &last_token, &last_token2);

        if ((last_token2 == TOK_PFS_ROW_MULTIPLE_VALUE ||
             last_token2 == TOK_PFS_ROW_MULTIPLE_VALUE_LIST) &&
            (last_token == ','))
        {
          /*
            REDUCE:
            TOK_PFS_ROW_MULTIPLE_VALUE_LIST :=
              TOK_PFS_ROW_MULTIPLE_VALUE ',' TOK_PFS_ROW_MULTIPLE_VALUE

            REDUCE:
            TOK_PFS_ROW_MULTIPLE_VALUE_LIST :=
              TOK_PFS_ROW_MULTIPLE_VALUE_LIST ',' TOK_PFS_ROW_MULTIPLE_VALUE
          */
          digest_storage->m_byte_count-= 2*PFS_SIZE_OF_A_TOKEN;
          token= TOK_PFS_ROW_MULTIPLE_VALUE_LIST;
        }
      }
      /*
        Add this token or the resulting reduce to digest storage.
      */
      store_token(digest_storage, token);
      break;
    }
    case IDENT:
    case IDENT_QUOTED:
    {
      LEX_YYSTYPE *lex_token= (LEX_YYSTYPE*) yylval;
      char *yytext= lex_token->lex_str.str;
      int yylen= lex_token->lex_str.length;

      /* Add this token and identifier string to digest storage. */
      store_token_identifier(digest_storage, token, yylen, yytext);

      /* Update the index of last identifier found. */
      state->m_last_id_index= digest_storage->m_byte_count;
      break;
    }
    default:
    {
      /* Add this token to digest storage. */
      store_token(digest_storage, token);
      break;
    }
  }

  return locker;
}
