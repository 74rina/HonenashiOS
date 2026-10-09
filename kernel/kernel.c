#include "kernel.h"
#include "../common/common.h"
#include "drivers/virtio.h"
#include "fs/fat16.h"

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef uint32_t size_t;

extern char __kernel_base[];
extern char __bss[], __bss_end[], __stack_top[];
extern char __free_ram[], __free_ram_end[];
extern char _binary_user_shell_bin_start[], _binary_user_shell_bin_size[];

// Simple Memory Allocaltion
paddr_t alloc_pages(uint32_t n) {
  static paddr_t next_paddr = (paddr_t)__free_ram;
  paddr_t paddr = next_paddr;
  next_paddr += n * PAGE_SIZE;

  if (next_paddr > (paddr_t)__free_ram_end) {
    PANIC("out of memory!!!!!");
  }

  memset((void *)paddr, 0, n * PAGE_SIZE);
  return paddr;
}

// Virtual Memory
void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags) {
  if (!is_aligned(vaddr, PAGE_SIZE))
    PANIC("unaligned vaddr %x", vaddr);

  if (!is_aligned(paddr, PAGE_SIZE))
    PANIC("unaligned paddr %x", paddr);

  uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
  if ((table1[vpn1] & PAGE_V) == 0) {
    // 1段目のページテーブル作成
    uint32_t pt_paddr = alloc_pages(1);
    table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
  }

  // 2段目のページテーブルにエントリを追加する
  uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
  uint32_t *table0 = (uint32_t *)((table1[vpn1] >> 10) * PAGE_SIZE);
  table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
}

// 割り込み処理 Interrupt
// sscratch レジスタには、カーネルエントリ用のスタックトップが入っている
// 1. 現在 sp と sscratch の値を swap
// 2. ↑割り込み用 sp の領域にスタックコンテキストを退避させる
// 3. handle_trap を呼び出す
// 4. 割り込み前のスタックコンテキストに戻す（lw）
__attribute__((naked)) __attribute__((aligned(4))) void kernel_entry(void) {
  __asm__ __volatile__("csrrw sp, sscratch, sp\n"
                       "addi sp, sp, -4 * 31\n"
                       "sw ra,  4 * 0(sp)\n"
                       "sw gp,  4 * 1(sp)\n"
                       "sw tp,  4 * 2(sp)\n"
                       "sw t0,  4 * 3(sp)\n"
                       "sw t1,  4 * 4(sp)\n"
                       "sw t2,  4 * 5(sp)\n"
                       "sw t3,  4 * 6(sp)\n"
                       "sw t4,  4 * 7(sp)\n"
                       "sw t5,  4 * 8(sp)\n"
                       "sw t6,  4 * 9(sp)\n"
                       "sw a0,  4 * 10(sp)\n"
                       "sw a1,  4 * 11(sp)\n"
                       "sw a2,  4 * 12(sp)\n"
                       "sw a3,  4 * 13(sp)\n"
                       "sw a4,  4 * 14(sp)\n"
                       "sw a5,  4 * 15(sp)\n"
                       "sw a6,  4 * 16(sp)\n"
                       "sw a7,  4 * 17(sp)\n"
                       "sw s0,  4 * 18(sp)\n"
                       "sw s1,  4 * 19(sp)\n"
                       "sw s2,  4 * 20(sp)\n"
                       "sw s3,  4 * 21(sp)\n"
                       "sw s4,  4 * 22(sp)\n"
                       "sw s5,  4 * 23(sp)\n"
                       "sw s6,  4 * 24(sp)\n"
                       "sw s7,  4 * 25(sp)\n"
                       "sw s8,  4 * 26(sp)\n"
                       "sw s9,  4 * 27(sp)\n"
                       "sw s10, 4 * 28(sp)\n"
                       "sw s11, 4 * 29(sp)\n"

                       "csrr a0, sscratch\n"
                       "sw a0, 4 * 30(sp)\n"

                       "addi a0, sp, 4 * 31\n"
                       "csrw sscratch, a0\n"

                       "mv a0, sp\n"
                       "call handle_trap\n"

                       "lw ra,  4 * 0(sp)\n"
                       "lw gp,  4 * 1(sp)\n"
                       "lw tp,  4 * 2(sp)\n"
                       "lw t0,  4 * 3(sp)\n"
                       "lw t1,  4 * 4(sp)\n"
                       "lw t2,  4 * 5(sp)\n"
                       "lw t3,  4 * 6(sp)\n"
                       "lw t4,  4 * 7(sp)\n"
                       "lw t5,  4 * 8(sp)\n"
                       "lw t6,  4 * 9(sp)\n"
                       "lw a0,  4 * 10(sp)\n"
                       "lw a1,  4 * 11(sp)\n"
                       "lw a2,  4 * 12(sp)\n"
                       "lw a3,  4 * 13(sp)\n"
                       "lw a4,  4 * 14(sp)\n"
                       "lw a5,  4 * 15(sp)\n"
                       "lw a6,  4 * 16(sp)\n"
                       "lw a7,  4 * 17(sp)\n"
                       "lw s0,  4 * 18(sp)\n"
                       "lw s1,  4 * 19(sp)\n"
                       "lw s2,  4 * 20(sp)\n"
                       "lw s3,  4 * 21(sp)\n"
                       "lw s4,  4 * 22(sp)\n"
                       "lw s5,  4 * 23(sp)\n"
                       "lw s6,  4 * 24(sp)\n"
                       "lw s7,  4 * 25(sp)\n"
                       "lw s8,  4 * 26(sp)\n"
                       "lw s9,  4 * 27(sp)\n"
                       "lw s10, 4 * 28(sp)\n"
                       "lw s11, 4 * 29(sp)\n"
                       "lw sp,  4 * 30(sp)\n"
                       "sret\n");
}

