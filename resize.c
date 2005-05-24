/* SRDISK - Disk image resizing
 * Copyright (C) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "fat.h"
#include <stdio.h>
#include <string.h>
#include <dir.h>
#include <direct.h>
#include <assert.h>
#include <alloc.h>
#include "max.h"

static byte far *sector_buffer;
static word buffer_size;

static void AllocSectorBuffer(void)
{
  word bps = conf->BPB_bps;

  if (!buffer_size) {
    if (farcoreleft() > 0xF000) {
      sector_buffer = (byte far *)farmalloc(0xF000);
      if (sector_buffer)
        buffer_size = 0xF000 / bps;
      else {
        sector_buffer = (byte far *)farmalloc(0x7800);
        if (sector_buffer)
          buffer_size = 0x7800 / bps;
      }
    }
    if (!buffer_size) {
      sector_buffer = (byte *)xalloc(bps);
      buffer_size = 1;
    }
  }
}

void MoveSectors(dword dest, dword orig, dword number)
{
  AllocSectorBuffer();

  if (orig > dest) {
    word batch = (word)min(orig - dest, (dword)buffer_size);
    while(number) {
      word n = (word)min((dword)batch, number);
      read_sector(n, orig, sector_buffer);
      write_sector(n, dest, sector_buffer);
      orig += n;
      dest += n;
      number -= n;
    }
  } else if (orig < dest) {
    word batch = (word)min(dest - orig, (dword)buffer_size);
    orig += number;
    dest += number;
    while(number) {
      word n = (word)min((dword)batch, number);
      read_sector(n, orig -= n, sector_buffer);
      write_sector(n, dest -= n, sector_buffer);
      number -= n;
    }
  }
}

void ClearSectors(dword dest, dword number)
{
  word batch;

  AllocSectorBuffer();

  batch = (word)min((dword)buffer_size, number);
  _fmemset(sector_buffer, 0, batch * conf->BPB_bps);

  while(number) {
    word n = (word)min((dword)batch, number);
    write_sector(n, dest, sector_buffer);
    dest += n;
    number -= n;
  }
}

static void FixFAT(void)
{
  word cluster;         /* Current cluster number */
  word next;
  byte *sector = NULL;  /* New FAT sector under construction */
  int secno;            /* New FAT sector number under construction */
  int soff;             /* Offset in sector for current cluster */
  int usedSectors;      /* Number of sectors used in FAT */

  assert(newf.reserved == f.reserved);

  save_FAT_buffer();    /* Make sure no buffer flush will damage FAT */

  usedSectors = f.spFAT;

  if (newf.FAT_type != f.FAT_type) {
    sector = (byte *)xalloc(newf.bps);

    if (newf.FAT_type == 16) {
      cluster = f.clusters + 1;
      { ldiv_t s = ldiv(cluster * 2, fat->bps);
        secno = (int)s.quot;
        soff = (int)s.rem;
      }
      usedSectors = secno + 1;
      memset(sector, 0, newf.bps);
      /* Go through all clusters constructing new FAT sectors into 'sector' */
      for ( ; cluster < 0xFFF0U; cluster--) {
        next = next_cluster(cluster);
        *(word*)(sector + soff) = next;
        if (soff == 0) {
          if (secno == 0)
            *sector = newf.media;
          write_sector(1, newf.reserved + secno, sector);
          secno--;
          soff = newf.bps;
        }
        soff -= 2;
      }
      assert(soff == newf.bps - 2);
    }
    else {
      assert(newf.FAT_type == 12);

      secno = 0;
      soff = 3;

      sector[0] = newf.media;
      sector[1] = sector[2] = 0xFF;

      for (cluster = 2; cluster < f.clusters + 2; cluster += 2) {
        union {
          byte b[3];
          struct {
            word low;
            byte high;
          } w;
        } n;
        word next2;
        next = next_cluster(cluster);
        next2 = cluster + 1 < f.clusters + 2 ? next_cluster(cluster+1) : 0;

        n.w.low = (next & 0xFFF) | (next2 << 12);
        n.w.high = next2 >> 4;

        for (next2 = 0; next2 < 3; next2++) {
          *(sector + soff) = n.b[next2];
          if (++soff >= newf.bps) {
            write_sector(1, newf.reserved + secno, sector);
            secno++;
            soff = 0;
          }
        }
      }
      usedSectors = secno;
      if (soff) {
        usedSectors++;
        memset(sector + soff, 0, newf.bps - soff);
        write_sector(1, newf.reserved + secno, sector);
      }
    }
  }
  else {
    set_next_cluster(0, 0xFF00 | newf.media);
    save_FAT_buffer();    /* Make sure FAT is right */
  }

  /* Clear extra sectors in the FAT */
  if (newf.spFAT > usedSectors) {
    if (!sector)
      sector = (byte *)xalloc(newf.bps);
    memset(sector, 0, newf.bps);
    for (secno = usedSectors; secno < newf.spFAT; secno++)
      write_sector(1, newf.reserved + secno, sector);
  }

  /* Make duplicates of the new FAT */
  if (newf.FATs > 1) {
    if (!sector)
      sector = (byte *)xalloc(newf.bps);
    for (secno = newf.spFAT - 1; secno >= 0; secno--) {
      dword s = newf.reserved + secno;
      read_sector(1, s, sector);
      for (next = newf.FATs; next > 1; next--) {
        s += newf.spFAT;
        write_sector(1, s, sector);
      }
    }
  }
  if (sector)
    free(sector);
}


