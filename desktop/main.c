// The PC host: the instrument in a window (SDL2), sound through SDL, MIDI through ALSA. Keys and pads map onto the
// R36S's buttons by position, so what the fingers learn here works on the handheld.
//
//   arrows            D-pad              Z X / A S        B A / Y X (where they sit on the R36S)
//   Q W  /  O P       L1 L2 / R1 R2      Enter            START
//   Backspace, RShift SELECT             I J K L          left stick      numpad 8 4 5 6  right stick
//   F11               full screen        Esc              quit
//
// An Xbox-style pad works by position too: bottom = B, right = A, left = Y, top = X; LB/LT = L1/L2, RB/RT = R1/R2.
// Hold SELECT + START for a second and a half to quit. Card: files are under $CARD (default build/card).
// On the R36S (no desktop): full screen, drums at 2x oversampling (RHYTHM_OS=4 for full), a 512-frame buffer
// (RHYTHM_BUFFER), a log at $RHYTHM_LOG.
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../core/core.h"

void midi_open(void);
void midi_close(void);
void midi_ports(char *out, int cap);
int workers_start(int n);
void realtime_thread(int prio);

#define SR 48000

static FILE *logf_;
static void say(const char *fmt, const char *a, int b)
{
    if (!logf_) logf_ = fopen(getenv("RHYTHM_LOG") ? getenv("RHYTHM_LOG") : "/dev/null", "w");
    fprintf(stderr, fmt, a, b); fputc('\n', stderr);
    if (logf_) { fprintf(logf_, fmt, a, b); fputc('\n', logf_); fflush(logf_); fsync(fileno(logf_)); }
}

// A pad SDL does not know as a game controller (the R36S's own, without a mapping): buttons by number, as the
// GO-Super layout ArkOS uses (PortMaster's gamecontrollerdb): 0 right, 1 bottom, 2 left, 3 top, then L1 R1 L2 R2,
// the D-pad, SELECT START, L3 R3. Every press is logged, so a wrong one shows up in the log.
static int joy_button(int b)
{
    static const int map[] = { BTN_A, BTN_B, BTN_Y, BTN_X, BTN_L1, BTN_R1, BTN_L2, BTN_R2,
                               BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_SELECT, BTN_START, BTN_L3, BTN_R3 };
    return b >= 0 && b < (int) (sizeof map / sizeof map[0]) ? map[b] : -1;
}

// Blocks that came late: rendering took longer than the block lasts, or the call came long after the last one.
static volatile int late_blocks;

static void audio(void *user, Uint8 *stream, int len)
{
    (void) user;
    static bool rt;
    static Uint64 last;
    if (!rt) { rt = true; realtime_thread(80); }
    float *out = (float *) stream;
    const int n = len / (int) (2 * sizeof(float));
    const Uint64 t0 = SDL_GetPerformanceCounter(), hz = SDL_GetPerformanceFrequency();
    const double block = (double) n / SR;
    if (last && (double) (t0 - last) / (double) hz > block * 1.9) late_blocks++;
    last = t0;
    static float L[4096], R[4096];
    for (int done = 0; done < n;) {
        const int k = n - done < 4096 ? n - done : 4096;
        eng_render(L, R, k);
        for (int i = 0; i < k; i++) { out[2 * (done + i)] = L[i]; out[2 * (done + i) + 1] = R[i]; }
        done += k;
    }
    if ((double) (SDL_GetPerformanceCounter() - t0) / (double) hz > block) late_blocks++;
}

static int key_button(SDL_Keycode k)
{
    switch (k) {
    case SDLK_UP: return BTN_UP;
    case SDLK_DOWN: return BTN_DOWN;
    case SDLK_LEFT: return BTN_LEFT;
    case SDLK_RIGHT: return BTN_RIGHT;
    case SDLK_x: return BTN_A;
    case SDLK_z: return BTN_B;
    case SDLK_s: return BTN_X;
    case SDLK_a: return BTN_Y;
    case SDLK_q: return BTN_L1;
    case SDLK_w: return BTN_L2;
    case SDLK_o: return BTN_R1;
    case SDLK_p: return BTN_R2;
    case SDLK_RETURN: return BTN_START;
    case SDLK_BACKSPACE: case SDLK_RSHIFT: return BTN_SELECT;
    default: return -1;
    }
}

