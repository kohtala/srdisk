/* ReSizeable RAMDisk - command line parser
 * Copyright (C) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "srdisk.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <io.h>
#include <fcntl.h>
#include "getopt.h"

/* Variables possibly supplied in command line */
int use_old_format_f=0; /* Take undefined parameters from the old format */
int f_set_env=NO;       /* Set environment variables */
int verbose=-1;         /* Verbose: 1 banner, */
                        /* 2 + new format, 3 + old format, 4 + long format */
char *bootsectorfile;   /* File for boot sector image */

enum switch_e {
  SW_NONE,
  SW_FATS,
  SW_CLUSTER,
  SW_DIR,
  SW_ENV,
  SW_DOSFORMAT,
  SW_HELP,
  SW_MAX,
  SW_OLD,
  SW_SECTOR,
  SW_SPT,
  SW_SIDES,
  SW_MEDIA,
  SW_ERASE,
  SW_VERBOSE,
  SW_WRITEP,
  SW_FORCE,
  SW_MINSIZE,
  SW_MAXSIZE
};

char *number(char *arg, struct switch_s *s);
char *filename(char *arg, struct switch_s *s);
char *yes_no(char *arg, struct switch_s *s);
char *maxsizes(char *arg, struct switch_s *s);
char *DOSform(char *arg, struct switch_s *s);

/* This list must be in alphabetical order and switches in upper case! */
static struct switch_s switches[] = {
  { "?",            NULL,     SW_HELP                                       },
  { "A",            number,   NO_OF_FATS,   &newf.FATs,         1,   255    },
  { "ASK",          yes_no,   0,            &force_f,           ASK         },
  { "AVAILABLE",    NULL,     SPACE_AVAILABLE, &newf.avail                  },
  { "BOOTSECTOR",   filename, 0,            &bootsectorfile                 },
  { "CLUSTER",      number,   CLUSTER_SIZE, &newf.cluster_size, 128, 32768L },
  { "D",            number,   DIR_ENTRIES,  &newf.dir_entries,  2,   8000   },
  { "DEVICETYPE",   number,   DEVICE_TYPE,  &newf.device_type,  0,   10     },
  { "DIRENTRIES",   number,   DIR_ENTRIES,  &newf.dir_entries,  2,   8000   },
  { "DOSFORMAT",    DOSform                                                 },
  { "E",            yes_no,   0,            &f_set_env,         YES         },
  { "ENVIRONMENT",  yes_no,   0,            &f_set_env,         YES         },
  { "ERASE",        NULL,     SW_ERASE                                      },
  { "F",            DOSform                                                 },
  { "FATS",         number,   NO_OF_FATS,   &newf.FATs,         1,   2      },
  { "FILESPACE",    NULL,     FILE_SPACE,   &newf.avail                     },
  { "FORCE",        yes_no,   0,            &force_f,           YES         },
  { "FREEMEM",      NULL,     FREE_MEMORY,  &newf.avail                     },
  { "H",            NULL,     SW_HELP                                       },
  { "HEADS",        number,   SIDES,        &newf.sides,        1,   255    },
  { "HELP",         NULL,     SW_HELP                                       },
  { "M",            maxsizes, SW_MAX                                        },
  { "MAXSIZE",      NULL,     SW_MAXSIZE                                    },
  { "MEDIA",        number,   MEDIA,        &newf.media,        0,   255    },
  { "MINSIZE",      NULL,     SW_MINSIZE                                    },
  { "NO",           yes_no,   0,            &force_f,           NO          },
  { "OLD",          NULL,     SW_OLD                                        },
  { "REMOVABLE",    yes_no,   REMOVABLE,    &newf.removable,    YES         },
  { "S",            number,   SECTOR_SIZE,  &newf.bps,          128, 512    },
  { "SECTORS",      number,   SEC_PER_TRACK,&newf.sec_per_track,1,   255    },
  { "SECTORSIZE",   number,   SECTOR_SIZE,  &newf.bps,          128, 512    },
  { "SIDES",        number,   SIDES,        &newf.sides,        1,   255    },
  { "SPT",          number,   SEC_PER_TRACK,&newf.sec_per_track,1,   255    },
  { "UNCONDITIONAL",NULL,     SW_ERASE                                      },
  { "VERBOSE",      number,   0,            &verbose,           0,   5      },
  { "WRITEPROTECT", yes_no,   WRITE_PROTECTION, &newf.write_prot, YES       },
  { "YES",          yes_no,   0,            &force_f,           YES         }
};

