#include "./common.h"
#include "../kernel/kernel.h"

int vprintf(putc_fn_t putc, const char *fmt, va_list vargs) {
  int count = 0;

  while (*fmt) {
    if (*fmt == '%') {
      fmt++;
      switch (*fmt) {
      case '\0':
        putc('%');
        count++;
        return count;

      case '%':
        putc('%');
        count++;
        break;

      case 's': {
        const char *s = va_arg(vargs, const char *);
        if (!s)
          s = "(null)";
        while (*s) {
          putc(*s++);
          count++;
        }
        break;
      }

      case 'd': {
        int value = va_arg(vargs, int);
        unsigned magnitude = value;

        if (value < 0) {
          putc('-');
          magnitude = -magnitude;
          count++;
        }

        unsigned divisor = 1;
        while (magnitude / divisor > 9)
          divisor *= 10;

        while (divisor) {
          putc('0' + magnitude / divisor);
          magnitude %= divisor;
          divisor /= 10;
          count++;
        }
        break;
      }

      case 'x': {
        unsigned value = va_arg(vargs, unsigned);
        for (int i = 7; i >= 0; i--) {
          unsigned nibble = (value >> (i * 4)) & 0xf;
          putc("0123456789abcdef"[nibble]);
          count++;
        }
        break;
      }
      }
    } else {
      putc(*fmt);
      count++;
    }
    fmt++;
  }

  return count;
}

void *memcpy(void *dst, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  while (n--) {
    *d++ = *s++;
  }
  return dst;
}

void *memset(void *buf, char c, size_t n) {
  uint8_t *p = (uint8_t *)buf;
  while (n--)
    *p++ = c;
  return buf;
}

char *strcpy(char *dst, const char *src) {
  char *d = dst;
  while (*src) {
    *d++ = *src++;
  }
  *d = '\0';
  return dst;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && *s2) {
    if (*s1 != *s2)
      break;
    s1++;
    s2++;
  }
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, uint32_t n) {
  for (uint32_t i = 0; i < n; i++) {
    if (s1[i] != s2[i] || s1[i] == '\0' || s2[i] == '\0') {
      return (unsigned char)s1[i] - (unsigned char)s2[i];
    }
  }
  return 0;
}

static unsigned long next = 1;

void srand(unsigned int seed) { next = seed; } // シード値からrandを呼び出す場合

int rand(void) {
  next = next * 1103515245 + 12345;
  return (unsigned int)(next >> 16) & 0x7fff;
}
