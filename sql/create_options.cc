/* Copyright (C) 2010 Monty Program Ab

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

/**
  @file

  Engine defined options of tables/fields/keys in CREATE/ALTER TABLE.
*/

#include "create_options.h"
#include <my_getopt.h>
#include "set_var.h"

#define FRM_QUOTED_VALUE 0x8000

/**
  Links this item to the given list end

  @param start           The list beginning or NULL
  @param end             The list last element or does not matter
*/

void engine_option_value::link(engine_option_value **start,
                               engine_option_value **end)
{
  DBUG_ENTER("engine_option_value::link");
  DBUG_PRINT("enter", ("name: '%s' (%u)  value: '%s' (%u)",
                       name.str, (uint) name.length,
                       value.str, (uint) value.length));
  engine_option_value *opt;
  /* check duplicates to avoid writing them to frm*/
  for(opt= *start;
      opt && ((opt->parsed && !opt->value.str) ||
              my_strnncoll(system_charset_info,
                           (uchar *)name.str, name.length,
                           (uchar*)opt->name.str, opt->name.length));
      opt= opt->next) /* no-op */;
  if (opt)
  {
    opt->value.str= NULL;       /* remove previous value */
    opt->parsed= TRUE;          /* and don't issue warnings for it anymore */
  }
  /*
    Add this option to the end of the list

    @note: We add even if it is opt->value.str == NULL because it can be
    ALTER TABLE to remove the option.
  */
  if (*start)
  {
    (*end)->next= this;
    *end= this;
  }
  else
  {
    /*
      note that is *start == 0, the value of *end does not matter,
      it can be uninitialized.
    */
    *start= *end= this;
  }
  DBUG_VOID_RETURN;
}

static bool report_wrong_value(THD *thd, const char *name, const char *val,
                               bool suppress_warning)
{
  if (suppress_warning)
    return 0;

  if (!(thd->variables.sql_mode & MODE_IGNORE_BAD_TABLE_OPTIONS) &&
      !thd->slave_thread)
  {
    my_error(ER_BAD_OPTION_VALUE, MYF(0), val, name);
    return 1;
  }

  push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN, ER_BAD_OPTION_VALUE,
                      ER(ER_BAD_OPTION_VALUE), val, name);
  return 0;
}

static bool report_unknown_option(THD *thd, engine_option_value *val,
                                  bool suppress_warning)
{
  DBUG_ENTER("report_unknown_option");

  if (val->parsed || suppress_warning)
  {
    DBUG_PRINT("info", ("parsed => exiting"));
    DBUG_RETURN(FALSE);
  }

  if (!(thd->variables.sql_mode & MODE_IGNORE_BAD_TABLE_OPTIONS) &&
      !thd->slave_thread)
  {
    my_error(ER_UNKNOWN_OPTION, MYF(0), val->name.str);
    DBUG_RETURN(TRUE);
  }

  push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                      ER_UNKNOWN_OPTION, ER(ER_UNKNOWN_OPTION), val->name.str);
  DBUG_RETURN(FALSE);
}

