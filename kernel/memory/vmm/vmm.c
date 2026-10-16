#include "vmm.h"
#include "../../kernel.h"

void map_page(page_table_t *root, vaddr_t vaddr, paddr_t paddr,
              uint32_t flags) {
  if (!root)
    PANIC("map_page: root page table is null");

  if (!is_aligned(vaddr, PAGE_SIZE))
    PANIC("map_page: unaligned vaddr %x", vaddr);

  if (!is_aligned(paddr, PAGE_SIZE))
    PANIC("map_page: unaligned paddr %x", paddr);

  uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
  uint32_t vpn0 = (vaddr >> 12) & 0x3ff;

  // 1層目のPTEを取得する
  pte_t pte1 = root->entries[vpn1];

  if ((pte1 & PAGE_V) == 0) {
    paddr_t new_table0_paddr = alloc_pages(1);
    root->entries[vpn1] = ((new_table0_paddr >> 12) << 10) | PAGE_V;
    pte1 = root->entries[vpn1];
  }

  // FLAG: V=1 && (R=1 || X=1) - 物理ページを指すリーフPTE
  if (pte1 & (PAGE_R | PAGE_W | PAGE_X))
    PANIC("map_page: level-1 PTE is a leaf");

  // FLAG: V=1 && R=W=X=0 - 次階層のPTE
  // 1層目のPTEから2層目のページテーブルを取得する
  paddr_t table0_paddr = (pte1 >> 10) << 12;

  // Identity Mapping
  page_table_t *table0 = (page_table_t *)table0_paddr;

  table0->entries[vpn0] = ((paddr >> 12) << 10) | flags | PAGE_V;

  // 設定したPTEが指す物理ページを確認する
  pte_t pte0 = table0->entries[vpn0];
  paddr_t mapped_paddr = (pte0 >> 10) << 12;
  if (mapped_paddr != paddr)
    PANIC("map_page: mapped paddr mismatch");
}

void unmap_page(page_table_t *root, vaddr_t vaddr) {
  if (!root)
    PANIC("unmap_page: root page table is null");

  if (!is_aligned(vaddr, PAGE_SIZE))
    PANIC("unmap_page: unaligned vaddr %x", vaddr);

  uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
  uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
  pte_t pte1 = root->entries[vpn1];

  if ((pte1 & PAGE_V) == 0)
    PANIC("unmap_page: page is not mapped");

  if (pte1 & (PAGE_R | PAGE_W | PAGE_X))
    PANIC("unmap_page: level-1 PTE is a leaf");

  // 2層目のページテーブルにアクセス
  // Identity Mapping
  paddr_t table0_paddr = (pte1 >> 10) << 12;
  page_table_t *table0 = (page_table_t *)table0_paddr;
  pte_t pte0 = table0->entries[vpn0];

  if ((pte0 & PAGE_V) == 0)
    PANIC("unmap_page: page is not mapped");

  if (!(pte0 & (PAGE_R | PAGE_X)) || ((pte0 & PAGE_W) && !(pte0 & PAGE_R)))
    PANIC("unmap_page: invalid level-0 leaf PTE");

  // 物理ページは解放せず、対応付けだけを削除する。
  table0->entries[vpn0] = 0;
  __asm__ __volatile__("sfence.vma" ::: "memory");
}
