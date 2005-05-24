/* ReSizeable RAMDisk - disk formatting for SRDISK.EXE
 * Copyright (c) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "srdisk.h"
#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <direct.h>

/*
**  Format printing
*/

char *stringisize_flags(int flags)
{
  static char _string[60];
  _string[0] = 0;
  if (!flags) {
    return " NONE";
  } else {
    if (flags & C_APPENDED) strcat(_string, " APPENDED");
    if (flags & C_MULTIPLE) strcat(_string, " MULTIPLE");
    if (flags & C_32BITSEC) strcat(_string, " 32BITSEC");
    if (flags & C_NOALLOC) strcat(_string, " NOALLOC");
    if (flags & C_GIOCTL) strcat(_string, " GIOCTL");
    if (flags & C_UNKNOWN) strcat(_string, " unknown");
  }
  return _string;
}

void print_format(struct format_s *f)
{
  printf("Drive %c:\n"
         "  Disk size: %luK\n"
         "  Cluster size: %u bytes\n"
         "  Sector size: %d bytes\n"
         "  Directory entries: %d\n"
         "  FAT copies: %d\n"
         "  Bytes available: %ld\n"
         "  Write protection: %s\n"
         "  Removable: %s\n"
         ,drive
         ,f->size
         ,f->cluster_size
         ,f->bps
         ,f->dir_entries
         ,f->FATs
         ,(dword)f->clusters*f->cluster_size
         ,(f->write_prot == ON ? "ON" : "OFF")
         ,(f->removable == ON ? "ON" : "OFF")
         );
  if (verbose > 3)
    printf("  Sectors: %lu\n"
           "  Reserved sectors: %d\n"
           "  FAT sectors: %d\n"
           "  Directory sectors: %d\n"
           "  Sectors per cluster: %d\n"
           "  Clusters: %u\n"
           "  FAT type: %u bit\n"
           "  Max size: %luK\n"
           "  Media: %02X\n"
           "  Device type: %d\n"
           "  Sectors per track: %d\n"
           "  Sides: %d\n"
           ,f->sectors
           ,f->reserved
           ,f->FAT_sectors
           ,f->dir_sectors
           ,f->spc
           ,f->clusters
           ,f->FAT_type
           ,f->max_size
           ,f->media
           ,f->device_type
           ,f->sec_per_track
           ,f->sides
           );
}

void print_newf(void)
{
  if (verbose > 1) {
    printf("New disk configuration:\n\n");
    print_format(&newf);
    puts("");
  }
}


/*
**  Write new format to the disk
**
**  May not return in error
*/

void WriteNewFormat(void)
{
  extern void makeNewDisk(void);

  if (!licence_to_kill()) {
    return_val = ERRL_NO_LICENCE;
    return;
  }

  print_newf();

  makeNewDisk();

  /* This is to get around some DOS 5 bug when the sector size is made
     larger than before: DOS 5 calculates the size wrong. Thus we access
     the disk through DOS and set media change flag again.
  */

  if (_osmajor == 5 && newf.bps > f.bps) {
    asm {
        mov ax,0x4409
        mov bl,byte ptr drive
        sub bl,'A'-1
        jc no_fix       /* Bad drive letter */
        int 0x21
        jc no_fix       /* Failed access drive */
        test dh,0x80
        jnz no_fix      /* SUBSTed drive */
    }
    asm {
        mov dl,drive
        sub dl,'A'-1
        mov ah,0x1C     /* Get Drive Data */
        push ds
        int 0x21
        pop ds
    }
    conf->media_change = -1;
   no_fix:;
  }

  if (verbose > 1)
    printf("Disk formatted\n");
}

/*
**  Disable the disk.
**
**  Expected not to return in error.
*/

void disable_disk(void)
{
  data_on_disk = 0;
  disk_repair = dr_disable;
  if (newf.size) {
    newf.size = 0;
    defined_format = DISK_SIZE;
    make_newf();
  }
  DiskAllocate();
  configure_drive();
  disk_bad = 0;   /* Disabled disk is not bad */
  if (verbose > 1)
    printf("RAMDisk disabled\n");
}


/*
**  Reconfigure disk without reformatting it
**  (change nonessential parameters)
*/

void ReConfig(void)
{
  /* We must look for the derived parameters used in BPB */
  newf.spc          = newf.cluster_size / newf.bps;
  newf.reserved     = f.reserved;
  newf.spFAT        = f.spFAT;
  newf.used_sectors = f.used_sectors;

  ConfigNonFormat();
}


/*
**  SET WRITE PROTECT
**
**  Called only from format()
*/

