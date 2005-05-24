/* SRDISK - directory reading and writing functions
 * Copyright (C) 1992-1996, 2005 Marko Kohtala 
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

/* Note! These functions have been designed so that it is easy to scan the
   directory and at the same time remove the deleted directory entries */

#include "director.h"
#include <assert.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "max.h"

/* !!!! Make the buffer optionally store less sectors to handle the end
   of directory gracefully */
/* Local variables */
struct buffer_s {
    char *data;
    word data_size;
    int sectors;      /* Number of sectors in this buffer */
    dword sector;
    int changed : 1;
};

static struct buffer_s buffer;

/* Write directory cluster buffer */

static void save_buffer(void)
{
    if (buffer.changed) {
        write_sector(buffer.sectors, buffer.sector, buffer.data);
        buffer.changed = 0;
    }
}

static void get_sectors(dword sector, int count)
{
    assert(buffer.data);
    assert(count * fat->bps <= buffer.data_size);

    if (sector == buffer.sector) return;

    if (buffer.changed) save_buffer();

    buffer.sector = sector;
    buffer.sectors = count;
    read_sector(count, sector, buffer.data);
}

/* Open directory starting at cluster (0 for root directory) */

struct directory_s *DirOpen(word cluster)
{
    struct directory_s *dir;

    if (!buffer.data) {
        assert(!buffer.changed);
        buffer.data = (char *)xalloc(fat->cluster_size);
        buffer.data_size = fat->cluster_size;
    }

    assert(buffer.data_size == fat->cluster_size);

    dir = (struct directory_s*)xalloc(sizeof (struct directory_s));

    memset(dir, 0, sizeof (struct directory_s));
    dir->start_cluster = cluster;
    DirRewind(dir);

    return dir;
}

void DirClose(struct directory_s *dir)
{
    save_buffer();
    free(dir);
}

static void DirRewind(struct directory_s *dir)
{
    static const div_t startloc = {0,0};

    dir->write.loc = dir->read.loc = startloc;
    dir->write.item = dir->read.item = 0;
    if (dir->start_cluster) {
        dir->write.cluster = dir->read.cluster = dir->start_cluster;
        get_sectors(cluster2sector(dir->start_cluster), fat->spc);
    }
    else {
        dir->write.cluster = dir->read.cluster = fat->dir_start;
        get_sectors(fat->dir_start,
                    (int)min((dword)fat->spc, fat->dir_sectors));
    }
    switch (((struct dir_item_s *)buffer.data)->name[0]) {
      case 0xE5:
        DirFindNext(dir);
        break;
      case 0:
        dir->at_end = 1;
      default:
        memmove(&dir->item, buffer.data, 32);
    }
}


/* Write the item back into the directory */

void DirWrite(struct directory_s *dir)
{
  dword sector;
  word count;

  if (dir->start_cluster) {
    sector = cluster2sector(dir->write.cluster);
    count = fat->spc;
  }
  else {
    sector = dir->write.cluster;
    count = min(fat->spc, (word)(fat->system_sectors - sector));
  }
  get_sectors(sector, count);
  buffer.changed = 1;

  if (dir->at_end) {
    /* Must write zero to the rest of directory */
    memset(buffer.data + dir->write.loc.rem, 0,
           buffer.data_size - dir->write.loc.rem);
    if (dir->start_cluster) {
      /* Free any extra clusters in the chain */
      word pc, nc;
      assert(dir->write.cluster < 0xFFF0);
      nc = next_cluster(dir->write.cluster);
      if (nc < 0xFFF8) {
        set_next_cluster(dir->write.cluster, 0xFFFF);
        do {
          pc = nc;
          nc = next_cluster(pc);
          set_next_cluster(pc, 0);
        } while (nc && nc < fat->clusters+2);
      }
    }
    else {
      /* Clear extra sectors in root directory */
      int first = (dir->write.loc.quot + 1) * fat->spc;
      if (fat->dir_sectors > first) {
        ClearSectors(fat->dir_start + first,
                     fat->dir_sectors - first);
      }
    }
  }
  else {
    memmove(buffer.data + dir->write.loc.rem, &dir->item, 32);
    dir->write.item++;
    if ( (dir->write.loc.rem += 32) >= fat->cluster_size ) {
      if (dir->start_cluster) {
        word c;
        c = next_cluster(dir->write.cluster);
        if (c < 0xFFF0) {
          dir->write.cluster = c;
          goto next_loc;
        }
      } else {
        assert(dir->write.item <= fat->dir_entries);
        if (dir->write.item < fat->dir_entries) {
          dir->write.cluster += fat->spc;
        next_loc:
          dir->write.loc.quot++;
          dir->write.loc.rem = 0;
        }
      }
    }
  }
}

