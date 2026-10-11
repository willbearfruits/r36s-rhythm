// One value on screen, wherever it lives in the document: how to read it, change it, show it.
#include "ui_int.h"

float prm_get(const prm_t *p)
{
    switch (p->type) {
    case PT_U8: return (float) *(uint8_t *) p->p;
    case PT_I8: return (float) *(int8_t *) p->p;
    default: return *(float *) p->p;
    }
}

void prm_set(const prm_t *p, float v)
{
    v = m_clampf(v, p->min, p->max);
    switch (p->type) {
    case PT_U8: { const uint8_t x = (uint8_t) m_lround(v); if (*(uint8_t *) p->p == x) return; *(uint8_t *) p->p = x; doc_touch(p->p, 1); break; }
    case PT_I8: { const int8_t x = (int8_t) m_lround(v); if (*(int8_t *) p->p == x) return; *(int8_t *) p->p = x; doc_touch(p->p, 1); break; }
    default:
        if (p->unit == UN_CHOICE || p->unit == UN_INT) v = m_round(v);
        if (*(float *) p->p == v) return;
        *(float *) p->p = v; doc_touch(p->p, 4); break;
    }
}

void prm_step(const prm_t *p, int dir, bool big)
{
    const float v = prm_get(p);
    if (p->log) {
        const float r = m_pow(p->step > 1 ? p->step : 1.03f, big ? 8.0f : 1.0f);
        prm_set(p, dir > 0 ? MAX(v, p->min) * r : v / r);
    } else {
        float st = p->step > 0 ? p->step : (p->max - p->min) / 100.0f;
        if (big && p->unit != UN_CHOICE) st *= 8;
        prm_set(p, v + st * (float) dir);
    }
}

float prm_norm(const prm_t *p)
{
    const float v = m_clampf(prm_get(p), p->min, p->max);
    if (p->log && p->min > 0) return m_log2(v / p->min) / m_log2(p->max / p->min);
    return p->max > p->min ? (v - p->min) / (p->max - p->min) : 0;
}

bool prm_bipolar(const prm_t *p) { return p->min < 0 && p->max > 0 && p->unit != UN_CHOICE; }

void prm_text(const prm_t *p, char *out, int cap)
{
    const float v = prm_get(p);
    switch (p->unit) {
    case UN_CHOICE: {
        const int idx = m_lround(v);
        int k = 0, n = 0;
        for (const char *c = p->choices;; c++) {
            if (*c == '|' || !*c) { if (k == idx) { out[MIN(n, cap - 1)] = 0; return; } k++; n = 0; if (!*c) break; continue; }
            if (k == idx && n < cap - 1) out[n] = *c;
            n++;
        }
        k_strcpy(out, "?", cap);
        return;
    }
    case UN_HZ:
        if (v >= 10000) k_snprintf(out, cap, "%.0fk", (double) v / 1000.0);
        else if (v >= 1000) k_snprintf(out, cap, "%.1fk", (double) v / 1000.0);
        else k_snprintf(out, cap, "%.0f", (double) v);
        return;
    case UN_MS:
        if (v >= 1000) k_snprintf(out, cap, "%.2fs", (double) v / 1000.0);
        else if (v < 10) k_snprintf(out, cap, "%.1fms", (double) v);
        else k_snprintf(out, cap, "%.0fms", (double) v);
        return;
    case UN_SEC: k_snprintf(out, cap, v < 10 ? "%.2fs" : "%.1fs", (double) v); return;
    case UN_DB: k_snprintf(out, cap, "%+.1f", (double) v); return;
    case UN_ST: k_snprintf(out, cap, "%+.0f", (double) v); return;
    case UN_PCT: k_snprintf(out, cap, "%.0f", (double) (v * 100)); return;
    case UN_X: k_snprintf(out, cap, "x%.2f", (double) v); return;
    case UN_BPM: k_snprintf(out, cap, "%.1f", (double) v); return;
    case UN_SIGNED: k_snprintf(out, cap, "%+d", m_lround(v)); return;
    case UN_INT: k_snprintf(out, cap, "%d", m_lround(v)); return;
    default:
        if (p->max - p->min >= 4 && m_fabs(v - m_round(v)) < 1e-3f) k_snprintf(out, cap, "%.0f", (double) v);
        else k_snprintf(out, cap, "%+.2f", (double) v);
    }
}

