/* ReSizeable RAMDisk - srdisk header file
 * Copyright (C) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifndef _SRDISK_H
#define _SRDISK_H

/* Byte aligned compilation is a must */
#pragma option -a-
#pragma pack(1)

#define VERSION "2.09"

#define MAX_CHAINED_DRIVERS 5

/* ERRORLEVEL RETURN VALUES */
extern int return_val;
extern char* return_msg;
#define ERRL_NO_LICENCE 1
#define ERRL_FATAL 1
#define ERRL_ERRORS 1
#define ERRL_ABNORMAL 2
#define ERRL_SYNTAX 10
#define ERRL_BADFORMAT 11
#define ERRL_NOMEM 12

#include <stdlib.h>
#include <time.h>       /* Only for DOS_time declaration */

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;

#define MULTIPLEXAH 0x72
#define V_FORMAT 1      /* config_s structure format version used here */
#define IOCTL_ID 0x5253 /* IOCTL write ID to make a call ('SR') */

#define YES  1
#define ON   1
#define ASK  0
#define NO  -1
#define OFF -1

#define C_APPENDED      1 /* Capable of having appended drivers */
#define C_MULTIPLE      2 /* Capable of driving many disks */
#define C_32BITSEC      4 /* Capable of handling over 32-bit sector addresses */
#define C_NOALLOC       8 /* Incapable of allocating it's owm memory */
#define C_GIOCTL     0x10 /* Supports generic IOCTL_calls */
#define C_DISK_IMAGE 0x20 /* Can load disk image on startup */
#define C_UNKNOWN    0xC0

enum DeviceType_e {     /* Device types for Generic IOCTL */
    DT360 = 0,
    DT1200,
    DT720,
    DT8SD,
    DT8DD,
    DTHD,
    DTTD,
    DT1440,
    DTCD,
    DT2880,
    DTUNKNOWN
};

#define READ_ACCESS  1  /* Bit masks for RW_access in IOCTL_msg_s */
#define WRITE_ACCESS 2
#define NONREMOVABLE 4

/* Configuration structure internal to device driver (byte aligned) */
struct dev_hdr {
  struct dev_hdr far *next;
  word attr;
  word strategy;
  word commands;
  byte units;
  union {
    char volume[12];            /* Volume label combined of fields below */
    struct {
      char ID[3];               /* Identification string 'SRD' */
      char memory[4];           /* Memory type string */
      char version[4];          /* Device driver version string */
      char null;
    } s;
  } u;
  byte v_format;                /* Config_s format version */
  struct config_s near *conf;   /* Offset to config_s */
};

struct config_s {               /* The whole structure */
  byte drive;                   /* Drive letter of this driver */
  byte flags;                   /* Capability flags */
  word (far *disk_IO)(void);    /* Disk I/O routine entry */
  dword (near *malloc_off)(dword _s); /* Memory allocation routine entry offset */
  dword (near *freemem_off)(dword _s); /* Free memory query routine entry offset */
  struct dev_hdr _seg *next;    /* Next chained driver */
  dword maxK;                   /* Maximum memory allowed for disk */
  dword size;                   /* Current size in Kbytes */
  word allocblock;              /* Allocation block size */
  dword sectors;                /* Total sectors in this part of the disk */
  byte bps_shift;               /* 2^bps_shift == c_BPB_bps */

  word BPB_bps;                 /* BPB - bytes per sector */
  /* The rest is removed from chained drivers, used only in the main driver */
  byte BPB_spc;                 /* BPB - sectors per cluster */
  word BPB_reserved;            /* BPB - reserved sectors in the beginning */
  byte BPB_FATs;                /* BPB - number of FATs on disk */
  word BPB_dir;                 /* BPB - root directory entries */
  word BPB_sectors;             /* BPB - sectors on disk (for DOS 16b only) */
  byte BPB_media;               /* BPB - identifies the media (default 0xFA) */
  word BPB_FATsectors;          /* BPB - sectors per FAT */
  word BPB_spt;                 /* BPB - sectors per track (imaginary) */
  word BPB_heads;               /* BPB - heads (imaginary) */
  dword BPB_hidden;             /* BPB - hidden sectors (Note 1*) */
  dword BPB_tsectors;           /* BPB - sectors on disk */

