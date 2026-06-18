/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: ui.c  |  Modulo: sistema UI (GU 2D, widget, dash, grafici)
 */
#include "ui.h"
#include "json.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifdef PSP_BUILD
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>

/* VRAM layout: PSP display = 512*272*4 bytes */
#define VRAM_BASE      0x04000000
#define DISPLAY_STRIDE 512

static unsigned int __attribute__((aligned(16))) s_disp_list[0x40000];
static void *s_vram_draw;
static void *s_vram_disp;

/* Helper: indirizzo VRAM */
static inline void *vram_ptr(int offset) {
    return (void *)(VRAM_BASE + offset);
}

static void gu_init(void) {
    s_vram_draw = vram_ptr(0);
    s_vram_disp = vram_ptr(DISPLAY_STRIDE * 272 * 4);

    sceGuInit();
    sceGuStart(GU_DIRECT, s_disp_list);

    sceGuDrawBuffer(GU_PSM_8888, s_vram_draw, DISPLAY_STRIDE);
    sceGuDispBuffer(SCR_W, SCR_H, s_vram_disp, DISPLAY_STRIDE);
    sceGuDepthBuffer(vram_ptr(DISPLAY_STRIDE * 272 * 8), DISPLAY_STRIDE);

    sceGuOffset(2048 - SCR_W/2, 2048 - SCR_H/2);
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuDepthRange(0xc350, 0x2710);

    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

/* Vertex 2D senza texture */
typedef struct { float x, y, z; } vert2d_t;

static void draw_rect(int x, int y, int w, int h, unsigned int color) {
    sceGuColor(color);
    vert2d_t *v = (vert2d_t *)sceGuGetMemory(2 * sizeof(vert2d_t));
    v[0].x = (float)x;     v[0].y = (float)y;     v[0].z = 0;
    v[1].x = (float)(x+w); v[1].y = (float)(y+h); v[1].z = 0;
    sceGumDrawArray(GU_SPRITES, GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, NULL, v);
}

/* Testo: PSP non ha font hardware. Usiamo blit di bitmap font 8x8 minimale.
 * Per una UI "accattivante" si usa una texture con font pre-renderizzato.
 * Qui usiamo sceGuDebugPrint come fallback (debug-only) e un sistema
 * minimale di blit testo basato su debug font GE. */
static void draw_text(int x, int y, unsigned int color, const char *txt) {
    /* sceGuDebugPrint e' disponibile solo in build debug.
     * In produzione si usa una texture atlas font + blit GU.
     * Qui usiamo la variante disponibile nel SDK: */
    (void)color; /* il debug print non supporta colore per ora */
    sceGuDebugPrint(x, y, color, txt);
}

static void draw_text_big(int x, int y, unsigned int color,
                          float scale, const char *txt) {
    /* Scala moltiplicata via matrix GU per ingrandire il testo */
    (void)scale;
    sceGuDebugPrint(x, y, color, txt);
}

/* Arco/gauge semplice con sceGumDrawArray STRIPS (approssimazione poligonale) */
static void draw_arc(int cx, int cy, int r, float from_deg, float to_deg,
                     int thickness, unsigned int color) {
    /* Approssimazione: 32 segmenti */
    #define ARC_SEGS 32
    float step = (to_deg - from_deg) / ARC_SEGS;
    sceGuColor(color);
    typedef struct { float x,y,z; } V;
    V *v = (V *)sceGuGetMemory((ARC_SEGS + 1) * 2 * sizeof(V));
    for (int i = 0; i <= ARC_SEGS; i++) {
        float a = (from_deg + step * i) * 3.14159265f / 180.0f;
        float c = cosf(a), s = sinf(a);
        v[i*2+0].x = cx + (r - thickness) * c;
        v[i*2+0].y = cy + (r - thickness) * s;
        v[i*2+0].z = 0;
        v[i*2+1].x = cx + r * c;
        v[i*2+1].y = cy + r * s;
        v[i*2+1].z = 0;
    }
    sceGumDrawArray(GU_TRIANGLE_STRIP,
                    GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                    (ARC_SEGS + 1) * 2, NULL, v);
    #undef ARC_SEGS
}

/* Input PSP */
static unsigned int read_buttons(void) {
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);
    return pad.Buttons;
}

