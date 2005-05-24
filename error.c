/* SRDISK - Error handling functions
 * Copyright (C) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "srdisk.h"
#include "fat.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int error_count;        /* This should only be read outside this module */
void crash(int);

/* Other modules indicate the state through these boolean variables */
int disk_touched;       /* Some changes have been made to the disk */
int disk_bad;           /* Disk image is damaged */
int data_on_disk;       /* There is still old data on the disk */
enum disk_repair_e disk_repair = dr_preserve; /* What to do when crashes */

void syntax(char *err, ...)
{
  va_list arg;
  va_start(arg, err);
  force_banner();
  fprintf(stderr, "Syntax error: ");
  vfprintf(stderr, err, arg);
  putchar('\n');
#if 0
  print_syntax();
#else
  puts("For help type 'SRDISK /?'.");
#endif
  exit(ERRL_SYNTAX);
}

void fatal(char *err, ...)
{
  va_list arg;
  va_start(arg, err);
  force_banner();
  fprintf(stderr, "Fatal error: ");
  vfprintf(stderr, err, arg);
  putchar('\n');
  error_count++;
  crash(ERRL_FATAL);
}

void error(char *err, ...)
{
  va_list arg;
  va_start(arg, err);
  force_banner();
  fprintf(stderr, "Error: ");
  vfprintf(stderr, err, arg);
  putchar('\n');

  return_val = ERRL_ERRORS;

  /* Crash program if disk not yet touched or too many errors */
  if (!disk_touched || error_count >= 10)
    crash(ERRL_FATAL);
  error_count++;
  return;
}

void warning(char *err, ...)
{
  va_list arg;
  va_start(arg, err);
  force_banner();
  fprintf(stderr, "Warning: ");
  vfprintf(stderr, err, arg);
  putchar('\n');
  return;
}


/*
**  Crash function - too many or too severe errors
*/

static int crashed = 0;

static void finnish_disk(void)
{
  int i;

  if (disk_repair == dr_preserve)
    return;

  for(i = 0; i < f.chain_len; i++)
    f.subconf[i].alloc_best = 0;

  if (disk_repair == dr_disable || f.size == 0 || newf.size == 0) {
    disable_disk();
  }
  else {
    if (f.size < newf.size || disk_repair == dr_old) {
      if (verbose > 1)
        puts("Trying to make clear disk with the old format");
      memcpy(&newf, &f, sizeof f);
// !!!!      make_new_format();
    }
    else
      if (verbose > 1)
        puts("Trying to make a clear disk");
    disk_repair = dr_disable;
    WriteNewFormat();
  }
  if (verbose == 1)
    puts("\nManaged to make a valid disk");
}

static void crash(int status)
{
  FAT_close();

  puts("\aAborted");

  if (++crashed > 2) {
    puts("\aToo many errors - aborting.");
    exit(ERRL_ABNORMAL);
  }

  if (conf) {
    if (data_on_disk) {
      if (disk_bad) {
        if (force_f == ASK) {
          printf("Formatting aborted due to error while disk image is bad\n"
                 "OK to wipe off any data possibly left on the disk (Y/N) ? ");
          getYN();  /* This will set the force_f if permission given */
        }
        else
          puts("Failed to reformat while disk image is bad");
        if (force_f == YES) {
          if (disk_repair == dr_preserve)
            disk_repair = dr_clear;
          finnish_disk();
        }
      }
      else if (disk_touched) {
        puts("Disk has been modified - it may not be reliable");
      }
      if (data_on_disk) {
        extern byte makeRWaccess(void);
        conf->RW_access = makeRWaccess();
      }
    }
    else if (disk_bad) {
//      puts("Failed to construct a valid disk");
      finnish_disk();
    }
  }

  exit(status);
}

