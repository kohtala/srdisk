/* Get Option Parser for Command Line Parsing
 * Copyright (C) 1993-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 *
 * This parser is designed to be usable in situations when stdio
 * is unavailable i.e. in device driver also.
 */

/* This list must be in alphabetical order and switches in upper case! */

struct switch_s {
  char *sw;                       /* Switch text */
  char *(*parse)(char *, struct switch_s *); /* Function to parse argument */
  int ID;                         /* Switch ID passed to arg parser */
  void *loc;                      /* Location where to store the value */
  unsigned min;                   /* Minimum value or default */
  unsigned max;                   /* Maximum value */
};

/* get_opt parses string "*str" for switches in "switches". "switches" has
   "length" items. the return value is a pointer into switches or
   one of GETOPT_* values defined below. */

extern int get_opt(char **str, struct switch_s* switches, int length);

/* Return values from get_opt if not index to switch table */
#define GETOPT_PARAM -1
#define GETOPT_NOMATCH -2
#define GETOPT_AMBIGUOUS -3

