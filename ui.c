/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: ui.c  |  Modulo: sistema UI (widget, dashboard, form, input)
 */
#include "ui.h"
#include "gfx.h"
#include "font.h"
#include "json.h"
#include "dtc_db.h"
#include "net.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef PSP_BUILD
#include <pspctrl.h>
#include <psprtc.h>
#endif

#define LAYOUT_VERSION 2
#define HDR_H   20
#define FTR_Y   261
#define TOAST_MS 3500

static const char * const s_dash_names[DASH_COUNT] = {
    "Guida", "Motore", "Grafici", "Costi", "Diagnostica", "0-100", "Camera", "Setup"
};

/* ================================================================ */
/*                          Utilita'                                */
/* ================================================================ */

static void toast(ui_state_t *ui, unsigned int color, const char *msg) {
    strncpy(ui->toast, msg, sizeof(ui->toast) - 1);
    ui->toast[sizeof(ui->toast) - 1] = '\0';
    ui->toast_ms = ui->now_ms ? ui->now_ms : 1;
    ui->toast_color = color;
}

static void text_center(int cx, int y, uint32_t col, int scale, const char *s) {
    gfx_text(cx - gfx_text_w(s, scale) / 2, y, col, scale, s);
}

static void text_right(int rx, int y, uint32_t col, int scale, const char *s) {
    gfx_text(rx - gfx_text_w(s, scale), y, col, scale, s);
}

/* Copia s troncandola a max_chars (con ".." finale) */
static void fit(char *out, int outlen, const char *s, int max_chars) {
    if (max_chars < 1) { out[0] = 0; return; }
    if (max_chars >= outlen) max_chars = outlen - 1;
    int n = (int)strlen(s);
    if (n <= max_chars) { strcpy(out, s); return; }
    if (max_chars <= 2) { memcpy(out, s, (size_t)max_chars); out[max_chars] = 0; return; }
    memcpy(out, s, (size_t)(max_chars - 2));
    strcpy(out + max_chars - 2, "..");
}

static void chart_push(chart_buf_t *c, float v) {
    c->samples[c->head] = v;
    c->head = (c->head + 1) % CHART_HIST_LEN;
    if (c->count < CHART_HIST_LEN) c->count++;
}

static float chart_get(const chart_buf_t *c, int ago) {
    int idx = (c->head - 1 - ago + CHART_HIST_LEN * 2) % CHART_HIST_LEN;
    return c->samples[idx];
}

float ui_km_since_fill(const ui_state_t *ui) {
    float v = ui->km_fill_base + (ui->live.trip_km - ui->trip_at_fill);
    return v < 0 ? 0 : v;
}

/* ================================================================ */
/*                       Valori metriche                            */
/* ================================================================ */

static int live_value(const ui_state_t *ui, metric_src_t src, float *out) {
    const live_t *L = &ui->live;
    int ok = (L->valid & LIVE_BIT(src)) != 0;
    int has_fills = ui->refuel && ui->refuel->count > 0;
    float v = 0;
    switch (src) {
        case SRC_RPM:        v = L->rpm; break;
        case SRC_SPEED:      v = (float)L->speed; break;
        case SRC_COOLANT:    v = (float)L->coolant; break;
        case SRC_MAF:        v = L->maf; break;
        case SRC_FUEL_LEVEL: v = L->fuel_level; break;
        case SRC_THROTTLE:   v = (float)L->throttle; break;
        case SRC_L100_LIVE:  v = L->l100_live; if (v < 0) ok = 0; break;
        case SRC_L100_TRIP:  v = L->l100_trip; break;
        case SRC_LH_LIVE:    v = L->lh_live; break;
        case SRC_MIL:        v = (float)L->mil; break;
        case SRC_DTC_COUNT:  v = (float)L->dtc_ecu; break;
        case SRC_BATT_V:     v = L->batt_v; break;
        case SRC_TRIP_KM:    v = L->trip_km; ok = 1; break;
        case SRC_KM_FILL:    v = ui_km_since_fill(ui); ok = 1; break;
        case SRC_AVG_L100:   v = ui->cost.avg_l100_all;   ok = has_fills; break;
        case SRC_AVG_EUR100: v = ui->cost.avg_eur100_all; ok = has_fills; break;
        case SRC_LAST_L100:  v = ui->cost.last_fill_l100; ok = has_fills; break;
        case SRC_LAST_EUR:   v = ui->cost.last_fill_eur;  ok = has_fills; break;
        case SRC_FUEL_TOTAL_KM:  v = ui->cost.total_km;   ok = has_fills; break;
        case SRC_FUEL_TOTAL_EUR: v = ui->cost.total_eur;  ok = has_fills; break;
        default: ok = 0; break;
    }
    *out = v;
    return ok;
}

static const char *metric_unit(metric_src_t src) {
    switch (src) {
        case SRC_RPM:        return "giri/min";
        case SRC_SPEED:      return "km/h";
        case SRC_COOLANT:    return "C";
        case SRC_MAF:        return "g/s";
        case SRC_FUEL_LEVEL: return "%";
        case SRC_THROTTLE:   return "%";
        case SRC_L100_LIVE:
        case SRC_L100_TRIP:
        case SRC_AVG_L100:
        case SRC_LAST_L100:  return "L/100km";
        case SRC_LH_LIVE:    return "L/h";
        case SRC_AVG_EUR100: return "EUR/100km";
        case SRC_LAST_EUR:
        case SRC_FUEL_TOTAL_EUR: return "EUR";
        case SRC_BATT_V:     return "V";
        case SRC_TRIP_KM:
        case SRC_KM_FILL:
        case SRC_FUEL_TOTAL_KM:  return "km";
        default:             return "";
    }
}

static int metric_decimals(metric_src_t src) {
    switch (src) {
        case SRC_L100_LIVE: case SRC_L100_TRIP: case SRC_LH_LIVE:
        case SRC_MAF: case SRC_BATT_V: case SRC_AVG_L100: case SRC_LAST_L100:
        case SRC_KM_FILL: case SRC_TRIP_KM:
            return 1;
        case SRC_AVG_EUR100: case SRC_LAST_EUR: case SRC_FUEL_TOTAL_EUR:
            return 2;
        default:
            return 0;
    }
}

static void fmt_metric(const ui_state_t *ui, metric_src_t src, char *buf, int len) {
    float v;
    if (!live_value(ui, src, &v)) { snprintf(buf, (size_t)len, "---"); return; }
    int d = metric_decimals(src);
    if (d && fabsf(v) >= 1000.0f) d = 0;
    snprintf(buf, (size_t)len, "%.*f", d, v);
}

/* ================================================================ */
/*                         Layout                                   */
/* ================================================================ */

