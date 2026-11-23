/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: obdtask.c  |  Modulo: thread OBD (connessione, polling, DTC)
 */
#include "obdtask.h"
#include "obd.h"
#include "net.h"
#include "cost.h"
#include <string.h>
#include <stdio.h>

#define WIFI_TIMEOUT_MS   30000
#define RETRY_DELAY_MS     3000
#define ECU_RETRY_MS       3000
#define VOLT_EVERY_CYCLES    20

const char *obdtask_link_name(link_state_t s) {
    switch (s) {
        case LINK_IDLE:     return "Avvio";
        case LINK_WIFI_OFF: return "WiFi spento";
        case LINK_WIFI:     return "WiFi...";
        case LINK_TCP:      return "ELM...";
        case LINK_ECU_WAIT: return "Attesa auto";
        case LINK_READY:    return "OK";
        case LINK_ERROR:    return "Errore";
    }
    return "?";
}

static void set_status(obd_task_t *t, const char *msg) {
    strncpy(t->status, msg, sizeof(t->status) - 1);
    t->status[sizeof(t->status) - 1] = '\0';
}

void obdtask_init(obd_task_t *t, const char *ip, int port, int apctl,
                  const car_profile_t *profile) {
    memset(t, 0, sizeof(*t));
    strncpy(t->ip, ip, sizeof(t->ip) - 1);
    t->port = port;
    t->apctl = apctl;
    t->profile = profile;
    t->link = LINK_IDLE;
    t->caps = caps_empty();
    elm_init(&t->elm, ip, port);
    set_status(t, "Avvio...");
}

/* Attende ms controllando richieste di uscita/riconnessione */
static void wait_ms(obd_task_t *t, unsigned int ms) {
    unsigned int t0 = net_time_ms();
    while (net_time_ms() - t0 < ms && !t->req_quit && !t->req_reconnect)
        net_sleep_ms(50);
}

static void go_error(obd_task_t *t, const char *msg) {
    elm_disconnect(&t->elm);
    t->live.valid = 0;
    set_status(t, msg);
    t->link = LINK_ERROR;
}

static void caps_from_mask(obd_task_t *t) {
    car_caps_t *c = &t->caps;
    *c = caps_empty();
    c->pid_mil        = obd_pid_is_supported(t->sup, 0x01);
    c->pid_coolant    = obd_pid_is_supported(t->sup, 0x05);
    c->pid_rpm        = obd_pid_is_supported(t->sup, 0x0C);
    c->pid_speed      = obd_pid_is_supported(t->sup, 0x0D);
    c->pid_maf        = obd_pid_is_supported(t->sup, 0x10);
    c->pid_throttle   = obd_pid_is_supported(t->sup, 0x11);
    c->pid_fuel_level = obd_pid_is_supported(t->sup, 0x2F);
    c->pid_fuel_rate  = obd_pid_is_supported(t->sup, 0x5E);
    profile_resolve_strategy(t->profile, c);
}

/* ---------------- DTC ---------------- */
static void do_read_dtc(obd_task_t *t) {
    char stored[DTC_MAX][6], pend[DTC_MAX][6];
    t->dtc_busy = 1;
    set_status(t, "Lettura DTC...");
    int ns = elm_read_dtc(&t->elm, 3, stored, DTC_MAX);
    int np = ns >= 0 ? elm_read_dtc(&t->elm, 7, pend, DTC_MAX) : -1;
    if (ns < 0) {
        snprintf(t->dtc_msg, sizeof(t->dtc_msg), "Lettura DTC fallita");
    } else {
        int n = 0;
        for (int i = 0; i < ns && n < DTC_MAX; i++) {
            memcpy(t->dtc[n], stored[i], 6); t->dtc_pending[n++] = 0;
        }
        for (int i = 0; i < np && n < DTC_MAX; i++) {
            int dup = 0;
            for (int j = 0; j < ns; j++) if (!strcmp(stored[j], pend[i])) dup = 1;
            if (dup) continue;
            memcpy(t->dtc[n], pend[i], 6); t->dtc_pending[n++] = 1;
        }
        t->dtc_count = n;
        t->dtc_valid = 1;
        if (n == 0) snprintf(t->dtc_msg, sizeof(t->dtc_msg), "Nessun DTC memorizzato");
        else snprintf(t->dtc_msg, sizeof(t->dtc_msg), "%d confermati, %d in attesa",
                      ns, n - ns);
        t->dtc_seq++;
    }
    t->dtc_busy = 0;
    set_status(t, "Connesso");
}

