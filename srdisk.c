/*
 *      ReSizeable RAMDisk formatter
 *
 *      Copyright (C) 1992-1996, 2005 Marko Kohtala
 *      Released under GNU GPL, read the file 'COPYING' for more information
 *
 *      Compilable with Borland C++ 3.0.
 *
 */

#include "srdisk.h"
#include <stdio.h>
#include <time.h>

word forced_format;    /* What values user explicitly defined for newf */
word defined_format;   /* What values user defined for newf */
word changed_format;   /* Differences between f and newf */

word root_files = 1;   /* Number of entries in root directory */
word max_bps = 512;    /* Largest possible sector size on system */

char drive=0;
int force_f=0;        /* -1 if not to, 0 to ask, 1 to destroy data */

struct config_s far *mainconf;  /* First drive configuration structure */
struct config_s far *conf;      /* Current drive configuration structure */

struct format_s f, newf;

int return_val = 0;   /* ERRORLEVEL value to return */
char *return_msg;     /* Pointer to message to display on exit */

char *exename;        /* Name of the executable */

/* To get enough stack */
unsigned _stklen = 0x1000;

void force_banner(void)
{
  static int wrote_banner = 0;
  if (!wrote_banner) {
    printf("ReSizeable RAMDisk Formatter version "VERSION". "
           "Copyright (c) 2005 Marko Kohtala.\n\n");
    wrote_banner = 1;
  }
}

void print_banner(void)
{
  if (verbose == -1 || verbose > 0)
    force_banner();
}

/*
**  MAIN FUNCTION
*/

int main(int argc, char *argv[])
{
  extern int mem_allocated;

  exename = *argv[0] ? argv[0] : "SRDISK.EXE";

  if (argc > 1) parse_cmdline(argc, argv);

  print_banner();

  if (argc == 1)
    printf("For help type 'SRDISK /?'.\n\n");

  { /* This is to solve a strange problem I am having with DR-DOS 5
       DR-DOS 5 seems to get into infinite loop reading FAT if sector
       size is larger than 128 bytes!
    */
    max_bps = 512;
#if 0   /* !!!! This is just to test if the bug has been solved */  
    asm {
      mov ax,0x4452
      stc
      int 0x21
      jc notDRDOS
      cmp ax,dx
      jne notDRDOS
      cmp ax,0x1065
      jne notDRDOS /* Actually "not DR-DOS 5" */
    }
    /* It is DR-DOS 5, so limit the sector size */
    max_bps = 128;
    if (newf.bps > max_bps) {
      warning("Sector size is limited to 128 bytes under DR-DOS 5");
      newf.bps = max_bps;
    }
   notDRDOS:
#endif
  }

  if (verbose == -1) verbose = 2;

  init_drive();         /* Find drive and collect old configuration */

  if (defined_format || bootsectorfile) {      /* If new format defined */
    format_disk();

    if (error_count) {
      warning("The disk is possibly damaged because of the errors\n");
    }

    if (mem_allocated && isWinEnh() && verbose > 1) {
      warning("Memory allocated for disk under MS-Windows will be released when you\n"
	      "end this DOS session\n");
    }
  }
  else {
    if (f_set_env != YES && 1 < verbose && verbose < 4) {
      if (f.size) print_format(&f);
      else printf("Drive %c: disabled\n", drive);
    }
  }
  if (f_set_env == YES)
    set_env();

  if (return_msg)
    puts(return_msg);

  return return_val;
}