prm_t prm_drum(int t, int param)
{
    const drum_param_t *d = &drum_params[param];
    static const uint8_t units[] = { UN_NONE, UN_HZ, UN_MS, UN_DB, UN_ST, UN_PCT, UN_X, UN_CHOICE };
    prm_t p = { d->label, &doc.trk[t].synth[param], PT_F32, units[d->unit], d->min, d->max, 0, d->log, d->choices };
    if (d->unit == DU_CHOICE || param == P_MODES_COUNT || param == P_CLAP_COUNT) { p.step = 1; if (d->unit != DU_CHOICE) p.unit = UN_INT; }
    else if (d->log) p.step = 1.03f;
    else if (d->unit == DU_SEMI) p.step = 1;
    else if (d->unit == DU_DB) p.step = 0.5f;
    else p.step = (d->max - d->min) / 100.0f;
    return p;
}

prm_t prm_f(const char *label, float *v, float min, float max, float step, uint8_t unit) { return (prm_t) { label, v, PT_F32, unit, min, max, step, false, NULL }; }
prm_t prm_flog(const char *label, float *v, float min, float max, uint8_t unit) { return (prm_t) { label, v, PT_F32, unit, min, max, 1.03f, true, NULL }; }
prm_t prm_u8(const char *label, uint8_t *v, float min, float max, uint8_t unit) { return (prm_t) { label, v, PT_U8, unit, min, max, 1, false, NULL }; }
prm_t prm_choice(const char *label, uint8_t *v, const char *choices)
{
    int n = 1;
    for (const char *c = choices; *c; c++) if (*c == '|') n++;
    return (prm_t) { label, v, PT_U8, UN_CHOICE, 0, (float) (n - 1), 1, false, choices };
}

bool prm_nav(const prm_t *ps, int n, int *sel, int cols, int b, bool down)
{
    if (!down || n <= 0) return false;
    *sel = CLAMP(*sel, 0, n - 1);
    if (ui.held[BTN_A]) {
        ui.used[BTN_A] = true;
        if (b == BTN_LEFT || b == BTN_RIGHT) { prm_step(&ps[*sel], b == BTN_RIGHT ? 1 : -1, false); return true; }
        if (b == BTN_UP || b == BTN_DOWN) { prm_step(&ps[*sel], b == BTN_UP ? 1 : -1, true); return true; }
        return false;
    }
    if (cols <= 0) cols = 1;
    switch (b) {
    case BTN_LEFT: if (cols > 1 && *sel % cols > 0) (*sel)--; else if (cols == 1) prm_step(&ps[*sel], -1, false); break;
    case BTN_RIGHT: if (cols > 1 && *sel % cols < cols - 1 && *sel + 1 < n) (*sel)++; else if (cols == 1) prm_step(&ps[*sel], 1, false); break;
    case BTN_UP: if (*sel - cols >= 0) *sel -= cols; break;
    case BTN_DOWN: if (*sel + cols < n) *sel += cols; break;
    default: return false;
    }
    return cols == 1 && (b == BTN_LEFT || b == BTN_RIGHT);
}

void w_knobs(const prm_t *ps, int n, int sel, int x0, int y0, int cols, int dx, int dy, int r, col_t col)
{
    char v[24], lab[24];
    for (int i = 0; i < n; i++) {
        const int cx = x0 + (i % cols) * dx, cy = y0 + (i / cols) * dy;
        prm_text(&ps[i], v, sizeof v);
        w_upper(v);
        k_strcpy(lab, ps[i].label, sizeof lab);
        w_upper(lab);
        const bool bip = prm_bipolar(&ps[i]);
        const float nv = prm_norm(&ps[i]);
        if (i == sel) g_rframe(cx - dx / 2 + 3, cy - r - 8, cx + dx / 2 - 4, cy + r + 20, 5, C_WHITE, 2);
        w_knob(cx, cy, r, bip ? nv * 2 - 1 : nv, col, lab, v, bip, r > 20 ? 4 : 3);
    }
}

void w_list(const prm_t *ps, int n, int sel, int x, int y, int w, int rowh, col_t col)
{
    char v[32], lab[32];
    for (int i = 0; i < n; i++) {
        prm_text(&ps[i], v, sizeof v);
        w_upper(v);
        k_strcpy(lab, ps[i].label, sizeof lab);
        w_upper(lab);
        w_setting(x, y + i * rowh, w, lab, v, i == sel, col, C_TEXT);
    }
}
