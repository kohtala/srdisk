/* SRDISK - FAT item reading and modifying
 * Copyright (C) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "srdisk.h"
#include "fat.h"
#include <assert.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

struct buffer_s {
    char *data;
    dword sector;
    int changed : 1;
};

#define BUFFERS 2

struct format_s *fat;
struct fatstat_s fatstat;       /* FAT statistics */
static struct buffer_s buffer[BUFFERS];
static int bufferLRU = 0;

static struct {
    word cluster;
    div_t loc;
} read_location;

static void save_buffer(int i)
{
    write_sector(1, buffer[i].sector, buffer[i].data);
    buffer[i].changed = 0;
}

void save_FAT_buffer(void)
{
    int i;
    for (i = BUFFERS - 1; i >= 0; i--)
        if (buffer[i].changed) {
            save_buffer(i);
        }
}

/* Returns index to table of buffers */
static int get_FAT_sector(long sector)
{
    int i;

    assert(buffer[0].data);

    sector += fat->reserved;

    for (i = BUFFERS - 1; i >= 0; i--)
        if (sector == buffer[i].sector) {
            bufferLRU = i;
            return i;
        }

    i = (bufferLRU + 1) % BUFFERS;

    if (buffer[i].changed)
        save_buffer(i);

    read_sector(1, sector, buffer[i].data);
    buffer[i].sector = sector;
    return i;
}

word next_cluster(word cluster)
{
    word FAT_w;
    char *b;

    if (read_location.cluster == cluster)
        goto next;

    read_location.cluster = cluster;
    if (fat->FAT_type == 12) {
      read_location.loc = div(cluster * 3 / 2, fat->bps);
    }
    else {
      read_location.loc.quot = cluster >> (fat->bps_shift - 1);
      read_location.loc.rem = (cluster % (fat->bps / 2)) * 2;
    }

  next:
    b = buffer[get_FAT_sector(read_location.loc.quot)].data;

    FAT_w = *(word *)(b + read_location.loc.rem);

    if (fat->FAT_type == 12) {
        if (read_location.loc.rem == fat->bps - 1) {
            b = buffer[get_FAT_sector(read_location.loc.quot + 1)].data;
            FAT_w = (FAT_w & 0xFF) | (*b << 8);
        }
        if (cluster & 1) {
            FAT_w >>= 4;
            read_location.loc.rem += 2;
        }
        else {
            FAT_w &= 0xFFF;
            read_location.loc.rem += 1;
        }

        if (read_location.loc.rem >= fat->bps) {
            read_location.loc.rem -= fat->bps;
            read_location.loc.quot++;
        }

        if ((FAT_w & 0xFF0) == 0xFF0)
          FAT_w |= 0xF000;
    }
    else {
        read_location.loc.rem += 2;
        if (read_location.loc.rem >= fat->bps) {
            read_location.loc.rem = 0;
            read_location.loc.quot++;
        }
    }
    read_location.cluster++;
    return FAT_w;
}

void set_next_cluster(word cluster, word set_to)
{
    div_t sector;
    word *FAT_w;
    int bno;
    char *b;

    if (fat->FAT_type == 12) {
      sector = div(cluster * 3 / 2, fat->bps);
    }
    else {
      sector.quot = cluster >> (fat->bps_shift - 1);
      sector.rem = (cluster % (fat->bps / 2)) * 2;
    }

    bno = get_FAT_sector(sector.quot);
    buffer[bno].changed = 1;
    b = buffer[bno].data;

    if (set_to == 0 && cluster < fatstat.first_free)
        fatstat.first_free = cluster;

    FAT_w = (word *)(b + sector.rem);

    if (fat->FAT_type == 12) {
        if (cluster & 1) {
            set_to <<= 4;
            if (sector.rem == fat->bps - 1) {
                *(byte *)FAT_w = (*(byte *)FAT_w & 0xF) | (set_to & 0xF0);
                bno = get_FAT_sector(sector.quot + 1);
                buffer[bno].changed = 1;
                *buffer[bno].data = set_to >> 8;
            }
            else
                *FAT_w = (*FAT_w & 0xF) | set_to;
        } else {
            set_to &= 0xFFF;
            if (sector.rem == fat->bps - 1) {
                *(byte *)FAT_w = set_to & 0xFF;
                bno = get_FAT_sector(sector.quot + 1);
                buffer[bno].changed = 1;
                b = buffer[bno].data;
                *b = (*b & 0xF0) | (set_to >> 8);
            }
            else
                *FAT_w = (*FAT_w & 0xF000) | set_to;
        }
    }
    else
        *FAT_w = set_to;
}

int FAT_open(struct format_s *format)
{
    int i;

    assert (!buffer[0].data);
    fat = format;
    memset(&buffer, 0, sizeof buffer);
    buffer[0].data = (char *)xalloc(BUFFERS * format->bps);
    for (i = 1; i < BUFFERS; i++)
        buffer[i].data = buffer[0].data + i * format->bps;
    memset(&fatstat, 0, sizeof fatstat);
    fatstat.first_free = 2;
    return 1;
}

void FAT_close(void)
{
  if (buffer[0].data) {
    save_FAT_buffer();
    free(buffer[0].data);
    buffer[0].data = NULL;
  }
}

word FindFirstFreeCluster(void)
{
    word c;
    for (c = fatstat.first_free; c < fat->clusters + 2; c++)
        if (next_cluster(c) == 0) {
            fatstat.first_free = c + 1;
            return c;
        }
    return 0;
}

void FAT_stats(void)
{
    if (!fatstat.valid) {
        word cluster, next;

        assert(buffer[0].data);

        /* !!!! Ought to compare FAT's? */

        memset(&fatstat, 0, sizeof fatstat);
        for (cluster = 2; cluster <= fat->clusters + 1; cluster++) {
            next = next_cluster(cluster);
            if (next == 0) {
                fatstat.free++;
                if (!fatstat.first_free)
                    fatstat.first_free = cluster;
            }
            else if (next < 0xFFF8 && next > fat->clusters + 1)
                fatstat.bad++;
            else {
                fatstat.used++;
                fatstat.last_used = cluster;
            }
        }
        fatstat.valid = 1;
    }
}