static bool set_one_value(ha_create_table_option *opt,
                          THD *thd, const LEX_STRING *value, void *base,
                          bool suppress_warning,
                          MEM_ROOT *root)
{
  DBUG_ENTER("set_one_value");
  DBUG_PRINT("enter", ("opt: 0x%lx type: %u name '%s' value: '%s'",
                       (ulong) opt,
                       opt->type, opt->name,
                       (value->str ? value->str : "<DEFAULT>")));
  switch (opt->type)
  {
  case HA_OPTION_TYPE_SYSVAR:
    DBUG_ASSERT(0); // HA_OPTION_TYPE_SYSVAR's are replaced in resolve_sysvars()
  case HA_OPTION_TYPE_ULL:
    {
      ulonglong *val= (ulonglong*)((char*)base + opt->offset);
      if (!value->str)
      {
        *val= opt->def_value;
        DBUG_RETURN(0);
      }

      my_option optp=
        { opt->name, 1, 0, (uchar **)val, 0, 0, GET_ULL,
          REQUIRED_ARG, (longlong)opt->def_value, (longlong)opt->min_value,
          opt->max_value, 0, (long) opt->block_size, 0};

      ulonglong orig_val= strtoull(value->str, NULL, 10);
      my_bool unused;
      *val= orig_val;
      *val= getopt_ull_limit_value(*val, &optp, &unused);
      if (*val == orig_val)
        DBUG_RETURN(0);

      DBUG_RETURN(report_wrong_value(thd, opt->name, value->str,
                                     suppress_warning));
    }
  case HA_OPTION_TYPE_STRING:
    {
      char **val= (char **)((char *)base + opt->offset);
      if (!value->str)
      {
        *val= 0;
        DBUG_RETURN(0);
      }

      if (!(*val= strmake_root(root, value->str, value->length)))
        DBUG_RETURN(1);
      DBUG_RETURN(0);
    }
  case HA_OPTION_TYPE_ENUM:
    {
      uint *val= (uint *)((char *)base + opt->offset), num;

      *val= (uint) opt->def_value;
      if (!value->str)
        DBUG_RETURN(0);

      const char *start= opt->values, *end;

      num= 0;
      while (*start)
      {
        for (end=start;
             *end && *end != ',';
             end+= my_mbcharlen(system_charset_info, *end)) /* no-op */;
        if (!my_strnncoll(system_charset_info,
                          (uchar*)start, end-start,
                          (uchar*)value->str, value->length))
        {
          *val= num;
          DBUG_RETURN(0);
        }
        if (*end)
          end++;
        start= end;
        num++;
      }

      DBUG_RETURN(report_wrong_value(thd, opt->name, value->str,
                                     suppress_warning));
    }
  case HA_OPTION_TYPE_BOOL:
    {
      bool *val= (bool *)((char *)base + opt->offset);
      *val= opt->def_value;

      if (!value->str)
        DBUG_RETURN(0);

      if (!my_strnncoll(system_charset_info,
                        (const uchar*)"NO", 2,
                        (uchar *)value->str, value->length) ||
          !my_strnncoll(system_charset_info,
                        (const uchar*)"OFF", 3,
                        (uchar *)value->str, value->length) ||
          !my_strnncoll(system_charset_info,
                        (const uchar*)"0", 1,
                        (uchar *)value->str, value->length))
      {
        *val= FALSE;
        DBUG_RETURN(FALSE);
      }

      if (!my_strnncoll(system_charset_info,
                        (const uchar*)"YES", 3,
                        (uchar *)value->str, value->length) ||
          !my_strnncoll(system_charset_info,
                        (const uchar*)"ON", 2,
                        (uchar *)value->str, value->length) ||
          !my_strnncoll(system_charset_info,
                        (const uchar*)"1", 1,
                        (uchar *)value->str, value->length))
      {
        *val= TRUE;
        DBUG_RETURN(FALSE);
      }

      DBUG_RETURN(report_wrong_value(thd, opt->name, value->str,
                                     suppress_warning));
    }
  }
  DBUG_ASSERT(0);
  my_error(ER_UNKNOWN_ERROR, MYF(0));
  DBUG_RETURN(1);
}

static const size_t ha_option_type_sizeof[]=
{ sizeof(ulonglong), sizeof(char *), sizeof(uint), sizeof(bool)};

/**
  Creates option structure and parses list of options in it

  @param thd              thread handler
  @param option_struct    where to store pointer on the option struct
  @param option_list      list of options given by user
  @param rules            list of option description by engine
  @param suppress_warning second parse so we do not need warnings
  @param root             MEM_ROOT where allocate memory

  @retval TRUE  Error
  @retval FALSE OK
*/

