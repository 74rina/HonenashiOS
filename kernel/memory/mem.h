#pragma once
#include "../../common/common.h"

#define SATP_SV32 (1u << 31)
#define PAGE_V (1 << 0) // 有効化ビット
#define PAGE_R (1 << 1) // 読み込み可能
#define PAGE_W (1 << 2) // 書き込み可能
#define PAGE_X (1 << 3) // 実行可能
#define PAGE_U (1 << 4) // ユーザーモードでアクセス可能

#define USER_BASE 0x1000000

typedef struct {
  paddr_t start;     // 管理対象の物理メモリ開始位置
  size_t page_count; // 物理ページ数
  uint8_t *bitmap;   // 物理ページ使用状況
} pmm_t;

paddr_t alloc_pages(uint32_t n);
void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags);
