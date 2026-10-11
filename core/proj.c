// Projects on the card: PROJECTS/NAME.RHY, a short header and the document as it is in memory. The open one is
// saved by itself a few seconds after the last change (the R36S can be switched off at any moment) and comes back
// at the next start.
#include "plat.h"
#include "ui_int.h"

#define PROJ_MAGIC 0x50594852u   // "RHYP"
typedef struct { uint32_t magic, version, size, pad; } proj_head_t;
static uint8_t filebuf[sizeof(proj_head_t) + sizeof(doc_t)];

static void file_name(const char *name, char *out, int cap)
{
    char f[32]; int n = 0;
    for (const char *c = name; *c && n < 28; c++) {
        char x = *c;
        if (x >= 'a' && x <= 'z') x -= 32;
        if ((x >= 'A' && x <= 'Z') || (x >= '0' && x <= '9') || x == '-' || x == '_') f[n++] = x;
        else if (x == ' ' || x == '.') f[n++] = '_';
    }
    if (!n) f[n++] = 'X';
    f[n] = 0;
    k_snprintf(out, cap, "PROJECTS/%s.RHY", f);
}

bool proj_save(const char *name)
{
    k_strcpy(doc.name, name, sizeof doc.name);
    doc_touch(doc.name, sizeof doc.name);
    char path[64];
    file_name(name, path, sizeof path);
    const proj_head_t h = { PROJ_MAGIC, DOC_VERSION, (uint32_t) sizeof(doc_t), 0 };
    m_copy(filebuf, &h, sizeof h);
    m_copy(filebuf + sizeof h, &doc, sizeof doc);
    if (plat_file_write(path, filebuf, (int) sizeof filebuf) != (int) sizeof filebuf) return false;
    ui.modified = false;
    ui.saved_at = ui.now;
    k_strcpy(ui.prefs.project, name, sizeof ui.prefs.project);
    prefs_save();
    return true;
}

bool proj_load(const char *name)
{
    char path[64];
    file_name(name, path, sizeof path);
    if (plat_file_read(path, filebuf, (int) sizeof filebuf) != (int) sizeof filebuf) return false;
    proj_head_t h;
    m_copy(&h, filebuf, sizeof h);
    if (h.magic != PROJ_MAGIC || h.version != DOC_VERSION || h.size != sizeof(doc_t)) return false;
    ui_msg(M_STOP, 0, 0, 0, 0);
    m_copy(&doc, filebuf + sizeof h, sizeof doc);
    doc_all();
    for (int t = 0; t < NTRK; t++) ui_bind(t);
    ui.pat = 0; ui.cur_t = 0; ui.cur_s = 0;
    ui_msg(M_QUEUE, 0, 1, 0, 0);
    ui.modified = false;
    ui.saved_at = ui.now;
    k_strcpy(ui.prefs.project, doc.name, sizeof ui.prefs.project);
    prefs_save();
    return true;
}

int proj_list(char (*names)[32], int max)
{
    int n = 0;
    char e[64];
    bool dir;
    for (int i = 0; n < max && plat_dir("PROJECTS", i, e, sizeof e, &dir); i++) {
        const int l = k_strlen(e);
        if (dir || l < 5 || k_strcmp(e + l - 4, ".RHY")) continue;
        e[l - 4] = 0;
        for (char *c = e; *c; c++) if (*c == '_') *c = ' ';
        k_strcpy(names[n++], e, 32);
    }
    // alphabetical
    for (int i = 1; i < n; i++) for (int j = i; j > 0 && k_strcmp(names[j - 1], names[j]) > 0; j--) {
        char t[32]; k_strcpy(t, names[j], 32); k_strcpy(names[j], names[j - 1], 32); k_strcpy(names[j - 1], t, 32);
    }
    return n;
}

void proj_new(void)
{
    ui_msg(M_STOP, 0, 0, 0, 0);
    doc_defaults(&doc);
    for (int i = 0; i < NPAT; i++) doc_pattern_clear(&doc.pat[i]);
    doc.song_len = 1;
    doc.song[0] = (song_t) { 0, 1, 0, 0, 0, 0, 0 };
    char name[32];
    char (*have)[32] = (char (*)[32]) filebuf;
    const int n = proj_list(have, (int) (sizeof filebuf / 32));
    for (int k = 1;; k++) {
        k_snprintf(name, sizeof name, "UNTITLED %d", k);
        bool used = false;
        for (int i = 0; i < n; i++) if (!k_strcmp(have[i], name)) used = true;
        if (!used) break;
    }
    k_strcpy(doc.name, name, sizeof doc.name);
    doc_all();
    for (int t = 0; t < NTRK; t++) ui_bind(t);
    ui.pat = 0;
    ui_msg(M_QUEUE, 0, 1, 0, 0);
    proj_save(name);
}

void proj_autosave(void)
{
    if (ui.modified && ui.now - ui.modified_at > 4000 && ui.now - ui.saved_at > 20000) proj_save(doc.name);
}