bool parse_option_list(THD* thd, handlerton *hton, void *option_struct_arg,
                       engine_option_value **option_list,
                       ha_create_table_option *rules,
                       bool suppress_warning, MEM_ROOT *root)
{
  ha_create_table_option *opt;
  size_t option_struct_size= 0;
  engine_option_value *val, *last;
  void **option_struct= (void**)option_struct_arg;
  DBUG_ENTER("parse_option_list");
  DBUG_PRINT("enter",
             ("struct: %p list: %p rules: %p suppress_warning: %u root: %p",
              *option_struct, *option_list, rules,
              (uint) suppress_warning, root));

  if (rules)
  {
    for (opt= rules; opt->name; opt++)
      set_if_bigger(option_struct_size, opt->offset +
                    ha_option_type_sizeof[opt->type]);

    *option_struct= alloc_root(root, option_struct_size);
  }

  for (opt= rules; opt && opt->name; opt++)
  {
    bool seen=false;
    for (val= *option_list; val; val= val->next)
    {
      last= val;
      if (my_strnncoll(system_charset_info,
                       (uchar*)opt->name, opt->name_length,
                       (uchar*)val->name.str, val->name.length))
        continue;

      seen=true;

      if (val->parsed && !val->value.str)
        continue;

      if (set_one_value(opt, thd, &val->value,
                        *option_struct, suppress_warning || val->parsed, root))
        DBUG_RETURN(TRUE);
      val->parsed= true;
      break;
    }
    if (!seen)
    {
      LEX_STRING default_val= null_lex_str;

      /*
        If it's CREATE/ALTER TABLE parsing mode (options are created in the
        transient thd->mem_root, not in the long living TABLE_SHARE::mem_root),
        and variable-backed option was not explicitly set.

        If it's not create, but opening of the existing frm (that was,
        probably, created with the older version of the storage engine and
        does not have this option stored), we take the *default* value of the
        sysvar, not the *current* value. Because we don't want to have
        different option values for the same table if it's opened many times.
      */
      if (root == thd->mem_root && opt->var)
      {
        // take a value from the variable and add it to the list
        sys_var *sysvar= find_hton_sysvar(hton, opt->var);
        DBUG_ASSERT(sysvar);

        char buf[256];
        String sbuf(buf, sizeof(buf), system_charset_info), *str;
        if ((str= sysvar->val_str(&sbuf, thd, OPT_SESSION, 0)))
        {
          LEX_STRING name= { const_cast<char*>(opt->name), opt->name_length };
          default_val.str= strmake_root(root, str->ptr(), str->length());
          default_val.length= str->length();
          val= new (root) engine_option_value(name, default_val, true,
                                              option_list, &last);
          val->parsed= true;
        }
      }
      set_one_value(opt, thd, &default_val, *option_struct,
                    suppress_warning, root);
    }
  }

  for (val= *option_list; val; val= val->next)
  {
    if (report_unknown_option(thd, val, suppress_warning))
      DBUG_RETURN(TRUE);
    val->parsed= true;
  }

  DBUG_RETURN(FALSE);
}


/**
  Resolves all HA_OPTION_TYPE_SYSVAR elements.

  This is done when an engine is loaded.
*/
static bool resolve_sysvars(handlerton *hton, ha_create_table_option *rules)
{
  for (ha_create_table_option *opt= rules; opt && opt->name; opt++)
  {
    if (opt->type == HA_OPTION_TYPE_SYSVAR)
    {
      struct my_option optp;
      plugin_opt_set_limits(&optp, opt->var);
      switch(optp.var_type) {
      case GET_ULL:
      case GET_ULONG:
      case GET_UINT:
        opt->type= HA_OPTION_TYPE_ULL;
        opt->def_value= (ulonglong)optp.def_value;
        opt->min_value= (ulonglong)optp.min_value;
        opt->max_value= (ulonglong)optp.max_value;
        opt->block_size= (ulonglong)optp.block_size;
        break;
      case GET_STR:
      case GET_STR_ALLOC:
        opt->type= HA_OPTION_TYPE_STRING;
        break;
      case GET_BOOL:
        opt->type= HA_OPTION_TYPE_BOOL;
        opt->def_value= optp.def_value;
        break;
      case GET_ENUM:
      {
        opt->type= HA_OPTION_TYPE_ENUM;
        opt->def_value= optp.def_value;

        char buf[256];
        String str(buf, sizeof(buf), system_charset_info);
        for (const char **s= optp.typelib->type_names; *s; s++)
        {
          if (str.append(*s) || str.append(','))
            return 1;
        }
        DBUG_ASSERT(str.length());
        opt->values= my_strndup(str.ptr(), str.length()-1, MYF(MY_WME));
        if (!opt->values)
          return 1;
        break;
      }
      default:
        DBUG_ASSERT(0);
      }
    }
  }
  return 0;
}

bool resolve_sysvar_table_options(handlerton *hton)
{
  return resolve_sysvars(hton, hton->table_options) ||
         resolve_sysvars(hton, hton->field_options) ||
         resolve_sysvars(hton, hton->index_options);
}

