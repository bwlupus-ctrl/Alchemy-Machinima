#!/usr/bin/env python3
"""[BDMerge A0.2] Depth-distribution sanity test.

Compares two depth24 snapshots (PNG, as saved by the snapshot floater with
type "depth24") and fails if the scene depth distribution shifted beyond
tolerance. See doc/BD_MERGE_DEPTH_BASELINE.md section 6 for the capture
procedure and the encoding this decodes:

    meters = near + ((R*65536 + G*256 + B) / 16777215) * (far - near)

Stdlib only (own minimal PNG reader; 8-bit RGB/RGBA, all standard filters).

Usage:
    python bdmerge_depth_compare.py baseline.png after.png \
        --near 0.1 --far 512 [--pct-tol 0.01] [--bucket-tol 0.02] \
        [--allow p1,p5]

Exit codes: 0 = distributions match within tolerance, 1 = drift detected,
2 = usage/decode error.
"""

import argparse
import struct
import sys
import zlib

U24MAX = 16777215
PERCENTILES = (1, 5, 25, 50, 75, 95, 99)
NUM_BUCKETS = 256


def read_png_rgb(path):
    """Minimal PNG reader. Returns (width, height, bytes of RGB triplets)."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG file")
    pos = 8
    width = height = None
    bit_depth = color_type = None
    idat = bytearray()
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        ctype = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            width, height, bit_depth, color_type, _comp, _filt, interlace = \
                struct.unpack(">IIBBBBB", chunk)
            if bit_depth != 8 or color_type not in (2, 6):
                raise ValueError(
                    f"{path}: unsupported PNG (bit depth {bit_depth}, "
                    f"color type {color_type}); need 8-bit RGB/RGBA")
            if interlace:
                raise ValueError(f"{path}: interlaced PNG not supported")
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"IEND":
            break
    if width is None:
        raise ValueError(f"{path}: missing IHDR")
    raw = zlib.decompress(bytes(idat))
    bpp = 3 if color_type == 2 else 4
    stride = width * bpp
    out = bytearray(height * width * 3)
    prev = bytearray(stride)
    pos = 0
    for y in range(height):
        filt = raw[pos]
        line = bytearray(raw[pos + 1:pos + 1 + stride])
        pos += 1 + stride
        if filt == 1:  # Sub
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif filt == 2:  # Up
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif filt == 3:  # Average
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif filt == 4:  # Paeth
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xFF
        elif filt != 0:
            raise ValueError(f"{path}: unknown PNG filter {filt}")
        row_off = y * width * 3
        for x in range(width):
            out[row_off + x * 3:row_off + x * 3 + 3] = \
                line[x * bpp:x * bpp + 3]
        prev = line
    return width, height, bytes(out)


def depth_histogram(rgb, near, far):
    """24-bit depth values -> (bucket histogram, sorted-percentile samples).

    Buckets are over the normalized [0,1] range; percentiles are computed
    exactly from the full value population (as U24 codes).
    """
    n = len(rgb) // 3
    counts = [0] * NUM_BUCKETS
    codes = [0] * n
    for i in range(n):
        code = (rgb[3 * i] << 16) | (rgb[3 * i + 1] << 8) | rgb[3 * i + 2]
        codes[i] = code
        counts[min(code * NUM_BUCKETS // (U24MAX + 1), NUM_BUCKETS - 1)] += 1
    codes.sort()
    pcts = {}
    for p in PERCENTILES:
        idx = min(n - 1, max(0, round(p / 100.0 * (n - 1))))
        pcts[p] = near + codes[idx] / U24MAX * (far - near)
    return counts, pcts, n


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("baseline")
    ap.add_argument("after")
    ap.add_argument("--near", type=float, default=0.1,
                    help="camera near plane at capture time (m)")
    ap.add_argument("--far", type=float, default=512.0,
                    help="camera far plane at capture time (m)")
    ap.add_argument("--pct-tol", type=float, default=0.01,
                    help="max allowed percentile shift, as a fraction of "
                         "the [near,far] range (default 0.01)")
    ap.add_argument("--bucket-tol", type=float, default=0.02,
                    help="max allowed fraction of pixels changing depth "
                         "bucket (default 0.02)")
    ap.add_argument("--allow", default="",
                    help="comma-separated percentiles expected to move for "
                         "this item, e.g. 'p1,p5' (excluded from failure)")
    args = ap.parse_args()

    allowed = {int(t.strip().lstrip("pP"))
               for t in args.allow.split(",") if t.strip()}
    span = args.far - args.near

    try:
        wa, ha, rgb_a = read_png_rgb(args.baseline)
        wb, hb, rgb_b = read_png_rgb(args.after)
    except (OSError, ValueError) as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 2
    if (wa, ha) != (wb, hb):
        print(f"ERROR: size mismatch {wa}x{ha} vs {wb}x{hb} — captures must "
              f"use identical window/snapshot resolution", file=sys.stderr)
        return 2

    hist_a, pct_a, n = depth_histogram(rgb_a, args.near, args.far)
    hist_b, pct_b, _ = depth_histogram(rgb_b, args.near, args.far)

    failures = []
    print(f"{'pct':>5} {'baseline (m)':>14} {'after (m)':>14} {'shift':>10}")
    for p in PERCENTILES:
        shift = abs(pct_b[p] - pct_a[p]) / span
        mark = ""
        if shift > args.pct_tol:
            if p in allowed:
                mark = "  (shift allowed for this item)"
            else:
                mark = "  << DRIFT"
                failures.append(f"p{p} shifted {shift:.2%} of range "
                                f"({pct_a[p]:.2f}m -> {pct_b[p]:.2f}m)")
        print(f"  p{p:<3} {pct_a[p]:>14.3f} {pct_b[p]:>14.3f} "
              f"{shift:>9.3%}{mark}")

    moved = sum(abs(a - b) for a, b in zip(hist_a, hist_b)) / 2 / n
    print(f"\npixels changing depth bucket: {moved:.2%} "
          f"(tolerance {args.bucket_tol:.2%})")
    if moved > args.bucket_tol:
        failures.append(f"{moved:.2%} of pixels changed depth bucket")

    if failures:
        print("\nFAIL — depth distribution drifted:", file=sys.stderr)
        for f in failures:
            print(f"  - {f}", file=sys.stderr)
        return 1
    print("\nPASS — depth distribution stable within tolerance")
    return 0


if __name__ == "__main__":
    sys.exit(main())
