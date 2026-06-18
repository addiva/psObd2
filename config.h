/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: config.h  |  Modulo: costanti globali, percorsi, macro condivise
 */
#ifndef CONFIG_H
#define CONFIG_H

#define APP_ID     "it.massirito.pspobd2"
#define APP_TITLE  "PSP OBD2 - massirito"
#define APP_DIR    "ms0:/PSP/GAME/it.massirito.pspobd2/"

#define CONFIG_FILE  APP_DIR "config.json"
#define LAYOUT_FILE  APP_DIR "layout.json"
#define SNAP_DIR     APP_DIR "snapshots/"

/* ELM327 defaults */
#define ELM_DEFAULT_IP    "192.168.0.10"
#define ELM_DEFAULT_PORT  35000
#define ELM_DEFAULT_APCTL 1

/* Timing */
#define OBD_POLL_MS     100
#define CHART_HIST_LEN  120

/* Limiti strutture */
#define DASH_COUNT   7
#define WIDGET_MAX   64

/* PSP display */
#define SCR_W  480
#define SCR_H  272

#endif /* CONFIG_H */
