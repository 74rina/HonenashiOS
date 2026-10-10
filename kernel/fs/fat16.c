#include "./fat16.h"
#include "../drivers/virtio.h"
#include "../kernel.h"

uint16_t current_dir_cluster = 0;
char current_path[MAX_PATH_LEN] = "/";

// FATボリュームの各領域を初期化
void init_fat16_disk() {
  uint8_t buf[SECTOR_SIZE];
  for (int i = 0; i < SECTOR_SIZE; i++)
    buf[i] = 0;

  // FATエントリを0埋め
  for (unsigned s = FAT1_START_SECTOR;
       s < FAT1_START_SECTOR + BPB_FATSz16 * BPB_NumFATs; s++) {
    read_write_disk(buf, s, true);
  }

  // ルートディレクトリ領域を0埋め
  for (unsigned s = ROOT_DIR_START_SECTOR;
       s < ROOT_DIR_START_SECTOR + ROOT_DIR_SECTORS; s++) {
    read_write_disk(buf, s, true);
  }

  // データ領域は必要に応じて初期化
}

// RAM上のFATとルートディレクトリ
uint16_t fat[FAT_ENTRY_NUM];
struct dir_entry root_dir[BPB_RootEntCnt];

#define ATTR_DIRECTORY 0x10
#define ATTR_LONG_NAME 0x0f

void read_fat_from_disk(void);
void write_fat_to_disk(void);
void read_root_dir_from_disk(void);
void write_root_dir_to_disk(void);

void list_files(const char *dir_path) {
  struct resolved_path resolved;
  struct dir_entry *entries = NULL;
  struct dir_entry subdir_entries[BPB_BytsPerSec / sizeof(struct dir_entry)];
  int entry_count = 0;
  const char *display_path = dir_path;

  if (resolve_path(dir_path, &resolved) == 0) {
    if (!resolved.is_directory) {
      kprintf("[ls] not a directory: %s\n", dir_path);
      return;
    }

    display_path = resolved.abs_path;
    if (resolved.target_cluster == 0) {
      read_root_dir_from_disk();
      entries = root_dir;
      entry_count = BPB_RootEntCnt;
    } else {
      read_cluster(resolved.target_cluster, subdir_entries);
      entries = subdir_entries;
      entry_count = BPB_BytsPerSec / sizeof(struct dir_entry);
    }
  } else if (strcmp(dir_path, "/") == 0 || strcmp(dir_path, ".") == 0) {
    if (strcmp(dir_path, ".") == 0 && current_dir_cluster != 0) {
      read_cluster(current_dir_cluster, subdir_entries);
      entries = subdir_entries;
      entry_count = BPB_BytsPerSec / sizeof(struct dir_entry);
    } else {
      read_root_dir_from_disk();
      entries = root_dir;
      entry_count = BPB_RootEntCnt;
    }
  } else {
    const char *lookup_name = dir_path;
    uint16_t base_cluster = current_dir_cluster;

    if (dir_path[0] == '/') {
      lookup_name = dir_path + 1;
      base_cluster = 0;
    }

    if (*lookup_name == '\0') {
      read_root_dir_from_disk();
      entries = root_dir;
      entry_count = BPB_RootEntCnt;
    } else if (strrchr(lookup_name, '/')) {
      kprintf("[ls] nested path is not supported yet: %s\n", dir_path);
      return;
    } else {
      struct dir_entry *target = iterate_dir(base_cluster, lookup_name);

      if (!target) {
        kprintf("[ls] directory not found: %s\n", dir_path);
        return;
      }

      if (!(target->attr & ATTR_DIRECTORY)) {
        kprintf("[ls] not a directory: %s\n", dir_path);
        return;
      }

      read_cluster(target->start_cluster, subdir_entries);
      entries = subdir_entries;
      entry_count = BPB_BytsPerSec / sizeof(struct dir_entry);
    }
  }

  kprintf("=== Directory: %s ===\n", display_path);
  for (int i = 0; i < entry_count; i++) {
    struct dir_entry *de = &entries[i];

    if (de->name[0] == 0x00)
      break;
    if ((uint8_t)de->name[0] == 0xE5)
      continue;
    if ((de->attr & ATTR_LONG_NAME) == ATTR_LONG_NAME)
      continue;

    char name[13];
    int p = 0;

    for (int j = 0; j < 8; j++) {
      if (de->name[j] != ' ')
        name[p++] = de->name[j];
    }

    if (de->ext[0] != ' ') {
      name[p++] = '.';
      for (int j = 0; j < 3; j++) {
        if (de->ext[j] != ' ')
          name[p++] = de->ext[j];
      }
    }

    name[p] = '\0';

    kprintf("%s", name);
    if (de->attr & ATTR_DIRECTORY)
      kprintf("  <DIR>");
    else
      kprintf("  size=%d", (int)de->size);
    kprintf("  cluster=%d\n", (int)de->start_cluster);
  }
}

