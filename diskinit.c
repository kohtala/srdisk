/* ReSizeable RAMDisk - disk initialization
 * Copyright (C) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

/*

This module initialized the drive for SRDISK.EXE

External functions called:

  conf_ptr
  div
  fatal
  fprintf
  printf
  print_format
  stringisize_flags
  toupper
  warning

External variables defined:

  mainconf
  conf
  data_on_disk
  f

External variables used:

  drive
  defined_format        ; For gimmics
  forced_format         ; For gimmics
  verbose

*/

#include "srdisk.h"
#include <stdio.h>
#include <dos.h>
#include <string.h>

/*
**  Resolves a drive letter in configuration structure
**
**  No must to return in error.
**  Must set a usable drive letter into conf->drive.
*/

static void resolve_drive(struct config_s far *conf)
{
  byte far *dpb;
  int device, next;
  struct devhdr far *dev;
  byte far *cp;
  int drives_searched = 0;

  asm {
    mov ah,0x52     /* SYSVARS call */
    int 0x21        /* Call DOS */
  }
  dpb = *(byte far * far *)MK_FP(_ES,_BX);

  if (_osmajor < 4) {
    device = 0x12;
    next = 0x18;
  }
  else {
    device = 0x13;
    next = 0x19;
  }

  do {
    if ( *dpb > 25 || drives_searched > 25) {
      warning("Cannot read DOS Drive Parameter Block chain");
      goto failed_return;
    }
    dev = *(struct devhdr far * far *)(dpb+device);
    if ( (dev->dh_attr & 0x8000) == 0 ) {      /* Is it block device? */
      if (   FP_SEG(dev) == FP_SEG(conf)
      /* The LastByte loadhigh utility */
      ||   ( *(cp = (byte far *)MK_FP(FP_SEG(dev), dev->dh_strat)) == 0xEA
            && *(word far *)(cp+3) == FP_SEG(conf)
          && *(cp = (byte far *)MK_FP(FP_SEG(dev), dev->dh_inter)) == 0xEA
            && *(word far *)(cp+3) == FP_SEG(conf) )
      /* QEMM loadhi utility */
      ||   ( *(word *)(((struct dev_hdr *)dev)->u.s.ID) == 0x8283
          && ((word *)dev)[10] == FP_SEG(conf) ) )
      {
        conf->drive = *dpb + 'A';
        return;
      }
    }
    dpb = *(byte far * far *)(dpb+next);
    drives_searched++;
  } while ( FP_OFF(dpb) != 0xFFFF );

  warning("SRDISK drive not in DOS Drive Parameter Block chain");
 failed_return:
  fprintf(stderr, "\nYou should define the proper drive letter in CONFIG.SYS\n"
                  "Example: DEVICE=SRDISK.SYS D:\n");
  { struct config_s far *c = mainconf;
    int drive = 1;
    while (c != conf) c = conf_ptr(c->next_drive), drive++;
    conf->drive = '0' + drive;
  }
}

/*
**  RETRIEVE OLD FORMAT FOR DISK
**
**  Must collect and fill global structure f to hold the old format of
**  the disk.
**  Expected to not return if error detected.
*/

