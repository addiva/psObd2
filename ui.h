/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: ui.h  |  Modulo: sistema UI (widget, dashboard, form, input)
 */
#ifndef UI_H
#define UI_H

#include "config.h"
#include "live.h"
#include "profile.h"
#include "fuel.h"
#include "cost.h"
#include "accel.h"
#include "camera.h"
#include "obdtask.h"
#include "appconfig.h"

/* ---- Dashboard ---- */
enum {
    DASH_DRIVE = 0, DASH_ENGINE, DASH_CHARTS, DASH_COSTS,
    DASH_DIAG, DASH_ACCEL, DASH_CAMERA, DASH_SETUP
};

/* ---- Tipi widget (valori salvati in layout.json) ---- */
typedef enum {
    W_NUMERIC,
    W_GAUGE,
    W_LINECHART,
    W_BARCHART,
    W_LAMP,
    W_DTC_LIST,
    W_COST_CARD,
    W_CAMERA
} widget_type_t;

typedef struct {
    int           id;
    widget_type_t type;
    char          title[20];
    metric_src_t  source;
    float         min, max;
    float         red_zone;
    int           group_id;
    int           visible;
    int           dash_id;
    int           x, y, w, h;
} widget_t;

typedef struct {
    float samples[CHART_HIST_LEN];
    int   head;
    int   count;
} chart_buf_t;

/* ---- Form di inserimento (rifornimento / impostazioni) ---- */
enum { FK_DIGITS, FK_ENUM };
enum { FORM_NONE, FORM_REFUEL, FORM_SETUP };

typedef struct {
    char label[28];
    int  kind;
    char txt[24];              /* FK_DIGITS: cifre modificabili + separatori */
    int  choice, nchoices;     /* FK_ENUM */
    const char *choices[8];
} field_t;

typedef struct {
    int     kind;              /* FORM_NONE = chiuso */
    char    title[40];
    int     nf, field, cursor;
    field_t f[6];
} form_t;

typedef struct {
    int         current_dash;
    int         config_mode;
    int         move_mode;
    int         selected_widget;   /* INDICE in widgets[] */
    int         layout_dirty;

    widget_t    widgets[WIDGET_MAX];
    int         widget_count;

    chart_buf_t chart_speed, chart_l100, chart_rpm;
    unsigned int last_live_seq;

    /* Copie per il frame corrente */
    live_t        live;
    accel_run_t   accel;
    accel_best_t  best;

    /* Collegamenti (impostati da main) */
    obd_task_t   *obd;
    cam_ctx_t    *cam;
    app_config_t *cfg;
    refuel_log_t *refuel;
    cost_summary_t cost;
    char          fuel_path[192];
    char          layout_path[192];
    char          config_path[192];
    char          snap_dir[192];

    /* Km dall'ultimo pieno = base + (trip_km - trip_at_fill) */
    float         km_fill_base;
    float         trip_at_fill;

    /* Diagnostica */
    int           dtc_scroll;
    unsigned int  clear_confirm_ms;

    form_t        form;

    char          toast[96];
    unsigned int  toast_ms;
    unsigned int  toast_color;

    unsigned int  buttons, last_buttons;
    unsigned int  hold_ms, repeat_ms;
    unsigned int  now_ms;
} ui_state_t;

int   ui_init(ui_state_t *ui);
void  ui_set_default_layout(ui_state_t *ui);
void  ui_load_layout(ui_state_t *ui, const char *layout_file);
int   ui_save_layout(ui_state_t *ui, const char *layout_file);

/* Copia lo stato pubblicato dal thread OBD e aggiorna i grafici */
void  ui_sync(ui_state_t *ui);

/* Input: ritorna 1 se l'utente chiede di uscire */
int   ui_handle_input(ui_state_t *ui);
/* Gestione di un set di tasti gia' letto (usata anche dai test su PC) */
int   ui_process_buttons(ui_state_t *ui, unsigned int buttons);

void  ui_draw(ui_state_t *ui);
void  ui_shutdown(ui_state_t *ui);

/* Km dall'ultimo pieno (per salvarli in config) */
float ui_km_since_fill(const ui_state_t *ui);

/* Pulsanti (codici PSP_CTRL_*; stessi valori anche su PC) */
#define BTN_SELECT   0x000001
#define BTN_START    0x000008
#define BTN_UP       0x000010
#define BTN_RIGHT    0x000020
#define BTN_DOWN     0x000040
#define BTN_LEFT     0x000080
#define BTN_L        0x000100
#define BTN_R        0x000200
#define BTN_TRIANGLE 0x001000
#define BTN_CIRCLE   0x002000
#define BTN_CROSS    0x004000
#define BTN_SQUARE   0x008000

/* ---- Colori (formato PSP 0xAABBGGRR) ---- */
#define COL_BG       0xFF141414
#define COL_PANEL    0xFF242424
#define COL_PANEL2   0xFF333333
#define COL_TRACK    0xFF3C3C3C
#define COL_ACCENT   0xFF00A5FF   /* arancio */
#define COL_BLUE     0xFFFFB040   /* azzurro */
#define COL_TEXT     0xFFE8E8E8
#define COL_DIMTEXT  0xFF8A8A8A
#define COL_RED      0xFF3030F0
#define COL_GREEN    0xFF40C040
#define COL_YELLOW   0xFF20D0E0
#define COL_WHITE    0xFFFFFFFF
#define COL_BLACK    0xFF000000

#endif /* UI_H */
