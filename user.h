#pragma once
#include "lib/common.h"

__attribute__((noreturn)) void exit(void);
void putchar(char ch);
int getchar(void);
int create_file(const char *name, const uint8_t *data, uint32_t size);
void list_root_dir();
void concatenate();