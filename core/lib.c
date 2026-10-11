#include "lib.h"
#include <stdarg.h>

#ifdef CORE_FREESTANDING
// The compiler may call these on its own, so a bare-metal build has to have them.
void *memcpy(void *d, const void *s, size_t n) { uint8_t *a = d; const uint8_t *b = s; while (n--) *a++ = *b++; return d; }
void *memmove(void *d, const void *s, size_t n)
{
    uint8_t *a = d; const uint8_t *b = s;
    if (a < b) while (n--) *a++ = *b++;
    else { a += n; b += n; while (n--) *--a = *--b; }
    return d;
}
void *memset(void *d, int c, size_t n) { uint8_t *a = d; while (n--) *a++ = (uint8_t) c; return d; }
int memcmp(const void *x, const void *y, size_t n)
{
    const uint8_t *a = x, *b = y;
    for (; n; n--, a++, b++) if (*a != *b) return *a - *b;
    return 0;
}
#endif

int k_strlen(const char *s) { int n = 0; while (s[n]) n++; return n; }
int k_strcmp(const char *a, const char *b) { while (*a && *a == *b) a++, b++; return (uint8_t) *a - (uint8_t) *b; }
int k_strncmp(const char *a, const char *b, int n)
{
    for (; n > 0; n--, a++, b++) { if (*a != *b) return (uint8_t) *a - (uint8_t) *b; if (!*a) return 0; }
    return 0;
}
void k_strcpy(char *dst, const char *src, int cap)
{
    if (cap <= 0) return;
    int i = 0;
    for (; i < cap - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}
void k_strcat(char *dst, const char *src, int cap)
{
    int n = k_strlen(dst);
    if (n < cap) k_strcpy(dst + n, src, cap - n);
}

typedef struct { char *p; int n, cap; } sink_t;
static void put(sink_t *s, char c) { if (s->n < s->cap - 1) s->p[s->n] = c; s->n++; }
static void pad_out(sink_t *s, const char *b, int len, int width, bool left, char padc)
{
    if (!left) for (int i = len; i < width; i++) put(s, padc);
    for (int i = 0; i < len; i++) put(s, b[i]);
    if (left) for (int i = len; i < width; i++) put(s, ' ');
}

int k_snprintf(char *out, int cap, const char *fmt, ...)
{
    sink_t s = { out, 0, cap };
    va_list ap;
    va_start(ap, fmt);
    for (const char *f = fmt; *f; f++) {
        if (*f != '%') { put(&s, *f); continue; }
        f++;
        bool left = false, plus = false; char padc = ' ';
        for (;; f++) {
            if (*f == '-') left = true;
            else if (*f == '+') plus = true;
            else if (*f == '0') padc = '0';
            else break;
        }
        int width = 0, prec = -1;
        while (*f >= '0' && *f <= '9') width = width * 10 + (*f++ - '0');
        if (*f == '.') { f++; prec = 0; while (*f >= '0' && *f <= '9') prec = prec * 10 + (*f++ - '0'); }
        char b[48]; int n = 0;
        switch (*f) {
        case 'd': case 'i': {
            int v = va_arg(ap, int);
            unsigned u = v < 0 ? (unsigned) -(long) v : (unsigned) v;
            char t[16]; int k = 0;
            do t[k++] = (char) ('0' + u % 10); while (u /= 10);
            if (v < 0) b[n++] = '-'; else if (plus) b[n++] = '+';
            while (k) b[n++] = t[--k];
            pad_out(&s, b, n, width, left, padc);
            break;
        }
        case 'u': case 'x': case 'X': {
            unsigned u = va_arg(ap, unsigned), base = *f == 'u' ? 10 : 16;
            const char *dg = *f == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
            char t[16]; int k = 0;
            do t[k++] = dg[u % base]; while (u /= base);
            while (k) b[n++] = t[--k];
            pad_out(&s, b, n, width, left, padc);
            break;
        }
        case 'c': b[n++] = (char) va_arg(ap, int); pad_out(&s, b, n, width, left, ' '); break;
        case 's': { const char *str = va_arg(ap, const char *); if (!str) str = "(null)"; pad_out(&s, str, k_strlen(str), width, left, ' '); break; }
        case 'f': {
            double v = va_arg(ap, double);
            if (prec < 0) prec = 6;
            if (prec > 6) prec = 6;
            bool neg = v < 0;
            if (neg) v = -v;
            double scale = 1; for (int i = 0; i < prec; i++) scale *= 10;
            double r = v * scale + 0.5;
            if (r > 4.0e18) r = 4.0e18;
            unsigned long long q = (unsigned long long) r;
            unsigned long long ip = q / (unsigned long long) scale, fp = q % (unsigned long long) scale;
            if (neg && q) b[n++] = '-'; else if (plus) b[n++] = '+';
            char t[24]; int k = 0;
            do t[k++] = (char) ('0' + ip % 10); while (ip /= 10);
            while (k) b[n++] = t[--k];
            if (prec > 0) {
                b[n++] = '.';
                for (int i = prec - 1; i >= 0; i--) { t[i] = (char) ('0' + fp % 10); fp /= 10; }
                for (int i = 0; i < prec; i++) b[n++] = t[i];
            }
            pad_out(&s, b, n, width, left, padc);
            break;
        }
        case '%': put(&s, '%'); break;
        case 0: f--; break;
        default: put(&s, '%'); put(&s, *f); break;
        }
    }
    va_end(ap);
    if (cap > 0) s.p[MIN(s.n, cap - 1)] = 0;
    return s.n;
}