static void do_clear_dtc(obd_task_t *t) {
    t->dtc_busy = 1;
    set_status(t, "Cancellazione DTC...");
    int r = elm_clear_dtc(&t->elm);
    t->dtc_busy = 0;
    if (r == 0) {
        do_read_dtc(t);
        snprintf(t->dtc_msg, sizeof(t->dtc_msg),
                 "Cancellati. Ora: %d codici", t->dtc_count);
    } else {
        snprintf(t->dtc_msg, sizeof(t->dtc_msg),
                 "Cancellazione rifiutata (motore spento, quadro ON)");
        t->dtc_seq++;
    }
    set_status(t, "Connesso");
}

/* ---------------- Polling ---------------- */
static unsigned int s_last_cycle_ms, s_cycle, s_rate_t0, s_rate_n;
static int s_slow_idx;

static int q(obd_task_t *t, int pid, uint8_t *d) {
    return elm_query_pid(&t->elm, 0x01, pid, d, 8);
}

static void poll_cycle(obd_task_t *t) {
    live_t *L = &t->live;
    const car_caps_t *c = &t->caps;
    const car_profile_t *p = t->profile;
    uint8_t d[8];
    int n;
    unsigned int t_speed = 0;

    /* Velocita' sempre (serve a trip e cronometro) */
    if (c->pid_speed) {
        n = q(t, 0x0D, d);
        if (n < 0) return;
        if (n > 0) { L->speed = obd_speed(d, n); L->valid |= LIVE_BIT(SRC_SPEED); t_speed = net_time_ms(); }
    }
    if (t_speed && (t->accel.armed || t->accel.active)) {
        if (accel_feed(&t->accel, L->speed, t_speed / 1000.0f)) {
            accel_best_t before = t->best;
            accel_update_best(&t->best, &t->accel);
            if (memcmp(&before, &t->best, sizeof(before))) t->best_seq++;
        }
    }

    if (!t->fast_mode) {
        if (c->pid_rpm) {
            n = q(t, 0x0C, d);
            if (n < 0) return;
            if (n > 0) { L->rpm = obd_rpm(d, n); L->valid |= LIVE_BIT(SRC_RPM); }
        }
        /* Sorgente consumo */
        if (c->cons == CONS_MAF || c->pid_maf) {
            n = c->pid_maf ? q(t, 0x10, d) : 0;
            if (n < 0) return;
            if (n > 0) { L->maf = obd_maf_gps(d, n); L->valid |= LIVE_BIT(SRC_MAF); }
        }
        if (c->cons == CONS_FUEL_RATE) {
            n = q(t, 0x5E, d);
            if (n < 0) return;
            if (n > 0) L->lh_live = obd_fuel_rate_lh(d, n);
        } else if (c->cons == CONS_MAF && (L->valid & LIVE_BIT(SRC_MAF)) && p) {
            L->lh_live = L->maf * 3600.0f / (p->afr_stoich * p->fuel_density);
        }
        if (c->cons != CONS_NONE && L->lh_live >= 0) {
            L->valid |= LIVE_BIT(SRC_LH_LIVE);
            L->l100_live = cost_l100_from_lh(L->lh_live, (float)L->speed);
            L->valid |= LIVE_BIT(SRC_L100_LIVE);
        }

        /* Un PID "lento" per ciclo, a rotazione */
        for (int tries = 0; tries < 5; tries++) {
            int k = s_slow_idx++ % 5;
            if (k == 0 && c->pid_coolant) {
                n = q(t, 0x05, d); if (n < 0) return;
                if (n > 0) { L->coolant = obd_coolant_c(d, n); L->valid |= LIVE_BIT(SRC_COOLANT); }
                break;
            }
            if (k == 1 && c->pid_throttle) {
                n = q(t, 0x11, d); if (n < 0) return;
                if (n > 0) { L->throttle = obd_throttle_pct(d, n); L->valid |= LIVE_BIT(SRC_THROTTLE); }
                break;
            }
            if (k == 2 && c->pid_fuel_level) {
                n = q(t, 0x2F, d); if (n < 0) return;
                if (n > 0) { L->fuel_level = obd_fuel_level_pct(d, n); L->valid |= LIVE_BIT(SRC_FUEL_LEVEL); }
                break;
            }
            if (k == 3 && c->pid_mil) {
                n = q(t, 0x01, d); if (n < 0) return;
                if (n > 0) {
                    int mil = 0;
                    L->dtc_ecu = obd_mil_status(d, n, &mil);
                    L->mil = mil;
                    L->valid |= LIVE_BIT(SRC_MIL) | LIVE_BIT(SRC_DTC_COUNT);
                }
                break;
            }
            if (k == 4) break;
        }
        if (s_cycle % VOLT_EVERY_CYCLES == 0) {
            float v = elm_read_voltage(&t->elm);
            if (v > 0) { L->batt_v = v; L->valid |= LIVE_BIT(SRC_BATT_V); }
        }
    }

    /* Integrazione trip con il tempo REALE trascorso tra due cicli */
    unsigned int now = net_time_ms();
    if (s_last_cycle_ms) {
        float dt = (now - s_last_cycle_ms) / 1000.0f;
        if (dt > 0 && dt < 5.0f && (L->valid & LIVE_BIT(SRC_SPEED))) {
            float lh = (L->valid & LIVE_BIT(SRC_LH_LIVE)) ? L->lh_live : 0.0f;
            if (lh >= 0) trip_update(&t->trip, lh, (float)L->speed, dt);
        }
    }
    s_last_cycle_ms = now;
    L->trip_km = (float)t->trip.sum_km;
    L->valid |= LIVE_BIT(SRC_TRIP_KM);
    if (c->cons != CONS_NONE && t->trip.sum_km > 0.05) {
        L->l100_trip = trip_avg_l100(&t->trip);
        L->valid |= LIVE_BIT(SRC_L100_TRIP);
    }

    s_cycle++;
    s_rate_n++;
    if (now - s_rate_t0 >= 2000) {
        t->poll_hz = s_rate_n * 1000.0f / (float)(now - s_rate_t0);
        s_rate_t0 = now; s_rate_n = 0;
    }
    t->live_seq++;
}

