#include <stdio.h>
#include <string.h>

int strcmp(const char *s1, const char *s2) {
  while (*s1 && *s2) {
    if (*s1 != *s2)
      break;
    s1++;
    s2++;
  }
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int main(void) {
  char *s1 = "Hello";
  char *s2 = "World";
  int compare = strcmp(s1, s2);
  if (!compare) {
    printf("same\n");
    printf("%d\n", compare);
  } else {
    printf("diff\n");
    printf("%d\n", compare);
  }
}
/*
printf(compare)はおかしい。
printfの第一引数は必ずフォーマット文字列
*/
/*
Hello と World （のASCIIの値）を比べる
H(72)とW(87)で違う→ループを抜ける
出力は、72-87=-15
*/

/*
touchコマンド実装のため、strncmp(nバイト比較)も実装したい。
*/