static void retrieve_old_format(void)
{
  struct config_s far *subconf;
  int i;
  int has_32bitsec = 1;

  memset(&f, 0, sizeof f);

  /* Scan the chain of drivers linked to the same drive */
  for (subconf = conf, i = 0; subconf; subconf = conf_ptr(subconf->next), i++) {
    /* Make sure f.max_size does not overflow */
    if (f.max_size && -f.max_size <= subconf->maxK)
      f.max_size = -1;
    else
      f.max_size += subconf->maxK;
    f.current_size += subconf->size;

    f.subconf[i].conf = subconf;
    f.subconf[i].maxK = subconf->maxK;
    f.subconf[i].size = subconf->size;
    if (!(subconf->flags & C_32BITSEC))
      has_32bitsec = 0;
    f.chain_len++;
  }

  if (!has_32bitsec)
    f.max_size = 32767L;

  f.size = conf->tsize;
  f.write_prot = (f.size == 0 || (conf->RW_access & WRITE_ACCESS)) ? OFF : ON;
  f.removable = (conf->RW_access & NONREMOVABLE) ? OFF : ON;
  f.bps_shift = conf->bps_shift;
  f.bps = conf->BPB_bps;
  f.spc = conf->BPB_spc;
  f.reserved = conf->BPB_reserved;
  f.FATs = conf->BPB_FATs;
  f.dir_entries = conf->BPB_dir;
  f.spFAT = conf->BPB_FATsectors;
  /* Must check both since writing to boot sector might change this */
  f.sectors = conf->BPB_sectors ? conf->BPB_sectors : conf->BPB_tsectors;
  f.used_sectors = f.sectors;
  f.sec_per_track = conf->BPB_spt;
  f.sides = conf->BPB_heads;
  f.device_type = (enum DeviceType_e)conf->device_type;
  f.media = conf->BPB_media;

  if (f.bps == 0)       /* This is used for division, so give it a default */
    f.bps = 128;
  if (f.spc == 0)       /* This is used for division, so give it a default */
    f.spc = 4;

  f.FAT_sectors = f.spFAT * f.FATs;
  { div_t divr;
    divr = div(f.dir_entries * 32, f.bps);
    f.dir_sectors = divr.quot + (divr.rem ? 1 : 0);
  }
  f.dir_start = f.reserved + f.FAT_sectors;
  f.system_sectors = f.dir_start + f.dir_sectors;
  f.cluster_size = f.spc * f.bps;
  if (f.size) {
    f.data_sectors = f.sectors - f.system_sectors;
    f.clusters = f.data_sectors / f.spc;
  }
  f.FAT_type = f.clusters > 4086 ? 16 : 12;
}

/*
**  Initialize device driver interface by locating the proper driver
**
**  Expected not to return if error found.
*/

void init_drive(void)
{
  union REGS inregs, outregs;
  struct SREGS sregs;
  struct dev_hdr _seg *dev;
  char suggest_drive;

  inregs.x.ax = MULTIPLEXAH * 0x100;
  inregs.x.bx = inregs.x.cx = inregs.x.dx = 0;
  int86x(0x2F, &inregs, &outregs, &sregs);
  if (outregs.h.al != 0xFF)
    fatal("No SRDISK driver installed");

  inregs.x.ax = MULTIPLEXAH * 0x100 + 1;
  int86x(0x2F, &inregs, &outregs, &sregs);
  dev = (struct dev_hdr _seg *)sregs.es;

  if (!dev
   || dev->u.s.ID[0] != 'S'
   || dev->u.s.ID[1] != 'R'
   || dev->u.s.ID[2] != 'D')
  {
    fatal("Some other driver found at SRDISK multiplex number");
  }
  else if (dev->v_format != V_FORMAT) {
    fatal("Invalid SRDISK driver version");
  }
  conf = mainconf = conf_ptr(dev);

  /* Check if driver does not know yet what drive it is */
  do {
    if ( conf->drive == '$' ) {
      resolve_drive(conf);
    }
    conf = conf_ptr(conf->next_drive);
  } while ( conf );
  conf = mainconf;

  suggest_drive = drive ? drive : _getdrive() - 1 + 'A';

  while(conf->drive != suggest_drive) {
    if ( ! (conf = conf_ptr(conf->next_drive)) )
      if (drive)
        fatal("Drive not ReSizeable RAMDisk");
      else {
        conf = mainconf;
        suggest_drive = conf->drive;
      }
  }
  drive = suggest_drive;

  retrieve_old_format();    /* Setup f */

  data_on_disk = (f.size && !(forced_format & CLEAR_DISK)) ? 1 : 0;

  // Calculate the free memory if it is needed
  if (defined_format & FREE_MEMORY) {
    extern void calcMaxMemory(void);
    calcMaxMemory();
  }

  if (verbose > 3) print_format(&f);
  if (verbose > 4) {
    struct config_s far *subconf = conf;
    int part = 1;
    for( ; subconf; subconf = conf_ptr(subconf->next), part++) {
      printf("Driver %d of %d\n"
             "  Version %.4Fs\n"
             "  Memory: %.4Fs\n"
             "  Flags:%s\n"
             "  Max size: %luK\n"
             "  Size: %luK\n"
             "  Sectors: %lu\n"
             ,part, f.chain_len
             ,((struct dev_hdr _seg *)FP_SEG(subconf))->u.s.version
             ,((struct dev_hdr _seg *)FP_SEG(subconf))->u.s.memory
             ,stringisize_flags(subconf->flags)
             ,subconf->maxK
             ,subconf->size
             ,subconf->sectors
             );
    }
  }
}


