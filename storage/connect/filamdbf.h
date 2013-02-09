/***************** FilAmDbf H Declares Source Code File (.H) ****************/
/*  Name: filamdbf.h    Version 1.3                                         */
/*                                                                          */
/*  (C) Copyright to the author Olivier BERTRAND          2005-2012         */
/*                                                                          */
/*  This file contains the DBF file access method classes declares.         */
/****************************************************************************/

#ifndef __FILAMDBF_H
#define __FILAMDBF_H

#include "filamfix.h"
#include "filamap.h"

typedef class DBFBASE *PDBF;
typedef class DBFFAM  *PDBFFAM;
typedef class DBMFAM  *PDBMFAM;

/****************************************************************************/
/*  Functions used externally.                                              */
/****************************************************************************/
PQRYRES DBFColumns(PGLOBAL g, char *fn, BOOL info);

/****************************************************************************/
/*  This is the base class for dBASE file access methods.                   */
/****************************************************************************/
class DllExport DBFBASE {
 public:
  // Constructors
  DBFBASE(PDOSDEF tdp);
  DBFBASE(PDBF txfp);

  // Implementation
  int  ScanHeader(PGLOBAL g, PSZ fname, int lrecl, char *defpath);

 protected:
  // Default constructor, not to be used
  DBFBASE(void) {}

  // Members
  int  Records;                     /*  records in the file                 */
  bool Accept;                      /*  true if bad lines are accepted      */
  int  Nerr;                        /*  Number of bad records                */
  int  Maxerr;                      /*  Maximum number of bad records        */
  int  ReadMode;                    /*  1: ALL 2: DEL 0: NOT DEL            */
//PSZ  Defpath;                      /*  Default data path                   */
  }; // end of class DBFBASE

/****************************************************************************/
/*  This is the DOS/UNIX Access Method class declaration for DBase files.   */
/****************************************************************************/
class DllExport DBFFAM : public FIXFAM, public DBFBASE {
 public:
  // Constructors
  DBFFAM(PDOSDEF tdp) : FIXFAM(tdp), DBFBASE(tdp) {}
  DBFFAM(PDBFFAM txfp) : FIXFAM(txfp), DBFBASE((PDBF)txfp) {}

  // Implementation
  virtual AMT  GetAmType(void) {return TYPE_AM_DBF;}
  virtual PTXF Duplicate(PGLOBAL g)
                {return (PTXF)new(g) DBFFAM(this);}

  // Methods
  virtual int  GetNerr(void) {return Nerr;}
  virtual int  Cardinality(PGLOBAL g);
//virtual int  GetRowID(void);              // Temporarily suppressed
  virtual bool OpenTableFile(PGLOBAL g);
  virtual bool AllocateBuffer(PGLOBAL g);
  virtual void ResetBuffer(PGLOBAL g);
  virtual int  ReadBuffer(PGLOBAL g);
//virtual int  WriteBuffer(PGLOBAL g);
  virtual int  DeleteRecords(PGLOBAL g, int irc);
  virtual void CloseTableFile(PGLOBAL g);
  virtual void Rewind(void);

 protected:
  // Members
  virtual bool CopyHeader(PGLOBAL g);

//int  Records;                       in TXTFAM
//int  Headlen;                       in TXTFAM
  }; // end of class DBFFAM

/****************************************************************************/
/*  This is the DOS/UNIX Access Method class declaration for DBase files    */
/*  using file mapping to access the file.                                  */
/****************************************************************************/
class DllExport DBMFAM : public MPXFAM, public DBFBASE {
 public:
  // Constructors
  DBMFAM(PDOSDEF tdp) : MPXFAM(tdp), DBFBASE(tdp) {}
  DBMFAM(PDBMFAM txfp) : MPXFAM(txfp), DBFBASE((PDBF)txfp) {}

  // Implementation
  virtual AMT  GetAmType(void) {return TYPE_AM_DBF;}
  virtual PTXF Duplicate(PGLOBAL g)
                {return (PTXF)new(g) DBMFAM(this);}
  virtual  int  GetDelRows(void);

  // Methods
  virtual int  GetNerr(void) {return Nerr;}
  virtual int  Cardinality(PGLOBAL g);
//virtual int  GetRowID(void);              // Temporarily suppressed
  virtual bool AllocateBuffer(PGLOBAL g);
  virtual int  ReadBuffer(PGLOBAL g);
//virtual int  WriteBuffer(PGLOBAL g);
  virtual int  DeleteRecords(PGLOBAL g, int irc);
  virtual void Rewind(void);

 protected:
  // Members
//int  Records;                       in TXTFAM
//int  Headlen;                       in TXTFAM
  }; // end of class DBFFAM

#endif // __FILAMDBF_H
