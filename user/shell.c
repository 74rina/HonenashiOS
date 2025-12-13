#include "user.h"

void main(void) {
  while (1) {
  prompt:
    printf("> ");
    char cmdline[128];
    for (int i = 0;; i++) {
      char ch = getchar();
      putchar(ch);
      if (i == sizeof(cmdline) - 1) {
        printf("command line too long\n");
        goto prompt;
      } else if (ch == '\r') {
        printf("\n");
        cmdline[i] = '\0';
        break;
      } else {
        cmdline[i] = ch;
      }
    }
    if (strcmp(cmdline, "hello") == 0)
      printf("Hello world from shell!\n");
    else if (strcmp(cmdline, "exit") == 0)
      exit();
    else if (strcmp(cmdline, "ls") == 0)
      list_root_dir();
    else if (strcmp(cmdline, "cat") == 0)
      concatenate();
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