void ui_set_default_layout(ui_state_t *ui) {
    int n = 0;
#define W(type_, title_, src_, min_, max_, red_, dash_, x_, y_, w_, h_) do { \
        widget_t *w = &ui->widgets[n]; memset(w, 0, sizeof(*w));          \
        w->id = n; w->type = type_;                                         \
        strncpy(w->title, title_, sizeof(w->title) - 1);                    \
        w->source = src_; w->min = min_; w->max = max_; w->red_zone = red_; \
        w->dash_id = dash_; w->x = x_; w->y = y_; w->w = w_; w->h = h_;     \
        w->visible = 1; n++; } while (0)

    /* 0 - Guida */
    W(W_GAUGE,   "Giri",       SRC_RPM,        0, 7500, 6700, DASH_DRIVE,   6,  24, 152, 150);
    W(W_GAUGE,   "Velocita'",  SRC_SPEED,      0,  220,    0, DASH_DRIVE, 164,  24, 152, 150);
    W(W_GAUGE,   "Carburante", SRC_FUEL_LEVEL, 0,  100,    0, DASH_DRIVE, 322,  24, 152, 150);
    W(W_NUMERIC, "Consumo",    SRC_L100_LIVE,  0,   30,    0, DASH_DRIVE,   6, 178, 115,  80);
    W(W_NUMERIC, "Media trip", SRC_L100_TRIP,  0,   30,    0, DASH_DRIVE, 125, 178, 115,  80);
    W(W_NUMERIC, "Orario",     SRC_LH_LIVE,    0,   30,    0, DASH_DRIVE, 244, 178, 115,  80);
    W(W_NUMERIC, "Acqua",      SRC_COOLANT,  -40,  130,  105, DASH_DRIVE, 363, 178, 111,  80);
    /* 1 - Motore */
    W(W_NUMERIC, "Acqua",      SRC_COOLANT,  -40,  130,  105, DASH_ENGINE,   6,  24, 115, 70);
    W(W_NUMERIC, "Farfalla",   SRC_THROTTLE,   0,  100,    0, DASH_ENGINE, 125,  24, 115, 70);
    W(W_NUMERIC, "Aria (MAF)", SRC_MAF,        0,  200,    0, DASH_ENGINE, 244,  24, 115, 70);
    W(W_NUMERIC, "Batteria",   SRC_BATT_V,     0,   16,    0, DASH_ENGINE, 363,  24, 111, 70);
    W(W_LINECHART,"Giri",      SRC_RPM,        0, 7500,    0, DASH_ENGINE,   6,  98, 468, 160);
    /* 2 - Grafici */
    W(W_LINECHART,"Velocita'", SRC_SPEED,      0,  200,    0, DASH_CHARTS,   6,  24, 468,  76);
    W(W_LINECHART,"Consumo",   SRC_L100_LIVE,  0,   25,    0, DASH_CHARTS,   6, 104, 468,  76);
    W(W_LINECHART,"Giri",      SRC_RPM,        0, 7500,    0, DASH_CHARTS,   6, 184, 468,  74);
    /* 3 - Costi */
    W(W_COST_CARD,"Costi",     SRC_AVG_L100,   0,    0,    0, DASH_COSTS,    6,  24, 230, 234);
    W(W_BARCHART, "Spesa pieni",SRC_LAST_EUR,  0,    0,    0, DASH_COSTS,  242,  24, 232, 168);
    W(W_NUMERIC, "Km dal pieno",SRC_KM_FILL,   0,    0,    0, DASH_COSTS,  242, 196, 232,  62);
    /* 4 - Diagnostica */
    W(W_LAMP,    "Spia MIL",   SRC_MIL,        0,    1,    0, DASH_DIAG,     6,  24,  80,  62);
    W(W_NUMERIC, "DTC (ECU)",  SRC_DTC_COUNT,  0,   99,    0, DASH_DIAG,    90,  24, 100,  62);
    W(W_NUMERIC, "Batteria",   SRC_BATT_V,     0,   16,    0, DASH_DIAG,   194,  24, 100,  62);
    W(W_DTC_LIST,"Codici",     SRC_DTC_COUNT,  0,    0,    0, DASH_DIAG,     6,  90, 468, 168);
    /* 6 - Camera */
    W(W_CAMERA,  "Camera",     SRC_SPEED,      0,    0,    0, DASH_CAMERA,   0,  20, 480, 240);
    W(W_NUMERIC, "km/h",       SRC_SPEED,      0,    0,    0, DASH_CAMERA,   6, 196, 100,  60);
    W(W_NUMERIC, "L/100",      SRC_L100_LIVE,  0,    0,    0, DASH_CAMERA, 110, 196, 100,  60);
#undef W
    ui->widget_count = n;
    ui->layout_dirty = 0;
}

void ui_load_layout(ui_state_t *ui, const char *layout_file) {
    static char buf[8192];
    FILE *f = fopen(layout_file, "r");
    if (f) {
        int n = (int)fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        buf[n < 0 ? 0 : n] = '\0';
        json_node_t *root = json_parse(buf);
        json_node_t *warr = root ? json_get(root, "widgets") : NULL;
        /* Layout di versioni precedenti: ignorato (si usa il nuovo default) */
        if (root && json_int(root, "ver", 0) == LAYOUT_VERSION &&
            warr && warr->type == JSON_ARRAY) {
            int wi = 0;
            for (json_node_t *wn = warr->v.children; wn && wi < WIDGET_MAX; wn = wn->next) {
                widget_t *w = &ui->widgets[wi];
                memset(w, 0, sizeof(*w));
                w->id       = wi;
                w->type     = (widget_type_t)json_int(wn, "type", 0);
                w->source   = (metric_src_t)json_int(wn, "source", 0);
                if ((int)w->type > W_CAMERA || (int)w->source >= SRC_COUNT) continue;
                w->min      = (float)json_num(wn, "min", 0);
                w->max      = (float)json_num(wn, "max", 100);
                w->red_zone = (float)json_num(wn, "red", 0);
                w->dash_id  = json_int(wn, "dash", 0);
                w->x        = json_int(wn, "x", 0);
                w->y        = json_int(wn, "y", 0);
                w->w        = json_int(wn, "w", 100);
                w->h        = json_int(wn, "h", 50);
                w->visible  = json_int(wn, "vis", 1);
                w->group_id = json_int(wn, "grp", 0);
                strncpy(w->title, json_str(wn, "title", ""), sizeof(w->title) - 1);
                if (w->dash_id < 0 || w->dash_id >= DASH_COUNT) continue;
                wi++;
            }
            json_free();
            if (wi > 0) { ui->widget_count = wi; ui->layout_dirty = 0; return; }
        } else {
            json_free();
        }
    }
    ui_set_default_layout(ui);
}

