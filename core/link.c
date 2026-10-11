#include "link.h"

ring_t to_eng, to_ui;
status_t eng_status;
float eng_scope_l[SCOPE_LEN], eng_scope_r[SCOPE_LEN];
_Atomic uint32_t eng_scope_head;
static uint8_t to_eng_mem[1 << 20], to_ui_mem[1 << 14];

void link_init(void)
{
    ring_init(&to_eng, to_eng_mem, sizeof to_eng_mem);
    ring_init(&to_ui, to_ui_mem, sizeof to_ui_mem);
    m_set((uint8_t *) &eng_status + sizeof eng_status.seq, 0, sizeof eng_status - sizeof eng_status.seq);
}

void ring_init(ring_t *q, void *mem, uint32_t size)
{
    q->buf = mem;
    q->size = size;
    atomic_store(&q->w, 0);
    atomic_store(&q->r, 0);
}

uint32_t ring_free(ring_t *q)
{
    const uint32_t w = atomic_load_explicit(&q->w, memory_order_relaxed), r = atomic_load_explicit(&q->r, memory_order_acquire);
    return q->size - (w - r);
}

static void put_bytes(ring_t *q, uint32_t at, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    for (uint32_t i = 0; i < n; i++) q->buf[(at + i) & (q->size - 1)] = s[i];
}

static void get_bytes(ring_t *q, uint32_t at, void *dst, uint32_t n)
{
    uint8_t *d = dst;
    for (uint32_t i = 0; i < n; i++) d[i] = q->buf[(at + i) & (q->size - 1)];
}

bool ring_put(ring_t *q, const void *msg, uint32_t len)
{
    if (ring_free(q) < len + 4) return false;
    const uint32_t w = atomic_load_explicit(&q->w, memory_order_relaxed);
    put_bytes(q, w, &len, 4);
    put_bytes(q, w + 4, msg, len);
    atomic_store_explicit(&q->w, w + 4 + len, memory_order_release);
    return true;
}

uint32_t ring_get(ring_t *q, void *out, uint32_t cap)
{
    const uint32_t r = atomic_load_explicit(&q->r, memory_order_relaxed), w = atomic_load_explicit(&q->w, memory_order_acquire);
    if (w == r) return 0;
    uint32_t len;
    get_bytes(q, r, &len, 4);
    if (len <= cap) get_bytes(q, r + 4, out, len);
    atomic_store_explicit(&q->r, r + 4 + len, memory_order_release);
    return len <= cap ? len : 0;
}

void status_begin(void)
{
    atomic_fetch_add_explicit(&eng_status.seq, 1, memory_order_relaxed);
    atomic_thread_fence(memory_order_release);
}

void status_end(void)
{
    atomic_thread_fence(memory_order_release);
    atomic_fetch_add_explicit(&eng_status.seq, 1, memory_order_relaxed);
}

void status_read(status_t *out)
{
    for (int tries = 0; tries < 100; tries++) {
        const uint32_t a = atomic_load_explicit(&eng_status.seq, memory_order_acquire);
        if (a & 1) continue;
        m_copy((uint8_t *) out + sizeof(out->seq), (const uint8_t *) &eng_status + sizeof(out->seq), sizeof *out - sizeof(out->seq));
        atomic_thread_fence(memory_order_acquire);
        if (atomic_load_explicit(&eng_status.seq, memory_order_relaxed) == a) return;
    }
}

uint32_t link_send_doc(const doc_t *d, uint32_t off, uint32_t len)
{
    static uint8_t m[sizeof(msg_patch_t) + PATCH_MAX];
    uint32_t sent = 0;
    while (sent < len) {
        const uint32_t n = MIN(len - sent, PATCH_MAX);
        msg_patch_t h = { M_PATCH, { 0, 0, 0 }, off + sent, n };
        m_copy(m, &h, sizeof h);
        m_copy(m + sizeof h, (const uint8_t *) d + off + sent, n);
        if (!ring_put(&to_eng, m, (uint32_t) sizeof h + n)) break;
        sent += n;
    }
    return sent;
}
