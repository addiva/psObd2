/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: font.c  |  Modulo: rasterizzatore testo bitmap (portabile)
 */
#include "font.h"
#include <string.h>

int font_text_width(const char *s, int scale) {
    if (!s) return 0;
    int n = (int)strlen(s);
    if (n == 0) return 0;
    return (n * FONT_ADV - 1) * scale;
}

void font_draw(uint32_t *fb, int stride, int fb_w, int fb_h,
               int x, int y, uint32_t color, int scale, const char *s) {
    if (!fb || !s || scale < 1) return;
    if (y >= fb_h || y + FONT_H_PX * scale <= 0) return;
    for (; *s; s++, x += FONT_ADV * scale) {
        if (x >= fb_w) break;
        if (x + 5 * scale <= 0) continue;
        unsigned char c = (unsigned char)*s;
        if (c < 32 || c > 126) c = '?';
        if (c == ' ') continue;
        const unsigned char *g = font8x8[c - 32];
        for (int row = 0; row < 8; row++) {
            unsigned char bits = g[row];
            if (!bits) continue;
            for (int sy = 0; sy < scale; sy++) {
                int py = y + row * scale + sy;
                if (py < 0 || py >= fb_h) continue;
                uint32_t *line = fb + py * stride;
                for (int col = 0; col < 5; col++) {
                    if (!(bits & (0x80 >> col))) continue;
                    int px = x + col * scale;
                    for (int sx = 0; sx < scale; sx++, px++)
                        if (px >= 0 && px < fb_w) line[px] = color;
                }
            }
        }
    }
}