/*
**  SYNTAX
*/

static void print_syntax(void)
{
  force_banner();
  fputs(
  "Syntax:  SRDISK [<drive letter>:] [<size>] [/F:<DOS disk type>]\n"
  "\t\t[/S:<sector size>] [/C:<cluster size>] [/D:<dir entries>]\n"
  "\t\t[/V:<verbose>] [/E] [/U] [/O] [/Y] [/N] [/?]\n"
  "\t\t<... and many more. See SRDISK.DOC for details.>\n\n"

  "Anything inside [] is optional, the brackets must not be typed.\n"
  "'<something>' must be replaced by what 'something' tells.\n\n"

  "<drive letter> specifies the drive that is the RAM disk.\n"
  "<size> determines the disk size in kilobytes.\n"
  "/F:<DOS disk type> some valid are 1, 160, 180, 320, 360, 720, 1200, 1440.\n"
  "/S:<sector size> is a power of 2 in range from 128 to 512 bytes.\n"
  "/C:<cluster size> is a power of 2 in range from 128 to 32768 bytes.\n"
  "/D:<dir entries> is the maximum number of entries in the root directory.\n"
  "/A:<FATs> number of File Allocation Tables on disk. 1 or 2. 1 is enough.\n"
  "/W Write protect disk, /W- enables writes.\n"
  "/V Verbose level from 0 (silent) to 5 (verbose); default 2.\n"
  "/E Set environment variables SRDISKn (n=1,2,...) to SRDISK drive letters.\n"
  "/U Clear disk contents.                 /O Old format as default.\n"
  "/Y Yes, destroy the contents.           /N No, preserve contents.\n"
  "/? This help.\n"
  , stderr);
}

/*
**  UTILITY FUNCTIONS
*/

static long parse_narg(char *argp, char **next)
{
  long res;
  res = strtol(argp, next, 0);
  if (argp == *next) return -1L;
  switch(toupper(**next))
  {
    case 'M':
      res *= 1024;
    case 'K':
      res *= 1024;
      ++(*next);
  }
  return res;
}

static long parse_kilobyte(char *argp, char **next)
{
  long res;
  res = strtol(argp, next, 0);
  if (argp == *next) return -1L;
  switch(toupper(**next))
  {
    case 'M':
      res *= 1024;
    case 'K':
      ++(*next);
  }
  return res;
}

static int ispow2(long size)
{
  long cmp;
  for (cmp = 128; cmp; cmp <<=1)
    if (cmp == size) return 1;
  return 0;
}

/*
**  Command line argument parsing
*/

static char *number(char *arg, struct switch_s *s)
{
  long ln;
  word n;
  char *next;

  n = ln = parse_narg(arg, &next);
  if (arg == next)
    syntax("Argument expected for switch /%s", s->sw);

  if (ln > 0xFFFFu || s->min > n || s->max < n)
    syntax("Argument /%s:%lu is out of range (%u-%u)",
           s->sw, ln, s->min, s->max);

  defined_format |= s->ID;
  forced_format |= s->ID;
  if (s->loc)
    *(int *)s->loc = (int)n;

  switch(s->ID) {
  case SECTOR_SIZE:
    if (!ispow2(n))
      syntax("Invalid sector size - must be a power of two");
    break;
  case CLUSTER_SIZE:
    if (!ispow2(n))
      syntax("Invalid cluster size - must be a power of two");
    break;
  }

  return next;
}

static char *filename(char *arg, struct switch_s *s)
{
  char *next = arg;
  char *val;
  if (isalpha(next[0]) && next[1] == ':')
    next += 2;
  while(*next > ' ' && *next != ':' && *next != '/')
    ++next;
  val = malloc(next - arg + 1);
  memcpy(val, arg, next - arg);
  val[next - arg] = 0;
  *(char**)(s->loc) = val;
  return next;
}

