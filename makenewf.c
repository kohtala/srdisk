/* ReSizeable RAMDisk - fill new disk format using old format
 * Copyright (C) 1993-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

/*

Purpose of this module is to finnish up filling newf and determine the
changes specified to configuration.

*/

#include "srdisk.h"
#include "fat.h"
#include <assert.h>
#include "max.h"

/*
**  FILL AND ADJUST THE newf AND changed_format
**
**  Fills newf fields with the user specified data and checks some
**  errors in the new format definitions
**
**  Called only from main()
*/

/*  COUNT KBYTES FOR DRIVERS IN CHAIN (Called only from make_newf()) */

static void make_subconfs(void)
{
  int i;
  int changed = 0;

  for (i = MAX_CHAINED_DRIVERS - 1; i >= f.chain_len; i--)
    if (newf.subconf[i].userdef) {
      error("Too many /M values");
      return;
    }

  newf.current_size = 0;
  newf.max_size = 0;
  for (i = 0; i < MAX_CHAINED_DRIVERS; i++)
  {
    if (!newf.subconf[i].userdef)
      newf.subconf[i].maxK = f.subconf[i].maxK;
    else if (newf.subconf[i].maxK != f.subconf[i].maxK)
      changed++;
    newf.max_size += newf.subconf[i].maxK;
    newf.subconf[i].size = f.subconf[i].size;
    newf.current_size += newf.subconf[i].size;
  }
  if (changed) changed_format |= MAX_PART_SIZES;
}

static word dirSize(dword size, word bps)
{
  word res;
  if (size > 16*512)
    res = 512;
  else {
    int entries_per_sec;
    res = size / 16;
    entries_per_sec = bps / 32;
    res += entries_per_sec - res % entries_per_sec;
  }
  if (res < root_files)
    res = root_files;
  return res;
}

static word clusterSize(dword size)
{
  word res;
  if (data_on_disk) {
    res = f.cluster_size;
  }
  else if (defined_format & CLUSTER_SIZE) {
    res = newf.cluster_size;
  }
  else {
    dword sectors = size * 1024 / newf.bps;
    res = sectors <=  60000L ? 512  :
          sectors <= 120000L ? 1024 :
          sectors <= 240000L ? 2048 :
          sectors <= 480000L ? 4096 : 8192;
  }

  if (res < newf.bps)
    res = newf.bps;

  return res;
}

static void freeDiskMemory(void)
{
  /* Free all the memory allocated to the disk so that it can be
     better rebuilt */
  int i;

  for (i = 0; i < f.chain_len; i++) {
    dword lastalloc;
    lastalloc = disk_alloc(f.subconf[i].conf, 0);
    if (f.subconf[i].size != lastalloc)
      disk_bad = 1;
    f.current_size += lastalloc - f.subconf[i].size;
    f.subconf[i].size = lastalloc;
  }
  data_on_disk = 0;
  root_files = 1;
}

static word allocBlockSize(void)
{
  int i;
  word min = f.subconf[0].conf->allocblock;

  for (i = 1; i < f.chain_len; i++) {
    min = min(f.subconf[i].conf->allocblock, min);
  }
  return min;
}

static void fix_dir_sectors(void)
{
  word files_in_sec;
  files_in_sec = newf.bps / 32;
  newf.dir_sectors = (newf.dir_entries + files_in_sec - 1) / files_in_sec;
  if (!(defined_format & DIR_ENTRIES)) {
    /* !!!! Can this affect /OLD parameter? */
    newf.dir_entries = newf.dir_sectors * files_in_sec;
  }
}

