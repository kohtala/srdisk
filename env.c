/* ReSizeable RAMDisk - environment utilities
 * Copyright (c) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "srdisk.h"
#include <stdio.h>
#include <dos.h>
#include <string.h>

/*
   envptr - returns pointer to parent's copy of the environment
   Provided by Doug Dougherty, original source by S. Palmer.
   Heavily modified for srdisk use.
*/

static char _seg *envptr(char near **size)
{
    char _seg* env;

    #pragma pack(1)
    struct PSP {    /* Program Segment Prefix */
        word int20;
        word nextParagraph;
        byte r1;
        byte dispatcher[5];
        dword terminateVector;
        dword breakVector;
        dword critErrVector;
        struct PSP _seg *parent;        /* Undocumented in DOS 5 */
        word r2[10];
        char _seg *environment;
    } _seg *parent_p;

    struct MCB {    /* Memory Control Block */
        byte id;
        struct PSP _seg *owner;
        word size;
    } _seg *mcb = NULL;
    #pragma pack()

    #define getMCB(s) ((struct MCB _seg *)((word)(s) - 1))

    parent_p = ((struct PSP _seg *)_psp)->parent;

    if (!parent_p || parent_p->int20 != 0x20CD)
        return NULL;
    mcb = getMCB(parent_p);

    if (mcb->owner != parent_p)
        return NULL;

    /* MS-DOS 3.2 first COMMAND.COM does not have environment pointer! */
    if (parent_p->environment)
    {
        env = parent_p->environment;
        mcb = getMCB(env);
    }
    else
    {
        mcb = (struct MCB _seg*)((word)parent_p + (word)getMCB(parent_p)->size);
        env = (char _seg*)((word)mcb + 1);
    }

    *size = (char near *)(mcb->size * 16);
    return env;
}

/*
   msetenv - place an environment variable in parent's copy of
             the environment.
   Provided by Doug Dougherty, original source by S. Palmer.
   Heavily modified for SRDISK use.
*/
static int msetenv(char *var, char *value)
{
    char _seg *env;
    char near *end, near *envp;
    char near *cp;
    int l;

    env = envptr(&end);
    if (!env) return -2;    /* Return error if no environment found */

    envp = 0;

    l = strlen(var);
    /* strupr(var); */

    /*
       Delete any existing variable with the name (var).
    */
    while (*(env+envp)) {
        if ((_fstrncmp(var,(env+envp),l) == 0) && ((env+envp)[l] == '=')) {
            cp = envp + _fstrlen((env+envp)) + 1;
            _fmemcpy((env+envp),(env+cp),end-cp);
        }
        else {
            envp += _fstrlen((env+envp)) + 1;
        }
    }

    /*
       If the variable fits, shovel it in at the end of the environment.
    */
    if (_fstrlen(value) && (end-envp) >= (l + _fstrlen(value) + 4)) {
        _fstrcpy((env+envp),var);
        _fstrcat((env+envp),"=");
        _fstrcat((env+envp),value);
        l = _fstrlen(env+envp);
        (env+envp)[l+1] = 0;
        *(word far *)(env+envp+l+2) = 0; /* Zero strings after environment */
        return 0;
    }

    /*
       Return error indication if variable did not fit.
    */
    return -1;
}

/*
**  Set environment variables to show SRDISK RAMDisks
**
**  Allowed not to return if error found, but if not serious error,
**  may return.
*/

void set_env()
{
    struct config_s far *conf = mainconf;
    char var[] = "SRDISK1";
    char drive[] = "A";
    int err;

    do {
      drive[0] = conf->drive;

      err = msetenv(var, drive);
      if (err == -1)
        fatal("Not enough environment space");
      if (err == -2)
        fatal("No environment found to modify");

      if (verbose > 1)
        printf("Set %s=%s\n", var, drive);

      var[6]++;
      conf = conf_ptr(conf->next_drive);
    } while ( conf );
}

