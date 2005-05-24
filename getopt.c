/* Get Option Parser for Command Line Parsing
 * Copyright (C) 1993-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <ctype.h>
#include "getopt.h"

/* Scan the given string to find options
    str       String to parse
    switches  Pointer to table of switches
    lenght    Lenght of switches table
   Return: index into switches table telling which switch was parsed
*/

int get_opt(char **str, struct switch_s* switches, int length)
{
  char *argp = *str;
  char *end_ptr = argp;
  int match = GETOPT_PARAM;

  /* Skip white space */
  while(isspace(*argp))
    argp++;

  *str = argp;

  if (*argp == '/' || *argp == '-') {
    int sw;             /* Switch in turn */
    int best_len = 0;   /* How long the switch is on command line */
    int ambiguous = 0;  /* Is the best switch ambiguous */

    argp++;
    for (sw = 0; sw < length; sw++) {
      char *s = switches[sw].sw;
      char *arg = argp;
      int m = 0;

      while(*arg && *s && *s == toupper(*arg))
        s++, arg++, m++;

      if (m && !isalpha(*arg)) {
        if (!*s || m > best_len) {
          best_len = m;             /* New best candidate (or exact match) */
          match = sw;
          end_ptr = arg;
          ambiguous = 0;
          if (!*s)
            break;  /* out of sw loop */
        }
        else if (m == best_len)
          ambiguous = 1;            /* As good choise as the previous */
      }
    }

    if (!best_len)
      match = GETOPT_NOMATCH;
    else if (ambiguous)
      match = GETOPT_AMBIGUOUS;
    else /* If Match */ {
      if (*end_ptr == ':')
        end_ptr++;
      *str = end_ptr;
    }
  }
  return match;
}