  dword tsize;                  /* Total size for the disk */

  byte RW_access;               /* b0 = enable, b1 = write */
  signed char media_change;     /* -1 if media changed, 1 if not */
  byte device_type;             /* Device type for Generic IOCTL */
  word open_files;              /* Number of open files on disk */
  struct dev_hdr _seg *next_drive;/* Next SRDISK drive */
                                /* Data transfer routine */
  void (near *batch_xfer)(char far *, dword, word, char);
};

#define c_disk_IO 2   /* Offset into disk_IO in config_s */
#define XMS_handle 0  /* Offsets into XMS_alloc structure */
#define XMS_entry 2
#define EMS_handle 0  /* Offsets into EMS_alloc structure */

extern struct config_s far *mainconf;
extern struct config_s far *conf;

/* Note 1!

   config_s field BPB_hidden is in the device driver file offset into
   the space reserved for the disk image data to be loaded during init.
   It is reset to zero during device driver initialization.
*/

/* Differences in f and newf, not all but those that we care about */

#define WRITE_PROTECTION 1
#define DISK_SIZE 2
#define SECTOR_SIZE 4
#define CLUSTER_SIZE 8
#define DIR_ENTRIES 0x10
#define NO_OF_FATS 0x20
#define MAX_PART_SIZES 0x40
#define MEDIA 0x80
#define SEC_PER_TRACK 0x100
#define SIDES 0x200
#define DEVICE_TYPE 0x400
#define SPACE_AVAILABLE 0x800
#define FILE_SPACE 0x1000
#define FREE_MEMORY 0x2000
#define REMOVABLE 0x4000
#define CLEAR_DISK 0x8000

/* reconfig_f tells if drive needs reconfiguration. (!format_f && reconfig_f)
   should tell that reformat is enough, no formatting necessary */
#define reconfig_f ((changed_format&(MEDIA|SEC_PER_TRACK|SIDES|DEVICE_TYPE)) \
  || bootsectorfile)

/* format_f tells if a format really needed */
#define format_f (changed_format & ( DISK_SIZE | SPACE_AVAILABLE | FILE_SPACE \
  | FREE_MEMORY | SECTOR_SIZE | CLUSTER_SIZE | DIR_ENTRIES | NO_OF_FATS \
  | CLEAR_DISK))

struct format_s {                 /* Disk format/configuration description */
  /* User defined parameters */
  int write_prot;               /* Write protection */
  int removable;                /* Tell DOS drive not removable */
  dword size;                   /* Defined current size */
  dword avail;                  /* Defined current size in bytes available */
  word bps;                     /* Bytes per sector */
  word cluster_size;            /* Size of one cluster in bytes */
  word FATs;                    /* Number of FAT copies */
  word dir_entries;             /* Directory entries in the root directory */
  word media;                   /* Media */
  word sec_per_track;           /* Sectors per track */
  word sides;                   /* Sides on disk */
  enum DeviceType_e device_type;/* Device type for Generic IOCTL */
  struct subconf_s {            /* List of the drivers chained to this disk */
    struct config_s far *conf;
    dword maxK;                 /* The maximum size of this part */
    dword size;                 /* The size of this part */
    int userdef:1;              /* True if user defined new max size */
    int alloc_best:1;           /* True if already allocated the most */
  } subconf[MAX_CHAINED_DRIVERS];
  /* Derived parameters */
  word bps_shift;               /* 2^bps_shift == bps */
  word chain_len;               /* Number of drivers chained to this drive */
  dword max_size;               /* Largest possible disk size (truth may be less) */
  dword current_size;           /* Counted current size from the driver chain */
  dword max_safe_mem;           /* Maximum amount of memory safely usable */
  dword max_mem;                /* Maximum amount of memory possibly usable */
  word reserved;                /* Reserved sectors in the beginning (boot) */
  word spFAT;                   /* Sectors per FAT */
  dword sectors;                /* Total sectors on drive */
  word FAT_sectors;             /* Total FAT sectors */
  word dir_sectors;             /* Directory sectors */
  word dir_start;               /* First root directory sector */
  word system_sectors;          /* Boot, FAT and root dir sectors combined */
  dword data_sectors;           /* Total number of usable data sectors */
  dword used_sectors;           /* Sectors actually used */
  word spc;                     /* Sectors per cluster */
  word clusters;                /* Total number of clusters */
  word FAT_type;                /* Number of bits in one FAT entry (12 or 16) */
};

