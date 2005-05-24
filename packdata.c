/* SRDISK - data packing and disk format checking
 * Copyright (C) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

/* Todo:
** Make separate directory packing function that can be used to compact the
   directory to minimum size before it is relocated (or shortened as
   root directory might be). NON RECURSIVE!
*/

/* Disable debugging output
*/
#define NDEBUG

#include "director.h"
#include "bitmap.h"
#include <string.h>
#include <assert.h>

static word topcluster;

static char path[300];

#ifndef NDEBUG
#include <stdio.h>
#endif

static char *buffer;                /* Buffer to store clusters in */
static bitmap visited_clusters;     /* Bitmap of visited clusters */

#define visit(cluster) bitmap_set(visited_clusters, cluster)
#define unvisit(cluster) bitmap_reset(visited_clusters, cluster)
#define test_loop(cluster)                                          \
    (bitmap_test(visited_clusters, cluster)                         \
        ? warning("File %s crosslinked at %ud", path, cluster), 1   \
        : 0)

static word relocate(word cluster)
{
    word c;

    assert(buffer);

    c = FindFirstFreeCluster();
    if (c) {
        read_sector(fat->spc, cluster2sector(cluster), buffer);
        write_sector(fat->spc, cluster2sector(c), buffer);
        set_next_cluster(cluster, 0);
    }
    return c;
}

static word packfile(word cluster)
{
    word prev_c, next_c, curr_c, new_c;

    if (test_loop(cluster))
        return 0;

    next_c = next_cluster(cluster);

    if (cluster > topcluster) {
        new_c = relocate(cluster);
        if (!new_c)
            return 0;
        cluster = new_c;
        set_next_cluster(cluster, next_c);
    }

    prev_c = cluster;

    visit(cluster);

    #ifndef NDEBUG
    printf("%d ", cluster);
    #endif

    while(next_c && next_c < 0xFFF0) {
        curr_c = next_c;
        if (test_loop(curr_c))
            return cluster;
        next_c = next_cluster(curr_c);
        if (curr_c > topcluster) {
            new_c = relocate(curr_c);
            if (!new_c)
                return 0;
            set_next_cluster(new_c, next_c);
            set_next_cluster(prev_c, new_c);
            prev_c = new_c;
        }
        else
            prev_c = curr_c;
        visit(prev_c);
        #ifndef NDEBUG
        printf("%d ", prev_c);
        #endif
    }

    return cluster;
}

/*
**  Pack a directory.
**
**  No need to check if this directory gone through twice, since if
**  it is a second time now, then this dir would have been packed twice
**  too and it would have been considered cross linked.
*/


static int packdir(struct directory_s *parent_dir)
{
    struct directory_s *dir;
    word cluster = parent_dir ? parent_dir->item.start : 0;
    int oldend = strlen(path);

    dir = DirOpen(cluster);
    if (!dir)
        return 0;

    path[oldend] = '\\';

    while(!dir->at_end) {
        int cp = oldend;
        int len = 0;

        /* Construct pathname */
        while(dir->item.name[len] != ' ' && len < 8)
            path[++cp] = dir->item.name[len++];
        len = 8;
        if (dir->item.name[len] != ' ') {
            path[++cp] = '.';
            while(dir->item.name[len] != ' ' && len < 11)
                path[++cp] = dir->item.name[len++];
        }
        path[++cp] = '\0';

        #ifndef NDEBUG
        puts(path);
        #endif

        if (strncmp(dir->item.name, ".          ", 11) == 0) {
            dir->item.start = cluster;
        }
        else if (strncmp(dir->item.name, "..         ", 11) == 0) {
            dir->item.start = parent_dir->start_cluster;
        }
        else {
            if (dir->item.start) {
                word c;
                c = packfile(dir->item.start);
                #ifndef NDEBUG
                printf("%s\n", c ? "OK" : "FAIL");
                #endif
                #if 0
                if (!c)
                    goto fail;
                #endif
                dir->item.start = c;
            }
            if (dir->item.attr & FA_DIREC) {
                if (!packdir(dir))
                    goto fail;
            }
        }
        disk_bad = 1;   /* Tell that the disk may now have bad format */
        DirWrite(dir);
        if (!DirFindNext(dir))
            return 0;
    }
    DirWrite(dir);  /* Ends the dir properly */
    DirClose(dir);
    path[oldend] = '\0';
    return 1;
  fail:
    DirClose(dir);
    path[oldend] = '\0';
    return 0;
}

void packdata(void)
{
    int old_disk_bad = disk_bad;

    topcluster = fatstat.used + 1;

    buffer = (char *)xalloc(fat->cluster_size);
    visited_clusters = bitmap_new((fat->clusters)+2);

    path[0] = '\0';

    if (!packdir(0))
        fatal("Packing files failed");

    disk_bad = old_disk_bad;
    fatstat.last_used = topcluster;

    bitmap_delete(visited_clusters);
    free(buffer);

    #ifndef NDEBUG
    printf("Data is packed\n");
    #endif
}