void handle_trap(struct trap_frame *f) {
  uint32_t scause = READ_CSR(scause);
  uint32_t stval = READ_CSR(stval);
  uint32_t user_pc = READ_CSR(sepc);

  if (scause == SCAUSE_ECALL) {
    handle_syscall(f);
    user_pc += 4;
  } else {
    PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval,
          user_pc);
  }

  WRITE_CSR(sepc, user_pc);
}

// process_switch_test
struct process *proc_a;
struct process *proc_b;

void delay(void) {
  for (int i = 0; i < 30000000; i++)
    __asm__ __volatile__("nop"); // 何もしない命令
}

void proc_a_entry(void) {
  kprintf("starting process A\n");
  while (1) {
    putchar('A');
    yield();
    delay();
  }
}

void proc_b_entry(void) {
  kprintf("starting process B\n");
  while (1) {
    putchar('B');
    yield();
    delay();
  }
}

// Boot
__attribute__((section(".text.boot"))) __attribute__((naked)) void boot(void) {
  __asm__ __volatile__("mv sp, %[stack_top]\n"
                       "j kernel_main\n"
                       :
                       : [stack_top] "r"(__stack_top));
}

void kernel_main(void) {
  memset(__bss, 0, (size_t)__bss_end - (size_t)__bss);
  WRITE_CSR(stvec, (uint32_t)kernel_entry);
  virtio_blk_init();
  init_fat16_disk();

  idle_proc = create_process(NULL, 0);
  idle_proc->pid = 0;
  current_proc = idle_proc;

  kprintf("\n\nWelcome to... %s!!!\n", "HonenashiOS ^p^");
  kprintf("1 + 2 = %d, %x\n", 1 + 2, 0x1234abcd);

  paddr_t paddr0 = alloc_pages(2);
  paddr_t paddr1 = alloc_pages(1);
  kprintf("alloc_pages test: paddr0=%x\n", paddr0);
  kprintf("alloc_pages test: paddr1=%x\n", paddr1);

  char buf[SECTOR_SIZE];
  read_write_disk(buf, 0, false);
  kprintf("first sector: %s\n", buf);

  static const uint8_t lorem_txt[] =
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit.\n"
      "This file is created by kernel_main for cat testing.\n";

  create_file("test.txt", "hello", 5);
  create_file("lorem.txt", lorem_txt, sizeof(lorem_txt) - 1);
  make_dir(0, "testdir");
  make_dir(3, "unreachable");
  // current_directory("testdir");

  create_process(_binary_user_shell_bin_start,
                 (size_t)_binary_user_shell_bin_size);
  yield();
  PANIC("shell discontinued");

  for (;;) {
    __asm__ __volatile__("wfi");
  }
}
