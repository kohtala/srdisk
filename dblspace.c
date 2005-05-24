/* Microsoft DoubleSpace disk compression software support
 * Copyright (C) 1994-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "srdisk.h"
#include <dos.h>

typedef struct
{
  char firstUsedDrive;
  unsigned char drivesUsed;
  word version;
} DblSpaceVersion;

typedef struct
{
  char hostDrive;
  unsigned char sequence;
} DblSpaceHost;

DblSpaceVersion DblSpace_getVersion(void)
{
  union REGS inregs, outregs;
  static const DblSpaceVersion retDefault = { 0, 0, 0 };
  DblSpaceVersion ret = retDefault;

  inregs.x.ax = 0x4A11;
  inregs.x.bx = inregs.x.cx = inregs.x.dx = 0;
  int86(0x2F, &inregs, &outregs);
  if (outregs.x.ax == 0 && outregs.x.bx == 0x444D)
  {
    ret.firstUsedDrive = outregs.h.cl;
    ret.drivesUsed = outregs.h.ch;
    ret.version = outregs.x.dx;
  }
  return ret;
}

DblSpaceHost DblSpace_getDriveMapping(char drive)
{
  union REGS inregs, outregs;
  static const DblSpace retDefault = { 0, 0 };
  DblSpaceHost ret = retDefault;

  inregs.x.ax = 0x4A11;
  inregs.x.bx = 1;
  inregs.h.dl = drive;
  int86(0x2F, &inregs, &outregs);
  if (outregs.x.ax == 0)
  {
    ret.hostDrive = outregs.h.bl;
    ret.sequence = outregs.h.bh;
  }
  return ret;
}

bool
DblSpace_activateDrive(char drive, char host, unsigned sequence)
{
  union REGS inregs, outregs;
  struct SREGS sregs;
  typedef struct
  {
    word signature;
    byte command;
    byte status;
    byte host;
    ????
  } ActivationRecord;
  static const ActivationRecord arInit = { 0x444D, 'M', 0xFF };
  ActivationRecord ar = arInit;

  inregs.x.ax = 0x4A11;
  inregs.x.bx = 5;
  inregs.h.dl = drive;
  inregs.x.si = &ar;
  sregs.es = sregs.ds = _DS;
  int86x(0x2F, &inregs, &outregs, &sregs);
  if (ar.status != 0)
  {
    return false;
  }
  return true;
}

bool
DblSpace_deactivateDrive(char drive)
{
  union REGS regs;

  inregs.x.ax = 0x4A11;
  inregs.x.bx = 6;
  inregs.h.dl = drive;
  int86(0x2F, &regs, &regs);
  if (regs.x.ax != 0)
  {
    return false
  }
  return true;
}

