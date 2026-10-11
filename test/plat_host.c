// The platform for the PC hosts: libc for the clock, the log and files (paths under CARD, default build/card/).
#define _POSIX_C_SOURCE 200809L
#include "../core/plat.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

static const char *root(void) { const char *r = getenv("CARD"); return r ? r : "build/card"; }
static void full(char *out, int cap, const char *path) { snprintf(out, (size_t) cap, "%s/%s", root(), path); }

uint64_t plat_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000u + (uint64_t) ts.tv_nsec / 1000u;
}
void plat_log(const char *line) { fprintf(stderr, "%s\n", line); }
int plat_file_size(const char *path)
{
    char p[512]; full(p, sizeof p, path);
    struct stat st;
    return stat(p, &st) == 0 ? (int) st.st_size : -1;
}
int plat_file_read(const char *path, void *buf, int max)
{
    char p[512]; full(p, sizeof p, path);
    FILE *f = fopen(p, "rb");
    if (!f) return -1;
    const int n = (int) fread(buf, 1, (size_t) max, f);
    fclose(f);
    return n;
}
int plat_file_write(const char *path, const void *buf, int n)
{
    char p[512]; full(p, sizeof p, path);
    for (char *c = p + strlen(root()) + 1; *c; c++) if (*c == '/') { *c = 0; mkdir(p, 0755); *c = '/'; }
    FILE *f = fopen(p, "wb");
    if (!f) return -1;
    const int w = (int) fwrite(buf, 1, (size_t) n, f);
    // onto the card now: the R36S is often switched off without Linux writing out what it holds back
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    char *slash = strrchr(p, '/');
    if (slash) { *slash = 0; const int d = open(p, O_RDONLY); if (d >= 0) { fsync(d); close(d); } }
    return w == n ? n : -1;
}
bool plat_dir(const char *path, int index, char *name, int cap, bool *is_dir)
{
    char p[512]; full(p, sizeof p, path);
    DIR *d = opendir(p);
    if (!d) return false;
    struct dirent *e; int i = 0; bool found = false;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (i++ == index) {
            snprintf(name, (size_t) cap, "%s", e->d_name);
            char q[1024]; snprintf(q, sizeof q, "%s/%s", p, e->d_name);
            struct stat st; *is_dir = stat(q, &st) == 0 && S_ISDIR(st.st_mode);
            found = true; break;
        }
    }
    closedir(d);
    return found;
}
