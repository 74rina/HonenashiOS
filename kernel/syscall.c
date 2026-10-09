#include "../common/syscall.h"
#include "../common/common.h"
#include "fs/fat16.h"
#include "kernel.h"

#define MAX_SYSCALL_PATH 128

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

  case SYS_LIST_FILE: {
    char dir_path[MAX_SYSCALL_PATH];
    if (strncpy_from_user(dir_path, (const char *)f->a0, sizeof(dir_path)) < 0)
      kprintf("ls: invalid directory path\n");
    else
      list_files(dir_path);
    yield();
    break;
  }

  case SYS_CONCATENATE: {
    char filename[MAX_SYSCALL_PATH];
    if (strncpy_from_user(filename, (const char *)f->a0, sizeof(filename)) < 0)
      kprintf("cat: invalid filename\n");
    else
      concatenate(filename);
    yield();
    break;
  }

  case SYS_PWD:
    print_working_directory();
    yield();
    break;

  case SYS_CD: {
    char path[MAX_SYSCALL_PATH];
    if (strncpy_from_user(path, (const char *)f->a0, sizeof(path)) < 0)
      kprintf("cd: invalid path\n");
    else
      current_directory(path);
    yield();
    break;
  }

  default:
    PANIC("unexpected syscall a3=%x\n", f->a3);
  }
}
