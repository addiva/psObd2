/* it.massirito.pspobd2 - strumento di sviluppo (solo PC)
 * File: tools/ui_preview.c
 *
 *   ui_preview demo OUTDIR        -> PPM di tutte le schermate con dati finti
 *   ui_preview e2e PORT OUTDIR    -> collega obdtask a un ELM327 simulato
 *                                    (tools/fake_elm.py), verifica il flusso e
 *                                    salva le schermate con i dati ricevuti
 * Usa lo stesso codice UI/testo della PSP con un rasterizzatore software.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ui.h"
#include "gfx.h"
#include "net.h"

static app_config_t cfg;
static refuel_log_t refuel;
static obd_task_t   obd;
static cam_ctx_t    cam;
static ui_state_t   ui;
static int fails = 0;

#define EXPECT(c, msg) do { if (c) printf("  OK   %s\n", msg); \
                            else { printf("  FAIL %s\n", msg); fails++; } } while (0)

static void save_ppm(const char *dir, const char *name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fprintf(f, "P6\n%d %d\n255\n", GFX_W, GFX_H);
    const uint32_t *px = gfx_soft_pixels();
    for (int y = 0; y < GFX_H; y++)
        for (int x = 0; x < GFX_W; x++) {
            uint32_t c = px[y * GFX_STRIDE + x];      /* 0xAABBGGRR */
            unsigned char rgb[3] = { c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF };
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
}

static void shot(const char *dir, int dash, const char *name) {
    ui.current_dash = dash;
    ui_sync(&ui);
    ui_draw(&ui);
    save_ppm(dir, name);
}

static void press(unsigned int b) {
    ui_process_buttons(&ui, b);
    ui_process_buttons(&ui, 0);
}

static void common_setup(void) {
    appconfig_defaults(&cfg);
    refuel_log_init(&refuel);
    refuel_log_add(&refuel, "2026-09-01 10:00", 512, 41.2f, 1.859f);
    refuel_log_add(&refuel, "2026-09-20 18:30", 488, 39.0f, 1.829f);
    refuel_log_add(&refuel, "2026-10-08 08:10", 530, 42.5f, 1.799f);
    refuel_log_add(&refuel, "2026-10-29 19:45", 470, 37.1f, 1.849f);
    memset(&cam, 0, sizeof(cam));
    ui_init(&ui);
    ui_set_default_layout(&ui);
    ui.obd = &obd; ui.cam = &cam; ui.cfg = &cfg; ui.refuel = &refuel;
    strcpy(ui.fuel_path, "/tmp/psobd2_fuel_preview.csv");
    strcpy(ui.config_path, "/tmp/psobd2_config_preview.json");
    strcpy(ui.layout_path, "/tmp/psobd2_layout_preview.json");
    cost_compute(&ui.cost, &refuel);
    ui.km_fill_base = 236.4f;
}

static int demo(const char *dir) {
    common_setup();
    obdtask_init(&obd, cfg.elm_ip, cfg.elm_port, 1, &profile_registry[0]);

    /* Non connesso */
    obd.link = LINK_ERROR;
    strcpy(obd.status, "WiFi profilo 1: rete ELM non trovata");
    shot(dir, DASH_DRIVE, "00_guida_non_connesso");
    shot(dir, DASH_SETUP, "01_setup_non_connesso");

    /* Connesso con dati */
    obd.link = LINK_READY;
    strcpy(obd.status, "Connesso");
    strcpy(obd.elm.version, "ELM327 v1.5");
    strcpy(obd.elm.protocol, "ISO 15765-4 (CAN 11/500)");
    strcpy(obd.local_ip, "192.168.0.11");
    obd.poll_hz = 5.6f;
    obd.caps.cons = CONS_MAF;
    live_t *L = &obd.live;
    L->rpm = 3420; L->speed = 96; L->coolant = 91; L->maf = 18.4f;
    L->fuel_level = 62.0f; L->throttle = 23; L->lh_live = 6.05f;
    L->l100_live = 6.3f; L->l100_trip = 7.4f; L->trip_km = 41.7f;
    L->batt_v = 14.1f; L->mil = 1; L->dtc_ecu = 2;
    L->valid = 0xFFFFFFFFu;
    for (int i = 0; i < 150; i++) {
        obd.live_seq++;
        L->speed = 50 + (i * 7) % 60; L->rpm = 1500 + (i * 53) % 4000;
        L->l100_live = 4 + (i % 17) * 0.6f;
        ui_sync(&ui);
    }
    L->speed = 96; L->rpm = 3420; L->l100_live = 6.3f;
    strcpy(obd.dtc[0], "P0133"); obd.dtc_pending[0] = 0;
    strcpy(obd.dtc[1], "P2004"); obd.dtc_pending[1] = 0;
    strcpy(obd.dtc[2], "P0301"); obd.dtc_pending[2] = 1;
    strcpy(obd.dtc[3], "P1260"); obd.dtc_pending[3] = 0;
    obd.dtc_count = 4; obd.dtc_valid = 1;
    strcpy(obd.dtc_msg, "3 confermati, 1 in attesa");
    obd.best.best[1] = 1.92f; obd.best.best[2] = 3.85f; obd.best.best[3] = 8.71f;
    obd.accel.hit[0] = obd.accel.hit[1] = obd.accel.hit[2] = obd.accel.hit[3] = 1;
    obd.accel.t_hit[1] = 2.01f; obd.accel.t_hit[2] = 4.02f; obd.accel.t_hit[3] = 9.10f;

    const char *names[DASH_COUNT] = { "02_guida", "03_motore", "04_grafici", "05_costi",
                                      "06_diagnostica", "07_accel", "08_camera", "09_setup" };
    for (int d = 0; d < DASH_COUNT; d++) shot(dir, d, names[d]);

    /* Form rifornimento */
    ui.current_dash = DASH_COSTS;
    press(BTN_CROSS);
    press(BTN_RIGHT); press(BTN_UP);
    shot(dir, DASH_COSTS, "10_form_rifornimento");
    press(BTN_CIRCLE);
    /* Form impostazioni */
    ui.current_dash = DASH_SETUP;
    press(BTN_CROSS);
    press(BTN_RIGHT); press(BTN_RIGHT);
    shot(dir, DASH_SETUP, "11_form_setup");
    press(BTN_CIRCLE);
    /* Modalita' layout */
    ui.current_dash = DASH_DRIVE;
    press(BTN_TRIANGLE);
    press(BTN_RIGHT);
    press(BTN_CROSS);          /* nasconde il widget selezionato */
    press(BTN_CIRCLE);         /* modalita' sposta */
    shot(dir, DASH_DRIVE, "12_layout_edit");
    /* Conferma cancellazione DTC */
    press(BTN_CIRCLE); press(BTN_TRIANGLE);
    ui.current_dash = DASH_DIAG;
    press(BTN_CIRCLE);
    shot(dir, DASH_DIAG, "13_diag_conferma_cancella");
    EXPECT(obd.req_clear_dtc == 0, "primo O chiede conferma");
    press(BTN_CIRCLE);
    EXPECT(obd.req_clear_dtc == 1, "secondo O invia la cancellazione");
    return fails;
}

