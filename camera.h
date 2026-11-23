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
    volatile int probing;    /* 1 = rilevamento in corso */
    int      available;      /* 1 = camera rilevata e inizializzata */
    int      running;        /* 1 = thread capture attivo */

    /* Frame buffer condiviso (doppio buffer: capture scrive in back, UI legge front) */
    uint8_t *rgba_front;     /* letto dal thread UI per blit */
    uint8_t *rgba_back;      /* scritto dal thread capture */
    int      frame_ready;    /* flag: 1 = front pronto per blit */
    int      frame_id;       /* incrementato ad ogni nuovo frame */

    /* Richiesta foto (UI -> thread camera) */
    volatile int snap_req;
    int      snap_speed;
    float    snap_l100;
    char     snap_dir[160];
    volatile int snap_result; /* 1 OK, -1 errore, 0 nessuna */
    char     snap_name[64];
} cam_ctx_t;

/* Avvia in background rilevamento + cattura (non blocca l'avvio dell'app).
 * Se la camera non c'e', il thread termina e available resta 0. */
int  cam_start(cam_ctx_t *ctx);

/* Ferma il thread di capture e rilascia le risorse. */
void cam_stop(cam_ctx_t *ctx);

/* Richiede il salvataggio del prossimo frame JPEG in snap_dir
 * (nome: snap_AAAAMMGG_HHMMSS_<v>kmh_<c>l100.jpg). Asincrono: esito in
 * ctx->snap_result. Ritorna 0 se la richiesta e' accettata. */
int  cam_snapshot(cam_ctx_t *ctx, const char *snap_dir,
                  int speed_kmh, float l100);

#endif /* CAMERA_H */
