#include "../common/common.h"
#include "fs/fat16.h"
#include "kernel.h"

// sbi legacy extension
void handle_syscall(struct trap_frame *f) {
  switch (f->a3) {
  case SYS_PUTCHAR:
    putchar(f->a0);
    break;

  case SYS_GETCHAR:
    while (1) {
      long ch = getchar();
      if (ch >= 0) {
        f->a0 = ch;
        break;
      }
      yield();
    }
    break;

  case SYS_EXIT:
    kprintf("process %d exited\n", current_proc->pid);
    current_proc->state = PROC_EXITED;
    yield();
    PANIC("unreachable");

  case SYS_CREATE_FILE:
    while (1) {
      /*
      long filename = getchar()もどき
      create_file(filename, filenameの長さ);
      yield();
      */
    }
    break;

  case SYS_LIST_FILE:
    list_root_dir();
    yield();
    break;

  case SYS_CONCATENATE:
    concatenate((const char *)f->a0);
    yield();
    break;

  case SYS_PWD:
    print_working_directory();
    yield();
    break;

  default:
    PANIC("unexpected syscall a3=%x\n", f->a3);
  }
}
