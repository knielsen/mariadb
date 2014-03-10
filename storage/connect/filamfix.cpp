/*********** File AM Fix C++ Program Source Code File (.CPP) ***********/
/* PROGRAM NAME: FILAMFIX                                              */
/* -------------                                                       */
/*  Version 1.5                                                        */
/*                                                                     */
/* COPYRIGHT:                                                          */
/* ----------                                                          */
/*  (C) Copyright to the author Olivier BERTRAND          2005-2014    */
/*                                                                     */
/* WHAT THIS PROGRAM DOES:                                             */
/* -----------------------                                             */
/*  This program are the FIX/BIN file access method classes.           */
/*                                                                     */
/***********************************************************************/

/***********************************************************************/
/*  Include relevant sections of the System header files.              */
/***********************************************************************/
#include "my_global.h"
#if defined(WIN32)
#include <io.h>
#include <fcntl.h>
#include <errno.h>
#if defined(__BORLANDC__)
#define __MFC_COMPAT__                   // To define min/max as macro
#endif   // __BORLANDC__
//#include <windows.h>
#else   // !WIN32
#if defined(UNIX)
#include <errno.h>
#include <unistd.h>
#else   // !UNIX
#include <io.h>
#endif  // !UNIX
#include <sys/stat.h>
#include <fcntl.h>
#endif  // !WIN32

/***********************************************************************/
/*  Include application header files:                                  */
/*  global.h    is header containing all global declarations.          */
/*  plgdbsem.h  is header containing the DB application declarations.  */
/*  filamfix.h  is header containing the file AM classes declarations. */
/***********************************************************************/
#include "global.h"
#include "plgdbsem.h"
#include "filamfix.h"
#include "tabdos.h"
#include "osutil.h"

#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER  0xFFFFFFFF
#endif

extern int num_read, num_there, num_eq[2];               // Statistics

/* --------------------------- Class FIXFAM -------------------------- */

/***********************************************************************/
/*  Constructors.                                                      */
/***********************************************************************/
FIXFAM::FIXFAM(PDOSDEF tdp) : BLKFAM(tdp)
  {
  Blksize = tdp->GetBlksize();
  Padded = tdp->GetPadded();

  if (Padded && Blksize)
    Nrec = Blksize / Lrecl;
  else {
    Nrec = (tdp->GetElemt()) ? tdp->GetElemt() : DOS_BUFF_LEN;
    Blksize = Nrec * Lrecl;
    Padded = false;
    } // endelse

  } // end of FIXFAM standard constructor

FIXFAM::FIXFAM(PFIXFAM txfp) : BLKFAM(txfp)
  {
  } // end of FIXFAM copy constructor

/***********************************************************************/
/*  Allocate the block buffer for the table.                           */
/***********************************************************************/
bool FIXFAM::AllocateBuffer(PGLOBAL g)
  {
  Buflen = Blksize;
  To_Buf = (char*)PlugSubAlloc(g, NULL, Buflen);

  if (UseTemp || Tdbp->GetMode() == MODE_DELETE) {
    if (Padded) {
      strcpy(g->Message, MSG(NO_MODE_PADDED));
      return true;
      } // endif Padded

    // Allocate a separate buffer so block reading can be kept
    Dbflen = Nrec;
    DelBuf = PlugSubAlloc(g, NULL, Blksize);
  } else if (Tdbp->GetMode() == MODE_INSERT) {
    /*******************************************************************/
    /*  For Insert the buffer must be prepared.                        */
    /*******************************************************************/
    memset(To_Buf, ' ', Buflen);

    if (/*Tdbp->GetFtype() < 2 &&*/ !Padded)
      // If not binary, the file is physically a text file.
      // We do it also for binary table because the lrecl can have been
      // specified with additional space to include line ending.
      for (int len = Lrecl; len <= Buflen; len += Lrecl) {
#if defined(WIN32)
        To_Buf[len - 2] = '\r';
#endif   // WIN32
        To_Buf[len - 1] = '\n';
        } // endfor len

    Rbuf = Nrec;                     // To be used by WriteDB
    } // endif Insert

  return false;
  } // end of AllocateBuffer

/***********************************************************************/
/*  Reset buffer access according to indexing and to mode.             */
/*  >>>>>>>>>>>>>> TO BE RE-VISITED AND CHECKED <<<<<<<<<<<<<<<<<<<<<< */
/***********************************************************************/
void FIXFAM::ResetBuffer(PGLOBAL g)
  {
  /*********************************************************************/
  /*  If access is random, performances can be much better when the    */
  /*  reads are done on only one row, except for small tables that can */
  /*  be entirely read in one block. If the index is just used as a    */
  /*  bitmap filter as for Update or Delete reading will be sequential */
  /*  and we better keep block reading.                                */
  /*********************************************************************/
  if (Tdbp->GetMode() == MODE_READ && ReadBlks != 1 && !Padded &&
      Tdbp->GetKindex() /*&& Tdbp->GetKindex()->IsRandom()*/) {
    Nrec = 1;                       // Better for random access
    Rbuf = 0;
    Blksize = Lrecl;
    OldBlk = -2;                    // Has no meaning anymore
    Block = Tdbp->Cardinality(g);   // Blocks are one line now
    } // endif Mode

  } // end of ResetBuffer

