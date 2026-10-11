#include "core.h"
#include "plat.h"
#include "ui_int.h"
#include "wav.h"

ui_t ui;
doc_t doc;
const char *const btn_name[BTN_COUNT] = { "UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y", "L1", "L2", "R1", "R2", "SELECT", "START", "L3", "R3" };
const col_t trk_col[NTRK] = {
    RGB(255, 72, 72), RGB(255, 146, 52), RGB(255, 214, 60), RGB(176, 234, 64), RGB(72, 224, 112), RGB(48, 220, 200),
    RGB(56, 186, 255), RGB(92, 128, 255), RGB(150, 112, 255), RGB(214, 104, 255), RGB(255, 84, 206), RGB(255, 128, 150),
};
const char *const nav_name[NAV_COUNT] = { "PATTERN", "PATTERNS", "SONG", "PERFORM", "KIT", "SOUND", "SAMPLE", "SAMPLING",
                                          "MIXER", "FX", "MASTER", "BROWSER", "PROJECT", "MIDI", "SYNC", "SETTINGS" };

const screen_t screens[SCR_COUNT] = {
    [NAV_PATTERN] = { draw_pattern, pattern_button }, [NAV_PATTERNS] = { draw_patterns, patterns_button },
    [NAV_SONG] = { draw_song, song_button }, [NAV_PERFORM] = { draw_perform, perform_button },
    [NAV_KIT] = { draw_kit, kit_button }, [NAV_SOUND] = { draw_sound, sound_button },
    [NAV_SAMPLE] = { draw_sample, sample_button }, [NAV_SAMPLING] = { draw_sampling, sampling_button },
    [NAV_MIXER] = { draw_mixer, mixer_button }, [NAV_FX] = { draw_fx, fx_button }, [NAV_MASTER] = { draw_master, master_button },
    [NAV_BROWSER] = { draw_browser, browser_button }, [NAV_PROJECT] = { draw_project, project_button },
    [NAV_MIDI] = { draw_midi, midi_button }, [NAV_SYNC] = { draw_sync, sync_button }, [NAV_SETTINGS] = { draw_settings, settings_button },
    [SCR_GENERATE] = { draw_generate, generate_button }, [SCR_DIAG] = { draw_diag, diag_button },
    [SCR_BUTTONS] = { draw_buttons, buttons_button }, [SCR_USB] = { draw_usb, usb_button },
};

col_t nav_col(int i)
{
    // hsv(i / 16, 0.66, 1), as the mockups
    const float h = (float) i / 16.0f * 6.0f, s = 0.66f;
    const int k = (int) h;
    const float f = h - (float) k, p = 1 - s, q = 1 - s * f, t = 1 - s * (1 - f);
    float r, g, b;
    switch (k % 6) {
    case 0: r = 1; g = t; b = p; break;
    case 1: r = q; g = 1; b = p; break;
    case 2: r = p; g = 1; b = t; break;
    case 3: r = p; g = q; b = 1; break;
    case 4: r = t; g = p; b = 1; break;
    default: r = 1; g = p; b = q; break;
    }
    return RGB((int) (r * 255), (int) (g * 255), (int) (b * 255));
}

void go(int s)
{
    if (s < 0 || s >= SCR_COUNT) return;
    if (s != ui.screen) ui.from = ui.screen;
    ui.screen = s;
    ui.overlay = OV_NONE;
    for (int k = 0; k < BTN_COUNT; k++) if (ui.held[k]) ui.used[k] = true;   // their release belongs to the old screen
}

// ------------------------------------------------------------------------------------------- the document

#define NDIRTY 64
typedef struct { uint32_t off, len; } range_t;
static range_t dirty[NDIRTY];
static int ndirty;
static bool loading;

void doc_touch(const void *p, size_t n)
{
    const uint32_t off = (uint32_t) ((const uint8_t *) p - (const uint8_t *) &doc), len = (uint32_t) n;
    if ((size_t) off + len > sizeof doc) return;
    if (!loading) { ui.modified = true; ui.modified_at = ui.now; }
    for (int i = 0; i < ndirty; i++) {
        const uint32_t a = dirty[i].off, b = a + dirty[i].len;
        if (off <= b && off + len >= a) {   // overlapping or touching: one range
            const uint32_t na = MIN(a, off), nb = MAX(b, off + len);
            dirty[i].off = na; dirty[i].len = nb - na;
            return;
        }
    }
    if (ndirty == NDIRTY) {   // full: fold everything into one range
        uint32_t a = dirty[0].off, b = a + dirty[0].len;
        for (int i = 1; i < ndirty; i++) { a = MIN(a, dirty[i].off); b = MAX(b, dirty[i].off + dirty[i].len); }
        ndirty = 1; dirty[0].off = MIN(a, off); dirty[0].len = MAX(b, off + len) - dirty[0].off;
        return;
    }
    dirty[ndirty].off = off; dirty[ndirty].len = len; ndirty++;
}