/*
  Restore HA_OPTION_TYPE_SYSVAR options back as they were
  before resolve_sysvars().

  This is done when the engine is unloaded, so that we could
  call resolve_sysvars() if the engine is installed again.
*/
static void free_sysvars(handlerton *hton, ha_create_table_option *rules)
{
  for (ha_create_table_option *opt= rules; opt && opt->name; opt++)
  {
    if (opt->var)
    {
      my_free(const_cast<char*>(opt->values));
      opt->type= HA_OPTION_TYPE_SYSVAR;
      opt->def_value= 0;
      opt->min_value= 0;
      opt->max_value= 0;
      opt->block_size= 0;
      opt->values= 0;
    }
  }
}

void free_sysvar_table_options(handlerton *hton)
{
  free_sysvars(hton, hton->table_options);
  free_sysvars(hton, hton->field_options);
  free_sysvars(hton, hton->index_options);
}


/**
  Parses all table/fields/keys options

  @param thd             thread handler
  @param file            handler of the table
  @parem share           descriptor of the table

  @retval TRUE  Error
  @retval FALSE OK
*/

bool parse_engine_table_options(THD *thd, handlerton *ht, TABLE_SHARE *share)
{
  MEM_ROOT *root= &share->mem_root;
  DBUG_ENTER("parse_engine_table_options");

  if (parse_option_list(thd, ht, &share->option_struct, & share->option_list,
                        ht->table_options, TRUE, root))
    DBUG_RETURN(TRUE);

  for (Field **field= share->field; *field; field++)
  {
    if (parse_option_list(thd, ht, &(*field)->option_struct,
                          & (*field)->option_list,
                          ht->field_options, TRUE, root))
      DBUG_RETURN(TRUE);
  }

  for (uint index= 0; index < share->keys; index ++)
  {
    if (parse_option_list(thd, ht, &share->key_info[index].option_struct,
                          & share->key_info[index].option_list,
                          ht->index_options, TRUE, root))
      DBUG_RETURN(TRUE);
  }

  DBUG_RETURN(FALSE);
}


/**
  Returns representation length of key and value in the frm file
*/

uint engine_option_value::frm_length()
{
  /*
    1 byte  - name length
    2 bytes - value length

    if value.str is NULL, this option is not written to frm (=DEFAULT)
  */
  return value.str ? 1 + name.length + 2 + value.length : 0;
}


/**
  Returns length of representation of option list in the frm file
*/

static uint option_list_frm_length(engine_option_value *opt)
{
  uint res= 0;

  for (; opt; opt= opt->next)
    res+= opt->frm_length();

  return res;
}


/**
  Calculates length of options image in the .frm

  @param table_option_list list of table options
  @param create_fields     field descriptors list
  @param keys              number of keys
  @param key_info          array of key descriptors

  @returns length of image in frm
*/

uint engine_table_options_frm_length(engine_option_value *table_option_list,
                                     List<Create_field> &create_fields,
                                     uint keys, KEY *key_info)
{
  List_iterator<Create_field> it(create_fields);
  Create_field *field;
  uint res, index;
  DBUG_ENTER("engine_table_options_frm_length");

  res= option_list_frm_length(table_option_list);

  while ((field= it++))
    res+= option_list_frm_length(field->option_list);

  for (index= 0; index < keys; index++, key_info++)
    res+= option_list_frm_length(key_info->option_list);

  /*
    if there's at least one option somewhere (res > 0)
    we write option lists for all fields and keys, zero-terminated.
    If there're no options we write nothing at all (backward compatibility)
  */
  DBUG_RETURN(res ? res + 1 + create_fields.elements + keys : 0);
}


/**
  Writes image of the key and value to the frm image buffer

  @param buff            pointer to the buffer free space beginning

  @returns pointer to byte after last recorded in the buffer
*/

uchar *engine_option_value::frm_image(uchar *buff)
{
  if (value.str)
  {
    *buff++= name.length;
    memcpy(buff, name.str, name.length);
    buff+= name.length;
    int2store(buff, value.length | (quoted_value ? FRM_QUOTED_VALUE : 0));
    buff+= 2;
    memcpy(buff, (const uchar *) value.str, value.length);
    buff+= value.length;
  }
  return buff;
}

/**
  Writes image of the key and value to the frm image buffer

  @param buff            pointer to the buffer to store the options in
  @param opt             list of options;

  @returns pointer to the end of the stored data in the buffer
*/
static uchar *option_list_frm_image(uchar *buff, engine_option_value *opt)
{
  for (; opt; opt= opt->next)
    buff= opt->frm_image(buff);

  *buff++= 0;
  return buff;
}