/***********************************************************************/
/*  ReadBuffer: Read one line for a FIX file.                          */
/***********************************************************************/
int FIXFAM::ReadBuffer(PGLOBAL g)
  {
  int n, rc = RC_OK;

  if (!Closing) {
    /*******************************************************************/
    /*  Sequential reading when Placed is not true.                    */
    /*******************************************************************/
    if (Placed) {
      Tdbp->SetLine(To_Buf + CurNum * Lrecl);
      Placed = false;
    } else if (++CurNum < Rbuf) {
      Tdbp->IncLine(Lrecl);                // Used by DOSCOL functions
      return RC_OK;
    } else if (Rbuf < Nrec && CurBlk != -1) {
      return RC_EF;
    } else {
      /*****************************************************************/
      /*  New block.                                                   */
      /*****************************************************************/
      CurNum = 0;
      Tdbp->SetLine(To_Buf);

      if (++CurBlk >= Block)
        return RC_EF;

     } // endif's

    if (OldBlk == CurBlk) {
      IsRead = true;            // Was read indeed
      return RC_OK;             // Block is already there
      } // endif OldBlk

    } // endif !Closing

  if (Modif) {
    /*******************************************************************/
    /*  The old block was modified in Update mode.                     */
    /*  In Update mode we simply rewrite the old block on itself.      */
    /*******************************************************************/
    bool moved = false;

    if (UseTemp)                // Copy any intermediate lines.
      if (MoveIntermediateLines(g, &moved))
        rc = RC_FX;

    if (rc == RC_OK) {
      // Fpos is last position, Headlen is DBF file header length
      if (!moved && fseek(Stream, Headlen + Fpos * Lrecl, SEEK_SET)) {
        sprintf(g->Message, MSG(FSETPOS_ERROR), 0);
        rc = RC_FX;
      } else if (fwrite(To_Buf, Lrecl, Rbuf, T_Stream) != (size_t)Rbuf) {
        sprintf(g->Message, MSG(FWRITE_ERROR), strerror(errno));
        rc = RC_FX;
      } // endif fwrite

      Spos = Fpos + Nrec;       // + Rbuf ???
      } // endif rc

    if (Closing || rc != RC_OK) {   // Error or called from CloseDB
      Closing = true;           // To tell CloseDB about error
      return rc;
      } // endif Closing

    // NOTE: Next line was added to avoid a very  strange fread bug.
    // When the fseek is not executed (even the file has the good
    // pointer position) the next read can happen anywhere in the file.
    OldBlk = CurBlk;            // This will force fseek to be executed
    Modif = 0;
//  Spos = Fpos + Nrec;         done above
    } // endif Mode

  // This could be done only for new block. However note that FPOS
  // is used as block position when updating and as line position
  // when deleting so this has to be carefully checked.
  Fpos = CurBlk * Nrec;         // Fpos is new line position

  // fseek is required only in non sequential reading
  if (CurBlk != OldBlk + 1)
    // Note: Headlen is for DBF tables
    if (fseek(Stream, Headlen + Fpos * Lrecl, SEEK_SET)) {
      sprintf(g->Message, MSG(FSETPOS_ERROR), Fpos);
      return RC_FX;
      } // endif fseek

#ifdef DEBTRACE
 htrc("File position is now %d\n", ftell(Stream));
#endif

//long tell = ftell(Stream);                not used

  if (Padded)
    n = fread(To_Buf, (size_t)Blksize, 1, Stream);
  else
    n = fread(To_Buf, (size_t)Lrecl, (size_t)Nrec, Stream);

  if (n) {
    rc = RC_OK;
    Rbuf = (Padded) ? n * Nrec : n;
    ReadBlks++;
    num_read++;
  } else if (feof(Stream)) {
    rc = RC_EF;
  } else {
#if defined(UNIX)
    sprintf(g->Message, MSG(READ_ERROR), To_File, strerror(errno));
#else
    sprintf(g->Message, MSG(READ_ERROR), To_File, _strerror(NULL));
#endif

#ifdef DEBTRACE
 htrc("%s\n", g->Message);
#endif
    return RC_FX;
  } // endelse

  OldBlk = CurBlk;              // Last block actually read
  IsRead = true;                // Is read indeed
  return rc;
  } // end of ReadBuffer

/***********************************************************************/
/*  WriteBuffer: File write routine for FIX access method.             */
/*  Updates are written into the (Temp) file in ReadBuffer.            */
/***********************************************************************/
int FIXFAM::WriteBuffer(PGLOBAL g)
  {
#ifdef DEBTRACE
 fprintf(debug,
  "FIX WriteDB: Mode=%d buf=%p line=%p Nrec=%d Rbuf=%d CurNum=%d\n",
  Tdbp->GetMode(), To_Buf, Tdbp->GetLine(), Nrec, Rbuf, CurNum);
#endif

  if (Tdbp->GetMode() == MODE_INSERT) {
    /*******************************************************************/
    /*  In Insert mode, blocs are added sequentialy to the file end.   */
    /*******************************************************************/
    if (++CurNum != Rbuf) {
      Tdbp->IncLine(Lrecl);            // Used by DOSCOL functions
      return RC_OK;                    // We write only full blocks
      } // endif CurNum

#ifdef DEBTRACE
 htrc(" First line is '%.*s'\n", Lrecl - 2, To_Buf);
#endif

    //  Now start the writing process.
    if (fwrite(To_Buf, Lrecl, Rbuf, Stream) != (size_t)Rbuf) {
      sprintf(g->Message, MSG(FWRITE_ERROR), strerror(errno));
      Closing = true;      // To tell CloseDB about a Write error
      return RC_FX;
      } // endif size

    CurBlk++;
    CurNum = 0;
    Tdbp->SetLine(To_Buf);

#ifdef DEBTRACE
 htrc("write done\n");
#endif

  } else {                           // Mode == MODE_UPDATE
    // T_Stream is the temporary stream or the table file stream itself
    if (!T_Stream)
    {
      if (UseTemp /*&& Tdbp->GetMode() == MODE_UPDATE*/) {
        if (OpenTempFile(g))
          return RC_FX;

        if (CopyHeader(g))           // For DBF tables
          return RC_FX;

      } else
        T_Stream = Stream;
    }
    Modif++;                         // Modified line in Update mode
  } // endif Mode

  return RC_OK;
  } // end of WriteBuffer