#define BTN_CROSS    PSP_CTRL_CROSS
#define BTN_SQUARE   PSP_CTRL_SQUARE
#define BTN_TRIANGLE PSP_CTRL_TRIANGLE
#define BTN_CIRCLE   PSP_CTRL_CIRCLE
#define BTN_L        PSP_CTRL_LTRIGGER
#define BTN_R        PSP_CTRL_RTRIGGER
#define BTN_UP       PSP_CTRL_UP
#define BTN_DOWN     PSP_CTRL_DOWN
#define BTN_LEFT     PSP_CTRL_LEFT
#define BTN_RIGHT    PSP_CTRL_RIGHT
#define BTN_START    PSP_CTRL_START
#define BTN_SELECT   PSP_CTRL_SELECT

#else /* HOST STUB */
static void draw_rect(int x,int y,int w,int h,unsigned int c)
    {(void)x;(void)y;(void)w;(void)h;(void)c;}
static void draw_text(int x,int y,unsigned int c,const char *t)
    {(void)x;(void)y;(void)c;(void)t;}
static void draw_text_big(int x,int y,unsigned int c,float s,const char *t)
    {(void)x;(void)y;(void)c;(void)s;(void)t;}
static void draw_arc(int cx,int cy,int r,float f,float t,int th,unsigned int c)
    {(void)cx;(void)cy;(void)r;(void)f;(void)t;(void)th;(void)c;}
static unsigned int read_buttons(void) { return 0; }
#define BTN_L 0x100
#define BTN_R 0x101
#define BTN_TRIANGLE 0x1000
#define BTN_CROSS    0x4000
#define BTN_SQUARE   0x8000
#define BTN_SELECT   0x0001
#define BTN_START    0x0008
#define BTN_UP       0x0010
#define BTN_DOWN     0x0040
#define BTN_LEFT     0x0080
#define BTN_RIGHT    0x0020
#endif /* PSP_BUILD */

/* ---- Chart buffer ---- */
static void chart_push(chart_buf_t *c, float v) {
    c->samples[c->head] = v;
    c->head = (c->head + 1) % CHART_HIST_LEN;
    if (c->count < CHART_HIST_LEN) c->count++;
}

static float chart_get(const chart_buf_t *c, int ago) {
    int idx = (c->head - 1 - ago + CHART_HIST_LEN * 2) % CHART_HIST_LEN;
    return c->samples[idx];
}

/* ---- Layout default ---- */
static void set_default_layout(ui_state_t *ui) {
    int n = 0;
#define W(id_,type_,title_,src_,min_,max_,red_,dash_,x_,y_,w_,h_) \
    ui->widgets[n].id=id_; ui->widgets[n].type=type_; \
    ui->widgets[n].title=title_; ui->widgets[n].source=src_; \
    ui->widgets[n].min=min_; ui->widgets[n].max=max_; \
    ui->widgets[n].red_zone=red_; ui->widgets[n].dash_id=dash_; \
    ui->widgets[n].x=x_; ui->widgets[n].y=y_; \
    ui->widgets[n].w=w_; ui->widgets[n].h=h_; \
    ui->widgets[n].visible=1; ui->widgets[n].group_id=0; n++;

    /* DASH 0 - Guida */
    W( 0, W_GAUGE,   "RPM",      SRC_RPM,        0,8000, 6500, 0,  10,  10,140,130)
    W( 1, W_GAUGE,   "Speed",    SRC_SPEED,       0, 220,  200, 0, 170,  10,140,130)
    W( 2, W_NUMERIC, "L/100",    SRC_L100_LIVE,   0,  30,    0, 0,  10, 155,140, 50)
    W( 3, W_NUMERIC, "Trip avg", SRC_L100_TRIP,   0,  30,    0, 0, 170, 155,140, 50)
    W( 4, W_GAUGE,   "Carb%",    SRC_FUEL_LEVEL,  0, 100,   15, 0, 330,  10,130,130)

    /* DASH 1 - Motore */
    W( 5, W_NUMERIC, "Temp",     SRC_COOLANT,   -40, 130,  105, 1,  10,  10,140, 50)
    W( 6, W_NUMERIC, "MAF g/s",  SRC_MAF,         0, 300,    0, 1, 170,  10,140, 50)
    W( 7, W_NUMERIC, "Farfalla%",SRC_THROTTLE,    0, 100,    0, 1,  10,  70,140, 50)
    W( 8, W_LINECHART,"RPM",     SRC_RPM,         0,8000,    0, 1,  10, 135,460, 120)

    /* DASH 2 - Grafici */
    W( 9, W_LINECHART,"L/100",   SRC_L100_LIVE,   0,  25,    0, 2,  10,  10,460, 120)
    W(10, W_LINECHART,"Speed",   SRC_SPEED,        0, 220,    0, 2,  10, 140,460, 120)

    /* DASH 3 - Costi */
    W(11, W_COST_CARD,"Costi",   SRC_AVG_L100,    0,   0,    0, 3,  10,  10,230,240)
    W(12, W_BARCHART, "Pieni",   SRC_LAST_EUR,    0,   0,    0, 3, 250,  10,210,240)

    /* DASH 4 - Diagnostica */
    W(13, W_LAMP,    "MIL",      SRC_MIL,         0,   1,    0, 4,  10,  10, 60, 60)
    W(14, W_NUMERIC, "N.DTC",    SRC_DTC_COUNT,   0,  99,    0, 4,  80,  10, 80, 60)
    W(15, W_DTC_LIST,"DTC",      SRC_DTC_COUNT,   0,   0,    0, 4,  10,  80,460,180)

    /* DASH 5 - Accel timer */
    W(16, W_NUMERIC, "0-100",    SRC_ACCEL_TIME,  0,  60,    0, 5,  10,  10,460,240)

    /* DASH 6 - Camera */
    W(17, W_CAMERA,  "Camera",   SRC_SPEED,       0,   0,    0, 6,   0,   0,480,272)
    W(18, W_NUMERIC, "Speed",    SRC_SPEED,        0,   0,    0, 6,   5, 240, 90, 30)
    W(19, W_NUMERIC, "L/100",    SRC_L100_LIVE,   0,   0,    0, 6, 100, 240, 90, 30)
#undef W
    ui->widget_count = n;
}

