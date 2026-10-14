#include "../kernel.h"
#include "mem.h"

extern char __free_ram[], __free_ram_end[];

static pmm_t pmm;
static bool pmm_initialized;

static bool pmm_is_used(size_t index) {
  return (pmm.bitmap[index / 8] & (1 << (index % 8))) != 0;
}

static void pmm_set_used(size_t index) {
  // 1byteで8ページ分を管理し、対象ページのbitを1にする
  pmm.bitmap[index / 8] |= 1 << (index % 8);
}

static void pmm_init(void) {
  size_t total_pages =
      ((paddr_t)__free_ram_end - (paddr_t)__free_ram) / PAGE_SIZE;
  size_t bitmap_bytes = align_up(total_pages, 8) / 8;
  size_t bitmap_pages = align_up(bitmap_bytes, PAGE_SIZE) / PAGE_SIZE;

  pmm.bitmap = (uint8_t *)__free_ram;
  pmm.start = (paddr_t)__free_ram + bitmap_pages * PAGE_SIZE;
  pmm.page_count = ((paddr_t)__free_ram_end - pmm.start) / PAGE_SIZE;

  memset(pmm.bitmap, 0, bitmap_bytes);
  pmm_initialized = true;
}

paddr_t alloc_pages(uint32_t n) {
  if (!pmm_initialized)
    pmm_init();

  if (n == 0)
    PANIC("alloc_pages: n must be greater than 0");

  size_t run_start = 0;
  size_t run_length = 0;

  for (size_t i = 0; i < pmm.page_count; i++) {
    if (pmm_is_used(i)) {
      run_length = 0;
      continue;
    }

    if (run_length == 0)
      run_start = i;

    run_length++;
    if (run_length < n)
      continue;

    for (size_t j = 0; j < n; j++)
      pmm_set_used(run_start + j);

    paddr_t paddr = pmm.start + run_start * PAGE_SIZE;
    memset((void *)paddr, 0, n * PAGE_SIZE);
    return paddr;
  }

  PANIC("out of memory!!!!!");
}
