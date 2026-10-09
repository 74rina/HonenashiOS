#pragma once
#include "../common/common.h"

__attribute__((noreturn)) void sys_exit(void);
void putchar(char ch);
int getchar(void);
int printf(const char *fmt, ...);
int sys_create_file(const char *name, const uint8_t *data, uint32_t size);
void sys_list_root_dir();
void sys_concatenate(const char *filename);
void sys_print_working_directory();
void sys_current_directory(const char *path);