int ui_save_layout(ui_state_t *ui, const char *layout_file) {
    static char buf[8192];
    json_writer_t w;
    jw_init(&w, buf, sizeof(buf));
    jw_obj_open(&w);
    jw_key_int(&w, "ver", LAYOUT_VERSION);
    jw_key_arr_open(&w, "widgets");
    for (int i = 0; i < ui->widget_count; i++) {
        const widget_t *wi = &ui->widgets[i];
        jw_arr_obj_open(&w);
        jw_key_int(&w, "type",   (int)wi->type);
        jw_key_str(&w, "title",  wi->title);
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
    if (!jw_ok(&w)) return -1;
    FILE *f = fopen(layout_file, "w");
    if (!f) return -1;
    fputs(buf, f);
    fclose(f);
    ui->layout_dirty = 0;
    return 0;
}

/* ================================================================ */
/*                     Rendering widget                             */
/* ================================================================ */

static void panel(const widget_t *w) {
    gfx_rect(w->x, w->y, w->w, w->h, COL_PANEL);
}

static void draw_widget_numeric(const ui_state_t *ui, const widget_t *w) {
    char buf[24];
    float v;
    int ok = live_value(ui, w->source, &v);
    fmt_metric(ui, w->source, buf, sizeof(buf));
    const char *unit = metric_unit(w->source);
    /* Consumo da fermo: mostra L/h al posto di L/100 */
    if (w->source == SRC_L100_LIVE && !ok && (ui->live.valid & LIVE_BIT(SRC_LH_LIVE))) {
        snprintf(buf, sizeof(buf), "%.1f", ui->live.lh_live);
        unit = "L/h (fermo)";
    }
    panel(w);
    uint32_t col = COL_TEXT;
    if (ok && w->red_zone > w->min && v >= w->red_zone) col = COL_RED;
    if (!ok && strcmp(buf, "---") == 0) col = COL_DIMTEXT;

    gfx_text(w->x + 5, w->y + 4, COL_DIMTEXT, 1, w->title);
    int avail_h = w->h - 26;
    int scale = 4;
    while (scale > 1 && (gfx_text_w(buf, scale) > w->w - 10 || 8 * scale > avail_h)) scale--;
    int ty = w->y + 14 + (avail_h - 8 * scale) / 2;
    text_center(w->x + w->w / 2, ty, col, scale, buf);
    text_right(w->x + w->w - 5, w->y + w->h - 11, COL_DIMTEXT, 1, unit);
}

static void draw_widget_gauge(const ui_state_t *ui, const widget_t *w) {
    float v;
    int ok = live_value(ui, w->source, &v);
    float range = w->max - w->min;
    if (range <= 0) range = 1;
    float pct = ok ? (v - w->min) / range : 0;
    if (pct < 0) pct = 0;
    if (pct > 1) pct = 1;

    panel(w);
    int r  = (w->w < w->h - 14 ? w->w : w->h - 14) / 2 - 6;
    int cx = w->x + w->w / 2;
    int cy = w->y + 14 + r + 2;
    const float A0 = 135.0f, SW = 270.0f;
    gfx_arc(cx, cy, r, 10, A0, A0 + SW, COL_TRACK);
    if (w->red_zone > w->min && w->red_zone < w->max)
        gfx_arc(cx, cy, r, 4, A0 + SW * (w->red_zone - w->min) / range, A0 + SW, COL_RED);
    uint32_t col = (ok && w->red_zone > w->min && v >= w->red_zone) ? COL_RED : COL_ACCENT;
    if (pct > 0.003f) gfx_arc(cx, cy, r - 1, 8, A0, A0 + SW * pct, col);

    char buf[24];
    fmt_metric(ui, w->source, buf, sizeof(buf));
    int scale = 3;
    while (scale > 1 && gfx_text_w(buf, scale) > 2 * (r - 12)) scale--;
    text_center(cx, cy - 4 * scale, ok ? COL_TEXT : COL_DIMTEXT, scale, buf);
    text_center(cx, cy + 4 * scale + 4, COL_DIMTEXT, 1, metric_unit(w->source));
    gfx_text(w->x + 5, w->y + 4, COL_DIMTEXT, 1, w->title);
}

static void draw_widget_linechart(const ui_state_t *ui, const widget_t *w) {
    panel(w);
    gfx_text(w->x + 5, w->y + 4, COL_DIMTEXT, 1, w->title);
    char buf[32], val[24];
    fmt_metric(ui, w->source, val, sizeof(val));
    snprintf(buf, sizeof(buf), "%s %s", val, metric_unit(w->source));
    text_right(w->x + w->w - 5, w->y + 4, COL_TEXT, 1, buf);

    const chart_buf_t *cb = &ui->chart_l100;
    if (w->source == SRC_SPEED)    cb = &ui->chart_speed;
    else if (w->source == SRC_RPM) cb = &ui->chart_rpm;

    float range = w->max - w->min;
    if (range <= 0) range = 1;
    int pw = w->w - 10, ph = w->h - 20;
    int bx = w->x + 5,  by = w->y + 15;
    gfx_rect(bx, by + ph / 2, pw, 1, COL_PANEL2);
    gfx_rect(bx, by + ph, pw, 1, COL_PANEL2);
    if (cb->count < 1) return;
    int n = cb->count;
    int bw = pw / CHART_HIST_LEN;          /* larghezza barra: riempie il pannello */
    if (bw < 1) bw = 1;
    int max_bars = pw / bw;
    if (n > max_bars) n = max_bars;
    for (int i = 0; i < n; i++) {
        float s = chart_get(cb, i);
        float p = (s - w->min) / range;
        if (p < 0) p = 0;
        if (p > 1) p = 1;
        int h = (int)(p * ph);
        if (h > 0) gfx_rect(bx + pw - (i + 1) * bw, by + ph - h, bw > 2 ? bw - 1 : bw, h, COL_BLUE);
    }
}

static void draw_widget_lamp(const ui_state_t *ui, const widget_t *w) {
    float v;
    int ok = live_value(ui, w->source, &v);
    panel(w);
    uint32_t col = !ok ? COL_TRACK : (v > 0.5f ? COL_RED : COL_GREEN);
    int s = (w->h - 32 < w->w - 16) ? w->h - 32 : w->w - 16;
    gfx_rect(w->x + (w->w - s) / 2, w->y + 16, s, s, col);
    gfx_text(w->x + 5, w->y + 4, COL_DIMTEXT, 1, w->title);
    text_center(w->x + w->w / 2, w->y + w->h - 10, ok ? COL_TEXT : COL_DIMTEXT, 1,
                !ok ? "---" : (v > 0.5f ? "ACCESA" : "spenta"));
}

static int dtc_rows_visible(const widget_t *w) { return (w->h - 28) / 26; }

static void draw_widget_dtc_list(const ui_state_t *ui, const widget_t *w) {
    const obd_task_t *t = ui->obd;
    panel(w);
    char hdr[96];
    if (t && t->dtc_busy) snprintf(hdr, sizeof(hdr), "Codici guasto - lettura in corso...");
    else if (t && t->dtc_msg[0]) snprintf(hdr, sizeof(hdr), "Codici guasto - %s", t->dtc_msg);
    else snprintf(hdr, sizeof(hdr), "Codici guasto - premi X per leggere");
    char line[96];
    fit(line, sizeof(line), hdr, (w->w - 10) / FONT_ADV);
    gfx_text(w->x + 5, w->y + 4, COL_DIMTEXT, 1, line);

    if (!t || !t->dtc_valid) return;
    if (t->dtc_count == 0) {
        text_center(w->x + w->w / 2, w->y + w->h / 2 - 8, COL_GREEN, 2, "Nessun codice guasto");
        return;
    }
    int rows = dtc_rows_visible(w);
    int first = ui->dtc_scroll;
    int max_chars = (w->w - 70) / FONT_ADV;
    for (int r = 0; r < rows && first + r < t->dtc_count; r++) {
        int i = first + r;
        int y0 = w->y + 18 + r * 26;
        const char *code = t->dtc[i];
        int pend = t->dtc_pending[i];
        gfx_rect(w->x + 4, y0, w->w - 8, 24, (r & 1) ? COL_PANEL : COL_PANEL2);
        gfx_text(w->x + 8, y0 + 4, pend ? COL_YELLOW : COL_RED, 2, code);
        const char *desc = dtc_lookup(code);
        char d1[96];
        snprintf(hdr, sizeof(hdr), "%s%s", desc ? desc : "Codice non presente nel dizionario",
                 dtc_is_manufacturer(code) ? " [Mazda]" : "");
        fit(d1, sizeof(d1), hdr, max_chars);
        gfx_text(w->x + 74, y0 + 3, COL_TEXT, 1, d1);
        gfx_text(w->x + 74, y0 + 13, COL_DIMTEXT, 1,
                 pend ? "in attesa (non ancora confermato)" : "confermato / memorizzato");
    }
    if (t->dtc_count > rows) {
        char more[32];
        snprintf(more, sizeof(more), "%d-%d di %d", first + 1,
                 first + rows > t->dtc_count ? t->dtc_count : first + rows, t->dtc_count);
        text_right(w->x + w->w - 6, w->y + w->h - 10, COL_DIMTEXT, 1, more);
    }
}

static void draw_widget_cost_card(const ui_state_t *ui, const widget_t *w) {
    panel(w);
    gfx_text(w->x + 6, w->y + 5, COL_ACCENT, 2, "Costi");
    if (!ui->refuel || ui->refuel->count == 0) {
        gfx_text(w->x + 6, w->y + 40, COL_TEXT, 1, "Nessun rifornimento");
        gfx_text(w->x + 6, w->y + 54, COL_TEXT, 1, "registrato.");
        gfx_text(w->x + 6, w->y + 76, COL_DIMTEXT, 1, "Premi X dopo ogni pieno");
        gfx_text(w->x + 6, w->y + 88, COL_DIMTEXT, 1, "per inserire km, litri");
        gfx_text(w->x + 6, w->y + 100, COL_DIMTEXT, 1, "e prezzo al litro.");
        return;
    }
    const cost_summary_t *c = &ui->cost;
    int y = w->y + 30;
    char buf[32];
#define ROW(label, fmt, val) do { snprintf(buf, sizeof(buf), fmt, val);   \
        gfx_text(w->x + 6, y, COL_DIMTEXT, 1, label);                      \
        text_right(w->x + w->w - 6, y, COL_TEXT, 1, buf); y += 20; } while (0)
    ROW("Media L/100km",      "%.2f", c->avg_l100_all);
    ROW("Media ultimi 5",     "%.2f", c->avg_l100_last5);
    ROW("EUR/100km",          "%.2f", c->avg_eur100_all);
    ROW("EUR/100km ultimi 5", "%.2f", c->avg_eur100_last5);
    ROW("Prezzo medio EUR/L", "%.3f", c->avg_eur_l);
    ROW("Km totali",          "%.0f", c->total_km);
    ROW("Spesa totale EUR",   "%.2f", c->total_eur);
    ROW("Ultimo pieno EUR",   "%.2f", c->last_fill_eur);
    ROW("Ultimo pieno L/100", "%.2f", c->last_fill_l100);
    ROW("Rifornimenti",       "%d",   ui->refuel->count);
#undef ROW
}