/**
  Writes options image in the .frm buffer

  @param buff              pointer to the buffer
  @param table_option_list list of table options
  @param create_fields     field descriptors list
  @param keys              number of keys
  @param key_info          array of key descriptors

  @returns pointer to byte after last recorded in the buffer
*/

uchar *engine_table_options_frm_image(uchar *buff,
                                      engine_option_value *table_option_list,
                                      List<Create_field> &create_fields,
                                      uint keys, KEY *key_info)
{
  List_iterator<Create_field> it(create_fields);
  Create_field *field;
  KEY *key_info_end= key_info + keys;
  DBUG_ENTER("engine_table_options_frm_image");

  buff= option_list_frm_image(buff, table_option_list);

  while ((field= it++))
    buff= option_list_frm_image(buff, field->option_list);

  while (key_info < key_info_end)
    buff= option_list_frm_image(buff, (key_info++)->option_list);

  DBUG_RETURN(buff);
}

/**
  Reads name and value from buffer, then link it in the list

  @param buff            the buffer to read from
  @param start           The list beginning or NULL
  @param end             The list last element or does not matter
  @param root            MEM_ROOT for allocating

  @returns pointer to byte after last recorded in the buffer
*/
uchar *engine_option_value::frm_read(const uchar *buff, engine_option_value **start,
                                     engine_option_value **end, MEM_ROOT *root)
{
  LEX_STRING name, value;
  uint len;

  name.length= buff[0];
  buff++;
  if (!(name.str= strmake_root(root, (const char*)buff, name.length)))
    return NULL;
  buff+= name.length;
  len= uint2korr(buff);
  value.length= len & ~FRM_QUOTED_VALUE;
  buff+= 2;
  if (!(value.str= strmake_root(root, (const char*)buff, value.length)))
    return NULL;
  buff+= value.length;

  engine_option_value *ptr=new (root)
    engine_option_value(name, value, len & FRM_QUOTED_VALUE, start, end);
  if (!ptr)
    return NULL;

  return (uchar *)buff;
}


/**
  Reads options from this buffer

  @param buff            the buffer to read from
  @param length          buffer length
  @param share           table descriptor
  @param root            MEM_ROOT for allocating

  @retval TRUE  Error
  @retval FALSE OK
*/

bool engine_table_options_frm_read(const uchar *buff, uint length,
                                   TABLE_SHARE *share)
{
  const uchar *buff_end= buff + length;
  engine_option_value *UNINIT_VAR(end);
  MEM_ROOT *root= &share->mem_root;
  uint count;
  DBUG_ENTER("engine_table_options_frm_read");

  while (buff < buff_end && *buff)
  {
    if (!(buff= engine_option_value::frm_read(buff, &share->option_list, &end,
                                              root)))
      DBUG_RETURN(TRUE);
  }
  buff++;

  for (count=0; count < share->fields; count++)
  {
    while (buff < buff_end && *buff)
    {
      if (!(buff= engine_option_value::frm_read(buff,
                                                &share->field[count]->option_list,
                                                &end, root)))
        DBUG_RETURN(TRUE);
    }
    buff++;
  }

  for (count=0; count < share->keys; count++)
  {
    while (buff < buff_end && *buff)
    {
      if (!(buff= engine_option_value::frm_read(buff,
                                                &share->key_info[count].option_list,
                                                &end, root)))
        DBUG_RETURN(TRUE);
    }
    buff++;
  }

  if (buff < buff_end)
    sql_print_warning("Table '%s' was created in a later MariaDB version - "
                      "unknown table attributes were ignored",
                      share->table_name.str);

  DBUG_RETURN(buff > buff_end);
}

/**
  Merges two lists of engine_option_value's with duplicate removal.
*/

engine_option_value *merge_engine_table_options(engine_option_value *first,
                                                engine_option_value *second,
                                                MEM_ROOT *root)
{
  engine_option_value *end, *opt;
  DBUG_ENTER("merge_engine_table_options");
  LINT_INIT(end);

  /* find last element */
  if (first && second)
    for (end= first; end->next; end= end->next) /* no-op */;

  for (opt= second; opt; opt= opt->next)
    new (root) engine_option_value(opt->name, opt->value, opt->quoted_value,
                                   &first, &end);
  DBUG_RETURN(first);
}
