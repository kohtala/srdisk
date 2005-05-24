/* SRDISK - FAT handling functions header
 * Copyright (C) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifndef _FAT_H
#define _FAT_H

#include "srdisk.h"

#define cluster2sector(cluster) ((dword)((cluster)-2) * fat->spc + \
    fat->system_sectors)

struct fatstat_s {
    word free;
    word used;
    word bad;
    word first_free;
    word last_used;
    int valid;
};

extern struct fatstat_s fatstat;
extern struct format_s *fat;

extern int FAT_open(struct format_s *);
extern void FAT_close(void);
extern word next_cluster(word cluster);
extern void set_next_cluster(word cluster, word set_to);
extern word FindFirstFreeCluster(void);
extern void save_FAT_buffer(void);
extern void FAT_stats(void);

#endif

