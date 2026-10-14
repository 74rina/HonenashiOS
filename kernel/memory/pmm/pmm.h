#pragma once
#include "../../../common/common.h"

typedef struct {
  paddr_t start;     // 管理対象の物理メモリ開始位置
  size_t page_count; // 物理ページ数
  uint8_t *bitmap;   // 物理ページ使用状況
} pmm_t;

paddr_t alloc_pages(uint32_t n);
void free_pages(paddr_t paddr, uint32_t n);