/***********************************************************************/
/*  Data Base delete line routine for FIXFAM access method.            */
/***********************************************************************/
int FIXFAM::DeleteRecords(PGLOBAL g, int irc)
  {
  bool moved;

  /*********************************************************************/
  /*  There is an alternative here:                                    */
  /*  1 - use a temporary file in which are copied all not deleted     */
  /*      lines, at the end the original file will be deleted and      */
  /*      the temporary file renamed to the original file name.        */
  /*  2 - directly move the not deleted lines inside the original      */
  /*      file, and at the end erase all trailing records.             */
  /*  This will be experimented.                                       */
  /*********************************************************************/
#ifdef DEBTRACE
 fprintf(debug,
  "DOS DeleteDB: rc=%d UseTemp=%d Fpos=%d Tpos=%d Spos=%d\n",
  irc, UseTemp, Fpos, Tpos, Spos);
#endif

  if (irc != RC_OK) {
    /*******************************************************************/
    /*  EOF: position Fpos at the end-of-file position.                */
    /*******************************************************************/
    Fpos = Tdbp->Cardinality(g);
#ifdef DEBTRACE
 htrc("Fpos placed at file end=%d\n", Fpos);
#endif
  } else    // Fpos is the deleted line position
    Fpos = CurBlk * Nrec + CurNum;

  if (Tpos == Spos) {
    /*******************************************************************/
    /*  First line to delete.                                          */
    /*******************************************************************/
    if (UseTemp) {
      /*****************************************************************/
      /*  Open temporary file, lines before this will be moved.        */
      /*****************************************************************/
      if (OpenTempFile(g))
        return RC_FX;

    } else {
      /*****************************************************************/
      /*  Move of eventual preceeding lines is not required here.      */
      /*  Set the target file as being the source file itself.         */
      /*  Set the future Tpos, and give Spos a value to block moving.  */
      /*****************************************************************/
      T_Stream = Stream;
      Spos = Tpos = Fpos;
    } // endif UseTemp

    } // endif Tpos == Spos

  /*********************************************************************/
  /*  Move any intermediate lines.                                     */
  /*********************************************************************/
  if (MoveIntermediateLines(g, &moved))
    return RC_FX;

  if (irc == RC_OK) {
    /*******************************************************************/
    /*  Reposition the file pointer and set Spos.                      */
    /*******************************************************************/
    Spos = Fpos + 1;          // New start position is on next line

    if (moved) {
      if (fseek(Stream, Spos * Lrecl, SEEK_SET)) {
        sprintf(g->Message, MSG(FSETPOS_ERROR), 0);
        return RC_FX;
        } // endif fseek

      OldBlk = -2;  // To force fseek to be executed on next block
      } // endif moved

#ifdef DEBTRACE
 htrc("after: Tpos=%d Spos=%d\n", Tpos, Spos);
#endif

  } else {
    /*******************************************************************/
    /*  Last call after EOF has been reached.                          */
    /*******************************************************************/
    if (UseTemp) {
      /*****************************************************************/
      /*  Ok, now delete old file and rename new temp file.            */
      /*****************************************************************/
      if (RenameTempFile(g))
        return RC_FX;

    } else {
      /*****************************************************************/
      /*  Because the chsize functionality is only accessible with a   */
      /*  system call we must close the file and reopen it with the    */
      /*  open function (_fopen for MS ??) this is still to be checked */
      /*  for compatibility with Text files and other OS's.            */
      /*****************************************************************/
      char filename[_MAX_PATH];
      int  h;

      /*rc= */PlugCloseFile(g, To_Fb);
      PlugSetPath(filename, To_File, Tdbp->GetPath());

      if ((h= global_open(g, MSGID_OPEN_STRERROR, filename, O_WRONLY)) <= 0)
        return RC_FX;

      /*****************************************************************/
      /*  Remove extra records.                                        */
      /*****************************************************************/
#if defined(UNIX)
      if (ftruncate(h, (off_t)(Tpos * Lrecl))) {
        sprintf(g->Message, MSG(TRUNCATE_ERROR), strerror(errno));
        close(h);
        return RC_FX;
        } // endif
#else
      if (chsize(h, Tpos * Lrecl)) {
        sprintf(g->Message, MSG(CHSIZE_ERROR), strerror(errno));
        close(h);
        return RC_FX;
        } // endif
#endif

      close(h);

#ifdef DEBTRACE
 htrc("done, h=%d irc=%d\n", h, irc);
#endif
    } // endif UseTemp

  } // endif irc

  return RC_OK;                                      // All is correct
  } // end of DeleteRecords

/***********************************************************************/
/*  Move intermediate deleted or updated lines.                        */
/*  This works only for file open in binary mode.                      */
/***********************************************************************/
bool FIXFAM::MoveIntermediateLines(PGLOBAL g, bool *b)
  {
  int   n;
  size_t req, len;

  for (*b = false, n = Fpos - Spos; n > 0; n -= req) {
    /*******************************************************************/
    /*  Non consecutive line to delete. Move intermediate lines.       */
    /*******************************************************************/
    if (!UseTemp || !*b)
      if (fseek(Stream, Headlen + Spos * Lrecl, SEEK_SET)) {
        sprintf(g->Message, MSG(READ_SEEK_ERROR), strerror(errno));
        return true;
        } // endif

    req = (size_t)min(n, Dbflen);
    len = fread(DelBuf, Lrecl, req, Stream);

#ifdef DEBTRACE
 htrc("after read req=%d len=%d\n", req, len);
#endif

    if (len != req) {
      sprintf(g->Message, MSG(DEL_READ_ERROR), (int) req, (int) len);
      return true;
      } // endif len

    if (!UseTemp)         // Delete mode, cannot be a DBF file
      if (fseek(T_Stream, Tpos * Lrecl, SEEK_SET)) {
        sprintf(g->Message, MSG(WRITE_SEEK_ERR), strerror(errno));
        return true;
        } // endif

    if ((len = fwrite(DelBuf, Lrecl, req, T_Stream)) != req) {
      sprintf(g->Message, MSG(DEL_WRITE_ERROR), strerror(errno));
      return true;
      } // endif

#ifdef DEBTRACE
 htrc("after write pos=%d\n", ftell(Stream));
#endif

    Tpos += (int)req;
    Spos += (int)req;

#ifdef DEBTRACE
 htrc("loop: Tpos=%d Spos=%d\n", Tpos, Spos);
#endif

    *b = true;
    } // endfor n

  return false;
  } // end of MoveIntermediate Lines

