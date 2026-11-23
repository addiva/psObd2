/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: main.c  |  Modulo: avvio, thread, loop UI, salvataggi
 *
 * Thread:
 *   main (UI, prio 0x20)  -> input + disegno a 60 Hz, mai bloccato dalla rete
 *   obd_worker (0x18)     -> WiFi, ELM327, polling PID, DTC  (obdtask.c)
 *   cam_capture (0x12)    -> Go!Cam opzionale                 (camera.c)
 *   exit callback (0x11)  -> tasto HOME > Esci: chiude salvando i dati
 */
#include <string.h>
#include <stdio.h>

#include "config.h"
#include "appconfig.h"
#include "profile.h"
#include "fuel.h"
#include "cost.h"
#include "net.h"
#include "obdtask.h"
#include "ui.h"
#include "camera.h"

#ifdef PSP_BUILD
#include <pspkernel.h>

PSP_MODULE_INFO("pspobd2", PSP_MODULE_USER, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
/* Heap moderato: lascia memoria ai moduli di rete/USB caricati a runtime */
PSP_HEAP_SIZE_KB(4096);

static volatile int g_exit_requested = 0;

static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1; (void)arg2; (void)common;
    g_exit_requested = 1;          /* il loop principale salva ed esce */
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
#else
static volatile int g_exit_requested = 0;
#endif

static app_config_t g_cfg;
static refuel_log_t g_refuel;
static obd_task_t   g_obd;
static cam_ctx_t    g_cam;
static ui_state_t   g_ui;

int main(int argc, char *argv[]) {
#ifdef PSP_BUILD
    setup_callbacks();
#endif
    if (argc > 0 && argv) appconfig_set_dir_from_argv(argv[0]);
    appconfig_ensure_dir(appconfig_dir());

    ui_state_t *ui = &g_ui;
    ui_init(ui);
    appconfig_path(CONFIG_NAME, ui->config_path, sizeof(ui->config_path));
    appconfig_path(LAYOUT_NAME, ui->layout_path, sizeof(ui->layout_path));
    appconfig_path(SNAP_SUBDIR, ui->snap_dir,    sizeof(ui->snap_dir));

    /* Config: se manca la creiamo subito, cosi' c'e' un modello da modificare */
    if (appconfig_load(&g_cfg, ui->config_path) < 0)
        appconfig_save(&g_cfg, ui->config_path);
    const car_profile_t *profile = profile_find(g_cfg.car_id);
    if (!profile) {
        profile = &profile_registry[0];
        strncpy(g_cfg.car_id, profile->id, sizeof(g_cfg.car_id) - 1);
    }

    /* Rifornimenti dell'auto selezionata */
    char name[64];
    snprintf(name, sizeof(name), "fuel_%s.csv", g_cfg.car_id);
    appconfig_path(name, ui->fuel_path, sizeof(ui->fuel_path));
    refuel_log_init(&g_refuel);
    refuel_load_csv(&g_refuel, ui->fuel_path);

    /* Thread OBD */
    obdtask_init(&g_obd, g_cfg.elm_ip, g_cfg.elm_port, g_cfg.elm_apctl_index, profile);
    for (int i = 0; i < ACCEL_THRESHOLDS_N; i++) g_obd.best.best[i] = g_cfg.accel_best[i];
    obdtask_start(&g_obd);
    int best_seq_seen = g_obd.best_seq;

    /* Camera (rilevamento in background) */
    cam_start(&g_cam);

    /* UI */
    ui_load_layout(ui, ui->layout_path);
    ui->obd    = &g_obd;
    ui->cam    = &g_cam;
    ui->cfg    = &g_cfg;
    ui->refuel = &g_refuel;
    ui->km_fill_base = g_cfg.km_since_fill;
    cost_compute(&ui->cost, &g_refuel);

    while (!g_exit_requested) {
        ui_sync(ui);
        if (ui_handle_input(ui)) break;
        ui_draw(ui);                 /* include l'attesa del vsync */

        /* Nuovo record 0-100: salvalo subito */
        if (g_obd.best_seq != best_seq_seen) {
            best_seq_seen = g_obd.best_seq;
            for (int i = 0; i < ACCEL_THRESHOLDS_N; i++) g_cfg.accel_best[i] = g_obd.best.best[i];
            appconfig_save(&g_cfg, ui->config_path);
        }
    }

    /* ---- Uscita: salva tutto ---- */
    obdtask_stop(&g_obd);
    cam_stop(&g_cam);
    for (int i = 0; i < ACCEL_THRESHOLDS_N; i++) g_cfg.accel_best[i] = g_obd.best.best[i];
    g_cfg.km_since_fill = ui_km_since_fill(ui);
    appconfig_save(&g_cfg, ui->config_path);
    if (ui->layout_dirty) ui_save_layout(ui, ui->layout_path);
    net_apctl_disconnect();
    net_cleanup();
    ui_shutdown(ui);

#ifdef PSP_BUILD
    sceKernelExitGame();
#endif
    return 0;
}