int read_file(uint16_t start_cluster, uint8_t *buf, uint32_t size) {
  read_fat_from_disk();

  if (start_cluster < 2 || start_cluster >= FAT_ENTRY_NUM)
    return -1;

  uint32_t remaining = size;
  uint16_t cluster = start_cluster;
  uint8_t cluster_buf[BPB_BytsPerSec * BPB_SecPerClus];

  while (cluster != 0xFFFF && remaining > 0) {
    read_cluster(cluster, cluster_buf);

    uint32_t to_copy = remaining;
    if (to_copy > BPB_BytsPerSec * BPB_SecPerClus)
      to_copy = BPB_BytsPerSec * BPB_SecPerClus;

    memcpy(buf, cluster_buf, to_copy);
    buf += to_copy;
    remaining -= to_copy;

    cluster = fat[cluster];
  }

  return 0;
}

void concatenate(const char *filename) {
  struct resolved_path resolved;
  struct dir_entry *target = NULL;

  if (resolve_path(filename, &resolved) == 0 && resolved.target_exists) {
    target = &resolved.target;
  } else {
    target = iterate_dir(current_dir_cluster, filename);
  }

  if (!target) {
    kprintf("[cat] file not found: %s\n", filename);
    return;
  }

  if (target->attr & ATTR_DIRECTORY) {
    kprintf("[cat] is a directory: %s\n", filename);
    return;
  }

  // サイズ0なら空ファイル
  if (target->size == 0) {
    kprintf("[cat] (empty file)\n");
    return;
  }

  // ファイルサイズぶんのバッファを確保
  uint32_t size = target->size;
  uint8_t buf[size]; // ※簡易実装としてスタック確保

  // read_file() でデータ領域を読む
  if (read_file(target->start_cluster, buf, size) < 0) {
    kprintf("[cat] read error.\n");
    return;
  }

  // ファイル内容をそのまま表示
  kprintf("===== cat: file content =====\n");
  for (uint32_t i = 0; i < size; i++) {
    putchar(buf[i]);
  }
  kprintf("\n===== end =====\n");
}

int current_directory(const char *name) {
  struct resolved_path resolved;

  if (resolve_path(name, &resolved) < 0) {
    kprintf("[cd] directory not found: %s\n", name);
    return -1;
  }

  if (!resolved.is_directory) {
    kprintf("[cd] not a directory: %s\n", name);
    return -1;
  }

  current_dir_cluster = resolved.target_cluster;
  strcpy(current_path, resolved.abs_path);
  return 0;
}

void print_working_directory(void) { kprintf("%s\n", current_path); }

// サブディレクトリを作る
int make_dir(uint16_t parent_cluster, const char *name) {
  read_fat_from_disk();
  read_root_dir_from_disk();

  struct dir_entry buf[BPB_BytsPerSec / sizeof(struct dir_entry)];
  struct dir_entry *parent_entries = NULL;
  int parent_entry_count = 0;

  /* ===== 親ディレクトリの実体を決定 ===== */
  if (parent_cluster == 0) {
    // ルートディレクトリ
    parent_entries = root_dir;
    parent_entry_count = BPB_RootEntCnt;
  } else {
    // サブディレクトリ
    read_cluster(parent_cluster, buf);
    parent_entries = buf;
    parent_entry_count = BPB_BytsPerSec / sizeof(struct dir_entry);
  }

  /* ===== 空きエントリ探索 ===== */
  int entry_index = -1;
  for (int i = 0; i < parent_entry_count; i++) {
    if (parent_entries[i].name[0] == 0x00 ||
        (uint8_t)parent_entries[i].name[0] == 0xE5) {
      entry_index = i;
      break;
    }
  }

  if (entry_index < 0) {
    kprintf("[FAT16] ERROR: Directory full.\n");
    return -1;
  }

  /* ===== 空きクラスタ探索 ===== */
  uint16_t new_cluster = 0;
  for (uint16_t i = 2; i < FAT_ENTRY_NUM; i++) {
    if (fat[i] == 0x0000) {
      new_cluster = i;
      break;
    }
  }

  if (new_cluster == 0) {
    kprintf("[FAT16] ERROR: No free cluster.\n");
    return -1;
  }

  fat[new_cluster] = 0xFFFF; // EOC

  /* ===== 親ディレクトリにエントリ追加 ===== */
  struct dir_entry *de = &parent_entries[entry_index];
  memset(de, 0, sizeof(struct dir_entry));
  memset(de->name, ' ', 8);
  memset(de->ext, ' ', 3);

  int n = 0;
  while (n < 8 && name[n] && name[n] != '.') {
    de->name[n] = name[n];
    n++;
  }

  de->attr = 0x10; // ATTR_DIRECTORY
  de->start_cluster = new_cluster;
  de->size = 0;

  /* ===== 新ディレクトリの中身を作る ===== */
  struct dir_entry newbuf[BPB_BytsPerSec / sizeof(struct dir_entry)];
  memset(newbuf, 0, sizeof(newbuf));

  // "."
  memset(newbuf[0].name, ' ', 8);
  memset(newbuf[0].ext, ' ', 3);
  newbuf[0].name[0] = '.';
  newbuf[0].attr = 0x10;
  newbuf[0].start_cluster = new_cluster;

  // ".."
  memset(newbuf[1].name, ' ', 8);
  memset(newbuf[1].ext, ' ', 3);
  newbuf[1].name[0] = '.';
  newbuf[1].name[1] = '.';
  newbuf[1].attr = 0x10;
  newbuf[1].start_cluster = parent_cluster;

  /* ===== 書き戻し ===== */
  write_cluster(new_cluster, newbuf);
  write_fat_to_disk();

  if (parent_cluster == 0) {
    write_root_dir_to_disk();
  } else {
    write_cluster(parent_cluster, parent_entries);
  }

  kprintf("[FAT16] Directory created: %s (cluster %d)\n", name, new_cluster);
  return 0;
}