extern struct format_s f, newf;
extern word forced_format;       /* Format parameters user explicitly defined */
extern word defined_format;      /* Format parameters user defined */
extern word changed_format;      /* Format parameters changed from the old */

extern word root_files;  /* Number of files in root directory */

/* Variables possibly supplied in command line */
extern char drive;          /* Drive letter of drive to format */
extern int force_f;         /* Nonzero if ok to format */
extern int use_old_format_f; /* Take undefined parameters from the old format */
extern int f_set_env;       /* Set environment variables */
extern int verbose;   /* Verbose: 1 banner, */
                      /* 2 + new format, 3 + old format, 4 + long format */
extern char *bootsectorfile;   /* File for boot sector image */

/*
**  Boot sector structure
*/

struct boot_s {
  word jump;
  byte nop;
  char oem[8];
  word bps;
  byte spc;
  word reserved;
  byte FATs;
  word dir_entries;
  word sectors;
  byte media;
  word spFAT;
  word spt;
  word sides;
  dword hidden;
  dword sectors32;
  byte physical;
  byte :8;
  byte signature;
  dword serial;
  char label[11];
  char filesystem[8];
  word bootcode;
};

/*
**  Declarations
*/

extern word max_bps;     /* Maximum sector size allowed on system */

void force_banner(void);
void parse_cmdline(int, char *[]);
void print_syntax(void);
void set_write_protect(void);

/*  Error handling functions */
void syntax(char *err, ...);
void fatal(char *err, ...);
void error(char *err, ...);
void warning(char *err, ...);
extern int error_count;     /* Number of errors occurred so far */
extern int disk_touched;    /* Disk processing state variables */
extern int disk_bad;
extern int data_on_disk;
extern enum disk_repair_e { /* In decreasing destructiveness */
  dr_disable,
  dr_clear,
  dr_old,
  dr_preserve
} disk_repair;

/* Utility */
void *xalloc(size_t s);
struct config_s far *conf_ptr(struct dev_hdr _seg *dev);
int getYN(void);
dword DOS_time(time_t);
int isWinEnh(void);         /* Return nonzero if in Windows Enhanced mode */

/* Environment */
void set_env(void);

/* disk I/O module */
void read_sector(int count, dword start, void far *buffer);
void write_sector(int count, dword start, void far *buffer);
dword disk_alloc(struct config_s far *conf, dword size);
dword safe_size(struct config_s far *conf);
dword max_size(struct config_s far *conf);

/* Disk Initialization module */
void init_drive(void);

/* Allocation module */
void calc_alloc(void);
int SavingDiskAllocate(dword);
void DiskAllocate(void);

/* New Format calculation module */
int make_newf(void);  /* return ERRL_* codes */
int count_new_format(void);
void make_new_format(void);

/* Format utility module */
int count_root(void);
int licence_to_kill(void);
void ConfigMaxAlloc(void);
void ConfigNonFormat(void);
void configure_drive(void);
void RefreshBootSector(void);
void FillBootSectorBPB(struct boot_s *);
enum filebootsector_status { fbs_ok, fbs_open, fbs_read, fbs_valid };
enum filebootsector_status FileBootSector(char *file, byte *sector);

/* Format module */
char *stringisize_flags(int flags);
void print_format(struct format_s *);
void print_newf(void);
void disable_disk(void);
void format_disk(void);
void WriteNewFormat(void);

/* Resize module */
void MoveSectors(dword dest, dword orig, dword number);
void ClearSectors(dword dest, dword number);
void Resize(void);

/* Directory and data packing module */
void packdata(void);

#endif

