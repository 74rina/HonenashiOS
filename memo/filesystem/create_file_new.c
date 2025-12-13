/*
1. ディスクから FAT 領域を RAM の fat[] に読み込む
2. ディスクから root_dir を RAM の root_dir[] に読み込む
3. root_dir[] 内で空きエントリを探索する
4. FATの空きクラスタを探す
5. ファイル/拡張子名、最初のクラスタ番号をセット
6. FAT チェーンを更新？
5. root_dir[], FAT をディスクに書き戻す  ←★これが最重要
*/

// RAM上のFATとルートディレクトリ
uint16_t fat[FAT_ENTRY_NUM];
struct dir_entry root_dir[16]; // ここでは16個だけ利用する簡易版

// FAT1/FAT2 の読み書き
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

// ------------------------------------------------------------
// create_file(name, size)
// ------------------------------------------------------------
int create_file(const char *name, uint32_t size) {
  printf("[FAT16] create_file: %s (%u bytes)\n", name, size);

  // 1. FAT 読み込み
  read_fat_from_disk();

  // 2. root directory 読み込み
  read_root_dir_from_disk();

  // 3. root_dir から空きエントリ探す
  int entry_index = -1;
  for (int i = 0; i < 16; i++) {
    if (root_dir[i].name[0] == 0x00 || root_dir[i].name[0] == 0xE5) {
      entry_index = i;
      break;
    }
  }
  if (entry_index < 0) {
    printf("[FAT16] ERROR: root directory full.\n");
    return -1;
  }

  // 4. FAT から空きクラスタを探す
  int free_cluster = -1;
  for (int i = 2; i < FAT_ENTRY_NUM; i++) {
    if (fat[i] == 0x0000) {
      free_cluster = i;
      break;
    }
  }
  if (free_cluster < 0) {
    printf("[FAT16] ERROR: no free FAT cluster.\n");
    return -1;
  }

  // 5. root_dir エントリの作成
  struct dir_entry *de = &root_dir[entry_index];

  // ファイル名8 + 拡張子3 (簡易実装: '.'無視)
  memset(de->name, ' ', 8);
  memset(de->ext, ' ', 3);

  int n = 0;
  while (n < 8 && name[n] && name[n] != '.') {
    de->name[n] = name[n];
    n++;
  }
  // 拡張子
  if (name[n] == '.') {
    n++;
    for (int e = 0; e < 3 && name[n + e]; e++) {
      de->ext[e] = name[n + e];
    }
  }

  de->start_cluster = free_cluster;
  de->size = size;

  // 6. FAT を更新
  fat[free_cluster] = 0xFFFF; // end of cluster chain

  // 7. FAT をディスクへ書き戻す（FAT1 & FAT2）
  write_fat_to_disk();

  // 8. root_dir を書き戻す
  write_root_dir_to_disk();

  printf("[FAT16] File created: %s at entry %d, cluster %d\n", name,
         entry_index, free_cluster);

  return 0;
}
