/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: ui.h  |  Modulo: sistema UI (GU 2D, widget, dash, grafici)
 */
#ifndef UI_H
#define UI_H

#include "config.h"
#include "profile.h"
#include "fuel.h"
#include "cost.h"
#include "accel.h"
#include "camera.h"
#include "elm.h"

/* ---- Sorgenti metriche ---- */
typedef enum {
    SRC_RPM = 0,
    SRC_SPEED,
    SRC_COOLANT,
    SRC_MAF,
    SRC_FUEL_LEVEL,
    SRC_THROTTLE,
    SRC_L100_LIVE,
    SRC_L100_TRIP,
    SRC_LH_LIVE,
    SRC_MIL,
    SRC_DTC_COUNT,
    SRC_FUEL_TOTAL_KM,
    SRC_FUEL_TOTAL_EUR,
    SRC_LAST_L100,
    SRC_LAST_EUR,
    SRC_AVG_L100,
    SRC_AVG_EUR100,
    SRC_ACCEL_TIME,    /* tempo corrente / best per la soglia selezionata */
    SRC_COUNT
} metric_src_t;

/* ---- Tipi widget ---- */
typedef enum {
    W_NUMERIC,    /* valore grande + unita' */
    W_GAUGE,      /* arco 0..max con zona rossa */
    W_LINECHART,  /* serie storica scrollante */
    W_BARCHART,   /* barre rifornimenti */
    W_LAMP,       /* spia on/off */
    W_DTC_LIST,   /* lista DTC */
    W_COST_CARD,  /* riepilogo costi */
    W_CAMERA      /* feed video camera */
} widget_type_t;

/* ---- Definizione widget ---- */
typedef struct {
    int          id;
    widget_type_t type;
    const char  *title;
    metric_src_t source;
    float        min, max;
    float        red_zone;    /* soglia zona rossa (gauge) */
    int          group_id;
    int          visible;
    int          dash_id;
    int          x, y, w, h;
} widget_t;

/* Ring buffer per linechart */
typedef struct {
    float samples[CHART_HIST_LEN];
    int   head;
    int   count;
} chart_buf_t;

/* ---- Stato live metriche (aggiornato dal loop OBD) ---- */
typedef struct {
    float rpm;
    int   speed;
    int   coolant;
    float maf;
    float fuel_level;
    int   throttle;
    float lh_live;
    float l100_live;
    float l100_trip;
    int   mil;
    int   dtc_count;
    char  dtcs[32][6];    /* max 32 DTC */
    int   dtc_valid;      /* 1 = lista aggiornata */

    /* Telemetria camera overlay */
    int   cam_speed;
    float cam_l100;
} live_t;

/* ---- Stato UI globale ---- */
typedef struct {
    int         current_dash;
    int         config_mode;           /* 1 = widget selezionabile/spostabile */
    int         selected_widget;

    widget_t    widgets[WIDGET_MAX];
    int         widget_count;

    chart_buf_t chart_speed;
    chart_buf_t chart_l100;
    chart_buf_t chart_rpm;

    live_t      live;
    cost_summary_t cost;
    refuel_log_t  *refuel;            /* puntatore al log rifornimenti */
    accel_run_t   *accel_run;
    accel_best_t  *accel_best;
    cam_ctx_t     *cam;
    elm_ctx_t     *elm;

    /* Input */
    unsigned int last_buttons;
    unsigned int buttons;
} ui_state_t;

/* ---- API UI ---- */

/* Inizializza GU e stato UI. Ritorna 0 OK. */
int  ui_init(ui_state_t *ui);

/* Carica il layout da file (o imposta il default).
 * profile puo' essere NULL (usa layout generico). */
void ui_load_layout(ui_state_t *ui, const car_profile_t *profile,
                    const char *layout_file);

/* Salva il layout corrente in layout_file. */
void ui_save_layout(ui_state_t *ui, const char *layout_file);

/* Aggiorna i grafici con il nuovo campione live. Chiamata dal loop OBD. */
void ui_push_sample(ui_state_t *ui, const live_t *live);

/* Processa gli input controller e aggiorna ui->current_dash ecc.
 * Ritorna 1 se l'utente ha richiesto uscita dall'app. */
int  ui_handle_input(ui_state_t *ui);

/* Disegna il frame corrente (GU). */
void ui_draw(ui_state_t *ui);

/* Libera le risorse GU. */
void ui_shutdown(ui_state_t *ui);

/* ---- Colori tema scuro ---- */
#define COL_BG       0xFF1A1A1A
#define COL_PANEL    0xFF262626
#define COL_ACCENT   0xFF00C0FF
#define COL_TEXT     0xFFE0E0E0
#define COL_DIMTEXT  0xFF808080
#define COL_RED      0xFF4040FF   /* GU ABGR: FF=rosso -> 0xFF0000FF? check endian */
#define COL_GREEN    0xFF00C040
#define COL_YELLOW   0xFF00C0C0
#define COL_WHITE    0xFFFFFFFF

#endif /* UI_H */