static int e2e(int port, const char *dir) {
    common_setup();
    obdtask_init(&obd, "127.0.0.1", port, 1, &profile_registry[0]);
    unsigned int t0 = net_time_ms();
    while (obd.link != LINK_READY && net_time_ms() - t0 < 15000) obdtask_step(&obd);
    printf("  stato: %s - %s\n", obdtask_link_name(obd.link), obd.status);
    EXPECT(obd.link == LINK_READY, "connessione ELM + auto");
    EXPECT(strcmp(obd.elm.version, "ELM327 v1.5") == 0, "versione ELM letta");
    EXPECT(obd.elm.is_can == 1, "protocollo CAN rilevato");
    EXPECT(obd.caps.pid_rpm && obd.caps.pid_speed && obd.caps.pid_maf, "PID base supportati");
    EXPECT(obd.caps.pid_fuel_level, "PID 012F (livello) rilevato dal blocco 0120");
    EXPECT(obd.caps.cons == CONS_MAF, "strategia consumo MAF per MX-5");

    /* Prima iterazione READY: lettura DTC automatica + polling */
    obdtask_step(&obd);
    EXPECT(obd.dtc_valid && obd.dtc_count == 3, "DTC letti (2 confermati + 1 in attesa)");
    EXPECT(obd.dtc_count >= 3 && !strcmp(obd.dtc[0], "P0133") && !strcmp(obd.dtc[1], "P2004")
           && obd.dtc_pending[2] && !strcmp(obd.dtc[2], "P0301"), "codici DTC corretti");

    /* Cronometro: arma da fermo e guida */
    obd.req_accel_arm = 1;
    obd.fast_mode = 1;
    t0 = net_time_ms();
    while (net_time_ms() - t0 < 30000 && !obd.accel.hit[3]) { obdtask_step(&obd); ui_sync(&ui); }
    printf("  0-100: %.2fs (simulato ~10s)\n", obd.accel.t_hit[3]);
    EXPECT(obd.accel.hit[3] && obd.accel.t_hit[3] > 8.5f && obd.accel.t_hit[3] < 11.5f,
           "0-100 misurato con partenza da fermo");
    obd.fast_mode = 0;
    t0 = net_time_ms();
    while (net_time_ms() - t0 < 3000) { obdtask_step(&obd); ui_sync(&ui); }
    EXPECT(obd.live.valid & LIVE_BIT(SRC_RPM), "giri ricevuti");
    EXPECT(obd.live.valid & LIVE_BIT(SRC_BATT_V), "tensione batteria (ATRV)");
    EXPECT(obd.live.valid & LIVE_BIT(SRC_COOLANT) && obd.live.coolant == 88, "temperatura acqua 88C");
    EXPECT(obd.live.valid & LIVE_BIT(SRC_L100_LIVE), "consumo istantaneo calcolato");
    EXPECT(obd.trip.sum_km > 0.05, "trip integrato con tempo reale");
    printf("  trip: %.3f km, %.3f L, %.1f letture/s\n", obd.trip.sum_km, obd.trip.sum_liters, obd.poll_hz);

    const char *names[3] = { "e2e_guida", "e2e_motore", "e2e_diagnostica" };
    int dash[3] = { DASH_DRIVE, DASH_ENGINE, DASH_DIAG };
    for (int i = 0; i < 3; i++) shot(dir, dash[i], names[i]);
    shot(dir, DASH_ACCEL, "e2e_accel");

    /* Cancellazione DTC */
    obd.req_clear_dtc = 1;
    obdtask_step(&obd);
    EXPECT(obd.dtc_valid && obd.dtc_count == 0, "DTC cancellati e riletti");
    shot(dir, DASH_DIAG, "e2e_diagnostica_dopo_clear");
    obdtask_stop(&obd);
    return fails;
}

int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "demo")) {
        int f = demo(argv[2]);
        printf("demo: %s\n", f ? "ERRORI" : "ok");
        return f != 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "e2e")) {
        int f = e2e(atoi(argv[2]), argv[3]);
        printf("e2e: %d errori\n", f);
        return f != 0;
    }
    fprintf(stderr, "uso: %s demo OUTDIR | e2e PORT OUTDIR\n", argv[0]);
    return 2;
}