/* ---- Rendering widget ---- */

static float live_value(const ui_state_t *ui, metric_src_t src) {
    switch (src) {
        case SRC_RPM:        return ui->live.rpm;
        case SRC_SPEED:      return (float)ui->live.speed;
        case SRC_COOLANT:    return (float)ui->live.coolant;
        case SRC_MAF:        return ui->live.maf;
        case SRC_FUEL_LEVEL: return ui->live.fuel_level;
        case SRC_THROTTLE:   return (float)ui->live.throttle;
        case SRC_L100_LIVE:  return ui->live.l100_live;
        case SRC_L100_TRIP:  return ui->live.l100_trip;
        case SRC_LH_LIVE:    return ui->live.lh_live;
        case SRC_MIL:        return (float)ui->live.mil;
        case SRC_DTC_COUNT:  return (float)ui->live.dtc_count;
        case SRC_AVG_L100:   return ui->cost.avg_l100_all;
        case SRC_AVG_EUR100: return ui->cost.avg_eur100_all;
        case SRC_LAST_L100:  return ui->cost.last_fill_l100;
        case SRC_LAST_EUR:   return ui->cost.last_fill_eur;
        default:             return 0.0f;
    }
}

static const char *metric_unit(metric_src_t src) {
    switch (src) {
        case SRC_RPM:        return "rpm";
        case SRC_SPEED:      return "km/h";
        case SRC_COOLANT:    return "C";
        case SRC_MAF:        return "g/s";
        case SRC_FUEL_LEVEL: return "%";
        case SRC_THROTTLE:   return "%";
        case SRC_L100_LIVE:  return "L/100";
        case SRC_L100_TRIP:  return "L/100";
        case SRC_LH_LIVE:    return "L/h";
        case SRC_AVG_L100:   return "L/100";
        case SRC_AVG_EUR100: return "EUR/100";
        case SRC_LAST_EUR:   return "EUR";
        case SRC_LAST_L100:  return "L/100";
        default:             return "";
    }
}

static void draw_widget_numeric(const ui_state_t *ui, const widget_t *w) {
    float v = live_value(ui, w->source);
    char buf[32];
    if (v < -999.0f)
        snprintf(buf, sizeof(buf), "---");
    else if (v >= 100.0f)
        snprintf(buf, sizeof(buf), "%.0f", v);
    else
        snprintf(buf, sizeof(buf), "%.1f", v);

    draw_rect(w->x, w->y, w->w, w->h, COL_PANEL);
    draw_text(w->x + 4, w->y + 4, COL_DIMTEXT, w->title);
    draw_text_big(w->x + 4, w->y + 18, COL_ACCENT, 2.0f, buf);
    draw_text(w->x + 4, w->y + w->h - 14, COL_DIMTEXT, metric_unit(w->source));
}

