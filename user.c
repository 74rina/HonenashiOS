#include "user.h"
#include "filesystem/fat16.h"
#include "kernel.h"
#include "lib/common.h"

extern char __stack_top[];

__attribute__((section(".text.start"))) __attribute__((naked)) void
start(void) {
  __asm__ __volatile__("mv sp, %[stack_top]\n"
                       "call main\n"
                       "call exit\n" ::[stack_top] "r"(__stack_top));
}

int syscall(int sysno, int arg0, int arg1, int arg2) {
  register int a0 __asm__("a0") = arg0;
  register int a1 __asm__("a1") = arg1;
  register int a2 __asm__("a2") = arg2;
  register int a3 __asm__("a3") = sysno;

  __asm__ __volatile__("ecall"
                       : "=r"(a0)
                       : "r"(a0), "r"(a1), "r"(a2), "r"(a3)
                       : "memory");

  return a0;
}

void putchar(char ch) { syscall(SYS_PUTCHAR, ch, 0, 0); }

int getchar(void) { syscall(SYS_GETCHAR, 0, 0, 0); }

__attribute__((noreturn)) void exit(void) {
  syscall(SYS_EXIT, 0, 0, 0);
  for (;;)
    ; // 念のため
}

int create_file(const char *name, const uint8_t *data, uint32_t size) {
  syscall(SYS_CREATE_FILE, 0, 0, 0);
}

void list_root_dir() { syscall(SYS_LIST_FILE, 0, 0, 0); }

void concatenate() { syscall(SYS_CONCATENATE, 0, 0, 0); }