/* SRDISK - Disk memory reallocation
 * Copyright (C) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "srdisk.h"
#include <stdio.h>
#include <setjmp.h>

extern dword AllocMem(int i);
extern void ConfigMaxAlloc(void);
extern void set_sectors(void);

static jmp_buf jmp_allocfail;
static int alloc_fail_f = 0;
static int data_lost_f = 0;
static dword used_sectors;

static void allocfail(int i)
{
  f.subconf[i].alloc_best = 1;
  longjmp(jmp_allocfail, 1);
}

static void data_lost(int i)
{
  data_lost_f = 1;
  allocfail(i);
}

static void ExpandAlloc(int i, dword newsize, dword oldsize, dword first)
{
  struct config_s far *conf = f.subconf[i].conf;
  signed long count;

  conf->sectors = newsize;

  /* Fill new space at the end of the allocated block */
  count = used_sectors - first - oldsize;
  if (count > 0)
    MoveSectors(first + oldsize, first + newsize, count);
}

static void AllocPart(int i, dword first, dword first_orig)

/*  i         Number of current part of disk
**  first     Sector number by which can access the first sector on this
**            part
**  first_orig Sector number the first sector on this part had before
*/

{
  struct config_s far *conf;
  int bps_shift;
  dword oldsize;
  dword newsize;
  dword lastalloc;
  dword targetsize;

  if (i >= f.chain_len)
    return;

  conf = f.subconf[i].conf;
  bps_shift = conf->bps_shift;
  oldsize = f.subconf[i].size << (10 - bps_shift);
  targetsize = newf.subconf[i].size;
  newsize = newf.subconf[i].size << (10 - bps_shift);

  if (newsize < oldsize) {
    /* We are shrinking this part */

    /* Make room where to move the excess data from this part */
    AllocPart(i + 1, first + oldsize, first_orig + oldsize);

    /* Move excess data from top to the next block */
    {
      signed long count = used_sectors - first - newsize;
      if (count > 0)
        MoveSectors(first + oldsize, first + newsize, count);
    }

    lastalloc = AllocMem(i);

    /* Test for success or failure in allocation */
    if (lastalloc != targetsize) {
      dword size = newsize;
      alloc_fail_f = 1;
      newsize = lastalloc << (10 - bps_shift);
      if (newsize > size) {
        ExpandAlloc(i, newsize, size, first);
        longjmp(jmp_allocfail, 1);
      }
      else {
        conf->sectors = newsize;
        data_lost(i);
      }
      /* NOTREACHED */
    }
    conf->sectors = newsize;
  }
  else if (newsize > oldsize) {
    /* We are expanding this part */

    lastalloc = AllocMem(i);

    /* Test for success or failure in allocation */
    if (lastalloc != targetsize) {
      alloc_fail_f = 1;
      newsize = lastalloc << (10 - bps_shift);
      if (newsize < oldsize)
        data_lost(i);
    }

    ExpandAlloc(i, newsize, oldsize, first);
    if (alloc_fail_f)
      allocfail(i);

    AllocPart(i + 1, first + newsize, first_orig + oldsize);
  }
  else {
    /* No change in size */

    AllocPart(i + 1, first + newsize, first_orig + oldsize);
  }
}

int SavingDiskAllocate(dword save_sectors)

/* Return: zero if memory could not be allocated.
   Error: Data lost -> fatal
*/

{
  static int old_disk_bad;
  static int ok;
  static int tries;

  old_disk_bad = disk_bad;
  ok = 1;
  tries = 0;

  ConfigMaxAlloc();

  used_sectors = save_sectors;

  if (setjmp(jmp_allocfail)) {
    if (data_lost_f) {
      fatal("Data lost due to allocation error");
    }
    tries++;
  }
  if (tries < 3) {
    calc_alloc();
    if (newf.current_size < newf.size)
      ok = 0;
    else {
      alloc_fail_f = 0;
      data_lost_f = 0;
      AllocPart(0, 0L, 0L);
      disk_bad = old_disk_bad;
    }
    set_sectors();
  }
  else {
    /* Too many tries and no success... */
    ok = 0;
  }
  return ok;
}


