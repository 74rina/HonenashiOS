/*
ラウンドロビンスケジューリング

プロセス数が増えると、
context_switchで切り替え後のプロセスを指定するのがむずい
→どのプロセスに切り替えるべきかも決めるのがスケジューラ yield();
*/
#include "./context_switch_test.c"
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef uint32_t vaddr_t;
#define PROCS_MAX 8
#define PROC_UNUSED 0
#define PROC_RUNNABLE 1

struct process procs[PROCS_MAX];
struct process *current_proc; // 現在実行中のプロセス
struct process *idle_proc;    // アイドルプロセス

void yield(void) {
  // 実行可能なプロセスを探す
  struct process *next = idle_proc;
  for (int i = 0; i < PROCS_MAX; i++) {
    struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
    if (proc->state == PROC_RUNNABLE && proc->pid > 0) {
      next = proc;
      break;
    }
  }

  // 現在実行中のプロセス以外に、実行可能なプロセスがない。戻って処理を続行する
  if (next == current_proc)
    return;

  // コンテキストスイッチ
  struct process *prev = current_proc;
  current_proc = next;
  switch_context(&prev->sp, &next->sp);
}

/*
プロセス管理用の配列procs[]には、
PROCS_MAX個のprocess構造体のインスタンス
が格納されている

for (int i = 0; i < PROCS_MAX; i++) {
    struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
    if (proc->state == PROC_RUNNABLE && proc->pid > 0) {
      next = proc;
      break;
    }
  }

で、その配列からRUNNABLEのやつを探索する
→適当なプロセスを見つけたら、next=proc;にセット。
→見つからなければ、nextはidle_procのまま。
*/
/*
kernel_mainの最初で、idle_procはNULLとして
create_process（スタック領域を確保）している。
NULL＝実行すべき関数を持たない
*/

/*
yieldスケジューラのまとめ
１、idle_proc は CPU がヒマなとき用の特別なプロセス。
２、yield はまずアイドルプロセスを next にセット。
３、通常プロセスがあれば next に切り替え、なければアイドルを走らせる。
*/

/*
csrw命令 = CSR(Control and Status Register) の特殊なレジスタを書き換える
*/