void doc_all(void) { loading = true; ndirty = 0; doc_touch(&doc, sizeof doc); loading = false; }

// Sends what changed, as far as the ring has room; the rest goes next frame.
static void sync(void)
{
    int keep = 0;
    for (int i = 0; i < ndirty; i++) {
        const uint32_t sent = link_send_doc(&doc, dirty[i].off, dirty[i].len);
        if (sent < dirty[i].len) dirty[keep++] = (range_t) { dirty[i].off + sent, dirty[i].len - sent };
    }
    ndirty = keep;
}

void ui_msg(int type, int a, int b, int c, float f)
{
    sync();   // a message goes after the document changes it depends on
    msg_t m = { (uint8_t) type, (uint8_t) a, (uint8_t) b, (uint8_t) c, f, 0 };
    ring_put(&to_eng, &m, sizeof m);
}

void ui_toast(const char *s) { k_strcpy(ui.toast, s, sizeof ui.toast); ui.toast_until = ui.now + 1500; }
pattern_t *cur_pat(void) { return &doc.pat[CLAMP(ui.pat, 0, NPAT - 1)]; }
void undo_save(void) { ui.undo = *cur_pat(); ui.undo_pat = ui.pat; ui.undo_ok = true; }

int pattern_len(const pattern_t *p)
{
    int n = p->len;
    for (int t = 0; t < NTRK; t++) n = MAX(n, p->trk[t].len);
    return CLAMP(n, 1, NSTEP);
}

// ------------------------------------------------------------------------------------------- samples

// Loaded samples stay for the session (memory is plentiful); the last two slots take turns for listening in the
// browser, in their own space, so browsing does not use it up.
#define PREVIEW_SLOT (NSMPSLOT - 2)
#define PREVIEW_BYTES (24u << 20)
static uint8_t *pool, *pool_end, *pool_top, *prev_mem[2];
static struct { char path[96]; sample_t s; } loaded[NSMPSLOT];
static int nloaded, prev_turn;

static void send_slot(int s)
{
    uint8_t m[4 + sizeof(sample_t)] = { M_SAMPLE, (uint8_t) s };
    m_copy(m + 4, &loaded[s].s, sizeof(sample_t));
    ring_put(&to_eng, m, sizeof m);
}

// A WAV file into memory at dst (room bytes): the sample, or frames 0.
static sample_t decode_into(const char *path, uint8_t *dst, size_t room)
{
    sample_t none = { 0 };
    const int size = plat_file_size(path);
    if (size <= 44 || (size_t) size > room) return none;
    if (plat_file_read(path, dst, size) != size) return none;
    wav_info_t w;
    if (!wav_parse(dst, size, &w)) return none;
    const size_t bytes = (size_t) w.frames * w.chans * 2;
    // decode after the file, then move down over it (16-bit is never bigger than the file's data)
    if ((size_t) size + bytes > room) {
        if (w.bits != 16 || w.format != 1 || w.file_chans != w.chans) return none;
        m_move(dst, dst + w.data_off, bytes);   // already the right form: just take the data
    } else {
        wav_decode(dst, &w, (int16_t *) (dst + size), w.frames);
        m_move(dst, dst + size, bytes);
    }
    return (sample_t) { (const int16_t *) dst, w.frames, w.rate, (uint8_t) w.chans, { 0, 0, 0 } };
}

int ui_sample_load(const char *path)
{
    for (int i = 0; i < nloaded; i++) if (!k_strcmp(loaded[i].path, path)) return i;
    if (nloaded >= PREVIEW_SLOT) { ui_toast("SAMPLE SLOTS FULL"); return -1; }
    sample_t s = decode_into(path, pool_top, (size_t) (pool_end - pool_top));
    if (!s.frames) return -1;
    pool_top += ((size_t) s.frames * s.chans * 2 + 15) & ~(size_t) 15;
    const int i = nloaded++;
    k_strcpy(loaded[i].path, path, sizeof loaded[i].path);
    loaded[i].s = s;
    send_slot(i);
    return i;
}

