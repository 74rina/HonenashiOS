#include "./fat16.h"
#include "../drivers/virtio.h"
#include "../kernel.h"

#define ATTR_LONG_NAME 0x0f

void read_fat_from_disk(void) {
  for (int i = 0; i < BPB_FATSz16; i++) {
    read_write_disk(&fat[i * (BPB_BytsPerSec / 2)], FAT1_START_SECTOR + i, 0);
  }
}

void write_fat_to_disk(void) {
  for (int i = 0; i < BPB_FATSz16; i++) {
    read_write_disk(&fat[i * (BPB_BytsPerSec / 2)], FAT1_START_SECTOR + i, 1);
  }

  for (int i = 0; i < BPB_FATSz16; i++) {
    read_write_disk(&fat[i * (BPB_BytsPerSec / 2)], FAT2_START_SECTOR + i, 1);
  }
}

void read_root_dir_from_disk(void) {
  for (int i = 0; i < ROOT_DIR_SECTORS; i++) {
    read_write_disk(&root_dir[i * (BPB_BytsPerSec / 32)],
                    ROOT_DIR_START_SECTOR + i, 0);
  }
}

void write_root_dir_to_disk(void) {
  for (int i = 0; i < ROOT_DIR_SECTORS; i++) {
    read_write_disk(&root_dir[i * (BPB_BytsPerSec / 32)],
                    ROOT_DIR_START_SECTOR + i, 1);
  }
}

uint32_t cluster_to_sector(uint16_t cluster) {
  return DATA_START_SECTOR + (cluster - 2) * BPB_SecPerClus;
}

static void dir_entry_name(const struct dir_entry *de, char *name) {
  int p = 0;

  for (int i = 0; i < 8; i++) {
    if (de->name[i] != ' ')
      name[p++] = de->name[i];
  }

  if (de->ext[0] != ' ') {
    name[p++] = '.';
    for (int i = 0; i < 3; i++) {
      if (de->ext[i] != ' ')
        name[p++] = de->ext[i];
    }
  }

  name[p] = '\0';
}

static struct dir_entry *find_entry_in_dir(struct dir_entry *entries,
                                           int entry_count,
                                           const char *filename) {
  static struct dir_entry found;
  char entry_name[13];

  for (int i = 0; i < entry_count; i++) {
    struct dir_entry *de = &entries[i];

    if (de->name[0] == 0x00)
      break;
    if ((uint8_t)de->name[0] == 0xE5)
      continue;
    if ((de->attr & ATTR_LONG_NAME) == ATTR_LONG_NAME)
      continue;

    dir_entry_name(de, entry_name);
    if (strcmp(entry_name, filename) == 0) {
      found = *de;
      return &found;
    }
  }

  return NULL;
}

struct dir_entry *iterate_dir(uint16_t dir_cluster, const char *filename) {
  if (!filename || *filename == '\0')
    return NULL;

  if (dir_cluster == 0) {
    read_root_dir_from_disk();
    return find_entry_in_dir(root_dir, BPB_RootEntCnt, filename);
  }

  if (dir_cluster < 2 || dir_cluster >= FAT_ENTRY_NUM)
    return NULL;

  struct dir_entry entries[BPB_BytsPerSec / sizeof(struct dir_entry)];
  read_cluster(dir_cluster, entries);
  return find_entry_in_dir(entries, BPB_BytsPerSec / sizeof(struct dir_entry),
                           filename);
}