static char *yes_no(char *arg, struct switch_s *s)
{
  int value = s->min;

  switch(arg[0]) {
  case '+':
    arg++;
    value = ON;
    break;
  case '-':
    arg++;
    value = OFF;
    break;
  case 'o':
  case 'O':
    switch(toupper(arg[1])) {
    case 'N':
      arg += 2;       /* ON */
      value = ON;
      break;
    case 'F':
      if (toupper(arg[2]) == 'F') {
        arg += 3;     /* OFF */
        value = OFF;
        break;
      }
    default:
      syntax("ON, OFF or nothing expected after /%s", s->sw);
    }
  }
  if (s->loc)
    *(int *)s->loc = value;
  defined_format |= s->ID;
  forced_format |= s->ID;
  return arg;
}

#pragma argsused
static char *maxsizes(char *arg, struct switch_s *s)
{
  int i;
  long n;

  /* MaxK for different partitions */
  memset(newf.subconf, 0, sizeof newf.subconf);
  defined_format |= MAX_PART_SIZES;
  forced_format |= MAX_PART_SIZES;
  i = 0;
  for(;;) {
    if (i == MAX_CHAINED_DRIVERS)
      syntax("Too many /M values - program limit exceeded");
    n = parse_kilobyte(arg, &arg);
    if (n < -1L || n > 0x3FFFFFL)
      syntax("Too large partition size: %ld", n);
    if (n != -1L) {
      newf.subconf[i].maxK = n;
      newf.subconf[i].userdef = 1;
    }
    if (*arg == ':') {
      i++;
      arg++;
    }
    else
      break;
  }
  return arg;
}

static char *DOSform(char *arg, struct switch_s *s)
{
  static struct {
    int disk_size;
    int media;
    enum DeviceType_e device_type;
    int sector_size;
    int cluster_size;
    int FATs;
    int dir_entries;
    int sec_per_track;
    int sides;
  } dos_disk[] = {
    {    1, 0xFA,  DT360, 128,  128, 1,   4,  1, 1 },   /* Special format */
    {  160, 0xFE,  DT360, 512,  512, 2,  64,  8, 1 },
    {  180, 0xFC,  DT360, 512,  512, 2,  64,  9, 1 },
    {  200, 0xFD,  DT360, 512, 1024, 2, 112, 10, 1 },   /* FDFORMAT format */
    {  205, 0xFD,  DT360, 512, 1024, 2, 112, 10, 1 },   /* FDFORMAT format */
    {  320, 0xFF,  DT360, 512, 1024, 2, 112,  8, 2 },
    {  360, 0xFD,  DT360, 512, 1024, 2, 112,  9, 2 },
    {  400, 0xFD,  DT360, 512, 1024, 2, 112, 10, 2 },   /* FDFORMAT format */
    {  410, 0xFD,  DT360, 512, 1024, 2, 112, 10, 2 },   /* FDFORMAT format */
    {  640, 0xFB,  DT720, 512, 1024, 2, 112,  8, 2 },
    {  720, 0xF9,  DT720, 512, 1024, 2, 112,  9, 2 },
    {  800, 0xF9,  DT720, 512, 1024, 2, 112, 10, 2 },   /* FDFORMAT format */
    {  820, 0xF9,  DT720, 512, 1024, 2, 112, 10, 2 },   /* FDFORMAT format */
    { 1200, 0xF9, DT1200, 512,  512, 2, 224, 15, 2 },
    { 1440, 0xF0, DT1440, 512,  512, 2, 224, 18, 2 },
    { 1476, 0xF0, DT1440, 512,  512, 2, 224, 18, 2 },   /* FDFORMAT format */
    { 1600, 0xF0, DT1440, 512,  512, 2, 224, 20, 2 },   /* FDFORMAT format */
    { 1640, 0xF0, DT1440, 512,  512, 2, 224, 20, 2 },   /* FDFORMAT format */
    { 1680, 0xF0, DT1440, 512,  512, 2, 224, 21, 2 },   /* FDFORMAT format */
    { 1722, 0xF0, DT1440, 512,  512, 2, 224, 21, 2 },   /* FDFORMAT format */
    { 2880, 0xF0, DT2880, 512, 1024, 2, 240, 36, 2 },
    {0}
  };
  long size;
  int i;
  char *next;

  size = parse_kilobyte(arg, &next);
  if (arg == next)
    syntax("Argument expected for switch /%s", s->sw);

  for (i=0; dos_disk[i].disk_size; i++)
    if (dos_disk[i].disk_size == size) {
      newf.size = size;
      newf.media = dos_disk[i].media;
      newf.device_type = dos_disk[i].device_type;
      newf.bps = dos_disk[i].sector_size;
      newf.cluster_size = dos_disk[i].cluster_size;
      newf.FATs = dos_disk[i].FATs;
      newf.dir_entries = dos_disk[i].dir_entries;
      newf.sec_per_track = dos_disk[i].sec_per_track;
      newf.sides = dos_disk[i].sides;
      defined_format |= DISK_SIZE
                      | SECTOR_SIZE
                      | CLUSTER_SIZE
                      | NO_OF_FATS
                      | DIR_ENTRIES
                      | MEDIA | SEC_PER_TRACK | DEVICE_TYPE | SIDES;
      forced_format |= DISK_SIZE;
      return next;
    }

  syntax("Unknown DOS disk size %ld", size);
  return NULL;  /* NOT REACHED */
}