static void draw_widget_barchart(const ui_state_t *ui, const widget_t *w) {
    panel(w);
    gfx_text(w->x + 5, w->y + 4, COL_DIMTEXT, 1, w->title);
    if (!ui->refuel || ui->refuel->count == 0) return;
    int n = ui->refuel->count;
    int show = n < 10 ? n : 10;
    int start = n - show;
    int slot = (w->w - 10) / 10;
    int bh_max = w->h - 40;
    float max_eur = 1;
    for (int i = start; i < n; i++) {
        float e = ui->refuel->entries[i].liters * ui->refuel->entries[i].eur_per_l;
        if (e > max_eur) max_eur = e;
    }
    for (int i = 0; i < show; i++) {
        const refuel_t *r = &ui->refuel->entries[start + i];
        float e = r->liters * r->eur_per_l;
        int bar_h = (int)(e / max_eur * bh_max);
        int bx = w->x + 5 + i * slot;
        int base = w->y + 18 + bh_max;
        gfx_rect(bx + 2, base - bar_h, slot - 4, bar_h, i == show - 1 ? COL_ACCENT : COL_BLUE);
        char b[12];
        snprintf(b, sizeof(b), "%.0f", e);
        text_center(bx + slot / 2, base + 4, COL_DIMTEXT, 1, b);
    }
    text_right(w->x + w->w - 5, w->y + w->h - 10, COL_DIMTEXT, 1, "EUR per pieno");
}

static void draw_widget_camera(const ui_state_t *ui, const widget_t *w) {
    const cam_ctx_t *c = ui->cam;
    if (c && c->available && c->frame_ready) {
        gfx_image((const uint32_t *)c->rgba_front, CAM_WIDTH, CAM_HEIGHT, CAM_WIDTH,
                  w->x, w->y, w->w, w->h);
        return;
    }
    gfx_rect(w->x, w->y, w->w, w->h, COL_BLACK);
    const char *msg = (c && c->probing) ? "Ricerca camera..." :
                      (c && c->available) ? "Avvio video..." :
                      "Camera non rilevata";
    text_center(w->x + w->w / 2, w->y + w->h / 2 - 30, COL_DIMTEXT, 2, msg);
    if (!(c && (c->probing || c->available)))
        text_center(w->x + w->w / 2, w->y + w->h / 2 - 8, COL_DIMTEXT, 1,
                    "Serve la Go!Cam PSP-300 collegata prima di avviare l'app");
}

static void draw_widget(const ui_state_t *ui, const widget_t *w, int selected) {
    if (w->visible) {
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
    } else if (ui->config_mode) {
        gfx_frame(w->x, w->y, w->w, w->h, 1, COL_PANEL2);
        text_center(w->x + w->w / 2, w->y + w->h / 2 - 4, COL_DIMTEXT, 1, "(nascosto)");
    }
    if (selected)
        gfx_frame(w->x - 1, w->y - 1, w->w + 2, w->h + 2, 2,
                  ui->move_mode ? COL_YELLOW : COL_ACCENT);
}

/* ================================================================ */
/*                Header / footer / pagine speciali                 */
/* ================================================================ */

static uint32_t link_color(link_state_t s) {
    switch (s) {
        case LINK_READY:    return COL_GREEN;
        case LINK_ERROR:
        case LINK_WIFI_OFF: return COL_RED;
        default:            return COL_YELLOW;
    }
}

static void draw_header(const ui_state_t *ui) {
    gfx_rect(0, 0, GFX_W, HDR_H, COL_PANEL);
    char b[64];
    snprintf(b, sizeof(b), "%s", s_dash_names[ui->current_dash]);
    gfx_text(8, 3, COL_ACCENT, 2, b);
    snprintf(b, sizeof(b), "L/R  %d/%d", ui->current_dash + 1, DASH_COUNT);
    gfx_text(8 + gfx_text_w(s_dash_names[ui->current_dash], 2) + 10, 7, COL_DIMTEXT, 1, b);

    const obd_task_t *t = ui->obd;
    link_state_t st = t ? t->link : LINK_IDLE;
    gfx_rect(GFX_W - 12, 5, 8, 8, link_color(st));
    if (ui->live.valid & LIVE_BIT(SRC_BATT_V))
        snprintf(b, sizeof(b), "%s  %.1fV", obdtask_link_name(st), ui->live.batt_v);
    else
        snprintf(b, sizeof(b), "%s", obdtask_link_name(st));
    text_right(GFX_W - 18, 2, COL_TEXT, 1, b);
    if (t && st != LINK_READY) {
        char s[64];
        fit(s, sizeof(s), t->status, 44);
        text_right(GFX_W - 18, 11, link_color(st), 1, s);
    } else if (t) {
        snprintf(b, sizeof(b), "%.1f letture/s", t->poll_hz);
        text_right(GFX_W - 18, 11, COL_DIMTEXT, 1, b);
    }
}