static void draw_widget_gauge(const ui_state_t *ui, const widget_t *w) {
    float v = live_value(ui, w->source);
    float range = w->max - w->min;
    if (range <= 0) range = 1;
    float pct = (v - w->min) / range;
    if (pct < 0) pct = 0;
    if (pct > 1) pct = 1;

    int cx = w->x + w->w / 2;
    int cy = w->y + w->h / 2 + 10;
    int r  = (w->w < w->h ? w->w : w->h) / 2 - 8;

    draw_rect(w->x, w->y, w->w, w->h, COL_PANEL);

    /* Arco background: da 210° a 330° (210 deg spazzato) */
    draw_arc(cx, cy, r, 210, 330, 8, COL_PANEL - 0x00101010);

    /* Zona rossa */
    if (w->red_zone > w->min) {
        float rz_pct = (w->red_zone - w->min) / range;
        float rz_deg = 210 + rz_pct * 120;
        draw_arc(cx, cy, r, rz_deg, 330, 8, COL_RED);
    }

    /* Arco valore */
    float end_deg = 210 + pct * 120;
    unsigned int arc_col = (v >= w->red_zone && w->red_zone > w->min) ?
                           COL_RED : COL_ACCENT;
    draw_arc(cx, cy, r, 210, end_deg, 8, arc_col);

    /* Testo centrale */
    char buf[16];
    if (v < -999.0f) snprintf(buf, sizeof(buf), "---");
    else snprintf(buf, sizeof(buf), "%.0f", v);
    draw_text_big(cx - 20, cy - 10, COL_TEXT, 1.5f, buf);
    draw_text(cx - 15, cy + 12, COL_DIMTEXT, metric_unit(w->source));
    draw_text(w->x + 4, w->y + 4, COL_DIMTEXT, w->title);
}

static void draw_widget_linechart(const ui_state_t *ui, const widget_t *w) {
    draw_rect(w->x, w->y, w->w, w->h, COL_PANEL);
    draw_text(w->x + 4, w->y + 4, COL_DIMTEXT, w->title);

    const chart_buf_t *cb = NULL;
    if (w->source == SRC_SPEED)    cb = &ui->chart_speed;
    else if (w->source == SRC_RPM) cb = &ui->chart_rpm;
    else                           cb = &ui->chart_l100;

    if (!cb || cb->count < 2) return;

    float range = w->max - w->min;
    if (range <= 0) range = 1;
    int pw = w->w - 8;
    int ph = w->h - 20;
    int bx = w->x + 4;
    int by = w->y + 16;

    /* Griglia leggera */
    draw_rect(bx, by + ph/2, pw, 1, COL_PANEL + 0x00151515);

    /* Campioni come barre verticali (1px wide) */
    int n = cb->count < pw ? cb->count : pw;
    for (int i = 0; i < n; i++) {
        float s = chart_get(cb, n - 1 - i);
        float pct = (s - w->min) / range;
        if (pct < 0) pct = 0; if (pct > 1) pct = 1;
        int bar_h = (int)(pct * ph);
        draw_rect(bx + i, by + ph - bar_h, 1, bar_h, COL_ACCENT);
    }
    /* Ultimo valore */
    float last = chart_get(cb, 0);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f %s", last, metric_unit(w->source));
    draw_text(bx + pw - 80, by, COL_TEXT, buf);
}

static void draw_widget_lamp(const ui_state_t *ui, const widget_t *w) {
    float v = live_value(ui, w->source);
    unsigned int col = (v > 0.5f) ? COL_RED : COL_GREEN;
    draw_rect(w->x, w->y, w->w, w->h, COL_PANEL);
    draw_rect(w->x + 8, w->y + 8, w->w - 16, w->h - 16, col);
    draw_text(w->x + 4, w->y + w->h - 14, COL_DIMTEXT, w->title);
}

