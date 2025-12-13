/*
FAT16
File Allocation Table（16bitでクラスタ番号を管理する）

ブートセクタ（FSの情報）
FAT1（各クラスタの状態、空き・使用中・次のクラスタ）
ルートディレクトリ（ファイル名、拡張子、属性etc）
データ領域（実際のファイルデータ）

からなる。
*/

#define SECTOR_SIZE 512
#define CLUSTER_SIZE 1
#define FAT_ENTRY_NUM 4096

uint16_t fat[FAT_ENTRY_NUM];
struct dir_entry {
  char name[8];
  char ext[3];
  uint8_t attr;
  uint16_t start_cluster;
  uint32_t size;
};

struct dir_entry root_dir[16];

// File System
uint32_t cluster_to_sector(uint16_t cluster) {
  uint32_t data_start =
      1 + 2 * FAT_ENTRY_NUM * 2 / SECTOR_SIZE + sizeof(root_dir) / SECTOR_SIZE;
  return data_start + (cluster - 2) * CLUSTER_SIZE;
}

void read_cluster(uint16_t cluster, void *buf) {
  read_write_disk(buf, cluster_to_sector(cluster), false);
}

void write_cluster(uint16_t cluster, void *buf) {
  read_write_disk(buf, cluster_to_sector(cluster), true);
}

// touchコマンドの元となるcreate_file関数の実装
int create_file(const char *name, uint32_t size) {
  // 空きディレクトリエントリを探す
  int dir_index = -1;
  for (int i = 0; i < 16; i++) {
    if (root_dir[i].name[0] == 0) {
      dir_index = i;
      break;
    }
  }
  if (dir_index == -1)
    return -1;

  // 空きクラスタを探す
  int cluster = -1;
  for (int i = 2; i < FAT_ENTRY_NUM; i++) {
    if (fat[i] == 0) {
      cluster = i;
      break;
    }
  }
  if (cluster == -1)
    return -1;

  // ディレクトリエントリ設定
  strncpy(root_dir[dir_index].name, name, 8);
  root_dir[dir_index].ext[0] = '\0';
  root_dir[dir_index].start_cluster = cluster;
  root_dir[dir_index].size = size;

  // FAT更新
  fat[cluster] = 0xFFFF; // EOF

  // ディスクに書き込み
  write_cluster(cluster, calloc(1, CLUSTER_SIZE * SECTOR_SIZE));
  return 0;
}

/*
問題１
ファイル名を引数nameで指定するところで、
strncpy(root_dir[dir_index].name, name, 8);
strnpy実装してない。
strcpy(root_dir[dir_index].name, name);
で置き換えたい。
*/

char *strcpy(char *dst, const char *src) {
  char *d = dst;
  while (*src)
    *d++ = *src++;
  *d = '\0';
  return dst;
}
/*
自作strcpyは、終端\0までコピーするもの。
FATのname[8]は、8バイト固定長で、余りバイトは0埋めする必要があるのでダメ。
長い名前でバッファオーバーフローできちゃう。

自作strcpyは、メモリ上にある既存の何かを、特定のメモリ領域にコピーするもの。
今回のように、a0レジスタ(∵create_fileの第一引数であるファイル名)の値を
メモリ上の【root_dir配列のroot_index番目の要素が指すテーブルのnameフィールド】に
コピーしたい場合は、strcpyは不適切。
*/
memset(name_field, ' ', 8);
memcpy(name_field, src, min(strlen(src), 8));
/*
みたいにすれば良いが、
引数nameが8バイト以下なら残りは0埋め、
8バイト以上ならfor文で回す？
のような追加処理が必要。↓
*/
void copy_name_to_fat16_field(char *name_field, const char *src) {
  size_t len = strlen(src);
  if (len > 8)
    len = 8; // 8 バイトに切り詰め
  for (size_t i = 0; i < 8; i++) {
    if (i < len)
      name_field[i] = src[i]; // コピー
    else
      name_field[i] = 0; // 残りは 0 埋め
  }
}
/*
しかしこれだと、8バイト以上は切り捨てられてしまう。
8バイト以上のファイル名にも対応するために、
dir_entry構造体のname属性を可変長にする。
*/
struct dir_entry {
  char *name;
  char ext[3];
  uint8_t attr;
  uint16_t start_cluster;
  uint32_t size;
};
void copy_name_dynamic(char **name_field, const char *src) {
  // コピー元が NULL 終端かどうかを 8 バイトずつ確認
  int len = 0;
  while (len < 256) { // 最大長 256 に制限（適宜変更）
    if (src[len] == 0)
      break;
    len++;
  }

  // 動的確保
  char *buf =
      alloc_pages((len + 1 + PAGE_SIZE - 1) / PAGE_SIZE); // ページ単位確保
  for (int i = 0; i <= len; i++) {
    buf[i] = src[i]; // 0 終端までコピー
  }

  *name_field = buf;
}
int create_file(const char *name, uint32_t size) {
  int dir_index = -1;
  for (int i = 0; i < 16; i++) {
    if (root_dir[i].name == NULL) {
      dir_index = i;
      break;
    }
  }
  if (dir_index == -1)
    return -1;

  root_dir[dir_index].start_cluster = find_free_cluster();
  root_dir[dir_index].size = size;
  copy_name_dynamic(&root_dir[dir_index].name, name);

  // ファイルデータを初期化
  write_cluster(root_dir[dir_index].start_cluster,
                alloc_pages(CLUSTER_SIZE * SECTOR_SIZE));

  return 0;
}
/*
のように、ファイル名を示すメモリ領域を動的確保する。256バイトは超えない。

copy_name_dynamic の第一引数で、&をとっているのはどうして？
*/
/*
問題２
RAM上にバッファを作ってそれをディスクに書き込むとき、
write_cluster(cluster, calloc(1, CLUSTER_SIZE * SECTOR_SIZE));
calloc実装してないので
write_cluster(cluster, alloc_pages(CLUSTER_SIZE * SECTOR_SIZE));
で置き換えたい。
*/
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
alloc_pages関数は、free_ram領域の空いてるところを、
memsetでn*4096バイト分埋めて、割り当て開始アドレスpaddrを返す。

今、【write_clusterの第二引数*buf】は、
cluster分のデータを含むメモリ領域を指すポインタなので、
Identity Mapping(paddr=vaddr)ならば、
alloc_pagesの戻り値paddrを代入して良い。

そのpaddrアドレスの中身の値は、
0で初期化されている。
write_cluster(cluster, buf) で書き込む際、
buf の内容がそのままディスク上にコピーされます。

違いは、ファイルの中身が、
自作alloc_pages: free_ram領域
calloc: ヒープ領域
に置かれるか
*/
paddr_t p = alloc_pages(CLUSTER_SIZE * SECTOR_SIZE);
write_cluster(cluster, (void *)p);
/*
pは int 型（∵ alloc_pagesの戻り値 return paddr;）のアドレスなので、
write_cluster(cluster, *p);じゃダメなの？
(void *)p・・・
void * は型情報なしポインタ。
pの型を無視してpをvoidポインタにキャストしている。

alloc_pagesはint型でアドレスが返ってくるので、
そのアドレスの中身を知りたいなら
void *
にキャスト変換する必要がある。
*/