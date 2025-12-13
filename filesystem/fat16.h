#pragma once

#include "../kernel.h"

// ブートセクタ
#define BPB_BytsPerSec 512
#define BPB_SecPerClus 1
#define BPB_RsvdSecCnt 1
#define BPB_NumFATs 2
#define BPB_RootEntCnt 512
#define BPB_FATSz16 32
#define BPB_TotSec16 32768

// FAT領域
#define FAT1_START_SECTOR BPB_RsvdSecCnt
#define FAT2_START_SECTOR (FAT1_START_SECTOR + BPB_FATSz16)
#define FAT_ENTRY_NUM ((BPB_FATSz16 * BPB_BytsPerSec) / 2)

// ルートディレクトリ
#define ROOT_DIR_START_SECTOR (BPB_RsvdSecCnt + BPB_NumFATs * BPB_FATSz16)
#define ROOT_DIR_SECTORS                                                       \
  ((BPB_RootEntCnt * 32 + BPB_BytsPerSec - 1) / BPB_BytsPerSec)

// データ領域
#define DATA_START_SECTOR (ROOT_DIR_START_SECTOR + ROOT_DIR_SECTORS)

extern uint16_t fat[FAT_ENTRY_NUM];
#pragma pack(push, 1)
struct dir_entry {
  char name[8];
  char ext[3];
  uint8_t attr;
  uint8_t reserved;
  uint8_t creation_time_tenths;
  uint16_t creation_time;
  uint16_t creation_date;
  uint16_t last_access_date;
  uint16_t high_cluster;
  uint16_t last_write_time;
  uint16_t last_write_date;
  uint16_t start_cluster;
  uint32_t size;
};
#pragma pack(pop)

extern struct dir_entry root_dir[BPB_RootEntCnt];

void init_fat16_disk();
void read_cluster(uint16_t cluster, void *buf);
void write_cluster(uint16_t cluster, void *buf);
void copy_name_dynamic(char **name_field, const char *src);
int create_file(const char *name, const uint8_t *data, uint32_t size);
int read_file(uint16_t start_cluster, uint8_t *buf, uint32_t size);
void list_root_dir();
void concatenate();