#include "./fat16.h"
#include "../drivers/virtio.h"
#include "../kernel.h"

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

// FAT領域の読み書き
static void read_fat_from_disk() {
  for (int i = 0; i < BPB_FATSz16; i++) {
    read_write_disk(&fat[i * (BPB_BytsPerSec / 2)], FAT1_START_SECTOR + i, 0);
  }
}
static void write_fat_to_disk() {
  // FAT1 書き戻し
  for (int i = 0; i < BPB_FATSz16; i++) {
    read_write_disk(&fat[i * (BPB_BytsPerSec / 2)], FAT1_START_SECTOR + i, 1);
  }
  // FAT2 書き戻し（ミラー）
  for (int i = 0; i < BPB_FATSz16; i++) {
    read_write_disk(&fat[i * (BPB_BytsPerSec / 2)], FAT2_START_SECTOR + i, 1);
  }
}

// ルートディレクトリの読み書き
static void read_root_dir_from_disk() {
  for (int i = 0; i < ROOT_DIR_SECTORS; i++) {
    read_write_disk(&root_dir[i * (BPB_BytsPerSec / 32)],
                    ROOT_DIR_START_SECTOR + i, 0);
  }
}
static void write_root_dir_to_disk() {
  for (int i = 0; i < ROOT_DIR_SECTORS; i++) {
    read_write_disk(&root_dir[i * (BPB_BytsPerSec / 32)],
                    ROOT_DIR_START_SECTOR + i, 1);
  }
}

// データ領域の読み書き
static inline uint32_t cluster_to_sector(uint16_t cluster) {
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

void list_root_dir() {
  // 1. ディスクから最新の root_dir を読み込む
  read_root_dir_from_disk();

  kprintf("=== Root Directory ===\n");

  for (int i = 0; i < BPB_RootEntCnt; i++) {
    // 未使用エントリ → ここから先は全部空
    if (root_dir[i].name[0] == 0x00) {
      break;
    }
    // 削除済み
    if (root_dir[i].name[0] == 0xE5) {
      continue;
    }

    // 2. ファイル名（8 + 3）を組み立て
    char name[13];
    int p = 0;

    // name（8文字）
    for (int j = 0; j < 8; j++) {
      if (root_dir[i].name[j] != ' ')
        name[p++] = root_dir[i].name[j];
    }

    // 拡張子
    if (root_dir[i].ext[0] != ' ') {
      name[p++] = '.';
      for (int j = 0; j < 3; j++) {
        if (root_dir[i].ext[j] != ' ')
          name[p++] = root_dir[i].ext[j];
      }
    }

    name[p] = '\0';

    // 3. 表示
    kprintf("%s  size=", name);
    kprintf("%d", (int)root_dir[i].size);
    kprintf("  cluster=");
    kprintf("%d\n", (int)root_dir[i].start_cluster);
  }
}

// ファイル読み込んでRAMに置く
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

// 最初のファイルを読む（未完成）
void concatenate() {
  // 1. 最新の FAT と root_dir を読み込む（FAT を必ず先に）
  read_fat_from_disk();
  read_root_dir_from_disk();

  // 2. 最初の有効エントリを探す
  struct dir_entry *target = NULL;
  for (int i = 0; i < 16; i++) {
    if (root_dir[i].name[0] == 0x00)
      break; // 以降は空
    if (root_dir[i].name[0] == 0xE5)
      continue; // 削除済み
    target = &root_dir[i];
    break;
  }

  if (!target) {
    kprintf("[cat] no file.\n");
    return;
  }

  // サイズ0なら空ファイル
  if (target->size == 0) {
    kprintf("[cat] (empty file)\n");
    return;
  }

  // 3. ファイルサイズぶんのバッファを確保
  uint32_t size = target->size;
  uint8_t buf[size]; // ※簡易実装としてスタック確保

  // 4. read_file() でデータ領域を読む
  if (read_file(target->start_cluster, buf, size) < 0) {
    kprintf("[cat] read error.\n");
    return;
  }

  // 5. ファイル内容をそのまま表示
  kprintf("===== cat: file content =====\n");
  for (uint32_t i = 0; i < size; i++) {
    putchar(buf[i]);
  }
  kprintf("\n===== end =====\n");
}

// サブディレクトリを作る
int make_dir(uint16_t parent_cluster, const char *name) {
  read_fat_from_disk();
  read_root_dir_from_disk();

  // 空きエントリを探す
  int entry_index = -1;
  for (int i = 0; i < BPB_RootEntCnt; i++) {
    if (root_dir[i].name[0] == 0x00 || (uint8_t)root_dir[i].name[0] == 0xE5) {
      entry_index = i;
      break;
    }
  }
  if (entry_index < 0) {
    kprintf("[FAT16] ERROR: Root directory full.\n");
    return -1;
  }

  // 空きクラスタを探す
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

  // ディレクトリエントリの設定
  struct dir_entry *de = &root_dir[entry_index];
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

  // 新ディレクトリクラスタに "." と ".." を書く
  struct dir_entry buf[BPB_BytsPerSec / sizeof(struct dir_entry)];
  memset(buf, 0, sizeof(buf));

  // "."
  memset(buf[0].name, ' ', 8);
  memset(buf[0].ext, ' ', 3);
  buf[0].name[0] = '.';
  buf[0].attr = 0x10;
  buf[0].start_cluster = new_cluster;

  // ".."（ルートなので 0）
  memset(buf[1].name, ' ', 8);
  memset(buf[1].ext, ' ', 3);
  buf[1].name[0] = '.';
  buf[1].name[1] = '.';
  buf[1].attr = 0x10;
  buf[1].start_cluster = parent_cluster;

  // 書き戻し
  write_cluster(new_cluster, buf);
  write_fat_to_disk();
  write_root_dir_to_disk();

  kprintf("[FAT16] Directory created: %s (cluster %d)\n", name, new_cluster);
  return 0;
}