static const char *dash_hint(const ui_state_t *ui) {
    if (ui->form.kind != FORM_NONE) return "SU/GIU cambia  SX/DX sposta  X salva  O annulla";
    if (ui->config_mode)
        return ui->move_mode ? "Frecce: sposta widget   O: fine spostamento   /\\: esci"
                             : "Frecce: scegli  X: mostra/nascondi  O: sposta  /\\: salva ed esci";
    switch (ui->current_dash) {
        case DASH_DRIVE:  return "[]: azzera trip   /\\: modifica layout   START+SELECT: esci";
        case DASH_COSTS:  return "X: registra un rifornimento   /\\: modifica layout";
        case DASH_DIAG:   return "X: leggi DTC   O: cancella DTC   SU/GIU: scorri";
        case DASH_ACCEL:  return "X: arma il cronometro (parte da fermo)";
        case DASH_CAMERA: return "X: scatta foto (salvata in snapshots/)";
        case DASH_SETUP:  return "X: modifica impostazioni   []: riconnetti";
        default:          return "/\\: modifica layout   START+SELECT: esci";
    }
}

static void draw_footer(const ui_state_t *ui) {
    gfx_rect(0, FTR_Y - 1, GFX_W, GFX_H - FTR_Y + 1, COL_PANEL);
    if (ui->toast[0] && ui->now_ms - ui->toast_ms < TOAST_MS) {
        char s[96];
        fit(s, sizeof(s), ui->toast, 78);
        gfx_text(6, FTR_Y + 1, ui->toast_color, 1, s);
        return;
    }
    gfx_text(6, FTR_Y + 1, COL_DIMTEXT, 1, dash_hint(ui));
}

static void draw_diag_info(const ui_state_t *ui) {
    const obd_task_t *t = ui->obd;
    int x = 298, y = 24, w = 176, h = 62;
    gfx_rect(x, y, w, h, COL_PANEL);
    gfx_text(x + 5, y + 4, COL_DIMTEXT, 1, "Adattatore / protocollo");
    char s[40];
    fit(s, sizeof(s), (t && t->elm.version[0]) ? t->elm.version : "ELM327 ---", 28);
    gfx_text(x + 5, y + 18, COL_TEXT, 1, s);
    fit(s, sizeof(s), (t && t->elm.protocol[0]) ? t->elm.protocol : "protocollo ---", 28);
    gfx_text(x + 5, y + 30, COL_TEXT, 1, s);
    if (ui->clear_confirm_ms && ui->now_ms - ui->clear_confirm_ms < 3000)
        gfx_text(x + 5, y + 44, COL_YELLOW, 1, "Premi O ancora: CANCELLA");
}

static void draw_accel_page(const ui_state_t *ui) {
    const accel_run_t *r = &ui->accel;
    const accel_best_t *b = &ui->best;
    const char *st; uint32_t sc;
    if (r->active)                     { st = "IN CORSO";                 sc = COL_ACCENT; }
    else if (r->armed && !r->have_zero){ st = "Armato: fermati";          sc = COL_YELLOW; }
    else if (r->armed)                 { st = "PRONTO: parti!";           sc = COL_GREEN;  }
    else if (r->hit[1])                { st = "Corsa completata";         sc = COL_TEXT;   }
    else                               { st = "Premi X per armare";       sc = COL_DIMTEXT;}
    gfx_rect(6, 24, 300, 234, COL_PANEL);
    gfx_text(14, 30, sc, 2, st);
    gfx_text(14, 56, COL_DIMTEXT, 1, "Soglia");
    gfx_text(110, 56, COL_DIMTEXT, 1, "Questa corsa");
    gfx_text(214, 56, COL_DIMTEXT, 1, "Record");
    for (int i = 1; i < ACCEL_THRESHOLDS_N; i++) {
        int y = 70 + (i - 1) * 28;
        char a[16], c[16], d[16];
        snprintf(a, sizeof(a), "0-%d", accel_thresholds[i]);
        if (r->hit[i]) snprintf(c, sizeof(c), "%.2fs", r->t_hit[i]); else strcpy(c, "--");
        if (b->best[i] > 0) snprintf(d, sizeof(d), "%.2fs", b->best[i]); else strcpy(d, "--");
        gfx_rect(10, y - 3, 292, 24, (i & 1) ? COL_PANEL2 : COL_PANEL);
        gfx_text(14, y, COL_TEXT, 2, a);
        gfx_text(110, y, r->hit[i] ? COL_ACCENT : COL_DIMTEXT, 2, c);
        gfx_text(214, y, COL_BLUE, 2, d);
    }
    gfx_text(14, 240, COL_DIMTEXT, 1, "Tempi indicativi (latenza WiFi, 1 km/h)");

    gfx_rect(312, 24, 162, 234, COL_PANEL);
    gfx_text(320, 30, COL_DIMTEXT, 1, "Velocita'");
    char v[16];
    fmt_metric(ui, SRC_SPEED, v, sizeof(v));
    text_center(393, 90, COL_TEXT, 6, v);
    text_center(393, 146, COL_DIMTEXT, 2, "km/h");
    if (r->active) {
        char e[16];
        snprintf(e, sizeof(e), "%.1fs", (net_time_ms() / 1000.0f) - r->t_start_s);
        text_center(393, 190, COL_ACCENT, 3, e);
    }
}

static void draw_setup_page(const ui_state_t *ui) {
    const obd_task_t *t = ui->obd;
    const app_config_t *c = ui->cfg;
    gfx_rect(6, 24, 468, 234, COL_PANEL);
    char b[96];
    int y = 32;
#define KV(k, ...) do { gfx_text(14, y, COL_DIMTEXT, 1, k);                 \
        snprintf(b, sizeof(b), __VA_ARGS__);                                \
        gfx_text(150, y, COL_TEXT, 1, b); y += 14; } while (0)
    const car_profile_t *p = c ? profile_find(c->car_id) : NULL;
    KV("Auto",              "%s", p ? p->display_name : (c ? c->car_id : "?"));
    KV("Profilo rete PSP",  "%d  (Impostazioni > Rete)", c ? c->elm_apctl_index : 0);
    KV("ELM327 WiFi",       "%s : %d", c ? c->elm_ip : "?", c ? c->elm_port : 0);
    KV("IP della PSP",      "%s", (t && t->local_ip[0]) ? t->local_ip : "---");
    KV("Stato",             "%s", t ? t->status : "---");
    KV("Adattatore",        "%s", (t && t->elm.version[0]) ? t->elm.version : "---");
    KV("Protocollo",        "%s", (t && t->elm.protocol[0]) ? t->elm.protocol : "---");
    if (t && t->link == LINK_READY) {
        const char *cs = "nessuna (solo rifornimenti)";
        if (t->caps.cons == CONS_FUEL_RATE) cs = "PID 015E (fuel rate)";
        else if (t->caps.cons == CONS_MAF) cs = "calcolo da MAF";
        KV("Consumo live",  "%s", cs);
    }
    KV("Cartella dati",     "%s", appconfig_dir());
#undef KV
    y += 6;
    if (!t || t->link != LINK_READY) {
        gfx_text(14, y, COL_ACCENT, 1, "Se non si connette:"); y += 14;
        gfx_text(14, y, COL_TEXT, 1, "1. Quadro acceso, ELM327 WiFi inserito nella presa OBD"); y += 12;
        gfx_text(14, y, COL_TEXT, 1, "2. PSP: Impostazioni > Impostazioni di rete > Infrastruttura:"); y += 12;
        gfx_text(14, y, COL_TEXT, 1, "   crea un profilo verso il WiFi dell'ELM (es. WiFi_OBDII)"); y += 12;
        gfx_text(14, y, COL_TEXT, 1, "3. Qui premi X e imposta numero profilo e IP:porta dell'ELM"); y += 12;
        gfx_text(14, y, COL_TEXT, 1, "   (di solito 192.168.0.10 : 35000), poi X per salvare");
    } else if (p) {
        char n1[80];
        fit(n1, sizeof(n1), p->notes, 75);
        gfx_text(14, y, COL_DIMTEXT, 1, n1);
    }
}

