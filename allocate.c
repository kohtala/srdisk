/* SRDISK - Disk memory allocation
 * Copyright (C) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "srdisk.h"
#include <stdio.h>
#include "max.h"

/******************** allocation calculations ************************/

#ifdef __cplusplus
#define LINKAGE inline
#else
#define LINKAGE static
#endif

LINKAGE dword adjust(dword a, word u)
{
  word mod = a % u;
  return mod ? u - mod + a : a;
}

#if 0
LINKAGE dword maxa(dword a, dword b, word u)
{
  return adjust(max(a,b), u);
}
#endif

LINKAGE dword mina(dword a, dword b, word u)
{
  return adjust(min(a,b), u);
}

void calc_alloc(void)
{
  int i;
  long to_alloc;
  int method = 0; /* Method to collect memory */

  do {
    /* Adjust drivers allocation not to exceed the maxK without making any
       drivers memory larger. This is assumed safe, since we expect memory
       can always be freed without fear of failure. */

    to_alloc = newf.size;
    for (i = 0; i < f.chain_len; i++) {
      newf.subconf[i].size =
        mina(min(f.subconf[i].size, newf.subconf[i].maxK),
             to_alloc,
             f.subconf[i].conf->allocblock);
      to_alloc -= newf.subconf[i].size;
    }

    /* If memory needs are not filled by now, then see where space can be
       safely allocated more */

    if (to_alloc > 0) {
      for (i = 0; i < f.chain_len && to_alloc > 0; i++) {
        if (!f.subconf[i].alloc_best) {
          /* This is to avoid double calls due to min() definition */

          dword free = method == 0 ? safe_size(f.subconf[i].conf)
                                   : max_size(f.subconf[i].conf);
          free = min(free, newf.subconf[i].maxK);

          if (newf.subconf[i].size < newf.subconf[i].maxK
          &&  newf.subconf[i].size < free)
          {
            dword alloc = mina(to_alloc, free - f.subconf[i].size,
                               f.subconf[i].conf->allocblock);
            newf.subconf[i].size += alloc;
            to_alloc -= alloc;
          }
        }
      }
    }
    method++;
  } while(to_alloc > 0 && method < 2);

  if (to_alloc < 0) {
    /* If too much memory allocated, see if some part has small enough
       allocation unit to be freed to allocate only so much memory as is
       needed. */

    int shrunk;

    do {
      shrunk = 0;
      for (i = 0; i < f.chain_len; i++) {
        if (f.subconf[i].conf->allocblock < -to_alloc) {
          dword shrink = -to_alloc / f.subconf[i].conf->allocblock;
          newf.subconf[i].size -= shrink;
          to_alloc += shrink;
          shrunk = 1;
        }
      }
    } while(shrunk);
  }

  newf.current_size = newf.size - to_alloc;
}

/****************** The allocation itself *********************/

extern void ConfigMaxAlloc(void);

dword AllocMem(int i)
{
  dword lastalloc;

  lastalloc = disk_alloc(f.subconf[i].conf, newf.subconf[i].size);
  if (f.subconf[i].size != lastalloc)
    disk_bad = 1;
  f.current_size += lastalloc - f.subconf[i].size;
  f.subconf[i].size = newf.subconf[i].size = lastalloc;
  return lastalloc;
}

void set_sectors(void)
{
  struct config_s far *subconf;
  for(subconf = conf; subconf; subconf = conf_ptr(subconf->next)) {
    subconf->sectors = subconf->size * 1024 / f.bps;
  }
}

void DiskAllocate(void)

/* Error: Can not allocate enough -> fatal
*/

{
  long lastalloc;
  long Kleft = 0;
  int i;
  int alloc_tries;
  enum disk_repair_e old_disk_repair = disk_repair;

  ConfigMaxAlloc();

  if (disk_repair > dr_clear)
    disk_repair = dr_clear; /* In case of error, try to make a clear disk */

  for(alloc_tries = f.chain_len; alloc_tries; alloc_tries--) {
    calc_alloc();    /* Calculate newf.subconf[].size */
    if (newf.current_size < newf.size)
      fatal("Not enough memory available");
    data_on_disk = 0;
    Kleft = newf.size;
    for(i = 0; i < f.chain_len; i++) {
      dword origsize = f.subconf[i].size;
      dword destsize = newf.subconf[i].size;
      if (origsize != destsize) {
        lastalloc = AllocMem(i);
        if (lastalloc != origsize) {
          disk_bad = 1;
        }
        if (lastalloc != destsize) {
          f.subconf[i].alloc_best = 1;
          goto new_try;
        }

        Kleft -= lastalloc;
      }
      else {
        Kleft -= f.subconf[i].size;
      }
    }
    break;
   new_try:;
  }

  if (Kleft > 0) {    /* If not enough memory could be allocated */
    fatal("Failed to allocate memory");
  }
  else if (Kleft < 0 && verbose > 2)
    printf("%ld Kbytes extra allocated, "
           "perhaps you should make your disk that much larger.\n"
           ,-Kleft);

  set_sectors();
  disk_repair = old_disk_repair;
}

