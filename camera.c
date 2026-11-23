/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: camera.c  |  Modulo: camera live PSP (Go!Cam, sceUsbCam) + decode MJPEG
 */
#include "camera.h"
#include <string.h>
#include <stdio.h>

int cam_snapshot(cam_ctx_t *ctx, const char *snap_dir, int speed_kmh, float l100) {
    if (!ctx->available || ctx->snap_req) return -1;
    strncpy(ctx->snap_dir, snap_dir ? snap_dir : "", sizeof(ctx->snap_dir) - 1);
    ctx->snap_speed  = speed_kmh;
    ctx->snap_l100   = l100;
    ctx->snap_result = 0;
    ctx->snap_req    = 1;
    return 0;
}

#ifdef PSP_BUILD
#include <pspkernel.h>
#include <psputility.h>
#include <pspusb.h>
#include <pspusbacc.h>
#include <pspusbcam.h>
#include <pspjpeg.h>
#include <psprtc.h>

#define MJPEG_WORK_SIZE (68 * 1024)

static uint8_t s_jpeg_buf[CAM_JPEG_BUF_SIZE]  __attribute__((aligned(64)));
static uint8_t s_rgba_a[CAM_RGBA_SIZE]        __attribute__((aligned(64)));
static uint8_t s_rgba_b[CAM_RGBA_SIZE]        __attribute__((aligned(64)));
static uint8_t s_mjpeg_work[MJPEG_WORK_SIZE]  __attribute__((aligned(64)));
static SceUID  s_thread_id = -1;
static cam_ctx_t *s_ctx;
static int s_mod_usb = 0, s_mod_av = 0, s_usb_started = 0, s_jpeg = 0;

static void cam_release(void) {
    if (s_jpeg) { sceJpegDeleteMJpeg(); sceJpegFinishMJpeg(); s_jpeg = 0; }
    if (s_usb_started) {
        sceUsbDeactivate(PSP_USBCAM_PID);
        sceUsbStop(PSP_USBCAMMIC_DRIVERNAME, 0, NULL);
        sceUsbStop(PSP_USBCAM_DRIVERNAME, 0, NULL);
        sceUsbStop(PSP_USBACC_DRIVERNAME, 0, NULL);
        sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, NULL);
        s_usb_started = 0;
    }
    if (s_mod_av)  { sceUtilityUnloadAvModule(PSP_AV_MODULE_AVCODEC); s_mod_av = 0; }
    if (s_mod_usb) {
        sceUtilityUnloadUsbModule(PSP_USB_MODULE_CAM);
        sceUtilityUnloadUsbModule(PSP_USB_MODULE_ACC);
        s_mod_usb = 0;
    }
}

static int cam_probe(void) {
    if (sceUtilityLoadUsbModule(PSP_USB_MODULE_ACC) < 0) return -1;
    if (sceUtilityLoadUsbModule(PSP_USB_MODULE_CAM) < 0) {
        sceUtilityUnloadUsbModule(PSP_USB_MODULE_ACC);
        return -1;
    }
    s_mod_usb = 1;
    if (sceUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC) < 0) return -1;
    s_mod_av = 1;

    if (sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, NULL) < 0) return -1;
    sceUsbStart(PSP_USBACC_DRIVERNAME, 0, NULL);
    sceUsbStart(PSP_USBCAM_DRIVERNAME, 0, NULL);
    sceUsbStart(PSP_USBCAMMIC_DRIVERNAME, 0, NULL);
    s_usb_started = 1;
    if (sceUsbActivate(PSP_USBCAM_PID) < 0) return -1;

    /* Camera presente? attendi il collegamento USB (max ~2 s) */
    int ok = 0;
    for (int i = 0; i < 40 && s_ctx->running; i++) {
        if (sceUsbGetState() & PSP_USB_CONNECTION_ESTABLISHED) { ok = 1; break; }
        sceKernelDelayThread(50000);
    }
    if (!ok) return -1;

    PspUsbCamSetupVideoParam param;
    memset(&param, 0, sizeof(param));
    param.size       = sizeof(param);
    param.resolution = PSP_USBCAM_RESOLUTION_320_240;
    param.framerate  = PSP_USBCAM_FRAMERATE_15_FPS;
    param.wb         = PSP_USBCAM_WB_AUTO;
    param.saturation = 125;
    param.brightness = 128;
    param.contrast   = 64;
    param.sharpness  = 0;
    param.effectmode = PSP_USBCAM_EFFECTMODE_NORMAL;
    param.framesize  = CAM_JPEG_BUF_SIZE;
    param.evlevel    = PSP_USBCAM_EVLEVEL_0_0;
    if (sceUsbCamSetupVideo(&param, s_mjpeg_work, MJPEG_WORK_SIZE) < 0) return -1;
    sceUsbCamAutoImageReverseSW(1);

    if (sceJpegInitMJpeg() < 0) return -1;
    if (sceJpegCreateMJpeg(CAM_WIDTH, CAM_HEIGHT) < 0) { sceJpegFinishMJpeg(); return -1; }
    s_jpeg = 1;
    if (sceUsbCamStartVideo() < 0) return -1;
    return 0;
}