/***********************************************************************/
/*  Table file close routine for FIX access method.                    */
/***********************************************************************/
void FIXFAM::CloseTableFile(PGLOBAL g)
  {
  int rc = RC_OK, wrc = RC_OK;
  MODE mode = Tdbp->GetMode();

  // Closing is True if last Write was in error
  if (mode == MODE_INSERT && CurNum && !Closing) {
    // Some more inserted lines remain to be written
    Rbuf = CurNum--;
//  Closing = true;
    wrc = WriteBuffer(g);
  } else if (mode == MODE_UPDATE) {
    if (Modif && !Closing) {
      // Last updated block remains to be written
      Closing = true;
      wrc = ReadBuffer(g);
      } // endif Modif

    if (UseTemp && T_Stream && wrc == RC_OK) {
      // Copy any remaining lines
      bool b;

      Fpos = Tdbp->Cardinality(g);

      if ((rc = MoveIntermediateLines(g, &b)) == RC_OK) {
        // Delete the old file and rename the new temp file.
        RenameTempFile(g);
        goto fin;
        } // endif rc

      } // endif UseTemp

  } // endif's mode

  // Finally close the file
  rc = PlugCloseFile(g, To_Fb);

 fin:
#ifdef DEBTRACE
 htrc("FIX CloseTableFile: closing %s mode=%d wrc=%d rc=%d\n",
  To_File, mode, wrc, rc);
#endif
  Stream = NULL;           // So we can know whether table is open
  } // end of CloseTableFile

/* ------------------------- Class BGXFAM ---------------------------- */

/***********************************************************************/
/*  Implementation of the BGXFAM class.                                */
/*  This is the FAM class for FIX tables of more than 2 gigabytes.     */
/***********************************************************************/
BGXFAM::BGXFAM(PDOSDEF tdp) : FIXFAM(tdp)
  {
  Hfile = INVALID_HANDLE_VALUE;
  Tfile = INVALID_HANDLE_VALUE;
  } // end of BGXFAM constructor

BGXFAM::BGXFAM(PBGXFAM txfp) : FIXFAM(txfp)
  {
  Hfile = txfp->Hfile;
  Tfile = txfp->Tfile;
  } // end of BGXFAM copy constructor

/***********************************************************************/
/*  Set current position in a big file.                                */
/***********************************************************************/
bool BGXFAM::BigSeek(PGLOBAL g, HANDLE h, BIGINT pos, int org)
  {
#if defined(WIN32)
  char          buf[256];
  DWORD         drc;
  LARGE_INTEGER of;

  of.QuadPart = pos;
  of.LowPart = SetFilePointer(h, of.LowPart, &of.HighPart, org);

  if (of.LowPart == INVALID_SET_FILE_POINTER &&
           (drc = GetLastError()) != NO_ERROR) {
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS, NULL, drc, 0,
                  (LPTSTR)buf, sizeof(buf), NULL);
    sprintf(g->Message, MSG(SFP_ERROR), buf);
    return true;
    } // endif
#else   // !WIN32
  if (lseek64(h, pos, org) < 0) {
    sprintf(g->Message, MSG(ERROR_IN_LSK), errno);
    return true;
    } // endif
#endif  // !WIN32

  return false;
  } // end of BigSeek

/***********************************************************************/
/*  Read from a big file.                                              */
/***********************************************************************/
int BGXFAM::BigRead(PGLOBAL g, HANDLE h, void *inbuf, int req)
  {
  int rc;

#if defined(WIN32)
  DWORD nbr, drc, len = (DWORD)req;
  bool  brc = ReadFile(h, inbuf, len, &nbr, NULL);

#ifdef DEBTRACE
 htrc("after read req=%d brc=%d nbr=%d\n", req, brc, nbr);
#endif

  if (!brc) {
    char buf[256];  // , *fn = (h == Hfile) ? To_File : "Tempfile";

    drc = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS, NULL, drc, 0,
                  (LPTSTR)buf, sizeof(buf), NULL);
    sprintf(g->Message, MSG(READ_ERROR), To_File, buf);
#ifdef DEBTRACE
 htrc("BIGREAD: %s\n", g->Message);
#endif
    rc = -1;
  } else
    rc = (int)nbr;
#else   // !WIN32
  size_t  len = (size_t)req;
  ssize_t nbr = read(h, inbuf, len);

  rc = (int)nbr;
#endif  // !WIN32

  return rc;
  } // end of BigRead

/***********************************************************************/
/*  Write into a big file.                                             */
/***********************************************************************/
bool BGXFAM::BigWrite(PGLOBAL g, HANDLE h, void *inbuf, int req)
  {
  bool rc = false;

#if defined(WIN32)
  DWORD nbw, drc, len = (DWORD)req;
  bool  brc = WriteFile(h, inbuf, len, &nbw, NULL);

#ifdef DEBTRACE
 htrc("after write req=%d brc=%d nbw=%d\n", req, brc, nbw);
#endif

  if (!brc || nbw != len) {
    char buf[256], *fn = (h == Hfile) ? To_File : "Tempfile";

    if (brc)
      strcpy(buf, MSG(BAD_BYTE_NUM));
    else {
      drc = GetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS, NULL, drc, 0,
                    (LPTSTR)buf, sizeof(buf), NULL);
      } // endelse brc

    sprintf(g->Message, MSG(WRITE_STRERROR), fn, buf);

#ifdef DEBTRACE
 htrc("BIGWRITE: nbw=%d len=%d errno=%d %s\n",
  nbw, len, drc, g->Message);
#endif
    rc = true;
    } // endif brc || nbw
#else   // !WIN32
  size_t  len = (size_t)req;
  ssize_t nbw = write(h, inbuf, len);

  if (nbw != (ssize_t)len) {
    const char *fn = (h == Hfile) ? To_File : "Tempfile";

    sprintf(g->Message, MSG(WRITE_STRERROR), fn, strerror(errno));
#ifdef DEBTRACE
 htrc("BIGWRITE: nbw=%d len=%d errno=%d %s\n",
  nbw, len, errno, g->Message);
#endif
    rc = true;
    } // endif nbr
#endif  // !WIN32

  return rc;
  } // end of BigWrite

#if 0
/***********************************************************************/
/*  Reset: reset position values at the beginning of file.             */
/***********************************************************************/
void BGXFAM::Reset(void)
  {
  FIXFAM::Reset();
  Xpos = 0;
  } // end of Reset
#endif // 0

