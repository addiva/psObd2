/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: main.c  |  Modulo: wiring principale, loop OBD, loop UI
 */
#include <string.h>
#include <stdio.h>

#include "config.h"
#include "appconfig.h"
#include "obd.h"
#include "profile.h"
#include "fuel.h"
#include "cost.h"
#include "accel.h"
#include "elm.h"
#include "net.h"
#include "ui.h"
#include "camera.h"

#ifdef PSP_BUILD
#include <pspkernel.h>
#include <pspdebug.h>

PSP_MODULE_INFO("pspobd2", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(20480);

/* Callback uscita PSP */
static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1; (void)arg2; (void)common;
    sceKernelExitGame();
    return 0;
}
static int callback_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("ExitCallback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}
static void setup_callbacks(void) {
    SceUID tid = sceKernelCreateThread("update_thread", callback_thread,
                                       0x11, 0xFA0, 0, NULL);
    if (tid >= 0) sceKernelStartThread(tid, 0, NULL);
}
#endif /* PSP_BUILD */

/* ---- Stato applicazione ---- */
typedef struct {
    app_config_t    cfg;
    const car_profile_t *profile;
    car_caps_t      caps;
    trip_t          trip;
    refuel_log_t    refuel;
    cost_summary_t  cost;
    accel_run_t     accel_run;
    accel_best_t    accel_best;
    elm_ctx_t       elm;
    cam_ctx_t       cam;
    ui_state_t      ui;
    live_t          live;

    int  connected;
    int  quit;

    unsigned int last_poll_ms;
} app_t;

static app_t g_app;

/* ---- Capability check: interroga PID support bitmask ---- */
static void do_capability_check(app_t *app) {
    uint8_t sup[16];
    memset(sup, 0, sizeof(sup));
    elm_query_supported_pids(&app->elm, sup);

    car_caps_t *c = &app->caps;
    /* Blocco 0100: bit 1-32 */
    c->pid_mil        = obd_pid_supported(sup,     4,  1); /* 0101 = bit 1 */
    c->pid_coolant    = obd_pid_supported(sup,     4,  5); /* 0105 = bit 5 */
    c->pid_rpm        = obd_pid_supported(sup,     4, 12); /* 010C = bit 12 */
    c->pid_speed      = obd_pid_supported(sup,     4, 13); /* 010D = bit 13 */
    c->pid_maf        = obd_pid_supported(sup,     4, 16); /* 0110 = bit 16 */
    c->pid_throttle   = obd_pid_supported(sup,     4, 17); /* 0111 = bit 17 */
    c->pid_fuel_level = obd_pid_supported(sup,     4, 47); /* 012F = bit 47 -> blocco 0120 bit 15 */
    /* Blocco 0140: bit 1-32 -> 015E = bit 30 nel blocco 0140 */
    c->pid_fuel_rate  = obd_pid_supported(sup + 8, 4, 30); /* 015E */

    profile_resolve_strategy(app->profile, c);
}

/* ---- Campionamento OBD ---- */
static void poll_obd(app_t *app) {
    live_t *L = &app->live;
    car_caps_t *c = &app->caps;
    const car_profile_t *p = app->profile;
    elm_ctx_t *e = &app->elm;
    uint8_t d[8];
    int n;

    /* RPM */
    if (c->pid_rpm) {
        n = elm_query_pid(e, "010C", d, sizeof(d));
        if (n > 0) L->rpm = obd_rpm(d, n);
    }
    /* Velocita' */
    if (c->pid_speed) {
        n = elm_query_pid(e, "010D", d, sizeof(d));
        if (n > 0) L->speed = obd_speed(d, n);
    }
    /* Temp refrigerante */
    if (c->pid_coolant) {
        n = elm_query_pid(e, "0105", d, sizeof(d));
        if (n > 0) L->coolant = obd_coolant_c(d, n);
    }
    /* MAF */
    if (c->pid_maf) {
        n = elm_query_pid(e, "0110", d, sizeof(d));
        if (n > 0) L->maf = obd_maf_gps(d, n);
    }
    /* Livello carburante */
    if (c->pid_fuel_level) {
        n = elm_query_pid(e, "012F", d, sizeof(d));
        if (n > 0) L->fuel_level = obd_fuel_level_pct(d, n);
    }
    /* Farfalla */
    if (c->pid_throttle) {
        n = elm_query_pid(e, "0111", d, sizeof(d));
        if (n > 0) L->throttle = obd_throttle_pct(d, n);
    }
    /* MIL */
    if (c->pid_mil) {
        n = elm_query_pid(e, "0101", d, sizeof(d));
        if (n > 0) {
            int mil;
            obd_mil_status(d, n, &mil);
            L->mil = mil;
        }
    }

    /* Consumo istantaneo in base alla strategia */
    L->lh_live   = -1.0f;
    L->l100_live = -1.0f;
    switch (c->cons) {
        case CONS_FUEL_RATE:
            if (c->pid_fuel_rate) {
                n = elm_query_pid(e, "015E", d, sizeof(d));
                if (n > 0) {
                    L->lh_live   = obd_fuel_rate_lh(d, n);
                    L->l100_live = cost_l100_from_lh(L->lh_live, (float)L->speed);
                }
            }
            break;
        case CONS_MAF:
            if (L->maf > 0 && p) {
                L->lh_live   = L->maf * 3600.0f /
                               (p->afr_stoich * p->fuel_density);
                L->l100_live = cost_l100_from_maf(L->maf, (float)L->speed,
                                                  p->afr_stoich, p->fuel_density);
            }
            break;
        default: break;
    }

    /* Trip integrator */
    float dt = OBD_POLL_MS / 1000.0f;
    if (L->lh_live >= 0) {
        trip_update(&app->trip, L->lh_live, (float)L->speed, dt);
        L->l100_trip = trip_avg_l100(&app->trip);
    }

    /* Accel timer */
    if (app->accel_run.active && c->pid_speed) {
        float now = net_time_ms() / 1000.0f;
        if (accel_update(&app->accel_run, L->speed, now))
            accel_update_best(&app->accel_best, &app->accel_run);
    }
    /* Reset accel se auto ferma */
    if (!app->accel_run.active && L->speed == 0)
        accel_reset(&app->accel_run);
}

/* ---- Init connessione ---- */
static int do_connect(app_t *app) {
    elm_init(&app->elm, app->cfg.elm_ip, app->cfg.elm_port);

    if (net_apctl_connect(app->cfg.elm_apctl_index) < 0)
        return -1;
    if (elm_connect(&app->elm) < 0)
        return -1;

    do_capability_check(app);
    app->connected = 1;
    return 0;
}

/* ---- Fuel log path ---- */
static void fuel_csv_path(const app_t *app, char *out, int len) {
    snprintf(out, len, "%sfuel_%s.csv", APP_DIR, app->cfg.car_id);
}

/* ---- Main ---- */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

#ifdef PSP_BUILD
    setup_callbacks();
#endif

    app_t *app = &g_app;
    memset(app, 0, sizeof(*app));

    /* Carica configurazione */
    appconfig_ensure_dir(APP_DIR);
    appconfig_load(&app->cfg, CONFIG_FILE);

    /* Profilo auto */
    app->profile = profile_find(app->cfg.car_id);
    if (!app->profile)
        app->profile = &profile_registry[0]; /* fallback MX5 NC */

    /* Fuel log */
    refuel_log_init(&app->refuel);
    char fpath[256];
    fuel_csv_path(app, fpath, sizeof(fpath));
    refuel_load_csv(&app->refuel, fpath);

    /* Accel best da config */
    for (int i = 0; i < 7; i++)
        app->accel_best.best[i] = app->cfg.accel_best[i];

    /* Camera */
    if (cam_init(&app->cam) == 0)
        cam_start(&app->cam);

    /* UI */
    ui_init(&app->ui);
    ui_load_layout(&app->ui, app->profile, LAYOUT_FILE);
    app->ui.refuel     = &app->refuel;
    app->ui.accel_run  = &app->accel_run;
    app->ui.accel_best = &app->accel_best;
    app->ui.cam        = &app->cam;
    app->ui.elm        = &app->elm;

    /* Tenta connessione al primo avvio */
    do_connect(app);

    /* ---- Main loop ---- */
    while (!app->quit) {
        /* Polling OBD (se connesso) */
        unsigned int now = net_time_ms();
        if (app->connected && now - app->last_poll_ms >= OBD_POLL_MS) {
            app->last_poll_ms = now;
            poll_obd(app);
            cost_compute(&app->cost, &app->refuel);
            app->ui.cost = app->cost;
            ui_push_sample(&app->ui, &app->live);
        }

        /* Se disconnesso, tenta riconnessione ogni 5s */
        if (!app->connected) {
            static unsigned int last_retry = 0;
            if (now - last_retry > 5000) {
                last_retry = now;
                do_connect(app);
            }
        }

        /* UI */
        if (ui_handle_input(&app->ui)) app->quit = 1;
        ui_draw(&app->ui);
    }

    /* ---- Cleanup ---- */
    ui_save_layout(&app->ui, LAYOUT_FILE);

    /* Salva best accel */
    for (int i = 0; i < 7; i++)
        app->cfg.accel_best[i] = app->accel_best.best[i];
    appconfig_save(&app->cfg, CONFIG_FILE);

    refuel_save_csv(&app->refuel, fpath);

    cam_stop(&app->cam);
    elm_disconnect(&app->elm);
    net_apctl_disconnect();
    net_cleanup();
    ui_shutdown(&app->ui);

#ifdef PSP_BUILD
    sceKernelExitGame();
#endif
    return 0;
}
