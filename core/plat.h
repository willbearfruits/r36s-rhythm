// What the core needs from a platform: a clock, a log and files. The PC hosts implement it with libc; bare metal
// with its own SD card and FAT driver.
#pragma once
#include "lib.h"

uint64_t plat_us(void);                                   // microseconds since start
void     plat_log(const char *line);
// Files, paths relative to the card's root ("SAMPLES/BREAKS/X.WAV"). Sizes and counts in bytes; -1 on failure.
int      plat_file_size(const char *path);
int      plat_file_read(const char *path, void *buf, int max);
int      plat_file_write(const char *path, const void *buf, int n);
// The index-th entry of a directory: false past the last. Folders come back with is_dir set.
bool     plat_dir(const char *path, int index, char *name, int cap, bool *is_dir);