// For the browser: a file into one of the two listening slots. The slot, or -1.
int ui_sample_preview(const char *path)
{
    for (int i = 0; i < nloaded; i++) if (!k_strcmp(loaded[i].path, path)) return i;   // already in memory
    const int k = prev_turn ^= 1, slot = PREVIEW_SLOT + k;
    if (!prev_mem[k]) return -1;
    sample_t s = decode_into(path, prev_mem[k], PREVIEW_BYTES);
    if (!s.frames) return -1;
    k_strcpy(loaded[slot].path, path, sizeof loaded[slot].path);
    loaded[slot].s = s;
    send_slot(slot);
    return slot;
}

static struct { uint8_t *buf; uint32_t frames; void (*done)(const uint8_t *, int); uint8_t *pool_was; } capt;

bool ui_capturing(void) { return capt.buf != NULL; }

bool ui_capture(uint8_t src, uint8_t start, uint32_t frames, void (*done)(const uint8_t *wav, int bytes))
{
    const size_t bytes = 44 + (size_t) frames * 4;
    if (capt.buf || !pool || pool_top + bytes > pool_end) return false;
    capt.buf = pool_top; capt.frames = frames; capt.done = done; capt.pool_was = pool_top;
    pool_top += (bytes + 15) & ~(size_t) 15;
    wav_header(capt.buf, frames, 2, 48000);
    uint8_t m[4 + sizeof(capture_t)] = { M_CAPTURE };
    const capture_t c = { (int16_t *) (capt.buf + 44), frames, src, start, { 0, 0 } };
    m_copy(m + 4, &c, sizeof c);
    sync();
    ring_put(&to_eng, m, sizeof m);
    return true;
}

static void capture_done(void)
{
    if (!capt.buf) return;
    uint8_t *buf = capt.buf;
    void (*done)(const uint8_t *, int) = capt.done;
    const int bytes = 44 + (int) capt.frames * 4;
    capt.buf = NULL;
    if (done) done(buf, bytes);
    if (pool_top == capt.pool_was + (((size_t) bytes + 15) & ~(size_t) 15)) pool_top = capt.pool_was;   // nothing came after it: give it back
}

const sample_t *ui_slot(int s) { return s >= 0 && s < NSMPSLOT && loaded[s].s.data ? &loaded[s].s : NULL; }

static int bound[NTRK];

void ui_bind(int t)
{
    const int s = doc.trk[t].engine == ENG_SAMPLE ? ui_sample_load(doc.trk[t].smp.path) : -1;
    bound[t] = s;
    ui_msg(M_BIND, t, s < 0 ? 255 : s, 0, 0);
}

const sample_t *ui_sample(int t) { return bound[t] >= 0 ? &loaded[bound[t]].s : NULL; }

// ------------------------------------------------------------------------------------------- the drum's shape

static struct { uint32_t key; float peaks[2 * 240]; int cols; float secs; } prev[NTRK];
static drum_t prev_drum;