/* ---------------- Macchina a stati ---------------- */
void obdtask_step(obd_task_t *t) {
    char msg[96];
    if (t->req_reconnect) {
        t->req_reconnect = 0;
        elm_disconnect(&t->elm);
        t->live.valid = 0;
        t->link = LINK_IDLE;
    }
    if (t->req_trip_reset) { t->req_trip_reset = 0; trip_reset(&t->trip); }
    if (t->req_accel_arm)  { t->req_accel_arm = 0;  accel_arm(&t->accel); }

    switch (t->link) {
    case LINK_IDLE:
    case LINK_WIFI:
    case LINK_TCP:
    case LINK_WIFI_OFF: {
        if (!net_wlan_switch_on()) {
            t->link = LINK_WIFI_OFF;
            set_status(t, "Accendi lo switch WiFi della PSP");
            wait_ms(t, 1000);
            return;
        }
        int r = net_init();
        if (r < 0) {
            snprintf(msg, sizeof(msg), "Init rete PSP fallito (0x%08X)", (unsigned)r);
            go_error(t, msg);
            return;
        }
        t->link = LINK_WIFI;
        snprintf(msg, sizeof(msg), "WiFi: connessione al profilo %d...", t->apctl);
        set_status(t, msg);
        r = net_apctl_connect(t->apctl, WIFI_TIMEOUT_MS);
        if (r < 0) {
            if (r == -1)
                snprintf(msg, sizeof(msg), "Profilo di rete %d non valido", t->apctl);
            else if (r == -2)
                snprintf(msg, sizeof(msg), "WiFi profilo %d: rete ELM non trovata", t->apctl);
            else
                snprintf(msg, sizeof(msg), "WiFi profilo %d: timeout", t->apctl);
            go_error(t, msg);
            return;
        }
        net_local_ip(t->local_ip, sizeof(t->local_ip));
        t->link = LINK_TCP;
        elm_init(&t->elm, t->ip, t->port);
        snprintf(msg, sizeof(msg), "Connessione ELM %s:%d...", t->ip, t->port);
        set_status(t, msg);
        if (elm_connect(&t->elm) < 0) { go_error(t, t->elm.last_error); return; }
        t->link = LINK_ECU_WAIT;
        set_status(t, "ELM OK, interrogo l'auto...");
        return;
    }
    case LINK_ECU_WAIT: {
        int r = elm_query_supported_pids(&t->elm, t->sup);
        if (r < 0) { go_error(t, "Connessione ELM persa"); return; }
        if (r == 0) {
            float v = elm_read_voltage(&t->elm);
            if (v > 0) { t->live.batt_v = v; t->live.valid |= LIVE_BIT(SRC_BATT_V); t->live_seq++; }
            set_status(t, t->elm.last_error[0] ? t->elm.last_error
                                               : "L'auto non risponde (quadro acceso?)");
            wait_ms(t, ECU_RETRY_MS);
            return;
        }
        elm_detect_protocol(&t->elm);
        caps_from_mask(t);
        s_last_cycle_ms = 0;
        t->link = LINK_READY;
        set_status(t, "Connesso");
        t->req_read_dtc = 1;      /* lettura DTC automatica all'aggancio */
        return;
    }
    case LINK_READY:
        if (t->req_clear_dtc) { t->req_clear_dtc = 0; t->req_read_dtc = 0; do_clear_dtc(t); }
        if (t->req_read_dtc)  { t->req_read_dtc = 0; do_read_dtc(t); }
        poll_cycle(t);
        if (t->elm.state != ELM_READY || t->elm.timeouts >= 3) {
            go_error(t, t->elm.timeouts >= 3 ? "ELM non risponde piu'" : "Connessione ELM persa");
            return;
        }
        return;
    case LINK_ERROR:
        wait_ms(t, RETRY_DELAY_MS);
        if (!t->req_quit) t->link = LINK_IDLE;
        return;
    }
}

