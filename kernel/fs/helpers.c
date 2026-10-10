#include "../drivers/virtio.h"
#include "../kernel.h"
#include "./fat16.h"

#define ATTR_DIRECTORY 0x10
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

static uint32_t cluster_to_sector(uint16_t cluster) {
  return DATA_START_SECTOR + (cluster - 2) * BPB_SecPerClus;
}

void read_cluster(uint16_t cluster, void *buf) {
  for (int i = 0; i < BPB_SecPerClus; i++) {
    read_write_disk((uint8_t *)buf + i * BPB_BytsPerSec,
                    cluster_to_sector(cluster) + i, 0);
  }
}

void write_cluster(uint16_t cluster, void *buf) {
  for (int i = 0; i < BPB_SecPerClus; i++) {
    read_write_disk((uint8_t *)buf + i * BPB_BytsPerSec,
                    cluster_to_sector(cluster) + i, 1);
  }
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

int resolve_path(const char *path, struct resolved_path *resolved) {
  if (!path || !resolved || path[0] == '\0')
    return -1;

  uint16_t current_cluster = (path[0] == '/') ? 0 : current_dir_cluster;

  memset(resolved, 0, sizeof(*resolved));
  resolved->parent_cluster = current_cluster;
  resolved->target_cluster = current_cluster;
  resolved->target_exists = true;
  resolved->is_directory = true;
  resolved->target.attr = ATTR_DIRECTORY;
  resolved->target.start_cluster = current_cluster;
  strcpy(resolved->abs_path, current_path);

  if (path[0] == '/')
    strcpy(resolved->abs_path, "/");

  while (*path) {
    char component[13];
    int component_len = 0;

    while (*path == '/')
      path++;

    if (*path == '\0')
      break;

    while (*path && *path != '/') {
      if (component_len + 1 >= (int)sizeof(component))
        return -1;
      component[component_len++] = *path++;
    }
    component[component_len] = '\0';

    if (strcmp(component, ".") == 0) {
      continue;
    }

    if (strcmp(component, "..") == 0) {
      char *last_slash;

      if (strcmp(resolved->abs_path, "/") != 0) {
        last_slash = strrchr(resolved->abs_path, '/');
        if (!last_slash || last_slash == resolved->abs_path) {
          strcpy(resolved->abs_path, "/");
        } else {
          *last_slash = '\0';
        }
      }

      if (current_cluster != 0) {
        struct dir_entry *target = iterate_dir(current_cluster, "..");
        if (!target || !(target->attr & ATTR_DIRECTORY))
          return -1;

        resolved->parent_cluster = current_cluster;
        resolved->target_cluster = target->start_cluster;
        resolved->target = *target;
        resolved->target_exists = true;
        resolved->is_directory = true;
        current_cluster = target->start_cluster;
      } else {
        resolved->parent_cluster = 0;
        resolved->target_cluster = 0;
        memset(&resolved->target, 0, sizeof(resolved->target));
        resolved->target.attr = ATTR_DIRECTORY;
        resolved->target.start_cluster = 0;
        resolved->target_exists = true;
        resolved->is_directory = true;
      }

      continue;
    }

    struct dir_entry *target = iterate_dir(current_cluster, component);
    if (!target)
      return -1;

    bool is_directory = (target->attr & ATTR_DIRECTORY) != 0;
    const char *rest = path;
    bool has_trailing_slash = *rest == '/';

    while (*rest == '/')
      rest++;

    if ((has_trailing_slash || *rest != '\0') && !is_directory)
      return -1;

    char *dst = resolved->abs_path;
    while (*dst)
      dst++;

    if (dst != resolved->abs_path && dst[-1] != '/') {
      if (dst + 1 >= resolved->abs_path + MAX_PATH_LEN)
        return -1;
      *dst++ = '/';
    }

    for (int i = 0; component[i]; i++) {
      if (dst + 1 >= resolved->abs_path + MAX_PATH_LEN)
        return -1;
      *dst++ = component[i];
    }

    *dst = '\0';

    resolved->parent_cluster = current_cluster;
    resolved->target_cluster = target->start_cluster;
    resolved->target = *target;
    resolved->target_exists = true;
    resolved->is_directory = is_directory;

    if (is_directory)
      current_cluster = target->start_cluster;
  }

  return 0;
}