// ファイルを作る
int create_file(const char *name, const uint8_t *data, uint32_t size) {
  read_fat_from_disk();
  read_root_dir_from_disk();

  // root_dir 空きエントリ探索
  int entry_index = -1;
  for (int i = 0; i < BPB_RootEntCnt; i++) {
    if (root_dir[i].name[0] == 0x00 || root_dir[i].name[0] == 0xE5) {
      entry_index = i;
      break;
    }
  }
  if (entry_index < 0) {
    kprintf("[FAT16] ERROR: Root directory is full. Cannot create new file.\n");
    return -1;
  }

  // 最初のクラスタ確保
  int free_cluster = -1;
  for (int i = 2; i < FAT_ENTRY_NUM; i++) {
    if (fat[i] == 0x0000) {
      free_cluster = i;
      break;
    }
  }
  if (free_cluster < 0) {
    kprintf("[FAT16] ERROR: no free FAT cluster.\n");
    return -1;
  }

  // ディレクトリエントリの設定
  struct dir_entry *de = &root_dir[entry_index];
  memset(de->name, ' ', 8);
  memset(de->ext, ' ', 3);

  int n = 0;
  while (n < 8 && name[n] && name[n] != '.') {
    de->name[n] = name[n];
    n++;
  }
  if (name[n] == '.') {
    n++;
    for (int e = 0; e < 3 && name[n + e]; e++) {
      de->ext[e] = name[n + e];
    }
  }

  de->start_cluster = free_cluster;
  de->size = size;

  // データ書き込み
  uint32_t remaining = size;
  uint16_t cluster = free_cluster;
  uint8_t cluster_buf[BPB_BytsPerSec * BPB_SecPerClus];

  while (remaining > 0) {
    uint32_t to_write = remaining;
    if (to_write > BPB_BytsPerSec * BPB_SecPerClus)
      to_write = BPB_BytsPerSec * BPB_SecPerClus;

    if (data) {
      memcpy(cluster_buf, data, to_write);
      data += to_write;
    } else {
      memset(cluster_buf, 0, to_write);
    }
    if (to_write < BPB_BytsPerSec * BPB_SecPerClus)
      memset(cluster_buf + to_write, 0,
             BPB_BytsPerSec * BPB_SecPerClus - to_write);

    write_cluster(cluster, cluster_buf);
    remaining -= to_write;

    if (remaining > 0) {
      // 次クラスタを確保
      uint16_t next_cluster = 0;
      for (uint16_t i = 2; i < FAT_ENTRY_NUM; i++) {
        if (fat[i] == 0x0000) {
          next_cluster = i;
          break;
        }
      }
      if (next_cluster == 0) {
        kprintf("[FAT16] ERROR: not enough clusters.\n");
        return -1;
      }
      fat[cluster] = next_cluster;
      fat[next_cluster] = 0xFFFF;
      cluster = next_cluster;
    } else {
      fat[cluster] = 0xFFFF; // 最後のクラスタ
    }
  }

  // 書き戻し
  write_fat_to_disk();
  write_root_dir_to_disk();

  kprintf("[FAT16] File created: %s at entry %d, cluster %d\n", name,
          entry_index, free_cluster);
  return 0;
}