/* ---- Form ---- */
static void draw_form(const ui_state_t *ui) {
    const form_t *f = &ui->form;
    int x = 30, y = 30, w = 420, h = 222;
    gfx_rect(x - 2, y - 2, w + 4, h + 4, COL_ACCENT);
    gfx_rect(x, y, w, h, COL_PANEL);
    gfx_text(x + 10, y + 8, COL_ACCENT, 2, f->title);
    for (int i = 0; i < f->nf; i++) {
        const field_t *fd = &f->f[i];
        int ry = y + 38 + i * 30;
        int sel = (i == f->field);
        if (sel) gfx_rect(x + 4, ry - 4, w - 8, 26, COL_PANEL2);
        gfx_text(x + 10, ry + 4, sel ? COL_TEXT : COL_DIMTEXT, 1, fd->label);
        const char *val = fd->kind == FK_ENUM ? fd->choices[fd->choice] : fd->txt;
        int vx = x + 170;
        if (sel && fd->kind == FK_DIGITS)
            gfx_rect(vx + f->cursor * FONT_ADV * 2 - 2, ry - 2, FONT_ADV * 2 + 2, 20, COL_ACCENT);
        if (sel && fd->kind == FK_ENUM)
            gfx_text(vx - 14, ry, COL_ACCENT, 2, "<");
        gfx_text(vx, ry, COL_TEXT, 2, val);
        if (sel && fd->kind == FK_ENUM)
            gfx_text(vx + gfx_text_w(val, 2) + 6, ry, COL_ACCENT, 2, ">");
    }
}

void ui_draw(ui_state_t *ui) {
    gfx_begin(COL_BG);
    int d = ui->current_dash;
    if (ui->form.kind != FORM_NONE) {
        /* Il testo e' sempre sopra la geometria: con il form aperto non si
         * disegna la pagina sotto, altrimenti trasparirebbe */
        draw_form(ui);
    } else {
        for (int i = 0; i < ui->widget_count; i++) {
            const widget_t *w = &ui->widgets[i];
            if (w->dash_id == d)
                draw_widget(ui, w, ui->config_mode && i == ui->selected_widget);
        }
        if (d == DASH_DIAG)  draw_diag_info(ui);
        if (d == DASH_ACCEL) draw_accel_page(ui);
        if (d == DASH_SETUP) draw_setup_page(ui);
    }
    draw_header(ui);
    draw_footer(ui);
    gfx_end();
}

/* ================================================================ */
/*                         Stato / sync                             */
/* ================================================================ */

int ui_init(ui_state_t *ui) {
    memset(ui, 0, sizeof(*ui));
#ifdef PSP_BUILD
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
#endif
    gfx_init();
    return 0;
}

void ui_shutdown(ui_state_t *ui) {
    (void)ui;
    gfx_shutdown();
}

void ui_sync(ui_state_t *ui) {
    ui->now_ms = net_time_ms();
    obd_task_t *t = ui->obd;
    if (!t) return;
    ui->live  = t->live;
    ui->accel = t->accel;
    ui->best  = t->best;
    if (t->live_seq != ui->last_live_seq) {
        ui->last_live_seq = t->live_seq;
        if (t->link == LINK_READY) {
            const live_t *L = &ui->live;
            chart_push(&ui->chart_speed, (L->valid & LIVE_BIT(SRC_SPEED)) ? (float)L->speed : 0);
            chart_push(&ui->chart_rpm,   (L->valid & LIVE_BIT(SRC_RPM)) ? L->rpm : 0);
            chart_push(&ui->chart_l100,  L->l100_live > 0 ? L->l100_live : 0);
        }
    }
    /* Il cronometro va campionato veloce solo quando serve */
    t->fast_mode = (ui->current_dash == DASH_ACCEL);
}

/* ================================================================ */
/*                             Form                                 */
/* ================================================================ */

static int is_digit_pos(const field_t *f, int i) {
    return f->kind == FK_DIGITS && f->txt[i] >= '0' && f->txt[i] <= '9';
}

static int first_digit(const field_t *f) {
    for (int i = 0; f->txt[i]; i++) if (is_digit_pos(f, i)) return i;
    return 0;
}

static int last_digit(const field_t *f) {
    int l = 0;
    for (int i = 0; f->txt[i]; i++) if (is_digit_pos(f, i)) l = i;
    return l;
}

static void field_digits(field_t *f, const char *label, const char *txt) {
    memset(f, 0, sizeof(*f));
    snprintf(f->label, sizeof(f->label), "%s", label);
    f->kind = FK_DIGITS;
    snprintf(f->txt, sizeof(f->txt), "%s", txt);
}

static void open_refuel_form(ui_state_t *ui) {
    form_t *f = &ui->form;
    memset(f, 0, sizeof(*f));
    f->kind = FORM_REFUEL;
    strcpy(f->title, "Nuovo rifornimento");
    char b[24];
    float km = ui_km_since_fill(ui);
    if (km > 9999.9f) km = 9999.9f;
    snprintf(b, sizeof(b), "%06.1f", km);
    field_digits(&f->f[0], "Km dal pieno prima", b);
    float lit = (ui->refuel && ui->refuel->count) ?
                ui->refuel->entries[ui->refuel->count - 1].liters : 30.0f;
    snprintf(b, sizeof(b), "%06.2f", lit > 999.99f ? 999.99f : lit);
    field_digits(&f->f[1], "Litri erogati", b);
    float e = ui->cfg ? ui->cfg->last_eur_per_l : 1.8f;
    if (e <= 0 || e > 9.999f) e = 1.8f;
    snprintf(b, sizeof(b), "%05.3f", e);
    field_digits(&f->f[2], "Prezzo EUR/litro", b);
    f->nf = 3;
    f->cursor = first_digit(&f->f[0]);
}

static void open_setup_form(ui_state_t *ui) {
    const app_config_t *c = ui->cfg;
    form_t *f = &ui->form;
    memset(f, 0, sizeof(*f));
    f->kind = FORM_SETUP;
    strcpy(f->title, "Impostazioni");
    field_t *car = &f->f[0];
    memset(car, 0, sizeof(*car));
    strcpy(car->label, "Auto");
    car->kind = FK_ENUM;
    car->nchoices = PROFILE_COUNT;
    for (int i = 0; i < PROFILE_COUNT; i++) {
        car->choices[i] = profile_registry[i].display_name;
        if (c && strcmp(profile_registry[i].id, c->car_id) == 0) car->choice = i;
    }
    char b[24];
    snprintf(b, sizeof(b), "%02d", c ? c->elm_apctl_index : 1);
    field_digits(&f->f[1], "Profilo rete PSP", b);
    int o[4] = {192, 168, 0, 10};
    if (c) sscanf(c->elm_ip, "%d.%d.%d.%d", &o[0], &o[1], &o[2], &o[3]);
    snprintf(b, sizeof(b), "%03d.%03d.%03d.%03d", o[0] & 255, o[1] & 255, o[2] & 255, o[3] & 255);
    field_digits(&f->f[2], "IP ELM327", b);
    snprintf(b, sizeof(b), "%05d", c ? c->elm_port : 35000);
    field_digits(&f->f[3], "Porta ELM327", b);
    f->nf = 4;
}