static void save_snapshot(cam_ctx_t *ctx, int jpeg_len) {
    ScePspDateTime t;
    memset(&t, 0, sizeof(t));
    sceRtcGetCurrentClockLocalTime(&t);
    snprintf(ctx->snap_name, sizeof(ctx->snap_name),
             "snap_%04d%02d%02d_%02d%02d%02d_%dkmh_%.1fl100.jpg",
             t.year, t.month, t.day, t.hour, t.minute, t.second,
             ctx->snap_speed, ctx->snap_l100 < 0 ? 0.0f : ctx->snap_l100);
    char path[256];
    snprintf(path, sizeof(path), "%s%s", ctx->snap_dir, ctx->snap_name);
    FILE *f = fopen(path, "wb");
    int ok = 0;
    if (f) {
        ok = fwrite(s_jpeg_buf, 1, (size_t)jpeg_len, f) == (size_t)jpeg_len;
        fclose(f);
    }
    ctx->snap_result = ok ? 1 : -1;
}

static int capture_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    cam_ctx_t *ctx = s_ctx;
    if (cam_probe() < 0) {
        cam_release();
        ctx->probing = 0;
        ctx->available = 0;
        return 0;
    }
    ctx->available = 1;
    ctx->probing = 0;
    while (ctx->running) {
        int jpeg_len = sceUsbCamReadVideoFrameBlocking(s_jpeg_buf, sizeof(s_jpeg_buf));
        if (jpeg_len <= 0) { sceKernelDelayThread(30000); continue; }
        if (ctx->snap_req) { save_snapshot(ctx, jpeg_len); ctx->snap_req = 0; }
        if (sceJpegDecodeMJpeg(s_jpeg_buf, (SceSize)jpeg_len, ctx->rgba_back, 0) >= 0) {
            uint8_t *tmp = ctx->rgba_front;
            ctx->rgba_front = ctx->rgba_back;
            ctx->rgba_back = tmp;
            ctx->frame_ready = 1;
            ctx->frame_id++;
        }
    }
    sceUsbCamStopVideo();
    cam_release();
    ctx->available = 0;
    return 0;
}

int cam_start(cam_ctx_t *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->rgba_front = s_rgba_a;
    ctx->rgba_back  = s_rgba_b;
    ctx->running = 1;
    ctx->probing = 1;
    s_ctx = ctx;
    s_thread_id = sceKernelCreateThread("cam_capture", capture_thread,
                                        0x12, 0x10000, 0, NULL);
    if (s_thread_id < 0) { ctx->running = 0; ctx->probing = 0; return -1; }
    sceKernelStartThread(s_thread_id, 0, NULL);
    return 0;
}

void cam_stop(cam_ctx_t *ctx) {
    ctx->running = 0;
    if (s_thread_id >= 0) {
        SceUInt timeout = 2000000;
        if (sceKernelWaitThreadEnd(s_thread_id, &timeout) < 0)
            sceKernelTerminateThread(s_thread_id);
        sceKernelDeleteThread(s_thread_id);
        s_thread_id = -1;
    }
}

#else /* PC: nessuna camera */
int  cam_start(cam_ctx_t *ctx) { memset(ctx, 0, sizeof(*ctx)); return -1; }
void cam_stop(cam_ctx_t *ctx)  { (void)ctx; }
#endif /* PSP_BUILD */