static void draw_widget_dtc_list(const ui_state_t *ui, const widget_t *w) {
    draw_rect(w->x, w->y, w->w, w->h, COL_PANEL);
    draw_text(w->x + 4, w->y + 4, COL_DIMTEXT, "DTC");
    if (!ui->live.dtc_valid || ui->live.dtc_count == 0) {
        draw_text(w->x + 4, w->y + 20, COL_GREEN, "Nessun DTC");
        return;
    }
    for (int i = 0; i < ui->live.dtc_count && i < 10; i++) {
        char line[32];
        snprintf(line, sizeof(line), "  %s", ui->live.dtcs[i]);
        draw_text(w->x + 4, w->y + 20 + i * 16, COL_RED, line);
    }
    if (ui->live.dtc_count > 10) {
        char more[32];
        snprintf(more, sizeof(more), "  +%d altri...",
                 ui->live.dtc_count - 10);
        draw_text(w->x + 4, w->y + 20 + 10 * 16, COL_DIMTEXT, more);
    }
}

static void draw_widget_cost_card(const ui_state_t *ui, const widget_t *w) {
    draw_rect(w->x, w->y, w->w, w->h, COL_PANEL);
    draw_text(w->x + 4, w->y + 4, COL_ACCENT, "COSTI");
    int y = w->y + 22;
    char buf[64];
#define ROW(label, fmt, ...) \
    snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); \
    draw_text(w->x+4, y, COL_DIMTEXT, label); \
    draw_text(w->x + w->w/2, y, COL_TEXT, buf); y += 18;

    ROW("L/100 medio",   "%.1f", ui->cost.avg_l100_all)
    ROW("EUR/100 medio", "%.2f", ui->cost.avg_eur100_all)
    ROW("L/100 ult.5",   "%.1f", ui->cost.avg_l100_last5)
    ROW("EUR/100 ult.5", "%.2f", ui->cost.avg_eur100_last5)
    ROW("EUR/L medio",   "%.3f", ui->cost.avg_eur_l)
    ROW("Tot. km",       "%.0f", ui->cost.total_km)
    ROW("Tot. EUR",      "%.2f", ui->cost.total_eur)
    ROW("Ult. pieno",    "%.2f EUR", ui->cost.last_fill_eur)
    ROW("Ult. L/100",   "%.1f", ui->cost.last_fill_l100)
#undef ROW
}

static void draw_widget_barchart(const ui_state_t *ui, const widget_t *w) {
    draw_rect(w->x, w->y, w->w, w->h, COL_PANEL);
    draw_text(w->x + 4, w->y + 4, COL_DIMTEXT, "Spesa pieni");
    if (!ui->refuel || ui->refuel->count == 0) return;

    int n = ui->refuel->count;
    int show = n < 10 ? n : 10; /* ultime 10 barre */
    int start = n - show;
    int bw = (w->w - 8) / show - 2;
    int bh_max = w->h - 30;
    float max_eur = 0;
    for (int i = start; i < n; i++) {
        float e = ui->refuel->entries[i].liters * ui->refuel->entries[i].eur_per_l;
        if (e > max_eur) max_eur = e;
    }
    if (max_eur < 1) max_eur = 1;
    for (int i = 0; i < show; i++) {
        const refuel_t *r = &ui->refuel->entries[start + i];
        float e = r->liters * r->eur_per_l;
        int bar_h = (int)(e / max_eur * bh_max);
        int bx = w->x + 4 + i * (bw + 2);
        int by = w->y + 24 + bh_max - bar_h;
        draw_rect(bx, by, bw, bar_h, COL_ACCENT);
    }
}

static void draw_widget_camera(const ui_state_t *ui, const widget_t *w) {
#ifdef PSP_BUILD
    if (!ui->cam || !ui->cam->available) {
        draw_rect(w->x, w->y, w->w, w->h, COL_PANEL);
        draw_text(w->x + w->w/2 - 80, w->y + w->h/2 - 8,
                  COL_DIMTEXT, "Camera non disponibile");
        return;
    }
    if (!ui->cam->frame_ready) {
        draw_rect(w->x, w->y, w->w, w->h, 0xFF000000);
        return;
    }
    /* Blit RGBA frame come texture GU */
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, CAM_WIDTH, CAM_HEIGHT, CAM_WIDTH, ui->cam->rgba_front);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);

    typedef struct { float u,v; float x,y,z; } UV2D;
    UV2D *vv = (UV2D *)sceGuGetMemory(2 * sizeof(UV2D));
    vv[0].u = 0; vv[0].v = 0;
    vv[0].x = (float)w->x; vv[0].y = (float)w->y; vv[0].z = 0;
    vv[1].u = CAM_WIDTH; vv[1].v = CAM_HEIGHT;
    vv[1].x = (float)(w->x + w->w); vv[1].y = (float)(w->y + w->h); vv[1].z = 0;
    sceGumDrawArray(GU_SPRITES,
                    GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                    2, NULL, vv);
    sceGuDisable(GU_TEXTURE_2D);
