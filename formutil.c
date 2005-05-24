/* ReSizeable RAMDisk - disk formatting utilities
 * Copyright (c) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */


#include "srdisk.h"
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

/* Calculate the available memory size into 'f' */

void calcMaxMemory(void)
{
  struct config_s far *subconf = conf;
  f.max_safe_mem = 0;
  f.max_mem = 0;
  do {
    f.max_safe_mem += safe_size(subconf);
    f.max_mem += max_size(subconf);
    subconf = conf_ptr(subconf->next);
  } while(subconf);
  if (f.max_safe_mem < f.current_size)
    f.max_safe_mem = f.current_size;
}

/*
**  CONFIGURE DRIVE FOR FORMAT newf
**
**  Disable drive and configure it. RW_access must be set by caller.
*/

/* Configure the new max allocation sizes */
void ConfigMaxAlloc(void)
{
  if (changed_format & MAX_PART_SIZES) {
    int i;

    for (i = 0; i < f.chain_len; i++)
      f.subconf[i].conf->maxK = newf.subconf[i].maxK;
    if (verbose > 1) {
      puts("Adjusted max allocation sizes");
    }
    changed_format &= ~MAX_PART_SIZES;  /* No reason to configure this twice */
  }
}

/* Rewrite boot sector for the new configuration */
void FillBootSectorBPB(struct boot_s *b)
{
  b->bps = newf.bps;
  b->spc = newf.spc;
  b->reserved = newf.reserved;
  b->FATs = newf.FATs;
  b->dir_entries = newf.dir_entries;
  b->media = newf.media;                /* Media */
  b->spFAT = newf.spFAT;
  b->spt = newf.sec_per_track;          /* Sectors per track */
  b->sides = newf.sides;                /* Sides */
  if (conf->flags & C_32BITSEC && newf.sectors > 0xFFFEL) {
    b->sectors = 0;
    b->sectors32 = newf.sectors;   /* Total number of sectors */
  }
  else {
    b->sectors = newf.sectors;
  }
}

enum filebootsector_status FileBootSector(char *file, byte *sector)
{
  /* Make the boot sector */
  int fd = open(file, O_RDONLY|O_BINARY);
  if (fd < 0) {
    warning("Can not open boot sector image file %s - using default",
            bootsectorfile);
    return fbs_open;
  }
  else if (read(fd, sector, newf.bps) != newf.bps) {
    warning("Can not read boot sector image - using default");
    return fbs_read;
  }
  else if (*(word *)(sector+newf.bps-2) != 0xAA55) {
    warning("Invalid boot sector image - using default");
    return fbs_valid;
  }
  else {
    FillBootSectorBPB((struct boot_s*)sector);    /* Fill the BPB */
  }
  return fbs_ok;
}

void RefreshBootSector(void)
{
  if (f.current_size && newf.current_size) {
    struct boot_s *b = (struct boot_s *)xalloc(newf.bps);
    if (!bootsectorfile ||
        FileBootSector(bootsectorfile, (byte*)b) != fbs_ok)
    {
      read_sector(1, 0, b);
    }
    FillBootSectorBPB(b);
    write_sector(1, 0, b);                /* Write boot sector */
  }
}

/* Configure for reconfig_f */
void ConfigNonFormat(void)
{
  /* Must set the config in case writing boot sector does not do it */
  conf->BPB_media = newf.media;
  conf->BPB_spt = newf.sec_per_track;
  conf->BPB_heads = newf.sides;
  conf->device_type = newf.device_type;
  conf->media_change = -1;
}

void configure_drive(void)
{
  struct config_s far *subconf;

#if 0
  extern void fill_some_derived();

  /* It is possible this gets called before the derived parameters are
  ** calculated: */
  fill_some_derived();
#endif

  conf->RW_access = 0;  /* Disable DOS access to drive */

  ConfigMaxAlloc();

  for(subconf = conf; subconf; subconf = conf_ptr(subconf->next)) {
    subconf->sectors = subconf->size * 1024 / newf.bps;
    subconf->BPB_bps = newf.bps;
    subconf->bps_shift = newf.bps_shift;
  }

  /* These often go to waist since the boot sector writes them over */
  conf->BPB_spc = newf.spc;
  conf->BPB_reserved = newf.reserved;
  conf->BPB_FATs = newf.FATs;
  conf->BPB_dir = newf.dir_entries;
  conf->BPB_sectors = ((conf->flags & C_32BITSEC) && newf.sectors > 0xFFFEL)
                      ? 0 : newf.sectors;
  conf->BPB_FATsectors = newf.spFAT;
  conf->BPB_hidden = 0L;
  conf->BPB_tsectors = newf.sectors;
  conf->tsize = newf.size;
  ConfigNonFormat();
}