static void RelocateClusters(void)
{
  MoveSectors(newf.system_sectors, f.system_sectors,
              (dword)(fatstat.last_used - 1) * newf.spc);
}

static void RelocateRootDir(void)
{
  MoveSectors(newf.dir_start, f.dir_start,
              newf.dir_sectors);
}

/* Resize() - if resize without destroying disk contents is impossible
then call WriteNewFormat(). If contents destroyed already, make
it fatal error. */

void Resize(void)
{
  char current_dir[MAXPATH];    /* Current working directory */
  dword used_sectors;           /* Used sectors from the beginning to save */
  extern byte makeRWaccess(void);

  /* !!!! Todo: If possible, move data only from the end to the start or
     from the start to the end leaving the middle part in place. */

  if (!_getdcwd(drive-'A'+1, current_dir, sizeof current_dir)) {
    fatal("Can not determine current directory on drive %c", drive);
  }

  if (!FAT_open(&f))
    fatal("Can not open fat");

  FAT_stats();

  if (fatstat.bad) {
    error("FAT has bad units in it");
  }

  if (!fatstat.used && !data_on_disk) {
    WriteNewFormat();
    return;
  }

  assert(newf.bps == f.bps && newf.cluster_size == f.cluster_size);
  assert(root_files <= newf.dir_entries);

  if (fatstat.used > newf.clusters) {
    warning("All data can not fit the new disk");
    WriteNewFormat();
    return;
  }

  print_newf();

  puts("Resizing in progress...");

  conf->RW_access = 0;  /* Disable DOS access to drive */

  /* Pack data if disk made smaller, root dir made smaller or
  ** much empty space in used data area. */
  if (newf.clusters < f.clusters
  ||  newf.dir_entries < f.dir_entries
  ||  fatstat.used < fatstat.last_used / 2) {
    packdata();
    save_FAT_buffer();
  }

  used_sectors = cluster2sector(fatstat.last_used + 1);

  if (newf.size > f.size && !SavingDiskAllocate(used_sectors)) {
    /* Try to make the disk as it was */
    disk_bad = 1;
    if (f.current_size > f.size) {
      newf.size = f.size;
      if (SavingDiskAllocate(used_sectors) && f.current_size >= f.size)
        disk_bad = 0;
    }
    fatal("Failed to allocate memory");
  }

  disk_bad = 1;

  if (newf.system_sectors > f.system_sectors)
    RelocateClusters();

  if (newf.dir_start > f.dir_start)
    RelocateRootDir();

  FixFAT();
  FAT_close();

  if (newf.dir_start < f.dir_start)
    RelocateRootDir();

  if (newf.system_sectors < f.system_sectors)
    RelocateClusters();

  if (newf.size < f.size
  && !SavingDiskAllocate(((dword)(fatstat.last_used - 1) * newf.spc) +
                         newf.system_sectors))
  {
    /* !!!! possible to do better */
    fatal("Failed to allocate memory");
  }

  if (newf.dir_sectors > f.dir_sectors)
    ClearSectors(newf.dir_start + f.dir_sectors,
                 newf.dir_sectors - f.dir_sectors);

  configure_drive();
  /* Update the boot sector */
  RefreshBootSector();

  conf->RW_access = makeRWaccess();

  disk_bad = 0;

  if (chdir(current_dir) != 0) {
    fatal("Could not find current directory");
  }

  if (verbose > 1)
    printf("Disk resized\n");

  return;
}

