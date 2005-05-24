/* SRDISK - Directory handling functions header
 * Copyright (C) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifndef _DIRECTORY_H
#define _DIRECTORY_H

#include <dos.h>
#include "fat.h"

struct dir_item_s {
    char name[8], ext[3];
    byte attr;
    char reserved[10];
    dword time;
    word start;
    dword size;
};

struct directory_s {
    word start_cluster;     /* Cluster # of the directory start (0 for root) */
    struct dir_loc {
        dword cluster;      /* Cluster # holding the current item */
        word item;          /* Item # in the directory */
        div_t loc;          /* Location of item in cluster chain */
    } read, write;
    struct dir_item_s item; /* The current item in the directory */
    int at_end : 1;         /* Nonzero if directory at end */
};

struct directory_s *DirOpen(word cluster);
void DirClose(struct directory_s *);
void DirRewind(struct directory_s *);
int DirFindNext(struct directory_s *);
void DirWrite(struct directory_s *);

#endif