/***********************************************************************/
/*  OpenTableFile: opens a huge file using Windows/Unix API's.         */
/***********************************************************************/
bool BGXFAM::OpenTableFile(PGLOBAL g)
  {
  char    filename[_MAX_PATH];
  MODE    mode = Tdbp->GetMode();
  PDBUSER dbuserp = PlgGetUser(g);

  if ((To_Fb && To_Fb->Count) || Hfile != INVALID_HANDLE_VALUE) {
    sprintf(g->Message, MSG(FILE_OPEN_YET), To_File);
    return true;
    } // endif

  PlugSetPath(filename, To_File, Tdbp->GetPath());

#ifdef DEBTRACE
 htrc("OpenTableFile: filename=%s mode=%d\n", filename, mode);
#endif

#if defined(WIN32)
  DWORD rc, access, creation, share = 0;

  /*********************************************************************/
  /*  Create the file object according to access mode                  */
  /*********************************************************************/
  switch (mode) {
    case MODE_READ:
      access = GENERIC_READ;
      share = FILE_SHARE_READ;
      creation = OPEN_EXISTING;
      break;
    case MODE_DELETE:
      if (!Tdbp->GetNext()) {
        // Store the number of deleted rows
        DelRows = Cardinality(g);

        // This will delete the whole file and provoque ReadDB to
        // return immediately.
        access = GENERIC_READ | GENERIC_WRITE;
        creation = TRUNCATE_EXISTING;
        Tdbp->ResetSize();
        Headlen = 0;
        break;
        } // endif

      // Selective delete, pass thru
    case MODE_UPDATE:
      if ((UseTemp = Tdbp->IsUsingTemp(g)))
        access = GENERIC_READ;
      else
        access = GENERIC_READ | GENERIC_WRITE;

      creation = OPEN_EXISTING;
      break;
    case MODE_INSERT:
      access = GENERIC_WRITE;
      creation = OPEN_ALWAYS;
      break;
    default:
      sprintf(g->Message, MSG(BAD_OPEN_MODE), mode);
      return true;
    } // endswitch

  Hfile = CreateFile(filename, access, share, NULL, creation,
                               FILE_ATTRIBUTE_NORMAL, NULL);

  if (Hfile == INVALID_HANDLE_VALUE) {
    rc = GetLastError();
    sprintf(g->Message, MSG(OPEN_ERROR), rc, mode, filename);
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS, NULL, rc, 0,
                  (LPTSTR)filename, sizeof(filename), NULL);
    strcat(g->Message, filename);
  } else
    rc = 0;

#ifdef DEBTRACE
 fprintf(debug,
 " rc=%d access=%p share=%p creation=%d handle=%p fn=%s\n",
  rc, access, share, creation, Hfile, filename);
#endif

  if (mode == MODE_INSERT)
    /*******************************************************************/
    /* In Insert mode we must position the cursor at end of file.      */
    /*******************************************************************/
    if (BigSeek(g, Hfile, (BIGINT)0, FILE_END))
      return true;

#else   // UNIX
  int    rc = 0;
  int    oflag = O_LARGEFILE;         // Enable file size > 2G
  mode_t tmode = 0;

  /*********************************************************************/
  /*  Create the file object according to access mode                  */
  /*********************************************************************/
  switch (mode) {
    case MODE_READ:
      oflag |= O_RDONLY;
      break;
    case MODE_DELETE:
      if (!Tdbp->GetNext()) {
        // This will delete the whole file and provoque ReadDB to
        // return immediately.
        oflag |= (O_RDWR | O_TRUNC);
        Tdbp->ResetSize();
        break;
        } // endif

      // Selective delete, pass thru
    case MODE_UPDATE:
      UseTemp = Tdbp->IsUsingTemp(g);
      oflag |= (UseTemp) ? O_RDONLY : O_RDWR;
      break;
    case MODE_INSERT:
      oflag |= (O_WRONLY | O_CREAT | O_APPEND);
      tmode = S_IREAD | S_IWRITE;
      break;
    default:
      sprintf(g->Message, MSG(BAD_OPEN_MODE), mode);
      return true;
    } // endswitch

  Hfile= global_open(g, MSGID_OPEN_ERROR_AND_STRERROR, filename, oflag, tmode);

  if (Hfile == INVALID_HANDLE_VALUE) {
    rc = errno;
  } else
    rc = 0;

#ifdef DEBTRACE
 htrc(" rc=%d oflag=%p tmode=%p handle=%p fn=%s\n",
  rc, oflag, tmode, Hfile, filename);
#endif

#endif  // UNIX

  if (!rc) {
    if (!To_Fb) {
      To_Fb = (PFBLOCK)PlugSubAlloc(g, NULL, sizeof(FBLOCK));
      To_Fb->Fname = To_File;
      To_Fb->Type = TYPE_FB_HANDLE;
      To_Fb->Memory = NULL;
      To_Fb->Length = 0;
      To_Fb->Mode = mode;
      To_Fb->File = NULL;
      To_Fb->Next = dbuserp->Openlist;
      dbuserp->Openlist = To_Fb;
      } // endif To_Fb

    To_Fb->Count = 1;
    To_Fb->Mode = mode;
    To_Fb->Handle = Hfile;

    /*******************************************************************/
    /*  Allocate the block buffer.                                     */
    /*******************************************************************/
    return AllocateBuffer(g);
  } else
    return (mode == MODE_READ && rc == ENOENT)
            ? PushWarning(g, Tdbp) : true;

  } // end of OpenTableFile

