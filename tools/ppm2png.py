#!/usr/bin/env python3
"""Converte i .ppm (P6) di ui_preview in .png (solo libreria standard)."""
import struct
import sys
import zlib
from pathlib import Path


def ppm_to_png(src: Path, dst: Path, scale: int = 1) -> None:
    data = src.read_bytes()
    parts = data.split(maxsplit=4)
    w, h = int(parts[1]), int(parts[2])
    px = parts[4]
    rows = []
    for y in range(h):
        row = px[y * w * 3:(y + 1) * w * 3]
        if scale > 1:
            row = b"".join(row[i:i + 3] * scale for i in range(0, len(row), 3))
        rows.extend([b"\x00" + row] * scale)

    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w * scale, h * scale, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(b"".join(rows), 9))
    png += chunk(b"IEND", b"")
    dst.write_bytes(png)


if __name__ == "__main__":
    scale = 2
    for p in sys.argv[1:]:
        src = Path(p)
        ppm_to_png(src, src.with_suffix(".png"), scale)
        src.unlink()