#ifdef PSP_BUILD
#include <pspkernel.h>

static SceUID s_tid = -1;
static obd_task_t *s_task;

static int obd_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    obd_task_t *t = s_task;
    while (!t->req_quit) {
        obdtask_step(t);
        sceKernelDelayThread(t->link == LINK_READY ? 2000 : 20000);
    }
    elm_disconnect(&t->elm);
    t->running = 0;
    return 0;
}

int obdtask_start(obd_task_t *t) {
    s_task = t;
    t->running = 1;
    /* priorita' 0x18: piu' alta della UI (0x20) ma dorme durante le attese */
    s_tid = sceKernelCreateThread("obd_worker", obd_thread, 0x18, 0x10000, 0, NULL);
    if (s_tid < 0) { t->running = 0; return -1; }
    sceKernelStartThread(s_tid, 0, NULL);
    return 0;
}

void obdtask_stop(obd_task_t *t) {
    t->req_quit = 1;
    if (s_tid >= 0) {
        SceUInt timeout = 3000000;
        if (sceKernelWaitThreadEnd(s_tid, &timeout) < 0)
            sceKernelTerminateThread(s_tid);
        sceKernelDeleteThread(s_tid);
        s_tid = -1;
    }
    elm_disconnect(&t->elm);
}
#else
int  obdtask_start(obd_task_t *t) { (void)t; return -1; }
void obdtask_stop(obd_task_t *t)  { t->req_quit = 1; elm_disconnect(&t->elm); }
#endif