/***********************************************************************/
/*  BIGFIX Cardinality: returns table cardinality in number of rows.   */
/*  This function can be called with a null argument to test the       */
/*  availability of Cardinality implementation (1 yes, 0 no).          */
/***********************************************************************/
int BGXFAM::Cardinality(PGLOBAL g)
  {
  if (g) {
    char   filename[_MAX_PATH];
    int   card = -1;
    BIGINT fsize;

    PlugSetPath(filename, To_File, Tdbp->GetPath());

#if defined(WIN32)  // OB
    LARGE_INTEGER len;
    DWORD         rc = 0;

    len.QuadPart = -1;

    if (Hfile == INVALID_HANDLE_VALUE) {
      HANDLE h = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ,
                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

      if (h == INVALID_HANDLE_VALUE)
        if ((rc = GetLastError()) != ERROR_FILE_NOT_FOUND) {
          sprintf(g->Message, MSG(OPEN_ERROR), rc, 10, filename);
          FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS, NULL, rc, 0,
                        (LPTSTR)filename, sizeof(filename), NULL);
          strcat(g->Message, filename);
          return -1;
        } else
          return 0;                     // File does not exist

      // Get the size of the file (can be greater than 4 GB)
      len.LowPart = GetFileSize(h, (LPDWORD)&len.HighPart);
      CloseHandle(h);
    } else
      len.LowPart = GetFileSize(Hfile, (LPDWORD)&len.HighPart);

    if (len.LowPart == 0xFFFFFFFF && (rc = GetLastError()) != NO_ERROR) {
      sprintf(g->Message, MSG(FILELEN_ERROR), "GetFileSize", filename);
      return -2;
    } else
      fsize = len.QuadPart;

#else    // UNIX
    if (Hfile == INVALID_HANDLE_VALUE) {
      int h = open64(filename, O_RDONLY, 0);

#ifdef DEBTRACE
 htrc(" h=%d\n", h);
#endif

      if (h == INVALID_HANDLE_VALUE) {
#ifdef DEBTRACE
 htrc("  errno=%d ENOENT=%d\n", errno, ENOENT);
#endif
        if (errno != ENOENT) {
          sprintf(g->Message, MSG(OPEN_ERROR_IS),
                              filename, strerror(errno));
          return -1;
        } else
          return 0;                      // File does not exist

        } // endif h

      // Get the size of the file (can be greater than 4 GB)
      fsize = lseek64(h, 0, SEEK_END);
      close(h);
    } else {
      BIGINT curpos = lseek64(Hfile, 0, SEEK_CUR);

      fsize = lseek64(Hfile, 0, SEEK_END);
      lseek64(Hfile, curpos, SEEK_SET);
    } // endif Hfile

    if (fsize < 0) {
      sprintf(g->Message, MSG(FILELEN_ERROR), "lseek64", filename);
      return -2;
      } // endif fsize

#endif   // UNIX

    // Check the real size of the file
    if (Padded && Blksize) {
      if (fsize % (BIGINT)Blksize) {
        sprintf(g->Message, MSG(NOT_FIXED_LEN),
                            filename, (int)fsize, Lrecl);
        return -3;
      } else
        card = (int)(fsize / (BIGINT)Blksize) * Nrec;

    } else if (fsize % (BIGINT)Lrecl) {
      sprintf(g->Message, MSG(NOT_FIXED_LEN), filename, (int)fsize, Lrecl);
      return -3;
    } else
      card = (int)(fsize / (BIGINT)Lrecl); // Fixed length file

#ifdef DEBTRACE
 htrc(" Computed max_K=%d fsize=%lf lrecl=%d\n",
  card, (double)fsize, Lrecl);
#endif

    // Set number of blocks for later use
    Block = (card + Nrec - 1) / Nrec;
    return card;
  } else
    return -1;

  } // end of Cardinality

/***********************************************************************/
/*  ReadBuffer: Read Nrec lines for a big fixed/binary file.           */
/***********************************************************************/
int BGXFAM::ReadBuffer(PGLOBAL g)
  {
  int nbr, rc = RC_OK;

  if (!Closing) {
    /*******************************************************************/
    /*  Sequential reading when Placed is not true.                    */
    /*******************************************************************/
    if (Placed) {
      Tdbp->SetLine(To_Buf + CurNum * Lrecl);
      Placed = false;
    } else if (++CurNum < Rbuf) {
      Tdbp->IncLine(Lrecl);                // Used by DOSCOL functions
      return RC_OK;
    } else if (Rbuf < Nrec && CurBlk != -1) {
      return RC_EF;
    } else {
      /*****************************************************************/
      /*  New block.                                                   */
      /*****************************************************************/
      CurNum = 0;
      Tdbp->SetLine(To_Buf);

      if (++CurBlk >= Block)
        return RC_EF;

     } // endif's

    if (OldBlk == CurBlk) {
      IsRead = true;       // Was read indeed
      return RC_OK;        // Block is already there
      } // endif OldBlk

    } // endif !Closing

  if (Modif) {
    /*******************************************************************/
    /*  The old block was modified in Update mode.                     */
    /*  In Update mode we simply rewrite the old block on itself.      */
    /*******************************************************************/
    bool moved = false;

    if (UseTemp)                // Copy any intermediate lines.
      if (MoveIntermediateLines(g, &moved))
        rc = RC_FX;

    if (rc == RC_OK) {
      // Set file position to OldBlk position (Fpos)
      if (!moved && BigSeek(g, Hfile, (BIGINT)Fpos * (BIGINT)Lrecl))
        rc = RC_FX;
      else if (BigWrite(g, Tfile, To_Buf, Lrecl * Rbuf))
        rc = RC_FX;

      Spos = Fpos + Nrec;       // + Rbuf ???
      } // endif rc

    if (Closing || rc != RC_OK) // Error or called from CloseDB
      return rc;

    // NOTE: Next line was added to avoid a very  strange fread bug.
    // When the fseek is not executed (even the file has the good
    // pointer position) the next read can happen anywhere in the file.
    OldBlk = CurBlk;       // This will force fseek to be executed
    Modif = 0;
    } // endif Mode

  Fpos = CurBlk * Nrec;

  // Setting file pointer is required only in non sequential reading
  if (CurBlk != OldBlk + 1)
    if (BigSeek(g, Hfile, (BIGINT)Fpos * (BIGINT)Lrecl))
      return RC_FX;

#ifdef DEBTRACE
 htrc("File position is now %d\n", Fpos);
#endif

  nbr = BigRead(g, Hfile, To_Buf, (Padded) ? Blksize : Lrecl * Nrec);

  if (nbr > 0) {
    Rbuf = (Padded) ? Nrec : nbr / Lrecl;
    rc = RC_OK;
    ReadBlks++;
    num_read++;
  } else
    rc = (nbr == 0) ? RC_EF : RC_FX;

  OldBlk = CurBlk;             // Last block actually read
  IsRead = true;               // Is read indeed
  return rc;
  } // end of ReadBuffer