/* Find next valid item in the directory */

int DirFindNext(struct directory_s *dir)
{
    enum { ok, not, advance } buff_valid = not;
    struct dir_item_s *item;

    if (dir->at_end) return 0;

    do {
        dir->read.item++;
        dir->read.loc.rem += 32;
        if (dir->start_cluster) {
          if ( dir->read.loc.rem >= fat->cluster_size ) {
            word c;
            c = next_cluster(dir->read.cluster);
            if (c >= 0xFFF8)
              goto end_dir;
            if (!c || c >= 0xFFF0) return 0;
            dir->read.cluster = c;
            buff_valid = advance;
          }
        }
        else {
          if (fat->dir_entries <= dir->read.item)
            goto end_dir;
          if ( dir->read.loc.rem >= fat->cluster_size ) {
            dir->read.cluster += fat->spc;
            buff_valid = advance;
          }
        }
        if (buff_valid != ok) {
          dword start;
          word count;
          if (buff_valid == advance) {
            dir->read.loc.quot++;
            dir->read.loc.rem = 0;
          }
          if (dir->start_cluster) {
            start = cluster2sector(dir->read.cluster);
            count = fat->spc;
          }
          else {
            start = dir->read.cluster;
            count = min(fat->spc, (word)(fat->system_sectors - start));
          }
          get_sectors(start, count);
          buff_valid = ok;
        }
        item = (struct dir_item_s *)(buffer.data + dir->read.loc.rem);
        if (!item->name[0]) {
          end_dir:
            dir->at_end = 1;
            dir->item.name[0] = 0;
            return 1;
        }
    } while(item->name[0] == (char)0xE5);
    memmove(&dir->item, item, 32);
    dir->at_end = 0;
    return 1;
}

#if 0
/* Read random item in a directory into the directory_s */

int DirRead(struct directory_s *dir, word item)
{
    dword sector;
    div_t item_loc = div(item * 32, fat->cluster_size);

    if (!dir->start_cluster) {
        if (item >= fat->dir_entries) {
            dir->at_end = 1;
            dir->item.name[0] = 0;
            return item == fat->dir_entries ? 1 : 0;
        }
        sector = fat->dir_start + item_loc.quot * fat->spc;
    }
    else {
        word c;
        if (dir->read.loc.quot != item_loc.quot) {
            int cluster = item_loc.quot;
            if (dir->read.loc.quot < item_loc.quot) {
                c = dir->start_cluster;
            } else {
                c = dir->read.cluster;
                cluster -= dir->read.loc.quot;
            }
            while (cluster) {
                cluster--;
                c = next_cluster(c);
                if (c >= 0xFFF8) {
                    dir->at_end = 1;
                    dir->item.name[0] = 0;
                    return 1;
                }
                if (!c || c >= 0xFFF0) return 0;
            }
        } else
            c = dir->read.cluster;
        sector = cluster2sector(c);
    }
    if (!get_sectors(sector))
        return 0;
    dir->read.loc = item_loc;
    dir->read.item = item;
    memmove(&dir->item, buffer.data + item_loc.rem, 32);
    dir->at_end = dir->item.name[0] == 0;
    return 1;
}

#endif