static char *parse_arg(char *arg, int sw)
{
  struct switch_s *s = &switches[sw];

  if (s->parse)
    arg = s->parse(arg, s);
  else {
    switch(s->ID) {
    case SW_HELP:
      print_syntax();
      exit(0);
    case SW_OLD:
      use_old_format_f = YES;
      break;
    case SW_ERASE:
      defined_format |= CLEAR_DISK;
      forced_format |= CLEAR_DISK;
      break;
    case SPACE_AVAILABLE:
    case FILE_SPACE:
    case FREE_MEMORY:
    {
      unsigned long ln;
      char *next;

      ln = parse_kilobyte(arg, &next);
      if (arg == next)
        syntax("Argument expected for switch /%s", s->sw);
      arg = next;

      if (ln > 0x3FFFFFL)
        syntax(s->ID == FREE_MEMORY ? "Invalid available memory size"
                                    : "Invalid available disk size");

      *(dword *)s->loc = ln;
      defined_format |= s->ID;
      forced_format |= s->ID;
      break;
    }
    case SW_MINSIZE:
      *(dword *)s->loc = 0;
      defined_format |= SPACE_AVAILABLE;
      forced_format |= SPACE_AVAILABLE;
      break;
    case SW_MAXSIZE:
      *(dword *)s->loc = 0;
      defined_format |= FREE_MEMORY;
      forced_format |= FREE_MEMORY;
      break;
    default:
      assert(("Bad switch", 0));
    }
  }
  return arg;
}

void parse_cmdline(int argc, char *argv[])
{
  int arg;

  for(arg = 1; arg < argc; arg++) {
    int sw;
    char *argp, *start_argp;
    argp = argv[arg];
    while(*argp) {
      start_argp = argp;
      sw = get_opt(&argp, switches, sizeof(switches) / sizeof(switches[0]));
      switch(sw) {
        case GETOPT_PARAM:
          if (isdigit(*argp) && *(argp+1) != ':') {
            long n;
            n = parse_kilobyte(argp, &argp);
            if (n > 0x3FFFFFL)
              syntax("Invalid disk size");
            newf.size = n;
            defined_format |= DISK_SIZE;
            forced_format |= DISK_SIZE;
          }
          else {
            if (drive)
              goto bad_char;
            drive = toupper(*argp);
            if ( !(   (drive >= 'A' && drive <= 'Z')
                   || (drive >= '1' && drive <= '9')) )
             bad_char:
              syntax("Unrecognised character '%c' on command line", *argp);
            if (*++argp == ':') argp++;
          }
          break;
        case GETOPT_NOMATCH:
          /* !!!! Should I try to cut any extra parameters off? */
          syntax("Unknown switch %s", argp);
          break;
        case GETOPT_AMBIGUOUS:
          /* !!!! Should list the ambiguities */
          syntax("Ambiguous switch %s", argp);
          break;
        default:
          argp = parse_arg(argp, sw);
          break;
      }
      assert(argp != start_argp);
    }
  }

  { int c = 0;
    if (defined_format & DISK_SIZE) c++;
    if (defined_format & SPACE_AVAILABLE) c++;
    if (defined_format & FILE_SPACE) c++;
    if (defined_format & FREE_MEMORY) c++;
    if (c > 1)
      syntax("You specified many sizes for the disk");
  }
}