/***********************************************************************/
/*  WriteBuffer: File write routine for BGXFAM access method.          */
/*  Updates are written into the (Temp) file in ReadBuffer.            */
/***********************************************************************/
int BGXFAM::WriteBuffer(PGLOBAL g)
  {
#ifdef DEBTRACE
 fprintf(debug,
  "BIG WriteDB: Mode=%d buf=%p line=%p Nrec=%d Rbuf=%d CurNum=%d\n",
  Tdbp->GetMode(), To_Buf, Tdbp->GetLine(), Nrec, Rbuf, CurNum);
#endif

  if (Tdbp->GetMode() == MODE_INSERT) {
    /*******************************************************************/
    /*  In Insert mode, blocks are added sequentialy to the file end.  */
    /*******************************************************************/
    if (++CurNum != Rbuf) {
      Tdbp->IncLine(Lrecl);            // Used by DOSCOL functions
      return RC_OK;                    // We write only full blocks
      } // endif CurNum

#ifdef DEBTRACE
 htrc(" First line is '%.*s'\n", Lrecl - 2, To_Buf);
#endif

    //  Now start the writing process.
    if (BigWrite(g, Hfile, To_Buf, Lrecl * Rbuf))
      return RC_FX;

    CurBlk++;
    CurNum = 0;
    Tdbp->SetLine(To_Buf);

#ifdef DEBTRACE
 htrc("write done\n");
#endif

  } else {                           // Mode == MODE_UPDATE
    // Tfile is the temporary file or the table file handle itself
    if (Tfile == INVALID_HANDLE_VALUE)
    {
      if (UseTemp /*&& Tdbp->GetMode() == MODE_UPDATE*/) {
        if (OpenTempFile(g))
          return RC_FX;

      } else
        Tfile = Hfile;
    }
    Modif++;                         // Modified line in Update mode
  } // endif Mode

  return RC_OK;
  } // end of WriteBuffer

/***********************************************************************/
/*  Data Base delete line routine for BGXFAM access method.            */
/***********************************************************************/
int BGXFAM::DeleteRecords(PGLOBAL g, int irc)
  {
  bool moved;

  /*********************************************************************/
  /*  There is an alternative here:                                    */
  /*  1 - use a temporary file in which are copied all not deleted     */
  /*      lines, at the end the original file will be deleted and      */
  /*      the temporary file renamed to the original file name.        */
  /*  2 - directly move the not deleted lines inside the original      */
  /*      file, and at the end erase all trailing records.             */
  /*  This will be experimented.                                       */
  /*********************************************************************/
#ifdef DEBTRACE
 fprintf(debug,
  "BGX DeleteDB: rc=%d UseTemp=%d Fpos=%d Tpos=%d Spos=%d\n",
  irc, UseTemp, Fpos, Tpos, Spos);
#endif

  if (irc != RC_OK) {
    /*******************************************************************/
    /*  EOF: position Fpos at the end-of-file position.                */
    /*******************************************************************/
    Fpos = Tdbp->Cardinality(g);
#ifdef DEBTRACE
 htrc("Fpos placed at file end=%d\n", Fpos);
#endif
  } else    // Fpos is the deleted line position
    Fpos = CurBlk * Nrec + CurNum;

  if (Tpos == Spos) {
    /*******************************************************************/
    /*  First line to delete. Move of eventual preceeding lines is     */
    /*  not required here if a temporary file is not used, just the    */
    /*  setting of future Spos and Tpos.                               */
    /*******************************************************************/
    if (UseTemp) {
      /*****************************************************************/
      /*  Open the temporary file, Spos is at the beginning of file.   */
      /*****************************************************************/
      if (OpenTempFile(g))
        return RC_FX;

    } else {
      /*****************************************************************/
      /*  Move of eventual preceeding lines is not required here.      */
      /*  Set the target file as being the source file itself.         */
      /*  Set the future Tpos, and give Spos a value to block copying. */
      /*****************************************************************/
      Tfile = Hfile;
      Spos = Tpos = Fpos;
    } // endif UseTemp

    } // endif Tpos == Spos

  /*********************************************************************/
  /*  Move any intermediate lines.                                     */
  /*********************************************************************/
  if (MoveIntermediateLines(g, &moved))
    return RC_FX;

  if (irc == RC_OK) {
#ifdef DEBTRACE
    assert(Spos == Fpos);
#endif
    Spos++;          // New start position is on next line

    if (moved) {
      if (BigSeek(g, Hfile, (BIGINT)Spos * (BIGINT)Lrecl))
        return RC_FX;

      OldBlk = -2;  // To force fseek to be executed on next block
      } // endif moved

#ifdef DEBTRACE
 htrc("after: Tpos=%d Spos=%d\n", Tpos, Spos);
#endif

  } else {
    /*******************************************************************/
    /*  Last call after EOF has been reached.                          */
    /*******************************************************************/
    char filename[_MAX_PATH];

    PlugSetPath(filename, To_File, Tdbp->GetPath());

    if (UseTemp) {
      /*****************************************************************/
      /*  Ok, now delete old file and rename new temp file.            */
      /*****************************************************************/
      if (RenameTempFile(g))
        return RC_FX;

    } else {
      /*****************************************************************/
      /*  Remove extra records.                                        */
      /*****************************************************************/
#if defined(WIN32)
      if (BigSeek(g, Hfile, (BIGINT)Tpos * (BIGINT)Lrecl))
        return RC_FX;

      if (!SetEndOfFile(Hfile)) {
        DWORD drc = GetLastError();

        sprintf(g->Message, MSG(SETEOF_ERROR), drc);
        return RC_FX;
        } // endif error
#else   // !WIN32
      if (ftruncate64(Hfile, (BIGINT)(Tpos * Lrecl))) {
        sprintf(g->Message, MSG(TRUNCATE_ERROR), strerror(errno));
        return RC_FX;
        } // endif
#endif  // !WIN32

    } // endif UseTemp

  } // endif irc

  return RC_OK;                                      // All is correct
  } // end of DeleteRecords

