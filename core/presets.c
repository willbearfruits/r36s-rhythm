#include "presets.h"

int drum_preset_find(const char *name)
{
    for (int i = 0; i < drum_preset_count; i++) if (!k_strcmp(drum_presets[i].name, name)) return i;
    return -1;
}

void drum_preset_values(int i, float *v, bool levelled)
{
    const drum_preset_t *p = &drum_presets[CLAMP(i, 0, drum_preset_count - 1)];
    drum_defaults(v, p->type);
    for (const drum_pv_t *s = p->set; s->p >= 0; s++) v[s->p] = s->v;
    if (levelled) v[P_LEVEL] = drum_preset_level[CLAMP(i, 0, drum_preset_count - 1)];
}
