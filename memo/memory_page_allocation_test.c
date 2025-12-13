#include "../common.h"
#include "../kernel.h"

#define PAGE_SIZE 4096

extern char __free_ram[], __free_ram_end[];

paddr_t alloc_pages(uint32_t n) {
  static paddr_t next_paddr = (paddr_t)__free_ram;
  paddr_t paddr = next_paddr;
  next_paddr += n * PAGE_SIZE;
  if (next_paddr > (paddr_t)__free_ram_end)
    PANIC("out of memory");
  memset((void *)paddr, 0, n * PAGE_SIZE);
  return paddr;
}

/*
__free_ram と __free_ram_end はリンカスクリプトで定義された、
RAM上の使用可能領域の先頭と末尾を示すシンボルです。
*/

/*
static paddr_t next_paddr = (paddr_t)__free_ram;
paddr_t paddr = next_paddr;
next_paddr += n * PAGE_SIZE;

次の空き領域のアドレスを next_paddr 変数で保持
それを、今回割り当てる paddr 変数にコピー
next_paddr を n*page_size分だけ進めて、次回呼び出しに備える
*/

/*
memset((void *)paddr, 0, n * PAGE_SIZE);
割り当てたページを0で初期化する←未定義を防ぐCの規約
*/

/*
return paddr;
で、割り当て開始アドレスを返す ← これは物理アドレス！
*/

/*
malloc, calloc, free も同様にメモリ確保だが、
これらは全て仮想メモリ上で操作されるもの。
物理メモリとの対応（マッピング）は、OSの仮想メモリ管理機構が担当

alloc_pagesは物理メモリの直接割り当て。
OS本体（カーネル）の処理で用いられる。ユーザー空間では使わない
↑例えば？
仮想メモリ管理、dmaバッファ←？？？
*/