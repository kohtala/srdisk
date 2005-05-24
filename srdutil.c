/* ReSizeable RAMDisk - general utilities for SRDISK.EXE only
 * Copyright (C) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "srdisk.h"
#include <stdio.h>
#include <conio.h>
#include <ctype.h>
#include <dos.h>

/*
**  Local memory allocation routine with error check
*/

void *xalloc(size_t s)
{
  void *b = malloc(s);
  if (!b)
    fatal("malloc() failed - no work space!");
  return b;
}

/*
**  Get Y/N response to query about permission to destroy data
*/

int getYN(void)
{
  int reply;

  switch(force_f) {
  case YES:
    reply = 'Y';
    break;
  case NO:
    reply = 'N';
    break;
  default:
    do reply = toupper(getch());
    while (reply != 'Y' && reply != 'N');
    break;
  }
  printf("%c\n", reply);
  if (reply == 'N') {
    force_f = NO;
    return 0;
  }
  force_f = YES;
  return 1;
}

/*
**  DOS time format conversions
*/

dword DOS_time(time_t time)
{
  struct tm *ltime;
  union {
    struct {
      unsigned int
           sec2 : 5,
           min : 6,
           hour : 5,
           day : 5,
           month : 4,
           year : 7;
    } f;
    dword l;
  } file_time;

  ltime = localtime(&time);
  file_time.f.sec2 = ltime->tm_sec;
  file_time.f.min = ltime->tm_min;
  file_time.f.hour = ltime->tm_hour;
  file_time.f.day = ltime->tm_mday;
  file_time.f.month = ltime->tm_mon + 1;
  file_time.f.year = ltime->tm_year - 80;

  return file_time.l;
}


/*
**  CONFIGURATION POINTER CHECKUP
*/

struct config_s far *conf_ptr(struct dev_hdr _seg *dev)
{
  struct config_s far *conf;
  if (!dev) return (void far *)NULL;
  conf = (struct config_s far *)MK_FP(dev, dev->conf);
  if (dev->u.s.ID[0] != 'S'
   || dev->u.s.ID[1] != 'R'
   || dev->u.s.ID[2] != 'D'
   || dev->v_format != V_FORMAT
   || (conf->drive != '$'
      && !(   (conf->drive >= 'A' && conf->drive <= 'Z')
           || (conf->drive >= '1' && conf->drive <= '9')) )
   || !conf->disk_IO
   || !conf->malloc_off)
    fatal("SRDISK devices' internal tables are messed up!");
  return conf;
}

/*
**  Check for Windows Enhanced mode (namely to protect memory handles)
*/

int isWinEnh(void)
{
  /* Windows 3.1 Enhanced mode gives problems with allocation, so
     check to see if handle can be changed */
  static int WinEnh = -1;

  if (WinEnh == -1) {
    WinEnh = 0;
    asm {
      mov ax,0x1600
      int 0x2f
      and al,0x7f
      jz notWinEnh
      mov WinEnh,1
    }
//    printf("In Windows Enhanced mode!\n");
  }
 notWinEnh:
 return WinEnh;
}