static int pad_button(int b)
{
    switch (b) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return BTN_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return BTN_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return BTN_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return BTN_RIGHT;
    case SDL_CONTROLLER_BUTTON_A: return BTN_B;      // bottom
    case SDL_CONTROLLER_BUTTON_B: return BTN_A;      // right
    case SDL_CONTROLLER_BUTTON_X: return BTN_Y;      // left
    case SDL_CONTROLLER_BUTTON_Y: return BTN_X;      // top
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return BTN_L1;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return BTN_R1;
    case SDL_CONTROLLER_BUTTON_BACK: return BTN_SELECT;
    case SDL_CONTROLLER_BUTTON_START: return BTN_START;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return BTN_L3;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return BTN_R3;
    default: return -1;
    }
}

int main(int argc, char **argv)
{
    (void) argc; (void) argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
#if defined(__aarch64__) || defined(__arm__)
    eng_drum_os = 2;
#endif
    if (getenv("RHYTHM_OS")) eng_drum_os = atoi(getenv("RHYTHM_OS"));
    const size_t pool = (size_t) (getenv("RHYTHM_POOL_MB") ? atoi(getenv("RHYTHM_POOL_MB")) : 256) << 20;
    core_init(SR, malloc(pool), pool);
    // the tracks on the other cores: all but one on a 4-core R36S (the screen keeps one), up to 3 on a PC
    const long cores = sysconf(_SC_NPROCESSORS_ONLN);
    const int nw = getenv("RHYTHM_WORKERS") ? atoi(getenv("RHYTHM_WORKERS")) : (int) (cores >= 8 ? 3 : cores - 2 > 0 ? cores - 2 : 0);
    const int got = workers_start(nw);
    const bool desktop = getenv("DISPLAY") || getenv("WAYLAND_DISPLAY");
    ui_power(-1, false);

    SDL_Window *win = SDL_CreateWindow("R36S rhythm machine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       desktop ? 1280 : 640, desktop ? 960 : 480, desktop ? SDL_WINDOW_RESIZABLE : SDL_WINDOW_FULLSCREEN);
    if (!win) { fprintf(stderr, "no window: %s\n", SDL_GetError()); return 1; }
    say("video: %s (%d)", SDL_GetCurrentVideoDriver(), desktop);
    say("cores: %s%d workers", "", got);
    if (!desktop) SDL_ShowCursor(SDL_DISABLE);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_RenderSetLogicalSize(ren, 640, 480);
    SDL_RenderSetIntegerScale(ren, SDL_TRUE);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, 640, 480);

    SDL_AudioSpec want = { 0 }, have = { 0 };
    want.freq = SR; want.format = AUDIO_F32SYS; want.channels = 2; want.samples = (Uint16) (getenv("RHYTHM_BUFFER") ? atoi(getenv("RHYTHM_BUFFER")) : ui_audio_buffer() ? ui_audio_buffer() : desktop ? 256 : 512); want.callback = audio;
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!dev) fprintf(stderr, "no audio: %s\n", SDL_GetError());
    else if (have.freq != SR) fprintf(stderr, "audio runs at %d Hz, not %d: pitches will be off\n", have.freq, SR);
    if (dev) { SDL_PauseAudioDevice(dev, 0); say("audio: %s, buffer %d", SDL_GetCurrentAudioDriver(), have.samples); }
    midi_open();

    SDL_GameController *pad = NULL;
    SDL_Joystick *joy = NULL;
    Uint32 quit_at = 0;
    bool held_sel = false, held_start = false;
    float lx = 0, ly = 0, rx = 0, ry = 0;
    bool l2 = false, r2 = false, full = false, quit = false;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: quit = true; break;
            case SDL_KEYDOWN: case SDL_KEYUP: {
                if (e.key.repeat) break;
                const bool down = e.type == SDL_KEYDOWN;
                const SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE && down) quit = true;
                else if (k == SDLK_F11 && down) { full = !full; SDL_SetWindowFullscreen(win, full ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0); }
                else if (k == SDLK_j) lx = down ? -1 : 0;
                else if (k == SDLK_l) lx = down ? 1 : 0;
                else if (k == SDLK_i) ly = down ? -1 : 0;
                else if (k == SDLK_k) ly = down ? 1 : 0;
                else if (k == SDLK_KP_4) rx = down ? -1 : 0;
                else if (k == SDLK_KP_6) rx = down ? 1 : 0;
                else if (k == SDLK_KP_8) ry = down ? -1 : 0;
                else if (k == SDLK_KP_5) ry = down ? 1 : 0;
                else { const int b = key_button(k); if (b >= 0) ui_button(b, down); }
                break;
            }
            case SDL_CONTROLLERDEVICEADDED: if (!pad) { pad = SDL_GameControllerOpen(e.cdevice.which); say("pad: %s (game controller)", SDL_GameControllerName(pad), 0); } break;
            case SDL_JOYDEVICEADDED:
                if (!SDL_IsGameController(e.jdevice.which) && !joy) { joy = SDL_JoystickOpen(e.jdevice.which); say("pad: %s (raw: no mapping, buttons by number)", SDL_JoystickName(joy), 0); }
                break;
            case SDL_JOYBUTTONDOWN: case SDL_JOYBUTTONUP:
                if (joy && e.jbutton.which == SDL_JoystickInstanceID(joy)) {
                    if (e.type == SDL_JOYBUTTONDOWN) say("raw button %s%d", "", e.jbutton.button);
                    const int b = joy_button(e.jbutton.button);
                    if (b >= 0) ui_button(b, e.type == SDL_JOYBUTTONDOWN);
                }
                break;
            case SDL_JOYHATMOTION:
                if (joy && e.jhat.which == SDL_JoystickInstanceID(joy)) {
                    ui_button(BTN_UP, e.jhat.value & SDL_HAT_UP); ui_button(BTN_DOWN, e.jhat.value & SDL_HAT_DOWN);
                    ui_button(BTN_LEFT, e.jhat.value & SDL_HAT_LEFT); ui_button(BTN_RIGHT, e.jhat.value & SDL_HAT_RIGHT);
                }
                break;
            case SDL_JOYAXISMOTION:
                if (joy && e.jaxis.which == SDL_JoystickInstanceID(joy)) {
                    const float v = (float) e.jaxis.value / 32767.0f;
                    if (e.jaxis.axis == 0) lx = v; else if (e.jaxis.axis == 1) ly = v; else if (e.jaxis.axis == 2) rx = v; else if (e.jaxis.axis == 3) ry = v;
                }
                break;
            case SDL_CONTROLLERDEVICEREMOVED: if (pad && e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))) { SDL_GameControllerClose(pad); pad = NULL; } break;
            case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP: {
                const int b = pad_button(e.cbutton.button);
                if (b == BTN_SELECT) held_sel = e.type == SDL_CONTROLLERBUTTONDOWN;
                if (b == BTN_START) held_start = e.type == SDL_CONTROLLERBUTTONDOWN;
                if (b >= 0) ui_button(b, e.type == SDL_CONTROLLERBUTTONDOWN);
                break;
            }
            case SDL_CONTROLLERAXISMOTION: {
                const float v = (float) e.caxis.value / 32767.0f;
                switch (e.caxis.axis) {
                case SDL_CONTROLLER_AXIS_LEFTX: lx = v; break;
                case SDL_CONTROLLER_AXIS_LEFTY: ly = v; break;
                case SDL_CONTROLLER_AXIS_RIGHTX: rx = v; break;
                case SDL_CONTROLLER_AXIS_RIGHTY: ry = v; break;
                case SDL_CONTROLLER_AXIS_TRIGGERLEFT: if ((v > 0.5f) != l2) { l2 = v > 0.5f; ui_button(BTN_L2, l2); } break;
                case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: if ((v > 0.5f) != r2) { r2 = v > 0.5f; ui_button(BTN_R2, r2); } break;
                default: break;
                }
                break;
            }
            default: break;
            }
        }
        // SELECT + START held for a second and a half: leave (back to ArkOS)
        if (joy) { held_sel = SDL_JoystickGetButton(joy, 12); held_start = SDL_JoystickGetButton(joy, 13); }
        if (held_sel && held_start) { if (!quit_at) quit_at = SDL_GetTicks() + 1500; else if (SDL_GetTicks() > quit_at) quit = true; }
        else quit_at = 0;
        ui_sticks(lx, ly, rx, ry);
        static uint32_t ports_at;
        if (SDL_GetTicks() >= ports_at) {
            char p[160];
            midi_ports(p, sizeof p); ui_midi_ports(p);
            snprintf(p, sizeof p, "%s  %d HZ  %d FRAMES  %.1f MS  %d WORKERS  LATE %d", dev ? SDL_GetCurrentAudioDriver() : "NO AUDIO",
                     have.freq, have.samples, have.samples * 1000.0 / (have.freq ? have.freq : SR), got, late_blocks);
            for (char *c = p; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
            ui_host_info(p);
            ports_at = SDL_GetTicks() + 1000;
        }
        const uint32_t *fb = ui_frame(SDL_GetTicks());
        SDL_UpdateTexture(tex, NULL, fb, 640 * 4);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }
    ui_quit();
    sync();
    midi_close();
    if (dev) SDL_CloseAudioDevice(dev);
    SDL_Quit();
    return 0;
}