static void current_date(char *out, int len) {
#ifdef PSP_BUILD
    ScePspDateTime t;
    memset(&t, 0, sizeof(t));
    sceRtcGetCurrentClockLocalTime(&t);
    snprintf(out, (size_t)len, "%04d-%02d-%02d %02d:%02d",
             t.year % 10000, t.month % 100, t.day % 100, t.hour % 100, t.minute % 100);
#else
    snprintf(out, (size_t)len, "2026-01-01 12:00");
#endif
}

static void reload_refuel(ui_state_t *ui) {
    if (!ui->refuel || !ui->cfg) return;
    char name[64];
    snprintf(name, sizeof(name), "fuel_%s.csv", ui->cfg->car_id);
    appconfig_path(name, ui->fuel_path, sizeof(ui->fuel_path));
    refuel_log_init(ui->refuel);
    refuel_load_csv(ui->refuel, ui->fuel_path);
    cost_compute(&ui->cost, ui->refuel);
}

static void form_save(ui_state_t *ui) {
    form_t *f = &ui->form;
    if (f->kind == FORM_REFUEL) {
        float km  = (float)atof(f->f[0].txt);
        float lit = (float)atof(f->f[1].txt);
        float eur = (float)atof(f->f[2].txt);
        if (km < 1 || lit < 0.5f || eur < 0.1f) {
            toast(ui, COL_RED, "Valori non validi: km, litri e prezzo devono essere > 0");
            return;
        }
        char date[20];
        current_date(date, sizeof(date));
        if (!ui->refuel || refuel_log_add(ui->refuel, date, km, lit, eur) < 0) {
            toast(ui, COL_RED, "Registro rifornimenti pieno");
            return;
        }
        cost_compute(&ui->cost, ui->refuel);
        int ok = refuel_save_csv(ui->refuel, ui->fuel_path) == 0;
        if (ui->cfg) ui->cfg->last_eur_per_l = eur;
        ui->km_fill_base = 0;
        ui->trip_at_fill = ui->live.trip_km;
        if (ui->cfg) { ui->cfg->km_since_fill = 0; appconfig_save(ui->cfg, ui->config_path); }
        char m[96];
        snprintf(m, sizeof(m), ok ? "Pieno salvato: %.2f L/100km, %.2f EUR"
                                  : "ATTENZIONE: errore salvataggio CSV (%.2f L/100, %.2f EUR)",
                 lit / km * 100.0f, lit * eur);
        toast(ui, ok ? COL_GREEN : COL_RED, m);
    } else if (f->kind == FORM_SETUP && ui->cfg) {
        app_config_t *c = ui->cfg;
        int o[4] = {0, 0, 0, 0};
        sscanf(f->f[2].txt, "%d.%d.%d.%d", &o[0], &o[1], &o[2], &o[3]);
        for (int i = 0; i < 4; i++) if (o[i] > 255) {
            toast(ui, COL_RED, "IP non valido (ogni numero 0-255)");
            return;
        }
        int ap = atoi(f->f[1].txt), port = atoi(f->f[3].txt);
        if (ap < 1 || ap > 10)     { toast(ui, COL_RED, "Profilo rete: da 1 a 10"); return; }
        if (port < 1 || port > 65535) { toast(ui, COL_RED, "Porta non valida"); return; }
        const char *old_car = c->car_id;
        int car_changed = strcmp(old_car, profile_registry[f->f[0].choice].id) != 0;
        /* Km dal pieno appartengono all'auto precedente: fissali prima del cambio */
        if (car_changed) { ui->km_fill_base = 0; ui->trip_at_fill = ui->live.trip_km; }
        strncpy(c->car_id, profile_registry[f->f[0].choice].id, sizeof(c->car_id) - 1);
        snprintf(c->elm_ip, sizeof(c->elm_ip), "%d.%d.%d.%d", o[0], o[1], o[2], o[3]);
        c->elm_port = port;
        c->elm_apctl_index = ap;
        c->km_since_fill = ui_km_since_fill(ui);
        appconfig_save(c, ui->config_path);
        if (car_changed) reload_refuel(ui);
        if (ui->obd) {
            snprintf(ui->obd->ip, sizeof(ui->obd->ip), "%s", c->elm_ip);
            ui->obd->port = port;
            ui->obd->apctl = ap;
            ui->obd->profile = &profile_registry[f->f[0].choice];
            ui->obd->req_reconnect = 1;
        }
        toast(ui, COL_GREEN, "Impostazioni salvate, riconnessione...");
    }
    f->kind = FORM_NONE;
}

static void form_input(ui_state_t *ui, unsigned int pressed) {
    form_t *f = &ui->form;
    field_t *fd = &f->f[f->field];
    if (pressed & BTN_CIRCLE) { f->kind = FORM_NONE; toast(ui, COL_DIMTEXT, "Annullato"); return; }
    if (pressed & BTN_CROSS)  { form_save(ui); return; }
    if (pressed & (BTN_UP | BTN_DOWN)) {
        int dir = (pressed & BTN_UP) ? 1 : -1;
        if (fd->kind == FK_ENUM) {
            fd->choice = (fd->choice + dir + fd->nchoices) % fd->nchoices;
        } else if (is_digit_pos(fd, f->cursor)) {
            int dg = fd->txt[f->cursor] - '0';
            fd->txt[f->cursor] = (char)('0' + (dg + dir + 10) % 10);
        }
    }
    if (pressed & BTN_RIGHT) {
        int i = f->cursor + 1;
        while (fd->kind == FK_DIGITS && fd->txt[i] && !is_digit_pos(fd, i)) i++;
        if (fd->kind == FK_ENUM || !fd->txt[i]) {
            f->field = (f->field + 1) % f->nf;
            f->cursor = first_digit(&f->f[f->field]);
        } else f->cursor = i;
    }
    if (pressed & BTN_LEFT) {
        int i = f->cursor - 1;
        while (fd->kind == FK_DIGITS && i >= 0 && !is_digit_pos(fd, i)) i--;
        if (fd->kind == FK_ENUM || i < 0) {
            f->field = (f->field - 1 + f->nf) % f->nf;
            f->cursor = last_digit(&f->f[f->field]);
        } else f->cursor = i;
    }
}

/* ================================================================ */
/*                             Input                                */
/* ================================================================ */

static int dash_has_layout(int d) {
    return d != DASH_ACCEL && d != DASH_SETUP;
}

static void select_first_on_dash(ui_state_t *ui) {
    for (int i = 0; i < ui->widget_count; i++)
        if (ui->widgets[i].dash_id == ui->current_dash) { ui->selected_widget = i; return; }
    ui->selected_widget = -1;
}

static void select_step(ui_state_t *ui, int dir) {
    int n = ui->widget_count;
    if (n <= 0) return;
    int start = ui->selected_widget < 0 ? 0 : ui->selected_widget;
    for (int k = 1; k <= n; k++) {
        int i = (start + dir * k + n * k) % n;
        if (ui->widgets[i].dash_id == ui->current_dash) { ui->selected_widget = i; return; }
    }
}

