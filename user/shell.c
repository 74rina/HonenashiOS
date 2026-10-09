#include "user.h"

void main(void) {
  while (1) {
  prompt:
    printf("> ");
    char cmdline[128];
    char *cmd_idx = cmdline;
    char *cmd_end = cmdline + sizeof(cmdline) - 1;

    for (;;) {
      char ch = getchar();

      // Enter
      if (ch == '\r') {
        printf("\n");
        *cmd_idx = '\0';
        break;
      }

      // Backspace
      else if (ch == '\b' || ch == 0x7f) {
        if (cmd_idx > cmdline) {
          cmd_idx--;
          putchar('\b');
          putchar(' ');
          putchar('\b');
        }
        continue;
      }

      if (cmd_idx == cmd_end) {
        printf("\ncommand line too long\n");
        goto prompt;
      }

      // Normal input
      *cmd_idx = ch;
      cmd_idx++;
      putchar(ch);
    }

    // Commands
    if (strcmp(cmdline, "hello") == 0)
      printf("Hello world from shell!\n");

    else if (strcmp(cmdline, "exit") == 0)
      sys_exit();

    else if (strcmp(cmdline, "ls") == 0)
      sys_list_root_dir();

    else if (strncmp(cmdline, "cat ", 4) == 0) {
      const char *filename = cmdline + 4;
      if (*filename == '\0') {
        printf("usage: cat filename\n");
      } else {
        sys_concatenate(filename);
      }

    } else if (strncmp(cmdline, "cd ", 3) == 0) {
      const char *path = cmdline + 3;
      if (*path == '\0') {
        printf("usage: cd path\n");
      } else {
        sys_current_directory(path);
      }

    } else if (strcmp(cmdline, "pwd") == 0)
      sys_print_working_directory();

    else if (strcmp(cmdline, "ohgiri") == 0) {
      int r = rand() % 3;
      if (r == 0)
        printf(
            "パクツイする人、ネタツイーター(ネタツイを食い物にしているので)\n");
      else if (r == 1)
        printf("熱海での自分探しの旅の末、ついに自分が見つかりました "
               "(あったme)\n");
      else if (r == 2)
        printf("このモデル、ついに2種類のカタカナを見分けられるようになりました"
               "!(キかイ学習)\n");
    } else
      printf("unknown command: %s\n", cmdline);
  }
}