static void set_write_protect(void)
{
  if (newf.write_prot == YES) {
    conf->RW_access &= ~WRITE_ACCESS;
    if (verbose > 1)
      printf("Write protect enabled\n");
  }
  else {
    conf->RW_access |= WRITE_ACCESS;
    if (verbose > 1)
      printf("Write protect disabled\n");
  }
}

/*
**  SET Removable state
**
**  Called only from format()
*/

static void set_fixed(void)
{
  if (newf.removable == NO) {
    conf->RW_access |= NONREMOVABLE;
    if (verbose > 1)
      printf("Drive made nonremovable\n");
  }
  else {
    conf->RW_access &= ~NONREMOVABLE;
    if (verbose > 1)
      printf("Drive made removable\n");
  }
}

/*
**  Count the files in root directory
**
**  On nonexistent disk there is not files in the root.
**  May not return in error.
*/ 

int count_root(void)
{
  byte *sp;
  int si;
  dword sector;
  int entries;
  int files = 0;
  int data = 0;

  if (data_on_disk) {
    sector = f.dir_start;
    entries = f.dir_entries;

    sp = (byte *)xalloc(f.bps);

    while(entries) {
      read_sector(1, sector, sp);
      for(si = 0; si < f.bps && entries; si += 32) {
        if (sp[si] == 0)              /* Unused, end of directory */
          goto end;
        if (sp[si] != 0xE5) {         /* Not deleted */
          files++;                    /* so it is a file */
          if (!(sp[si+11] & 8))       /* If not label */
            data = 1;                 /* then there is data with it */
        }
        entries--;
      }
      sector++;
    }
   end:
    free(sp);
  }

  data_on_disk = data;
  return files;
}

/*
**  PERMISSION TO DELETE DATA
*/

int licence_to_kill(void)
{
  if (data_on_disk && !(defined_format & CLEAR_DISK)) {
    if (force_f == ASK) {
        force_banner();
        printf("\aAbout to destroy all files on drive %c!\n\a"
               "Continue (Y/N) ? ", drive);
        getYN();
    }
    if (force_f == NO) {
      if (verbose > 0)
        printf("Operation aborted\n");
      return 0;
    }
  }
  data_on_disk = 0;     /* Consider there is no longer data on disk */
  return 1;
}

/*
**  FORMAT OR REFORMAT THE DISK.
**
**  Expected not to return in error.
*/

void format_disk(void)
{
  if (force_f != YES && conf->open_files)
    fatal("Files open on drive");

  if (force_f != YES && f.removable == OFF
   && ((defined_format & REMOVABLE) == 0 || newf.removable == OFF))
  {
    changed_format = defined_format; /* Only for format_f */
    if (format_f || reconfig_f)
    {
      fatal("Changes requested for nonremovable drive");
    }
  }

  /* fill new format structure and check for changes */
  return_val = make_newf();
  if (return_val) {
    /* Could not make the disk as requested */
    if (return_val == ERRL_BADFORMAT)
      return_msg = "Aborted: Impossible format for disk";
    else if (return_val == ERRL_NOMEM)
      return_msg = "Aborted: Not enough memory for the disk";
    if (disk_bad) {
      warning("Impossible format for disk - restoring old format");
      memcpy(&newf, &f, sizeof f);
      WriteNewFormat();
    }
    return;
  }

  if (!disk_bad && !format_f && !reconfig_f) {
    if (!changed_format) {
      if (verbose > 0)
        warning("No change in format - disk remains untouched");
    }
    else {
      if (changed_format & WRITE_PROTECTION)
        set_write_protect();
      if (changed_format & REMOVABLE)
        set_fixed();
      if (changed_format & MAX_PART_SIZES) {
        if (f.size) {
          if (!SavingDiskAllocate(f.sectors))
            error("Failed to rearrange memory");
        }
        else
          ConfigMaxAlloc();
      }
    }
    return;
  }

  /* If Disk will be disabled */
  /* !!!! Move this before the code to adjust parameterf for resize */
  if (!newf.size) {
    if (!f.size) {
      /* If was disabled also before */
      configure_drive();
      if (verbose > 1)
        printf("New configuration saved for later use\n");
    } else {
      /* If disk now get's disabled */
      if (!licence_to_kill()) {
        return_val = ERRL_NO_LICENCE;
        return;
      }
      disable_disk();
    }
    return;
  }

  if (format_f || disk_bad) {
    if (!data_on_disk)
      WriteNewFormat();
    else
      Resize();
  }
  else if (reconfig_f) {
    ReConfig();
    if (f.size)
      RefreshBootSector();
    if (verbose > 1)
      printf("Drive %c: reconfigured\n", drive);
  }
}

