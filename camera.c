/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: camera.c  |  Modulo: camera live PSP (sceUsbCam) + decode MJPEG
 *
 * NOTA: API sceUsbCam* - verificare le firme contro gli header pspdev locali.
 * Alcune versioni usano sceUtilityLoadUsbModule, altre pspSdkLoadStartModule.
 */
#include "camera.h"

#ifdef PSP_BUILD
#include <pspkernel.h>
#include <pspusb.h>
#include <pspusbcam.h>
#include <pspjpeg.h>
#include <psprtc.h>
#include <psptypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pspsdk.h>

/* Work buffer per il decoder MJPEG (dimensione consigliata SDK) */
#define MJPEG_WORK_SIZE (64 * 1024)

static uint8_t s_jpeg_buf[CAM_JPEG_BUF_SIZE];
static uint8_t s_rgba_a[CAM_RGBA_SIZE];
static uint8_t s_rgba_b[CAM_RGBA_SIZE];
static uint8_t s_mjpeg_work[MJPEG_WORK_SIZE];
static SceUID  s_thread_id = -1;
static cam_ctx_t *s_ctx_ptr = NULL;

static int capture_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    cam_ctx_t *ctx = s_ctx_ptr;

    while (ctx->running) {
        /* Leggi frame MJPEG dalla camera */
        int jpeg_len = sceUsbCamReadVideoFrameBlocking(s_jpeg_buf, sizeof(s_jpeg_buf));
        if (jpeg_len <= 0) {
            sceKernelDelayThread(33000); /* ~30fps wait */
            continue;
        }

        /* Decode MJPEG -> RGBA nel buffer back */
        int err = sceJpegDecodeMJpeg(
            s_jpeg_buf, (SceSize)jpeg_len,
            ctx->rgba_back, 0);

        if (err >= 0) {
            /* Swap front/back */
            uint8_t *tmp  = ctx->rgba_front;
            ctx->rgba_front = ctx->rgba_back;
            ctx->rgba_back  = tmp;
            ctx->frame_ready = 1;
            ctx->frame_id++;
        }
    }
    return 0;
}

int cam_init(cam_ctx_t *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->rgba_front = s_rgba_a;
    ctx->rgba_back  = s_rgba_b;

    /* Avvia moduli USB */
    if (sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, NULL) < 0) return -1;
    if (sceUsbStart(PSP_USBCAM_DRIVERNAME, 0, NULL) < 0) return -1;
    if (sceUsbActivate(PSP_USBCAM_PID) < 0)              return -1;

    /* Setup video */
    /* Struttura parametri: verificare con gli header locali */
    PspUsbCamSetupVideoParam param;
    memset(&param, 0, sizeof(param));
    param.size       = sizeof(param);
    param.resolution = PSP_USBCAM_RESOLUTION_320_240;
    param.framerate  = PSP_USBCAM_FRAMERATE_15_FPS;
    param.wb         = PSP_USBCAM_WB_AUTO;
    param.saturation = 128;
    param.brightness = 128;
    param.contrast   = 128;
    param.sharpness  = 128;
    param.effectmode = PSP_USBCAM_EFFECTMODE_NORMAL;
    param.framesize  = CAM_JPEG_BUF_SIZE;
    param.unk        = 0;
    param.evlevel    = PSP_USBCAM_EVLEVEL_0_0;

    if (sceUsbCamSetupVideo(&param, s_mjpeg_work, MJPEG_WORK_SIZE) < 0)
        return -1;

    /* Init decoder MJPEG */
    if (sceJpegInitMJpeg() < 0) return -1;
    if (sceJpegCreateMJpeg(CAM_WIDTH, CAM_HEIGHT) < 0) return -1;

    ctx->available = 1;
    return 0;
}

int cam_start(cam_ctx_t *ctx) {
    if (!ctx->available) return -1;
    if (sceUsbCamStartVideo() < 0) return -1;

    ctx->running = 1;
    s_ctx_ptr = ctx;

    s_thread_id = sceKernelCreateThread("cam_capture", capture_thread,
                                        0x11, 0x8000, 0, NULL);
    if (s_thread_id < 0) { ctx->running = 0; return -1; }
    sceKernelStartThread(s_thread_id, 0, NULL);
    return 0;
}

void cam_stop(cam_ctx_t *ctx) {
    ctx->running = 0;
    if (s_thread_id >= 0) {
        sceKernelWaitThreadEnd(s_thread_id, NULL);
        sceKernelDeleteThread(s_thread_id);
        s_thread_id = -1;
    }
    sceUsbCamStopVideo();
    sceJpegDeleteMJpeg();
    sceJpegFinishMJpeg();
    sceUsbDeactivate(PSP_USBCAM_PID);
    sceUsbStop(PSP_USBCAM_DRIVERNAME, 0, NULL);
    sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, NULL);
    ctx->available = 0;
}

int cam_snapshot(cam_ctx_t *ctx, const char *snap_dir,
                 int speed_kmh, float l100) {
    if (!ctx->available || !ctx->frame_ready) return -1;

    /* Leggi un frame fresco direttamente */
    int jpeg_len = sceUsbCamReadVideoFrameBlocking(s_jpeg_buf, sizeof(s_jpeg_buf));
    if (jpeg_len <= 0) return -1;

    /* Nome file: snap_YYYYMMDD_HHMMSS_Xkmh_Yl100.jpg */
    u64 tick;
    sceRtcGetCurrentTick(&tick);
    ScePspDateTime t;
    sceRtcSetTick(&t, &tick);
    char fname[256];
    snprintf(fname, sizeof(fname),
             "%ssnap_%04d%02d%02d_%02d%02d%02d_%dkmh_%.1fl100.jpg",
             snap_dir ? snap_dir : "",
             t.year, t.month, t.day,
             t.hour, t.minute, t.second,
             speed_kmh, l100);

    FILE *f = fopen(fname, "wb");
    if (!f) return -1;
    fwrite(s_jpeg_buf, 1, (size_t)jpeg_len, f);
    fclose(f);
    return 0;
}

#else /* HOST BUILD: stub */
#include <string.h>

int  cam_init(cam_ctx_t *ctx) { memset(ctx, 0, sizeof(*ctx)); return -1; }
int  cam_start(cam_ctx_t *ctx) { (void)ctx; return -1; }
void cam_stop(cam_ctx_t *ctx)  { (void)ctx; }
int  cam_snapshot(cam_ctx_t *ctx, const char *d, int s, float l)
    { (void)ctx;(void)d;(void)s;(void)l; return -1; }
#endif /* PSP_BUILD */