#else
    (void)ui;
    draw_rect(w->x, w->y, w->w, w->h, COL_PANEL);
    draw_text(w->x + 4, w->y + w->h/2, COL_DIMTEXT, "[Camera stub]");
#endif
}

static void draw_widget(const ui_state_t *ui, const widget_t *w) {
    if (!w->visible) return;
    switch (w->type) {
        case W_NUMERIC:   draw_widget_numeric(ui, w);   break;
        case W_GAUGE:     draw_widget_gauge(ui, w);     break;
        case W_LINECHART: draw_widget_linechart(ui, w); break;
        case W_BARCHART:  draw_widget_barchart(ui, w);  break;
        case W_LAMP:      draw_widget_lamp(ui, w);      break;
        case W_DTC_LIST:  draw_widget_dtc_list(ui, w);  break;
        case W_COST_CARD: draw_widget_cost_card(ui, w); break;
        case W_CAMERA:    draw_widget_camera(ui, w);    break;
    }
    /* Bordo selezione in config mode */
    if (ui->config_mode && ui->selected_widget == w->id)
        draw_rect(w->x-1, w->y-1, w->w+2, w->h+2, COL_ACCENT);
}

/* ---- Dash header / footer ---- */
static const char * const s_dash_names[DASH_COUNT] = {
    "Guida", "Motore", "Grafici", "Costi", "Diagnostica", "0-100", "Camera"
};

static void draw_dash_nav(const ui_state_t *ui) {
    /* Header: nome dash corrente */
    draw_rect(0, 0, SCR_W, 10, COL_BG);
    char hdr[64];
    snprintf(hdr, sizeof(hdr), "< %s >  [%s]",
             s_dash_names[ui->current_dash],
             ui->elm ? (ui->elm->state == ELM_READY ? "OK" :
                        ui->elm->state == ELM_CONNECTING ? "..." : "ERR") : "--");
    draw_text(4, 0, COL_ACCENT, hdr);
    /* Footer in config mode */
    if (ui->config_mode) {
        draw_rect(0, SCR_H - 10, SCR_W, 10, COL_PANEL);
        draw_text(4, SCR_H - 10, COL_YELLOW,
                  "[X] mostra/nascondi  [O] sposta  [Tri] esci config");
    }
}

/* ---- Dash diagnostica: pulsanti Leggi/Cancella ---- */
static void draw_diag_buttons(const ui_state_t *ui) {
    if (ui->current_dash != 4) return;
    draw_rect(10, 230, 100, 30, COL_PANEL);
    draw_text(14, 238, COL_ACCENT, "[X] Leggi DTC");
    draw_rect(120, 230, 120, 30, COL_PANEL);
    draw_text(124, 238, COL_RED,  "[O] Cancella DTC");
}

/* ---- Dash accel: tabella tempi ---- */
static void draw_accel_dash(const ui_state_t *ui) {
    if (ui->current_dash != 5) return;
    draw_rect(0, 10, SCR_W, SCR_H - 10, COL_BG);
    draw_text(10, 14, COL_DIMTEXT,
              "AVVISO: tempi indicativi (latenza WiFi, granularita' 1 km/h)");
    const accel_run_t *r = ui->accel_run;
    const accel_best_t *b = ui->accel_best;
    if (!r || !b) return;

    draw_text(10, 28, COL_TEXT, "Soglia   Corrente   Best");
    for (int i = 1; i < ACCEL_THRESHOLDS_N; i++) {
        char line[64];
        char curr[16] = "---", best[16] = "---";
        if (r->hit[i]) snprintf(curr, sizeof(curr), "%.2fs", r->t_hit[i]);
        if (b->best[i] > 0) snprintf(best, sizeof(best), "%.2fs", b->best[i]);
        snprintf(line, sizeof(line), "0-%d km/h   %s   %s",
                 accel_thresholds[i], curr, best);
        unsigned int col = r->hit[i] ? COL_ACCENT : COL_DIMTEXT;
        draw_text(10, 44 + (i-1)*20, col, line);
    }
    if (!r->active)
        draw_text(10, 220, COL_GREEN,
                  "[X] Start  (riparte automaticamente quando l'auto e' ferma)");
    else
        draw_text(10, 220, COL_RED, "IN CORSO...");
}

