/* ReSizeable RAMDisk - disk image creation and preparing
 * Copyright (c) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "srdisk.h"
#include <string.h>
#include <dos.h>

/* This code is located at offset 0x3E in the boot sector */
const byte bootcode[] = {
    0x33,0xC0,
    0x8E,0xD8,0xBE,0x64,0x7C,0xFC,0xAC,0xBB,
    0x07,0x00,0xB4,0x0E,0xCD,0x10,0xAC,0x0A,
    0xC0,0x75,0xF4,0x33,0xC0,0xCD,0x16,0xB8,
    0x0D,0x0E,0xCD,0x10,0xB0,0x0A,0xCD,0x10,
    0xCD,0x19,0xEB,0xFE,0x0D,0x0A,0x4E,0x6F,
    0x6E,0x2D,0x53,0x79,0x73,0x74,0x65,0x6D,
    0x20,0x64,0x69,0x73,0x6B,0x2C,0x20,0x72,
    0x65,0x6D,0x6F,0x76,0x65,0x20,0x6F,0x72,
    0x20,0x72,0x65,0x70,0x6C,0x61,0x63,0x65,
    0x20,0x61,0x6E,0x64,0x20,0x70,0x72,0x65,
    0x73,0x73,0x20,0x6B,0x65,0x79,0x00
};

byte makeRWaccess(void)
{
  return READ_ACCESS | (newf.write_prot == ON ? 0 : WRITE_ACCESS)
                     | (newf.removable == OFF ? NONREMOVABLE : 0);
}

static void makebootsector(byte *sector)
{
  struct boot_s *b;
  b = (struct boot_s*)sector;
  memset(sector, 0, newf.bps);
  b->jump = 0x3CEB;                     /* Boot record JMP instruction */
  b->nop = 0x90;                        /* NOP instruction */
  memcpy(&b->oem, "SRD "VERSION, 8);    /* OEM code and version */
  FillBootSectorBPB(b);                 /* Fill the BPB */
  b->physical = 0;                      /* Physical drive number */
  b->signature = 0x29;                  /* Signature byte */
  b->serial = time(NULL);               /* Serial number */
  _fmemcpy(&b->label,
           ((struct dev_hdr far *)MK_FP(FP_SEG(conf), 0))->u.volume,
           11);                         /* Volume label */
  memcpy(&b->filesystem, newf.FAT_type == 12 ? "FAT12   " :
                         newf.FAT_type == 16 ? "FAT16   " : "        "
                         ,8);
  if (newf.bps < 0x3E + sizeof(bootcode))
    b->bootcode = 0xFEEB;           /* Boot code (JMP $) */
  else
    memcpy(&b->bootcode, bootcode, sizeof(bootcode));
  *(word  *)(sector+newf.bps-2) = 0xAA55;      /* Validity code */
  write_sector(1, 0, sector);           /* Write boot sector */
}

void makeNewDisk(void)
{
  int Fsec;
  int i;
  byte *sector;

  DiskAllocate();
  disk_bad = 1;
  if (disk_repair > dr_old)
    disk_repair = dr_old;
  configure_drive();

  sector = (byte *)xalloc(newf.bps);

  /* Write the new disk */

  /* Make the boot sector */
  if (bootsectorfile && FileBootSector(bootsectorfile, sector) == fbs_ok) {
    FillBootSectorBPB((struct boot_s*)sector);    /* Fill the BPB */
    write_sector(1, 0, sector);           /* Write boot sector */
  }
  else
    makebootsector(sector);

  for (i = 0; i < newf.FATs; i++) {
    word sector_n =
        newf.reserved + newf.spFAT * i;
    /* Write 1st FAT sector */
    memset(sector, 0, newf.bps);  /* Make 1st FAT sector */
    ((word *)sector)[0] = newf.media | 0xFF00;
    ((word *)sector)[1] = newf.FAT_type == 12 ? 0xFF : 0xFFFF;
    write_sector(1, sector_n++, sector);

    /* Write FAT sectors from 2nd to last */
    *(dword *)sector = 0L;
    for (Fsec = 1; Fsec < newf.spFAT; Fsec++)
        write_sector(1, sector_n++, sector);
  }

  /* Write 1st directory sector */
  newf.dir_start = newf.reserved + newf.FAT_sectors;
  _fmemcpy(sector, ((struct dev_hdr far *)MK_FP(FP_SEG(conf), 0))->u.volume, 11);
  sector[11] = FA_LABEL;
  *(dword *)(sector+22) = DOS_time(time(NULL));
  write_sector(1, newf.dir_start, sector);

  /* Write directory sectors from 2nd to last */
  memset(sector, 0, 32);
  for (Fsec = 1; Fsec < newf.dir_sectors; Fsec++)
      write_sector(1, newf.dir_start+Fsec, sector);

  conf->RW_access = makeRWaccess();

  free(sector);

  disk_bad = 0;
}

