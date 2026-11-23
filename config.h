/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: config.h  |  Modulo: costanti globali, percorsi, macro condivise
 */
#ifndef CONFIG_H
#define CONFIG_H

#define APP_ID     "it.massirito.pspobd2"
#define APP_TITLE  "PSP OBD2 - massirito"
#define APP_DIR    "ms0:/PSP/GAME/it.massirito.pspobd2/"

/* Cartella predefinita: a runtime si usa quella dell'EBOOT (vedi appconfig_dir) */
#define CONFIG_NAME  "config.json"
#define LAYOUT_NAME  "layout.json"
#define SNAP_SUBDIR  "snapshots/"

/* ELM327 defaults */
#define ELM_DEFAULT_IP    "192.168.0.10"
#define ELM_DEFAULT_PORT  35000
#define ELM_DEFAULT_APCTL 1

/* Grafici: campioni in memoria (uno per ciclo OBD) */
#define CHART_HIST_LEN  150

/* Limiti strutture */
#define DASH_COUNT   8
#define WIDGET_MAX   64

/* PSP display */
#define SCR_W  480
#define SCR_H  272

#endif /* CONFIG_H */