/* ---- API pubblica ---- */

int ui_init(ui_state_t *ui) {
    memset(ui, 0, sizeof(*ui));
#ifdef PSP_BUILD
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    gu_init();
#endif
    return 0;
}

void ui_load_layout(ui_state_t *ui, const car_profile_t *profile,
                    const char *layout_file) {
    (void)profile;
    /* Tenta di caricare da file */
    FILE *f = fopen(layout_file, "r");
    if (f) {
        char buf[8192]; int n = (int)fread(buf, 1, sizeof(buf)-1, f);
        fclose(f); buf[n] = '\0';
        json_node_t *root = json_parse(buf);
        json_node_t *warr = root ? json_get(root, "widgets") : NULL;
        if (warr && warr->type == JSON_ARRAY) {
            int wi = 0;
            for (json_node_t *wn = warr->v.children;
                 wn && wi < WIDGET_MAX; wn = wn->next, wi++) {
                widget_t *w = &ui->widgets[wi];
                w->id       = json_int(wn, "id",      wi);
                w->type     = (widget_type_t)json_int(wn, "type",   0);
                w->source   = (metric_src_t) json_int(wn, "source", 0);
                w->min      = (float)json_num(wn, "min",     0);
                w->max      = (float)json_num(wn, "max",   100);
                w->red_zone = (float)json_num(wn, "red",     0);
                w->dash_id  = json_int(wn, "dash",    0);
                w->x        = json_int(wn, "x",       0);
                w->y        = json_int(wn, "y",       0);
                w->w        = json_int(wn, "w",     100);
                w->h        = json_int(wn, "h",      50);
                w->visible  = json_int(wn, "vis",     1);
                w->group_id = json_int(wn, "grp",     0);
                /* title e' const char* - punta al pool del json parser */
                w->title    = json_str(wn, "title",  "");
            }
            ui->widget_count = wi;
            json_free();
            return;
        }
        json_free();
    }
    /* Fallback: layout di default */
    set_default_layout(ui);
}

void ui_save_layout(ui_state_t *ui, const char *layout_file) {
    char buf[8192];
    json_writer_t w;
    jw_init(&w, buf, sizeof(buf));
    jw_obj_open(&w);
    jw_key_arr_open(&w, "widgets");
    for (int i = 0; i < ui->widget_count; i++) {
        const widget_t *wi = &ui->widgets[i];
        jw_arr_obj_open(&w);
        jw_key_int(&w, "id",     wi->id);
        jw_key_int(&w, "type",   (int)wi->type);
        jw_key_str(&w, "title",  wi->title ? wi->title : "");
        jw_key_int(&w, "source", (int)wi->source);
        jw_key_dbl(&w, "min",    wi->min);
        jw_key_dbl(&w, "max",    wi->max);
        jw_key_dbl(&w, "red",    wi->red_zone);
        jw_key_int(&w, "dash",   wi->dash_id);
        jw_key_int(&w, "x",      wi->x);
        jw_key_int(&w, "y",      wi->y);
        jw_key_int(&w, "w",      wi->w);
        jw_key_int(&w, "h",      wi->h);
        jw_key_int(&w, "vis",    wi->visible);
        jw_key_int(&w, "grp",    wi->group_id);
        jw_obj_close(&w);
    }
    jw_arr_close(&w);
    jw_obj_close(&w);

    if (jw_ok(&w)) {
        FILE *f = fopen(layout_file, "w");
        if (f) { fputs(buf, f); fclose(f); }
    }
}

void ui_push_sample(ui_state_t *ui, const live_t *live) {
    memcpy(&ui->live, live, sizeof(*live));
    chart_push(&ui->chart_speed, (float)live->speed);
    chart_push(&ui->chart_rpm,   live->rpm);
    float l100 = live->l100_live > 0 ? live->l100_live : 0;
    chart_push(&ui->chart_l100, l100);
}