static void config_input(ui_state_t *ui, unsigned int pressed) {
    widget_t *w = (ui->selected_widget >= 0 && ui->selected_widget < ui->widget_count)
                  ? &ui->widgets[ui->selected_widget] : NULL;
    if (pressed & BTN_TRIANGLE) {
        ui->config_mode = 0;
        ui->move_mode = 0;
        if (ui->layout_dirty) {
            int ok = ui_save_layout(ui, ui->layout_path) == 0;
            toast(ui, ok ? COL_GREEN : COL_RED, ok ? "Layout salvato" : "Errore salvataggio layout");
        }
        return;
    }
    if (ui->move_mode && w) {
        int step = 2;
        if (pressed & BTN_LEFT)  w->x -= step;
        if (pressed & BTN_RIGHT) w->x += step;
        if (pressed & BTN_UP)    w->y -= step;
        if (pressed & BTN_DOWN)  w->y += step;
        if (w->x < 0) w->x = 0;
        if (w->y < HDR_H) w->y = HDR_H;
        if (w->x + w->w > GFX_W) w->x = GFX_W - w->w;
        if (w->y + w->h > FTR_Y - 1) w->y = FTR_Y - 1 - w->h;
        if (pressed & (BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN)) ui->layout_dirty = 1;
        if (pressed & BTN_CIRCLE) ui->move_mode = 0;
        return;
    }
    if (pressed & (BTN_RIGHT | BTN_DOWN)) select_step(ui, 1);
    if (pressed & (BTN_LEFT | BTN_UP))    select_step(ui, -1);
    if (w && (pressed & BTN_CROSS))  { w->visible ^= 1; ui->layout_dirty = 1; }
    if (w && (pressed & BTN_CIRCLE)) ui->move_mode = 1;
    if (pressed & BTN_SQUARE) {
        ui_set_default_layout(ui);
        ui->layout_dirty = 1;
        select_first_on_dash(ui);
        toast(ui, COL_YELLOW, "Layout predefinito ripristinato");
    }
}

static void dash_input(ui_state_t *ui, unsigned int pressed) {
    obd_task_t *t = ui->obd;
    int ready = t && t->link == LINK_READY;
    switch (ui->current_dash) {
    case DASH_DRIVE:
        if ((pressed & BTN_SQUARE) && t) {
            ui->km_fill_base = ui_km_since_fill(ui);
            ui->trip_at_fill = 0;
            t->req_trip_reset = 1;
            toast(ui, COL_GREEN, "Trip azzerato");
        }
        break;
    case DASH_COSTS:
        if (pressed & BTN_CROSS) open_refuel_form(ui);
        break;
    case DASH_DIAG:
        if (pressed & BTN_CROSS) {
            if (ready) { t->req_read_dtc = 1; toast(ui, COL_TEXT, "Lettura DTC..."); }
            else toast(ui, COL_RED, "Non connesso all'auto");
        }
        if (pressed & BTN_CIRCLE) {
            if (!ready) toast(ui, COL_RED, "Non connesso all'auto");
            else if (ui->clear_confirm_ms && ui->now_ms - ui->clear_confirm_ms < 3000) {
                ui->clear_confirm_ms = 0;
                t->req_clear_dtc = 1;
                toast(ui, COL_YELLOW, "Cancellazione DTC inviata...");
            } else {
                ui->clear_confirm_ms = ui->now_ms ? ui->now_ms : 1;
                toast(ui, COL_YELLOW, "Premi di nuovo O entro 3s per cancellare DTC e spia MIL");
            }
        }
        if (t && (pressed & (BTN_UP | BTN_DOWN))) {
            int rows = 5;
            for (int i = 0; i < ui->widget_count; i++)
                if (ui->widgets[i].type == W_DTC_LIST && ui->widgets[i].dash_id == DASH_DIAG)
                    rows = dtc_rows_visible(&ui->widgets[i]);
            int maxs = t->dtc_count - rows;
            if (maxs < 0) maxs = 0;
            if (pressed & BTN_DOWN) ui->dtc_scroll++;
            if (pressed & BTN_UP)   ui->dtc_scroll--;
            if (ui->dtc_scroll > maxs) ui->dtc_scroll = maxs;
            if (ui->dtc_scroll < 0) ui->dtc_scroll = 0;
        }
        break;
    case DASH_ACCEL:
        if ((pressed & BTN_CROSS) && t) {
            if (!ready) toast(ui, COL_RED, "Non connesso all'auto");
            else { t->req_accel_arm = 1; toast(ui, COL_GREEN, "Cronometro armato: parti da fermo"); }
        }
        break;
    case DASH_CAMERA:
        if (pressed & BTN_CROSS) {
            if (!ui->cam || !ui->cam->available) toast(ui, COL_RED, "Camera non disponibile");
            else if (cam_snapshot(ui->cam, ui->snap_dir, ui->live.speed,
                                  ui->live.l100_live) == 0)
                toast(ui, COL_GREEN, "Foto in salvataggio...");
        }
        break;
    case DASH_SETUP:
        if (pressed & BTN_CROSS) open_setup_form(ui);
        if ((pressed & BTN_SQUARE) && t) {
            t->req_reconnect = 1;
            toast(ui, COL_YELLOW, "Riconnessione...");
        }
        break;
    }
}

int ui_process_buttons(ui_state_t *ui, unsigned int buttons) {
    ui->last_buttons = ui->buttons;
    ui->buttons = buttons;
    unsigned int pressed = ui->buttons & ~ui->last_buttons;

    /* Auto-ripetizione frecce (form, spostamento widget, scroll) */
    const unsigned int arrows = BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT;
    if ((ui->buttons & arrows) && (ui->buttons & arrows) == (ui->last_buttons & arrows)) {
        if (ui->now_ms - ui->hold_ms > 400 && ui->now_ms - ui->repeat_ms > 90) {
            pressed |= ui->buttons & arrows;
            ui->repeat_ms = ui->now_ms;
        }
    } else {
        ui->hold_ms = ui->now_ms;
    }

    if ((ui->buttons & BTN_START) && (ui->buttons & BTN_SELECT)) return 1;

    /* Esito foto (asincrono) */
    if (ui->cam && ui->cam->snap_result) {
        char m[96];
        if (ui->cam->snap_result > 0) snprintf(m, sizeof(m), "Foto salvata: %s", ui->cam->snap_name);
        else snprintf(m, sizeof(m), "Errore salvataggio foto");
        toast(ui, ui->cam->snap_result > 0 ? COL_GREEN : COL_RED, m);
        ui->cam->snap_result = 0;
    }

    if (ui->form.kind != FORM_NONE) { form_input(ui, pressed); return 0; }
    if (ui->config_mode)            { config_input(ui, pressed); return 0; }

    if (pressed & BTN_L) ui->current_dash = (ui->current_dash - 1 + DASH_COUNT) % DASH_COUNT;
    if (pressed & BTN_R) ui->current_dash = (ui->current_dash + 1) % DASH_COUNT;
    if (pressed & (BTN_L | BTN_R)) { ui->clear_confirm_ms = 0; return 0; }

    if ((pressed & BTN_TRIANGLE) && dash_has_layout(ui->current_dash)) {
        ui->config_mode = 1;
        ui->move_mode = 0;
        select_first_on_dash(ui);
        return 0;
    }
    dash_input(ui, pressed);
    return 0;
}

int ui_handle_input(ui_state_t *ui) {
    unsigned int b = 0;
#ifdef PSP_BUILD
    SceCtrlData pad;
    sceCtrlPeekBufferPositive(&pad, 1);
    b = pad.Buttons;
#endif
    return ui_process_buttons(ui, b);
}
