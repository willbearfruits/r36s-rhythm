#pragma once
#include <stdbool.h>
#include <stdio.h>

extern int check_failed, check_passed;
void check_report(bool ok, const char *file, int line, const char *what);

#define CHECK(c) check_report((c), __FILE__, __LINE__, #c)
#define CHECKF(c, ...) do { char check_msg_[256]; snprintf(check_msg_, sizeof check_msg_, __VA_ARGS__); \
                            check_report((c), __FILE__, __LINE__, check_msg_); } while (0)