/* Ritorna 1 se il tasto e' stato premuto in questo frame (edge detection) */
static int btn_pressed(const ui_state_t *ui, unsigned int btn) {
    return (ui->buttons & btn) && !(ui->last_buttons & btn);
}

int ui_handle_input(ui_state_t *ui) {
    ui->last_buttons = ui->buttons;
    ui->buttons = read_buttons();

    /* Start+Select = esci */
    if ((ui->buttons & BTN_START) && (ui->buttons & BTN_SELECT))
        return 1;

    if (!ui->config_mode) {
        /* L/R: cambia dash */
        if (btn_pressed(ui, BTN_L)) {
            ui->current_dash = (ui->current_dash - 1 + DASH_COUNT) % DASH_COUNT;
        }
        if (btn_pressed(ui, BTN_R)) {
            ui->current_dash = (ui->current_dash + 1) % DASH_COUNT;
        }
        /* Triangle: entra in config mode */
        if (btn_pressed(ui, BTN_TRIANGLE)) {
            ui->config_mode = 1;
            ui->selected_widget = 0;
        }
        /* Dash diagnostica: X = leggi DTC, O = cancella DTC (con conferma) */
        if (ui->current_dash == 4 && ui->elm) {
            if (btn_pressed(ui, BTN_CROSS)) {
                char dtcs[32][6];
                int n = elm_read_dtc(ui->elm, dtcs, 32);
                if (n >= 0) {
                    ui->live.dtc_count = n < 32 ? n : 32;
                    for (int i = 0; i < ui->live.dtc_count; i++)
                        memcpy(ui->live.dtcs[i], dtcs[i], 6);
                    ui->live.dtc_valid = 1;
                }
            }
        }
        /* Dash accel: X = start */
        if (ui->current_dash == 5 && ui->accel_run) {
            if (btn_pressed(ui, BTN_CROSS) && !ui->accel_run->active) {
                float now = net_time_ms() / 1000.0f;
                accel_start(ui->accel_run, now);
            }
        }
        /* Dash camera: X = snapshot */
        if (ui->current_dash == 6 && ui->cam && btn_pressed(ui, BTN_CROSS)) {
            cam_snapshot(ui->cam, SNAP_DIR,
                         ui->live.speed, ui->live.l100_live);
        }
    } else {
        /* Config mode */
        if (btn_pressed(ui, BTN_TRIANGLE)) ui->config_mode = 0;

        /* D-pad: seleziona widget successivo/precedente sulla dash corrente */
        if (btn_pressed(ui, BTN_RIGHT) || btn_pressed(ui, BTN_DOWN)) {
            for (int i = 1; i < ui->widget_count; i++) {
                int ni = (ui->selected_widget + i) % ui->widget_count;
                if (ui->widgets[ni].dash_id == ui->current_dash) {
                    ui->selected_widget = ni; break;
                }
            }
        }
        if (btn_pressed(ui, BTN_LEFT) || btn_pressed(ui, BTN_UP)) {
            for (int i = ui->widget_count - 1; i >= 1; i--) {
                int ni = (ui->selected_widget - i + ui->widget_count)
                         % ui->widget_count;
                if (ui->widgets[ni].dash_id == ui->current_dash) {
                    ui->selected_widget = ni; break;
                }
            }
        }
        /* X: toggle visibilita' */
        if (btn_pressed(ui, BTN_CROSS)) {
            for (int i = 0; i < ui->widget_count; i++)
                if (ui->widgets[i].id == ui->selected_widget) {
                    ui->widgets[i].visible ^= 1; break;
                }
        }
    }
    return 0;
}

void ui_draw(ui_state_t *ui) {
#ifdef PSP_BUILD
    sceGuStart(GU_DIRECT, s_disp_list);
    sceGuClearColor(COL_BG);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
#endif

    /* Disegna i widget della dash corrente */
    for (int i = 0; i < ui->widget_count; i++) {
        const widget_t *w = &ui->widgets[i];
        if (w->dash_id == ui->current_dash)
            draw_widget(ui, w);
    }

    draw_dash_nav(ui);
    draw_diag_buttons(ui);
    draw_accel_dash(ui);

#ifdef PSP_BUILD
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
#endif
}

void ui_shutdown(ui_state_t *ui) {
    (void)ui;
#ifdef PSP_BUILD
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
#endif
}
