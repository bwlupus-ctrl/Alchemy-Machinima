#!/usr/bin/env python3
"""Generate deterministic white RGBA masks used by the Director kit."""
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "indra/newview/skins/default/textures/kit"

def chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)

def alpha_at(x: float, y: float, w: int, h: int, radius: float) -> int:
    cx = min(max(x, radius), w - radius)
    cy = min(max(y, radius), h - radius)
    return 255 if (x-cx)**2 + (y-cy)**2 <= radius**2 else 0

def write_mask(name: str, w: int, h: int, radius: float) -> None:
    rows = bytearray()
    for y in range(h):
        rows.append(0)
        for x in range(w):
            coverage = sum(alpha_at(x+(sx+.5)/4, y+(sy+.5)/4, w, h, radius) for sy in range(4) for sx in range(4)) // 16
            rows.extend((255, 255, 255, coverage))
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b"")
    path = OUT / name
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)

def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    write_mask("Kit_RoundRect4.png", 16, 16, 4)
    write_mask("Kit_RoundRect6.png", 16, 16, 6)
    write_mask("Kit_RoundRect8.png", 20, 20, 8)
    write_mask("Kit_Pill9.png", 18, 18, 9)
    write_mask("Kit_Circle.png", 16, 16, 8)
    write_mask("Kit_SliderTrack.png", 12, 4, 2)
    write_mask("Kit_SliderThumb.png", 14, 14, 7)

if __name__ == "__main__":
    main()
