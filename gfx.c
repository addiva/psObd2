/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: gfx.c  |  Modulo: grafica 2D (GU su PSP, rasterizzatore software su PC)
 *
 * NOTE PSP:
 *  - sceGuDrawBuffer/DispBuffer/DepthBuffer vogliono offset RELATIVI all'inizio
 *    della VRAM (0, 0x88000, ...): la libreria GU somma da sola l'indirizzo base.
 *  - Il testo viene scritto dalla CPU nel draw buffer dopo sceGuSync(), usando
 *    l'alias non-cached della VRAM (base | 0x40000000) per evitare pixel
 *    rimasti nella data cache.
 */
#include "gfx.h"
#include "font.h"
#include <string.h>
#include <math.h>

/* ---------------- Coda del testo (comune) ---------------- */
#define TEXTQ_MAX   256
#define TEXTQ_POOL  12288

typedef struct {
    short x, y;
    uint32_t color;
    unsigned char scale;
    unsigned short off;
} textq_item_t;

static textq_item_t s_tq[TEXTQ_MAX];
static char         s_tq_pool[TEXTQ_POOL];
static int          s_tq_n, s_tq_used;

void gfx_text(int x, int y, uint32_t color, int scale, const char *s) {
    if (!s || !*s || s_tq_n >= TEXTQ_MAX) return;
    int len = (int)strlen(s);
    if (s_tq_used + len + 1 > TEXTQ_POOL) return;
    textq_item_t *it = &s_tq[s_tq_n++];
    it->x = (short)x; it->y = (short)y;
    it->color = color;
    it->scale = (unsigned char)(scale < 1 ? 1 : scale);
    it->off = (unsigned short)s_tq_used;
    memcpy(s_tq_pool + s_tq_used, s, (size_t)len + 1);
    s_tq_used += len + 1;
}

int gfx_text_w(const char *s, int scale) { return font_text_width(s, scale); }

static void textq_flush(uint32_t *fb) {
    for (int i = 0; i < s_tq_n; i++) {
        const textq_item_t *it = &s_tq[i];
        font_draw(fb, GFX_STRIDE, GFX_W, GFX_H, it->x, it->y,
                  it->color, it->scale, s_tq_pool + it->off);
    }
    s_tq_n = 0;
    s_tq_used = 0;
}

void gfx_frame(int x, int y, int w, int h, int t, uint32_t color) {
    gfx_rect(x, y, w, t, color);
    gfx_rect(x, y + h - t, w, t, color);
    gfx_rect(x, y + t, t, h - 2 * t, color);
    gfx_rect(x + w - t, y + t, t, h - 2 * t, color);
}

#ifdef PSP_BUILD
/* =========================== PSP / GU =========================== */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>

#define FRAME_BYTES (GFX_STRIDE * GFX_H * 4)   /* 0x88000 */

static unsigned int __attribute__((aligned(16))) s_list[0x10000];
static void *s_draw_rel;                       /* draw buffer (offset VRAM) */

int gfx_init(void) {
    void *fbp0 = (void *)0;
    void *fbp1 = (void *)FRAME_BYTES;
    void *zbp  = (void *)(FRAME_BYTES * 2);

    sceGuInit();
    sceGuStart(GU_DIRECT, s_list);
    sceGuDrawBuffer(GU_PSM_8888, fbp0, GFX_STRIDE);
    sceGuDispBuffer(GFX_W, GFX_H, fbp1, GFX_STRIDE);
    sceGuDepthBuffer(zbp, GFX_STRIDE);
    sceGuOffset(2048 - GFX_W / 2, 2048 - GFX_H / 2);
    sceGuViewport(2048, 2048, GFX_W, GFX_H);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, GFX_W, GFX_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_FLAT);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    s_draw_rel = fbp0;
    return 0;
}

void gfx_shutdown(void) {
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
}

void gfx_begin(uint32_t clear_color) {
    sceGuStart(GU_DIRECT, s_list);
    sceGuClearColor(clear_color);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

void gfx_end(void) {
    sceGuFinish();
    sceGuSync(0, 0);
    /* Testo: CPU -> VRAM non-cached del draw buffer appena completato */
    uint32_t *fb = (uint32_t *)(0x40000000u |
                   ((uint32_t)sceGeEdramGetAddr() + (uint32_t)s_draw_rel));
    textq_flush(fb);
    sceDisplayWaitVblankStart();
    s_draw_rel = sceGuSwapBuffers();
}

typedef struct { short x, y, z; } v2s_t;
typedef struct { float x, y, z; } v2f_t;
typedef struct { short u, v; short x, y, z; } v2t_t;

void gfx_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    v2s_t *v = (v2s_t *)sceGuGetMemory(2 * sizeof(v2s_t));
    v[0].x = (short)x;       v[0].y = (short)y;       v[0].z = 0;
    v[1].x = (short)(x + w); v[1].y = (short)(y + h); v[1].z = 0;
    sceGuColor(color);
    sceGuDrawArray(GU_SPRITES, GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, v);
}