static uint32_t vals_key(const float *v)
{
    uint32_t h = 2166136261u;
    const uint8_t *b = (const uint8_t *) v;
    for (size_t i = 0; i < sizeof(float) * DRUM_NP; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}

void ui_preview(int t, float *peaks, int cols, float *secs)
{
    cols = MIN(cols, 240);
    const uint32_t key = vals_key(doc.trk[t].synth) ^ (uint32_t) cols;
    if (prev[t].key != key) {
        static float L[48000], R[48000];
        float v[DRUM_NP];
        m_copy(v, doc.trk[t].synth, sizeof v);
        v[P_PAN] = 0;
        drum_init(&prev_drum, 48000, 2);
        drum_set(&prev_drum, v);
        drum_trigger(&prev_drum, 1.0f, 1, NULL);
        m_set(L, 0, sizeof L); m_set(R, 0, sizeof R);
        int n = 0;
        while (n < 48000) { drum_render(&prev_drum, L + n, R + n, 256); n += 256; if (!drum_active(&prev_drum)) break; }
        n = MIN(n, 48000);
        float pk = 1e-6f; int last = 0;
        for (int i = 0; i < n; i++) pk = MAX(pk, m_fabs(L[i]));
        for (int i = 0; i < n; i++) if (m_fabs(L[i]) > pk * 0.01f) last = i;
        n = MAX(last + 1, 480);
        for (int c = 0; c < cols; c++) {
            float lo = 0, hi = 0;
            for (int i = c * n / cols; i < (c + 1) * n / cols; i++) { lo = MIN(lo, L[i] / pk); hi = MAX(hi, L[i] / pk); }
            prev[t].peaks[2 * c] = lo; prev[t].peaks[2 * c + 1] = hi;
        }
        prev[t].cols = cols;
        prev[t].secs = (float) n / 48000.0f;
        prev[t].key = key;
    }
    m_copy(peaks, prev[t].peaks, sizeof(float) * 2 * (size_t) cols);
    *secs = prev[t].secs;
}

// ------------------------------------------------------------------------------------------- preferences

#define PREFS_MAGIC 0x46525052u
static void prefs_defaults(prefs_t *p)
{
    m_set(p, 0, sizeof *p);
    p->magic = PREFS_MAGIC; p->version = 1;
    p->dead = 0.12f; p->curve = 1; p->repeat_ms = 70; p->pad_accent = 1; p->quality = 0;
    p->l_x = P_TUNE; p->l_y = P_LENGTH; p->r_x = 4; p->r_y = 0;   // FILTER across, REPEAT up
}

char ui_host_text[160];
void ui_host_info(const char *text) { k_strcpy(ui_host_text, text, sizeof ui_host_text); }
int  ui_audio_buffer(void) { static const int f[4] = { 0, 256, 512, 1024 }; return f[ui.prefs.buffer & 3]; }
void ui_mem(uint32_t *used, uint32_t *total) { *used = (uint32_t) (pool_top - pool); *total = (uint32_t) (pool_end - pool); }

void prefs_save(void) { plat_file_write("SETTINGS.CFG", &ui.prefs, (int) sizeof ui.prefs); }

static void prefs_load(void)
{
    prefs_t p;
    if (plat_file_read("SETTINGS.CFG", &p, (int) sizeof p) == (int) sizeof p && p.magic == PREFS_MAGIC && p.version == 1) ui.prefs = p;
    else prefs_defaults(&ui.prefs);
}

// ------------------------------------------------------------------------------------------- input

static void dispatch(int b, bool down, bool rep)
{
    const int ov = ui.overlay;
    if (ov == OV_NAME) { name_button(b, down, rep); return; }
    if (ov == OV_CONFIRM) { confirm_button(b, down); return; }
    if (ui.screen == SCR_DIAG && ov == OV_NONE) { diag_button(b, down, rep); return; }   // every button is under test there
    // the pads come up while L1 or L2 is held, on any screen
    if ((b == BTN_L1 || b == BTN_L2) && down && ov != OV_MENU) { ui.overlay = OV_PADS; ui.pads_b = b == BTN_L2; return; }
    if ((b == BTN_L1 || b == BTN_L2) && !down && ov == OV_PADS) {
        if (!ui.held[BTN_L1] && !ui.held[BTN_L2]) ui.overlay = OV_NONE; else ui.pads_b = ui.held[BTN_L2];
        return;
    }
    if (ov == OV_PADS) { pads_button(b, down); return; }
    if (b == BTN_START && down && ov != OV_MENU) {
        if (ui.screen == NAV_SONG) ui_msg(M_SONG, !ui.st.playing, 0, 0, 0);
        ui_msg(ui.st.playing ? M_STOP : M_PLAY, 0, 0, 0, 0);
        return;
    }
    // SELECT: a tap opens the menu; held with the D-pad it jumps
    if (ui.held[BTN_SELECT] && down && b >= BTN_UP && b <= BTN_RIGHT && ov != OV_MENU) {
        ui.used[BTN_SELECT] = true;
        static const int jump[4] = { NAV_MIXER, NAV_PERFORM, NAV_PATTERN, NAV_SOUND };
        go(jump[b - BTN_UP]);
        return;
    }
    if (b == BTN_SELECT && !down && !ui.used[BTN_SELECT]) {
        if (ov == OV_MENU) ui.overlay = OV_NONE;
        else { ui.overlay = OV_MENU; ui.menu_sel = ui.screen < NAV_COUNT ? ui.screen : NAV_PATTERN; }
        return;
    }
    if (b == BTN_SELECT) return;
    if (ov == OV_MENU) { menu_button(b, down, rep); return; }
    if (ov == OV_STEP) { step_button(b, down, rep); return; }
    if (b == BTN_R3) { if (down) perform_latch(); return; }   // the right stick is everywhere
    screens[ui.screen].button(b, down, rep);
}

void ui_button(int b, bool down)
{
    if (b < 0 || b >= BTN_COUNT || ui.held[b] == down) return;
    if (ui.prefs.swap_ab && (b == BTN_A || b == BTN_B)) b = b == BTN_A ? BTN_B : BTN_A;
    if (ui.held[b] == down) return;
    ui.held[b] = down;
    if (down) {
        ui.down_at[b] = ui.now; ui.rep_at[b] = ui.now + 280; ui.used[b] = false;
        for (int k = 0; k < BTN_COUNT; k++) if (k != b && ui.held[k]) ui.used[k] = true;   // it was held for this
    }
    dispatch(b, down, false);
}

static float shape(float v)
{
    const float d = ui.prefs.dead, a = m_fabs(v);
    if (a <= d) return 0;
    float u = (a - d) / (1 - d);
    if (ui.prefs.curve == 1) u = u * u * (3 - 2 * u) * 0.5f + u * 0.5f;
    else if (ui.prefs.curve == 2) u = u * u;
    return m_copysign(MIN(u, 1.0f), v);
}

void ui_sticks(float lx, float ly, float rx, float ry) { ui.lx = shape(lx); ui.ly = shape(ly); ui.rx = shape(rx); ui.ry = shape(ry); }
void ui_power(int pct, bool charging) { ui.battery = pct; ui.charging = charging; }

// ------------------------------------------------------------------------------------------- the frame

void ui_init(void *mem, size_t bytes, float sr)
{
    m_set(&ui, 0, sizeof ui);
    ui.sr = sr;
    ui.battery = -1;
    ui.cur_s = 8;
    pool = mem; pool_top = mem; pool_end = (uint8_t *) mem + bytes;
    if (bytes > 3 * PREVIEW_BYTES) { prev_mem[0] = pool_end - 2 * PREVIEW_BYTES; prev_mem[1] = pool_end - PREVIEW_BYTES; pool_end = prev_mem[0]; }
    nloaded = 0;
    m_set(prev, 0, sizeof prev);
    m_set(loaded, 0, sizeof loaded);
    prefs_load();
    ui.perf_fx_x = ui.prefs.r_x; ui.perf_fx_y = ui.prefs.r_y;
    if (ui.prefs.quality) ui_msg(M_QUALITY, ui.prefs.quality, 0, 0, 0);
    doc_defaults(&doc);
    doc_all();
    sync();
    if (plat_file_size("SAMPLES/README.TXT") < 0) {
        static const char note[] = "Put your WAV files here (any folders inside). The rhythm machine's BROWSER finds them.\n";
        plat_file_write("SAMPLES/README.TXT", note, (int) sizeof note - 1);
    }
    if (!ui.prefs.project[0] || !proj_load(ui.prefs.project)) {
        for (int t = 0; t < NTRK; t++) ui_bind(t);
        ui.modified = false;
    }
}

void ui_quit(void)
{
    if (ui.modified) proj_save(doc.name);
    prefs_save();
}

const uint32_t *ui_frame(uint32_t ms)
{
    ui.now = ms;
    const uint32_t rep = CLAMP(ui.prefs.repeat_ms, 30, 250);
    for (int b = BTN_UP; b <= BTN_RIGHT; b++)
        if (ui.held[b] && ms >= ui.rep_at[b]) { ui.rep_at[b] = ms + rep; dispatch(b, true, true); }
    status_read(&ui.st);
    for (int t = 0; t < NTRK; t++)
        if (ui.st.hits[t] != ui.seen_hits[t]) { ui.seen_hits[t] = ui.st.hits[t]; ui.flash[t] = ms + 110; }
    msg_t m;
    while (ring_get(&to_ui, &m, sizeof m)) {
        if (m.type == M_ACK && m.a == M_CAPTURE) capture_done();
        else if (m.type == M_MON) midi_monitor(m.a, m.b, m.c, (uint8_t) m.f);
    }
    if (ui.overlay == OV_NONE || ui.overlay == OV_STEP) { if (ui.screen == NAV_PATTERN) pattern_sticks(); }
    perform_sticks();
    sync();
    proj_autosave();
    g_noclip();
    if (ui.overlay == OV_PADS) draw_pads();
    else {
        screens[ui.screen].draw();
        if (ui.overlay == OV_STEP) draw_step();
        else if (ui.overlay == OV_MENU) draw_menu();
        else if (ui.overlay == OV_NAME) draw_name();
        else if (ui.overlay == OV_CONFIRM) draw_confirm();
    }
    w_toast();
    return gfx_fb;
}

void core_init(float sr, void *pool_mem, size_t bytes)
{
    link_init();
    eng_init(sr);
    ui_init(pool_mem, bytes, sr);
}
