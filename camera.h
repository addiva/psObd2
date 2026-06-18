/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: camera.h  |  Modulo: camera live PSP (sceUsbCam) + decode MJPEG
 *
 * Richiede accessorio camera USB ufficiale (Go!Cam / PSP-300).
 * Funziona su PSP-2000/3000. Rilevamento a runtime: se assente,
 * la dash Camera resta navigabile ma mostra "Camera non disponibile".
 */
#ifndef CAMERA_H
#define CAMERA_H

#include <stdint.h>

/* Risoluzione capture (trade-off: qualita' vs CPU a 333 MHz) */
#define CAM_WIDTH   320
#define CAM_HEIGHT  240

/* Dimensione buffer JPEG in ingresso (PSP camera output MJPEG) */
#define CAM_JPEG_BUF_SIZE (CAM_WIDTH * CAM_HEIGHT * 3)

/* Dimensione buffer RGBA output decode */
#define CAM_RGBA_SIZE (CAM_WIDTH * CAM_HEIGHT * 4)

typedef struct {
    int      available;      /* 1 = camera rilevata e inizializzata */
    int      running;        /* 1 = thread capture attivo */

    /* Frame buffer condiviso (doppio buffer: capture scrive in back, UI legge front) */
    uint8_t *rgba_front;     /* letto dal thread UI per blit */
    uint8_t *rgba_back;      /* scritto dal thread capture */
    int      frame_ready;    /* flag: 1 = front pronto per blit */
    int      frame_id;       /* incrementato ad ogni nuovo frame */
} cam_ctx_t;

/* Inizializza il sottosistema camera. Ritorna 0 OK, -1 se assente/errore. */
int  cam_init(cam_ctx_t *ctx);

/* Avvia il thread di capture/decode.
 * Ritorna 0 OK, -1 errore. */
int  cam_start(cam_ctx_t *ctx);

/* Ferma il thread di capture e rilascia le risorse. */
void cam_stop(cam_ctx_t *ctx);

/* Salva un fermo immagine JPEG in snap_dir con nome timestamp+telemetria.
 * speed_kmh e l100 vanno nel nome file come telemetria.
 * Ritorna 0 OK, -1 errore. */
int  cam_snapshot(cam_ctx_t *ctx, const char *snap_dir,
                  int speed_kmh, float l100);

#endif /* CAMERA_H */