void gfx_arc(int cx, int cy, int r, int thickness,
             float from_deg, float to_deg, uint32_t color) {
    if (to_deg <= from_deg || r <= 0) return;
    int segs = (int)((to_deg - from_deg) / 6.0f) + 1;
    if (segs > 64) segs = 64;
    float step = (to_deg - from_deg) / segs;
    float ri = (float)(r - thickness);
    v2f_t *v = (v2f_t *)sceGuGetMemory((segs + 1) * 2 * sizeof(v2f_t));
    for (int i = 0; i <= segs; i++) {
        float a = (from_deg + step * i) * 3.14159265f / 180.0f;
        float c = cosf(a), s = sinf(a);
        v[i * 2 + 0].x = cx + ri * c; v[i * 2 + 0].y = cy + ri * s; v[i * 2 + 0].z = 0;
        v[i * 2 + 1].x = cx + r * c;  v[i * 2 + 1].y = cy + r * s;  v[i * 2 + 1].z = 0;
    }
    sceGuColor(color);
    sceGuDrawArray(GU_TRIANGLE_STRIP, GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   (segs + 1) * 2, NULL, v);
}

static int pow2_ge(int v) { int p = 1; while (p < v) p <<= 1; return p; }

void gfx_image(const uint32_t *rgba, int w, int h, int stride,
               int dx, int dy, int dw, int dh) {
    if (!rgba || w <= 0 || h <= 0) return;
    /* I dati arrivano dal decoder: scarica la cache prima che il GE li legga */
    sceKernelDcacheWritebackRange(rgba, (unsigned int)(stride * h * 4));
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    /* Dimensioni texture: potenze di 2 >= immagine; tbw = stride reale */
    sceGuTexImage(0, pow2_ge(w), pow2_ge(h), stride, rgba);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexFlush();
    /* Strisce larghe 64 px: piu' efficienti per la texture cache del GE */
    int strips = (dw + 63) / 64;
    v2t_t *v = (v2t_t *)sceGuGetMemory(strips * 2 * sizeof(v2t_t));
    for (int i = 0; i < strips; i++) {
        int x0 = i * 64, x1 = (i + 1) * 64 > dw ? dw : (i + 1) * 64;
        v[i * 2 + 0].u = (short)(x0 * w / dw); v[i * 2 + 0].v = 0;
        v[i * 2 + 0].x = (short)(dx + x0);     v[i * 2 + 0].y = (short)dy; v[i * 2 + 0].z = 0;
        v[i * 2 + 1].u = (short)(x1 * w / dw); v[i * 2 + 1].v = (short)h;
        v[i * 2 + 1].x = (short)(dx + x1);     v[i * 2 + 1].y = (short)(dy + dh); v[i * 2 + 1].z = 0;
    }
    sceGuColor(0xFFFFFFFF);
    sceGuDrawArray(GU_SPRITES,
                   GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                   strips * 2, NULL, v);
    sceGuDisable(GU_TEXTURE_2D);
}

#else
/* ===================== PC: rasterizzatore software ===================== */
static uint32_t s_canvas[GFX_STRIDE * GFX_H];

int  gfx_init(void) { memset(s_canvas, 0, sizeof(s_canvas)); return 0; }
void gfx_shutdown(void) {}
const uint32_t *gfx_soft_pixels(void) { return s_canvas; }

void gfx_begin(uint32_t clear_color) {
    for (int i = 0; i < GFX_STRIDE * GFX_H; i++) s_canvas[i] = clear_color;
    s_tq_n = 0; s_tq_used = 0;
}

void gfx_end(void) { textq_flush(s_canvas); }

static void put(int x, int y, uint32_t c) {
    if (x < 0 || y < 0 || x >= GFX_W || y >= GFX_H) return;
    s_canvas[y * GFX_STRIDE + x] = c;
}

void gfx_rect(int x, int y, int w, int h, uint32_t color) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) put(i, j, color);
}

void gfx_arc(int cx, int cy, int r, int thickness,
             float from_deg, float to_deg, uint32_t color) {
    if (to_deg <= from_deg) return;
    float ri = (float)(r - thickness);
    for (int y = cy - r; y <= cy + r; y++)
        for (int x = cx - r; x <= cx + r; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < ri || d > r) continue;
            float a = atan2f(dy, dx) * 180.0f / 3.14159265f;
            while (a < from_deg) a += 360.0f;
            if (a <= to_deg) put(x, y, color);
        }
}

void gfx_image(const uint32_t *rgba, int w, int h, int stride,
               int dx, int dy, int dw, int dh) {
    if (!rgba) return;
    for (int y = 0; y < dh; y++)
        for (int x = 0; x < dw; x++)
            put(dx + x, dy + y, rgba[(y * h / dh) * stride + (x * w / dw)]);
}
#endif
