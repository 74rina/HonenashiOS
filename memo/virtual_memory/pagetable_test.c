/*
全体のアーキテクチャ
heap (kmalloc) <- vmm (map_page) <- pmm (alloc_pages)


・プロセスを新規作成したとき、
そのページテーブルがfree_ram（物理メモリ上）に置かれる。
（↑alloc_pagesで確保する）

・実際、物理メモリ上でページテーブルの領域はこんな感じ

[table1]  ← 1段目（ページディレクトリ）
   ├── entry[ vpn1 ] → table0 の物理アドレス
   ├── entry[  ... ]
   └── ...

[table0]  ← 2段目（ページテーブル）
   ├── entry[ vpn0 ] → あるページの物理アドレス
   ├── entry[ ... ]
   └── ...

・物理メモリにページテーブルを作るのはmap_page()
１、1段目の該当エントリをチェック
  無ければ alloc_pages(1) で 新しいページ表を確保して追加
２、2段目の表を取り出して、該当エントリに物理ページを登録


・仮想アドレス空間をちゃんと作った場合（リンカスクリプト２種類）
create_process()の戻り値のプロセス管理構造体procsの中に
page_table
があるが、この戻り値がsatpレジスタにロードされる。

→CPUが現在のページテーブルの位置を知るのはsatpレジスタ
→satpレジスタにロードされるのは物理メモリ上のページテーブル領域のアドレス
*/

#include "../../common.c"
#include "../../common.h"
#include "../../kernel.h"

/*
CPUはメモリアクセスする際に、
VPN[1]とVPN[0]で対応するページテーブルのエントリを特定し、
そのエントリの物理アドレスとoffsetを足し合わせることで、
最終的にアクセスする物理アドレスを計算します。
*/

// 必要なマクロ
#define SATP_SV32 (1u << 31)
#define PAGE_V (1 << 0) // 有効化ビット
#define PAGE_R (1 << 1) // 読み込み可能
#define PAGE_W (1 << 2) // 書き込み可能
#define PAGE_X (1 << 3) // 実行可能
#define PAGE_U (1 << 4) // ユーザーモードでアクセス可能

/*
1 << n　の解釈
1は2進数で　0000....00001
<<n　は左にnビットシフト
→n番目のビットだけが1になる

重要！！！
1 << 0  → 0000...0001  → 10進数: 1
1 << 1  → 0000...0010  → 10進数: 2
1 << 2  → 0000...0100  → 10進数: 4
1 << 3  → 0000...1000  → 10進数: 8
1 << 31 → 1000...0000  → 32bit 環境で 2^31
*/

// 「仮想メモリ→物理メモリ」の1ページのマッピング
void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags) {
  if (!is_aligned(vaddr, PAGE_SIZE))
    PANIC("unaligned vaddr %x", vaddr);

  if (!is_aligned(paddr, PAGE_SIZE))
    PANIC("unaligned paddr %x", paddr);

  uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
  if ((table1[vpn1] & PAGE_V) == 0) {
    // 1段目のページテーブルが存在しないので作成する
    uint32_t pt_paddr = alloc_pages(1);
    table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
  }

  // 2段目のページテーブルにエントリを追加する
  uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
  uint32_t *table0 = (uint32_t *)((table1[vpn1] >> 10) * PAGE_SIZE);
  table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
}

/*
仮想アドレスを2段階ページテーブルで管理とは
31           22 21          12 11         0
+-------------+------------+-------------+
|   VPN1      |   VPN0     |  オフセット |
+-------------+------------+-------------+
VPN1 → 1段目テーブルのインデックス（0〜1023）
VPN0 → 2段目テーブルのインデックス（0〜1023）
オフセット → ページ内の位置（0〜4095）

各々のページテーブルの中身は
31        10 9 8 7 6 5 4 3 2 1 0
+----------+---------------------+
|   PPN    |    フラグ(PAGE_Vなど) |
+----------+---------------------+

table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
1段目テーブルが「2段目のテーブルの物理アドレス」を指しているので、
2段目テーブル（table0）を取り出して、VPN0番目に実際の 物理ページ
をマッピングします。
↑
このとき、2段目テーブルのインデックスは
uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
で計算。
物理アドレスの取得は
uint32_t *table0 = (uint32_t *)((table1[vpn1] >> 10) * PAGE_SIZE);
で行なっている。
*/

// 具体例
/*
void proc_a_entry (void) { switch_context( paddr_a, paddr_b )};
void proc_b_entry (void) { switch_context( paddr_b, paddr_a )};
proc_a = create_process(*proc_a_entry);
proc_b = create_process(*proc_b_entry);
proc_a();
としたとき、

proc_a の VA = 0x40000000 → PA = 0x10000
table1[0] → table0 の物理アドレス 0x31000
table0[0] → PPN = 0x10 → PA = 0x10000

proc_b の VA = 0x40000000 → PA = 0x20000
table1[0] → table0 の物理アドレス 0x33000
table0[0] → PPN = 0x20 → PA = 0x20000

ここで、proc_a();が呼ばれると、
MMUがVA→PAを見て、正しいproc_aのコード領域にジャンプする
*/

// 補足
/*
3句構造のfor文＝for(初期化；継続条件；更新)
for (paddr_t paddr = (paddr_t)__kernel_base;
     paddr < (paddr_t)__free_ram_end;
     paddr += PAGE_SIZE)
*/

/*
Identity Mapping
＝カーネル用の仮想アドレスと物理アドレスが同じ領域のマッピング
・物理用リンカスクリプトで十分。）
・ブート時やカーネル初期化時に便利で、安全にメモリにアクセスできるようにするために使われる。
今回もそう↓
map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);

仮想空間に移行するなら、
リンカスクリプトが2種類必要（仮想、物理）
*/

/*
va→paの変換はもっと低レイヤー？

create_process でやること
・スタック領域の初期化
・ページテーブル用領域を確保
・map_pageで、実際にページテーブル内にプロセスのエントリ（コード領域のはじめのアドレス）を書き込む

プロセス切り替え時yield内でsatpレジスタにproc->page_tableの値をセット
__asm__ __volatile__(
      "sfence.vma\n"
      "csrw satp, %[satp]\n"
      "sfence.vma\n"
      "csrw sscratch, %[sscratch]\n"
      :
      : [satp] "r"(SATP_SV32 | ((uint32_t)next->page_table / PAGE_SIZE)),
        [sscratch] "r"((uint32_t)&next->stack[sizeof(next->stack)]));

プロセス切り替えの流れ（仮想↔︎物理にフォーカス）
１、カーネルが新しいプロセスの proc->page_table の物理アドレスを satp に書き込む
２、sfence.vma 命令で TLB をフラッシュ
３、CPU が仮想アドレスで命令やデータをアクセスするとき：
    MMU が satp に書かれたページテーブルの物理アドレスを辿る（仕様）
    ページテーブルから対応する PPN を取得
    仮想アドレス → 物理アドレス変換が完了
*/