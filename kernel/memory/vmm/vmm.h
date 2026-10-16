#pragma once
#include "../../../common/common.h"

// Sv32 flags
#define SATP_SV32 (1u << 31)
#define PAGE_V (1 << 0) // 有効化ビット
#define PAGE_R (1 << 1) // 読み込み可能
#define PAGE_W (1 << 2) // 書き込み可能
#define PAGE_X (1 << 3) // 実行可能
#define PAGE_U (1 << 4) // ユーザーモードでアクセス可能

#define USER_BASE 0x1000000

typedef uint32_t pte_t;

typedef struct {
  pte_t entries[1024];
} page_table_t;

void map_page(page_table_t *root, vaddr_t vaddr, paddr_t paddr, uint32_t flags);
void unmap_page(page_table_t *root, vaddr_t vaddr);