int make_newf(void)
{
  enum type_e {tword, tdword};
  static struct change_s {
    int IDmask;
    enum type_e type;
    void *pNew, *pOld;
    dword def;
  } change[] = {
    { DISK_SIZE,        tdword, &newf.size,          &f.size,          0    },
    { SECTOR_SIZE,      tword,  &newf.bps,           &f.bps,           512  },
    { CLUSTER_SIZE,     tword,  &newf.cluster_size,  &f.cluster_size,  512  },
    { DIR_ENTRIES,      tword,  &newf.dir_entries,   &f.dir_entries,   256  },
    { MEDIA,            tword,  &newf.media,         &f.media,         0xFA },
    { SEC_PER_TRACK,    tword,  &newf.sec_per_track, &f.sec_per_track, 36   },
    { SIDES,            tword,  &newf.sides,         &f.sides,         16   },
    { DEVICE_TYPE,      tword,  &newf.device_type,   &f.device_type,   10   },
    { WRITE_PROTECTION, tword,  &newf.write_prot,    &f.write_prot,    OFF  },
    { REMOVABLE,        tword,  &newf.removable,     &f.removable,     ON   },
    { NO_OF_FATS,       tword,  &newf.FATs,          &f.FATs,          1    }
  };
  struct change_s *cp = change;
  dword clusters;

  newf.chain_len = f.chain_len;
  newf.reserved = 1;

  changed_format = 0;

  make_subconfs();    /* Sets changed_format bit MAX_PART_SIZES */

  /* Collect the default values for undefined fields in newf */
  change[0].def = f.size;
  for (cp = change; cp < &change[sizeof change / sizeof change[0]]; cp++) {
    if (!(defined_format & cp->IDmask)) {
      register dword val = use_old_format_f ? *(dword *)cp->pOld : cp->def;
      if (cp->type == tdword)
        *(dword *)cp->pNew = val;
      else
        *(word *)cp->pNew = val;
    }
  }

  /* Check that no more than 128 sectors per cluster */
  if (newf.cluster_size / newf.bps > 128)
  {
    word newbps = newf.bps;
    word newbpc = newf.cluster_size;
    do
    {
      unsigned clusterscore =
        (forced_format & CLUSTER_SIZE) ? 0 :
        (defined_format & CLUSTER_SIZE) ? 1 : 2;
      unsigned sectorscore =
        newbps >= 512 || (forced_format & SECTOR_SIZE) ? 0 :
        (defined_format & SECTOR_SIZE) ? 1 : 2;

      if (clusterscore + sectorscore == 0)
        return ERRL_BADFORMAT;

      /* Attempt to correct by changing sector size */
      if (sectorscore && sectorscore >= clusterscore)
      {
        newbps = newbpc / 128;
        if (newbps > 512)
          newbps = 512;
      }
      else if (clusterscore && clusterscore >= sectorscore)
      {
        newbpc /= 2;
      }
      else
        return ERRL_BADFORMAT;
    } while(newbpc / newbps > 128);

    if ((defined_format & SECTOR_SIZE) && newf.bps != newbps)
      warning("Too many sectors per cluster - using larger sectors");
    if ((defined_format & CLUSTER_SIZE) && newf.cluster_size != newbpc)
      warning("Too many sectors per cluster - using smaller clusters");
    newf.cluster_size = newbpc;
    newf.bps = newbps;
  }

  if (defined_format & CLEAR_DISK)
    freeDiskMemory();

  root_files = count_root();

  if (data_on_disk) {
    /* Adjust parameters so that the disk can be resized */
    if (newf.bps != f.bps || newf.cluster_size != f.cluster_size) {
      /* If data on disk, then we must not change the sector
         nor cluster size */
      if (forced_format & (SECTOR_SIZE|CLUSTER_SIZE)) {
        warning("Can not preserve contents when sector"
                " or cluster size change");
        if (!licence_to_kill())
          return ERRL_NO_LICENCE;
        /* data_on_disk == 0 and Resize() not used if execution get's here */
      }
      else {
        if (defined_format & (SECTOR_SIZE|CLUSTER_SIZE)) {
          warning("Disk contains data, can not change cluster nor sector size");
        }
        newf.bps = f.bps;
        newf.cluster_size = f.cluster_size;
      }
    }
    if (root_files > newf.dir_entries) {
      newf.dir_entries = root_files;
      if (defined_format & DIR_ENTRIES)
        warning("Root directory has %d entries, can not make it smaller",
                root_files);
    }
  }

  /* newf should now have somewhat good settings,
     which might get changed if seems appropriate */

  if (defined_format & (SPACE_AVAILABLE | FILE_SPACE)) {
    /* Here we calculate the disk parameters according to the number of
    ** clusters on the disk */

    /* Cases:
      - No data on disk
        - Cluster size and sector size changeable
      - Data on disk
        - File space -> count how many clusters we need
        - Available -> count how many clusters used and how much needed
        - Then count the new format for the cluster and sector size and
          number of clusters
    */

    dword spFAT;

    if (data_on_disk) {
      clusters = newf.avail * 1024 / newf.cluster_size;
      if (defined_format & SPACE_AVAILABLE) {
        FAT_open(&f);
        FAT_stats();
        FAT_close();
        clusters += fatstat.used;
      }

      /* --- Now we know how many clusters the disk must have --- */
      /* Do not change cluster size! */

      /* Make root directory smaller if no free space left on drive */
      if (newf.avail == 0) {
        if (!(forced_format & DIR_ENTRIES))
          newf.dir_entries = root_files;
      }
    }
    else {  /* No data_on_disk */
      newf.cluster_size = clusterSize(newf.avail);
      if (newf.avail == 0) {
        clusters = 0;
      }
      else {
        clusters = newf.avail * 1024 / newf.cluster_size;
      }
    }

    if (clusters == 0) {
      /* If no space for data on disk, better revert to make it disabled */
      newf.size = 0;
      goto by_memory;
    }

    while ((0xFEF <= clusters && clusters <= 0xFF7) || 0xFFEFUL <= clusters)
    {
      if (0xFFEFUL <= clusters) {
        if (defined_format & CLUSTER_SIZE || data_on_disk) {
          clusters = 0xFFEEUL;
          warning("Defined cluster size can not support large enough disk");
          break;
        }
        else {
          newf.cluster_size *= 2;
          if (clusters & 1)
            clusters++;
          clusters /= 2;
        }
      }
      else {
        clusters = 0xFF8;
        break;
      }
    }

    newf.FAT_type = clusters <= 0xFF7 ? 12 : 16;

    if (!(defined_format & DIR_ENTRIES)) {
      newf.dir_entries = dirSize(clusters * newf.cluster_size / 1024, newf.bps);
    }

    fix_dir_sectors();
    
    spFAT = (clusters + 2) * newf.FAT_type;         /* Bits in FAT */
    spFAT = (spFAT + 7) / 8;                        /* Bytes in FAT */
    newf.spFAT = (spFAT + newf.bps - 1) / newf.bps;
    newf.FAT_sectors = newf.spFAT * newf.FATs;
    newf.system_sectors = newf.reserved + newf.dir_sectors + newf.FAT_sectors;
    newf.spc = newf.cluster_size / newf.bps;
    newf.data_sectors = clusters * newf.spc;
    newf.sectors = newf.system_sectors + newf.data_sectors;
    assert(newf.bps <= 1024);
    newf.size = (newf.sectors + (1024 / newf.bps) - 1) / (1024 / newf.bps);

    if ( newf.sectors > ((conf->flags & C_32BITSEC) ? 0x7FFFFFL : 0xFFFEL) )
      return ERRL_NOMEM;
  }
  else {
    int tried_FAT_to_16bit;
    dword clusters_at_16bit_FAT;   /* Clusters when tried the 16bit FAT */
    extern void calcMaxMemory(void);

    if (defined_format & FREE_MEMORY) {
      dword suggested;
      if (!data_on_disk)
        freeDiskMemory();
      calcMaxMemory();
      suggested = f.max_mem < newf.avail ? 0 : f.max_mem - newf.avail;
      newf.size = min(suggested, f.max_safe_mem);
      newf.size -= newf.size % allocBlockSize();
    }
    else if (!(defined_format & DISK_SIZE))
      newf.size = f.size;

   by_memory:
    /* Define format by memory available for disk */

    tried_FAT_to_16bit = 0;
    clusters_at_16bit_FAT = 0;
    newf.FAT_type = 12;        /* By default try to use 12 bit FAT */

    /* Make sure sectors are big enough for the disk and count sectors */
    while((newf.sectors = newf.size * 1024 / newf.bps) >
        ((conf->flags & C_32BITSEC) ? 0x7FFFFFL : 0xFFFEL) )
    {
      if (data_on_disk || (forced_format & SECTOR_SIZE))
        return ERRL_BADFORMAT;
      else
        newf.bps *= 2;
    }

    if (newf.cluster_size < newf.bps) {
      if (data_on_disk || (forced_format & CLUSTER_SIZE))
        return ERRL_BADFORMAT;
      else
        newf.cluster_size = newf.bps;
    }

    if (!(defined_format & DIR_ENTRIES)) {
      newf.dir_entries = dirSize(newf.size, newf.bps);
    }

    fix_dir_sectors();

   count_clusters:
    newf.system_sectors = newf.reserved + newf.dir_sectors;
    newf.data_sectors = newf.sectors < newf.system_sectors ?
                        0 : newf.sectors - newf.system_sectors;

    newf.spc = newf.cluster_size / newf.bps;

    { ldiv_t divr;
      long spFAT;
      divr = ldiv(((long)newf.data_sectors + 2 * newf.spc) * newf.FAT_type,
                  (long)8 * newf.cluster_size + newf.FATs * newf.FAT_type);
      spFAT = divr.quot + (divr.rem ? 1 : 0);
      if (spFAT > 0xFFFF) {
        if (!data_on_disk) {
          if (newf.bps < 512) {
            newf.bps *= 2;
            if (newf.cluster_size < newf.bps)
              newf.cluster_size = newf.bps;
          }
          else
            newf.cluster_size *= 2;
          goto count_clusters;
        }
        return ERRL_BADFORMAT;
      }
      newf.spFAT = (word)spFAT;
    }

    newf.FAT_sectors = newf.spFAT * newf.FATs;
    newf.system_sectors += newf.FAT_sectors;
    newf.data_sectors = newf.data_sectors < newf.FAT_sectors ?
                        0 : newf.data_sectors - newf.FAT_sectors;
    clusters = newf.data_sectors / newf.spc;

    /* Make sure we use the right FAT type */
    /* Disks with 0xFF7 or fewer clusters have 12 bit FATs */
    /* Disks with 0xFEF to 0xFF7 clusters do not work */
    if (newf.FAT_type < 16 && clusters > 0xFEE) {
      if (tried_FAT_to_16bit && clusters_at_16bit_FAT <= 0xFF7) {
        /* Since over 0xFEE clusters does not work not does 16 bit FAT,
           then just drop some clusters. */
        clusters = 0xFEE;
      }
      else {
        newf.FAT_type = 16;
        goto count_clusters;
      }
    }
    if (newf.FAT_type > 12 && (clusters <= 0xFF7 || clusters > 0xFFEEL)) {
      if (!data_on_disk) {
        newf.FAT_type = 12;
        newf.cluster_size *= 2;
        goto count_clusters;
      }
      else if (!tried_FAT_to_16bit) {
        newf.FAT_type = 12;
        tried_FAT_to_16bit = 1;
        clusters_at_16bit_FAT = clusters;
        goto count_clusters;
      }
      else {
        /* Second time trying for 16bit fat, so must just drop some extra */
        if (clusters > 0xFFEEL)
          clusters = 0xFFEEL;
      }
    }
  }

  if (newf.bps > 512 && verbose > 1)
    warning("Sector size larger than 512 bytes, may crash DOS");

  /* !!!! Check to see that everything is set that should be set */

  assert(newf.FAT_type == 12 || newf.FAT_type == 16);
/*  assert(newf.spFAT); */
/*  assert(newf.sectors == newf.size * (1024 / newf.bps)); */
  assert(newf.FAT_sectors == newf.spFAT * newf.FATs);
  assert(newf.spc * newf.bps == newf.cluster_size);

  /*--- Now the format has been decided upon ---*/
  newf.clusters = clusters;

  { word val = newf.bps;
    newf.bps_shift = 0;
    while(val >>= 1)
      newf.bps_shift++;
  }

  newf.dir_start = newf.reserved + newf.FAT_sectors;
  newf.data_sectors = (dword)newf.clusters * newf.spc;
  newf.used_sectors = newf.sectors == 0 ?
                      0 : newf.system_sectors + newf.data_sectors;

  { ldiv_t used;
    long too_large;
    assert(newf.bps <= 1024);
    used = ldiv(newf.used_sectors, 1024 / newf.bps);
    too_large = newf.size - used.quot - (used.rem ? 1 : 0);
    if (too_large < 0)
      return ERRL_BADFORMAT;
    else if (0 < too_large) {
      newf.size -= too_large;
      newf.sectors = newf.size * 1024 / newf.bps;
      if (verbose > 2)
        warning("Can not take use of the last %ld Kbyte(s); size adjusted",
                too_large);
    }
  }

  /* Check out what changed after all */
  for (cp = change; cp < &change[sizeof change / sizeof change[0]]; cp++) {
    if (cp->type == tdword ? *(dword *)cp->pOld != *(dword *)cp->pNew
                           : *(word *)cp->pOld != *(word *)cp->pNew)
    {
      changed_format |= cp->IDmask;
    }
  }

  calc_alloc();    /* Calculate newf.subconf[].size */
  if (newf.current_size < newf.size)
    return ERRL_NOMEM;

  /* If Disk will be disabled */
  if (!newf.size) {
    newf.used_sectors = 0;
    return 0;
  }

  if (newf.sectors < newf.used_sectors || !newf.clusters)
    return ERRL_BADFORMAT;

  return 0;
}