/***********************************************************************/
/*  Open a temporary file used while updating or deleting.             */
/***********************************************************************/
bool BGXFAM::OpenTempFile(PGLOBAL g)
  {
  char   *tempname;
  PDBUSER dup = PlgGetUser(g);

  /*********************************************************************/
  /*  Open the temporary file, Spos is at the beginning of file.       */
  /*********************************************************************/
  tempname = (char*)PlugSubAlloc(g, NULL, _MAX_PATH);
  PlugSetPath(tempname, To_File, Tdbp->GetPath());
  strcat(PlugRemoveType(tempname, tempname), ".t");
  remove(tempname);       // Be sure it does not exist yet

#if defined(WIN32)
  Tfile = CreateFile(tempname, GENERIC_WRITE, 0, NULL,
                     CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

  if (Tfile == INVALID_HANDLE_VALUE) {
    DWORD rc = GetLastError();
    sprintf(g->Message, MSG(OPEN_ERROR), rc, MODE_INSERT, tempname);
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
              FORMAT_MESSAGE_IGNORE_INSERTS, NULL, rc, 0,
              (LPTSTR)tempname, _MAX_PATH, NULL);
    strcat(g->Message, tempname);
    return true;
    } // endif Tfile
#else    // UNIX
  Tfile = open64(tempname, O_WRONLY | O_TRUNC, S_IWRITE);

  if (Tfile == INVALID_HANDLE_VALUE) {
    int rc = errno;
    sprintf(g->Message, MSG(OPEN_ERROR), rc, MODE_INSERT, tempname);
    strcat(g->Message, strerror(errno));
    return true;
    } //endif Tfile
#endif   // UNIX

  To_Fbt = (PFBLOCK)PlugSubAlloc(g, NULL, sizeof(FBLOCK));
  To_Fbt->Fname = tempname;
  To_Fbt->Type = TYPE_FB_HANDLE;
  To_Fbt->Memory = NULL;
  To_Fbt->Length = 0;
  To_Fbt->File = NULL;
  To_Fbt->Next = dup->Openlist;
  To_Fbt->Count = 1;
  To_Fbt->Mode = MODE_INSERT;
  To_Fbt->Handle = Tfile;
  dup->Openlist = To_Fbt;
  return false;
  } // end of OpenTempFile

/***********************************************************************/
/*  Move intermediate deleted or updated lines.                        */
/***********************************************************************/
bool BGXFAM::MoveIntermediateLines(PGLOBAL g, bool *b)
  {
  int n, req, nbr;

  for (*b = false, n = Fpos - Spos; n > 0; n -= req) {
    /*******************************************************************/
    /*  Non consecutive line to delete. Move intermediate lines.       */
    /*******************************************************************/
    if (!UseTemp || !*b)
      if (BigSeek(g, Hfile, (BIGINT)Spos * (BIGINT)Lrecl))
        return true;

    req = min(n, Dbflen) * Lrecl;

    if ((nbr = BigRead(g, Hfile, DelBuf, req)) != req) {
      sprintf(g->Message, MSG(DEL_READ_ERROR), req, nbr);
      return true;
      } // endif nbr

    if (!UseTemp)
      if (BigSeek(g, Tfile, (BIGINT)Tpos * (BIGINT)Lrecl))
        return true;

    if (BigWrite(g, Tfile, DelBuf, req))
      return true;

    req /= Lrecl;
    Tpos += (int)req;
    Spos += (int)req;

#ifdef DEBTRACE
 htrc("loop: Tpos=%d Spos=%d\n", Tpos, Spos);
#endif

    *b = true;
    } // endfor n

  return false;
  } // end of MoveIntermediateLines

/***********************************************************************/
/*  Data Base close routine for BIGFIX access method.                  */
/***********************************************************************/
void BGXFAM::CloseTableFile(PGLOBAL g)
  {
  int rc = RC_OK, wrc = RC_OK;
  MODE mode = Tdbp->GetMode();

  // Closing is True if last Write was in error
  if (mode == MODE_INSERT && CurNum && !Closing) {
    // Some more inserted lines remain to be written
    Rbuf = CurNum--;
    wrc = WriteBuffer(g);
  } else if (mode == MODE_UPDATE) {
    if (Modif && !Closing) {
      // Last updated block remains to be written
      Closing = true;
      wrc = ReadBuffer(g);
      } // endif Modif

    if (UseTemp && Tfile && wrc == RC_OK) {
      // Copy any remaining lines
      bool b;

      Fpos = Tdbp->Cardinality(g);

      if ((rc = MoveIntermediateLines(g, &b)) == RC_OK) {
        // Delete the old file and rename the new temp file.
        RenameTempFile(g);
        goto fin;
        } // endif rc

      } // endif UseTemp

  } // endif's mode

  // Finally close the file
  rc = PlugCloseFile(g, To_Fb);

 fin:
#ifdef DEBTRACE
 htrc("BGX CloseTableFile: closing %s mode=%d wrc=%d rc=%d\n",
  To_File, mode, wrc, rc);
#endif
  Hfile = INVALID_HANDLE_VALUE; // So we can know whether table is open
  } // end of CloseTableFile

/***********************************************************************/
/*  Rewind routine for huge FIX access method.                         */
/*  Note: commenting out OldBlk = -1 has two advantages:               */
/*  1 - It forces fseek on  first block, thus suppressing the need to  */
/*      rewind the file, anyway unuseful when second pass if indexed.  */
/*  2 - It permit to avoid re-reading small tables having only 1 block.*/
/*      (even very unlikely for huge files!)                           */
/***********************************************************************/
void BGXFAM::Rewind(void)
  {
#if 0    // This is probably unuseful because file is accessed directly
#if defined(WIN32)  //OB
  SetFilePointer(Hfile, 0, NULL, FILE_BEGIN);
#else    // UNIX
  lseek64(Hfile, 0, SEEK_SET);
#endif   // UNIX
#endif // 0
  CurBlk = -1;
  CurNum = Rbuf;
//OldBlk = -1;
//Rbuf = 0;        commented out in case we reuse last read block
  Fpos = 0;
  } // end of Rewind
