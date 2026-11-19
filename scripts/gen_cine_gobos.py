#!/usr/bin/env python3
"""Generate the committed Cinematic Light Rig gobo textures.

Regenerate from the repository root with:
    python scripts/gen_cine_gobos.py

The PNG outputs are committed artifacts. The build does not run this script.
Pillow and NumPy are the only dependencies; fixed seeds make every output
byte-stable for a given Pillow/NumPy toolchain.
"""

from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


SIZE = 512
VIGNETTE_INNER = 0.72
VIGNETTE_OUTER = 1.0
BLINDS_BANDS = 9
BLINDS_DUTY = 0.55
BLINDS_BLUR = 3.0
PANE_COLUMNS = 2
PANE_ROWS = 3
PANE_MULLION_FRACTION = 0.07
PANE_BLUR = 2.0
BAR_COUNT = 7
BAR_DUTY = 0.12
BAR_BLUR = 1.0
SLAT_PERIODS = 6
SLAT_DUTY = 0.50
SLAT_BLUR = 3.0
GRID_CELLS = 6
GRID_LINE_FRACTION = 0.08
DAPPLE_BLUR = 12.0
BRANCH_BLUR = 3.0
DAPPLE_SEED = 0xC1FE
BRANCH_SEED = 0xB07A

OUTPUT_DIR = (
    Path(__file__).resolve().parents[1]
    / "indra/newview/skins/default/textures/cine_gobos"
)


def soften(pattern: np.ndarray, radius: float) -> np.ndarray:
    image = Image.fromarray(
        np.rint(np.clip(pattern, 0.0, 1.0) * 255.0).astype(np.uint8),
        mode="L",
    )
    image = image.filter(ImageFilter.GaussianBlur(radius=radius))
    return np.asarray(image, dtype=np.float32) / 255.0


def blinds() -> np.ndarray:
    y = np.arange(SIZE, dtype=np.float32)[:, None]
    period = SIZE / BLINDS_BANDS
    pattern = ((y % period) < period * BLINDS_DUTY).astype(np.float32)
    return soften(np.broadcast_to(pattern, (SIZE, SIZE)), BLINDS_BLUR)


def panes() -> np.ndarray:
    y, x = np.indices((SIZE, SIZE), dtype=np.float32)
    cell_w = SIZE / PANE_COLUMNS
    cell_h = SIZE / PANE_ROWS
    half_v = cell_w * PANE_MULLION_FRACTION * 0.5
    half_h = cell_h * PANE_MULLION_FRACTION * 0.5
    pattern = np.ones((SIZE, SIZE), dtype=np.float32)
    pattern[(x < half_v) | (x >= SIZE - half_v)] = 0.0
    pattern[(y < half_h) | (y >= SIZE - half_h)] = 0.0
    for column in range(1, PANE_COLUMNS):
        pattern[np.abs(x - column * cell_w) < half_v] = 0.0
    for row in range(1, PANE_ROWS):
        pattern[np.abs(y - row * cell_h) < half_h] = 0.0
    return soften(pattern, PANE_BLUR)


def bars() -> np.ndarray:
    _, x = np.indices((SIZE, SIZE), dtype=np.float32)
    period = SIZE / BAR_COUNT
    half_width = period * BAR_DUTY * 0.5
    pattern = np.ones((SIZE, SIZE), dtype=np.float32)
    for bar in range(BAR_COUNT):
        centre = (bar + 0.5) * period
        pattern[np.abs(x - centre) < half_width] = 0.0
    return soften(pattern, BAR_BLUR)


def slats() -> np.ndarray:
    y, x = np.indices((SIZE, SIZE), dtype=np.float32)
    coordinate = (x + y) / np.sqrt(2.0)
    period = (SIZE * np.sqrt(2.0)) / SLAT_PERIODS
    pattern = ((coordinate % period) < period * SLAT_DUTY).astype(np.float32)
    return soften(pattern, SLAT_BLUR)


def grid() -> np.ndarray:
    y, x = np.indices((SIZE, SIZE), dtype=np.float32)
    cell = SIZE / GRID_CELLS
    half_line = cell * GRID_LINE_FRACTION * 0.5
    dx = np.minimum(x % cell, cell - (x % cell))
    dy = np.minimum(y % cell, cell - (y % cell))
    pattern = ((dx >= half_line) & (dy >= half_line)).astype(np.float32)
    return soften(pattern, PANE_BLUR)


def resized_noise(seed: int, coarse_size: int) -> np.ndarray:
    rng = np.random.default_rng(seed)
    coarse = np.rint(rng.random((coarse_size, coarse_size)) * 255.0).astype(
        np.uint8
    )
    image = Image.fromarray(coarse, mode="L").resize(
        (SIZE, SIZE), resample=Image.Resampling.BICUBIC
    )
    return np.asarray(image, dtype=np.float32) / 255.0


def dapple(seed: int = DAPPLE_SEED) -> np.ndarray:
    noise = resized_noise(seed, 14)
    pools = (noise > 0.50).astype(np.float32)
    return soften(pools, DAPPLE_BLUR)


def branches(seed: int = BRANCH_SEED) -> np.ndarray:
    noise = resized_noise(seed, 52)
    ridge = np.exp(-np.square(np.abs(noise - 0.5) / 0.085))
    pattern = 1.0 - 0.82 * ridge
    return soften(pattern, BRANCH_BLUR)


def vignette(pattern: np.ndarray) -> np.ndarray:
    y, x = np.indices((SIZE, SIZE), dtype=np.float32)
    centre = (SIZE - 1) * 0.5
    radius = np.sqrt(np.square(x - centre) + np.square(y - centre)) / (SIZE * 0.5)
    t = np.clip(
        (radius - VIGNETTE_INNER) / (VIGNETTE_OUTER - VIGNETTE_INNER),
        0.0,
        1.0,
    )
    smooth = t * t * (3.0 - 2.0 * t)
    return np.clip(pattern * (1.0 - smooth), 0.0, 1.0)


def normalize_luminance(pattern: np.ndarray) -> np.ndarray:
    """Keep gobo swaps exposure-neutral without lifting the black border."""
    mean = float(pattern.mean())
    if 0.35 <= mean <= 0.65:
        return pattern
    target = 0.40 if mean < 0.35 else 0.60
    low, high = (1.0, 16.0) if mean < target else (0.0, 1.0)
    for _ in range(24):
        scale = (low + high) * 0.5
        candidate_mean = float(np.clip(pattern * scale, 0.0, 1.0).mean())
        if candidate_mean < target:
            low = scale
        else:
            high = scale
    return np.clip(pattern * ((low + high) * 0.5), 0.0, 1.0)


def save_gobo(filename: str, pattern: np.ndarray) -> None:
    pixels = np.rint(
        normalize_luminance(vignette(pattern)) * 255.0
    ).astype(np.uint8)
    Image.fromarray(pixels, mode="L").save(
        OUTPUT_DIR / filename,
        format="PNG",
        optimize=False,
        compress_level=9,
    )
    print(f"{filename}: mean={pixels.mean() / 255.0:.4f}")


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    patterns = (
        ("gobo_blinds.png", blinds()),
        ("gobo_panes.png", panes()),
        ("gobo_bars.png", bars()),
        ("gobo_slats.png", slats()),
        ("gobo_grid.png", grid()),
        ("gobo_dapple.png", dapple()),
        ("gobo_branches.png", branches()),
    )
    for filename, pattern in patterns:
        save_gobo(filename, pattern)


if __name__ == "__main__":
    main()
