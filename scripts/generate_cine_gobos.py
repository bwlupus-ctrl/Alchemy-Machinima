#!/usr/bin/env python3
"""Generate the bundled Cinematic Lighting 2.0 achromatic cookie library.

The seven 1.x masks remain the sharp source of truth.  This script adds medium
and heavy pre-blur variants for them and authors sixteen deterministic 2.0
masks at sharp/medium/heavy softness.  It intentionally emits grayscale PNGs:
fixture Kelvin and gels own all projector colour.
"""

from __future__ import annotations

import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont


SIZE = 512
ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "indra/newview/skins/default/textures/cine_gobos"
LEGACY = (
    "blinds", "panes", "bars", "slats", "grid", "dapple", "branches",
)


def canvas(value: int = 255) -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("L", (SIZE, SIZE), value)
    return image, ImageDraw.Draw(image)


def save_family(stem: str, image: Image.Image) -> None:
    image = image.convert("L")
    image.save(OUTPUT / f"gobo_{stem}_sharp.png", optimize=True)
    image.filter(ImageFilter.GaussianBlur(5.0)).save(
        OUTPUT / f"gobo_{stem}_medium.png", optimize=True)
    image.filter(ImageFilter.GaussianBlur(14.0)).save(
        OUTPUT / f"gobo_{stem}_heavy.png", optimize=True)


def arched_window() -> Image.Image:
    image, draw = canvas(0)
    draw.rectangle((110, 210, 402, 474), fill=255)
    draw.pieslice((110, 54, 402, 346), 180, 360, fill=255)
    draw.rectangle((246, 70, 266, 474), fill=0)
    draw.rectangle((110, 260, 402, 280), fill=0)
    return image


def french_door() -> Image.Image:
    image, draw = canvas(0)
    draw.rectangle((82, 46, 430, 478), fill=255)
    for x in (168, 256, 344):
        draw.rectangle((x - 8, 46, x + 8, 478), fill=0)
    for y in (154, 262, 370):
        draw.rectangle((82, y - 8, 430, y + 8), fill=0)
    draw.rectangle((70, 34, 442, 490), outline=0, width=24)
    return image


def curtain_edge() -> Image.Image:
    image, draw = canvas(255)
    points = [(0, 0), (215, 0)]
    for y in range(0, SIZE + 24, 24):
        x = 220 + int(32 * math.sin(y * 0.043) + 14 * math.sin(y * 0.11))
        points.append((x, y))
    points.extend([(0, SIZE), (0, 0)])
    draw.polygon(points, fill=0)
    return image


def stairwell_rail() -> Image.Image:
    image, draw = canvas(255)
    draw.line((-40, 420, 552, 72), fill=0, width=30)
    for x in range(-80, 620, 82):
        draw.line((x, 520, x + 230, -10), fill=0, width=20)
    return image


def door_crack() -> Image.Image:
    image, draw = canvas(0)
    draw.polygon(((228, 0), (278, 0), (342, SIZE), (184, SIZE)), fill=255)
    return image


def dense_foliage() -> Image.Image:
    image, draw = canvas(28)
    rng = random.Random(2001)
    for _ in range(420):
        x, y = rng.randrange(-20, SIZE + 20), rng.randrange(-20, SIZE + 20)
        rx, ry = rng.randrange(7, 24), rng.randrange(4, 15)
        value = rng.randrange(145, 256)
        draw.ellipse((x - rx, y - ry, x + rx, y + ry), fill=value)
    return image.filter(ImageFilter.GaussianBlur(0.8))


def palm_dapple() -> Image.Image:
    image, draw = canvas(25)
    origin = (266, 478)
    for branch in range(13):
        angle = -2.85 + branch * 0.225
        length = 430
        tip = (origin[0] + length * math.cos(angle),
               origin[1] + length * math.sin(angle))
        draw.line((origin, tip), fill=230, width=9)
        for leaf in range(5, 17):
            t = leaf / 18.0
            cx = origin[0] + (tip[0] - origin[0]) * t
            cy = origin[1] + (tip[1] - origin[1]) * t
            side = -1 if leaf % 2 else 1
            perpendicular = angle + side * math.pi / 2
            span = 34 * (1.0 - 0.45 * t)
            end = (cx + span * math.cos(perpendicular),
                   cy + span * math.sin(perpendicular))
            draw.line((cx, cy, end[0], end[1]), fill=220, width=8)
    return image


def water_caustics() -> Image.Image:
    image, draw = canvas(30)
    for row in range(-1, 10):
        for col in range(-1, 10):
            cx = col * 62 + (row % 2) * 29
            cy = row * 58
            points = []
            for step in range(25):
                angle = step * math.tau / 24.0
                radius = 27 + 8 * math.sin(angle * 3 + row * 0.7 + col)
                points.append((cx + radius * math.cos(angle),
                               cy + radius * math.sin(angle)))
            draw.line(points + [points[0]], fill=245, width=7, joint="curve")
    return image.filter(ImageFilter.GaussianBlur(1.0))


def cucoloris() -> Image.Image:
    image, draw = canvas(255)
    rng = random.Random(2002)
    for _ in range(78):
        x, y = rng.randrange(-40, SIZE + 40), rng.randrange(-40, SIZE + 40)
        rx, ry = rng.randrange(18, 72), rng.randrange(10, 48)
        draw.ellipse((x - rx, y - ry, x + rx, y + ry), fill=rng.randrange(0, 65))
    return image.filter(ImageFilter.GaussianBlur(2.0))


def fine_celo() -> Image.Image:
    image, draw = canvas(255)
    rng = random.Random(2003)
    for _ in range(55):
        points = []
        y = rng.randrange(-30, SIZE + 30)
        phase = rng.random() * math.tau
        for x in range(-20, SIZE + 21, 12):
            points.append((x, y + 15 * math.sin(x * 0.035 + phase)))
        draw.line(points, fill=rng.randrange(0, 95), width=rng.randrange(3, 8))
    return image


def scrim_wave() -> Image.Image:
    image, draw = canvas(245)
    for y in range(-20, SIZE + 20, 28):
        points = [(x, y + 15 * math.sin(x * 0.027 + y * 0.014))
                  for x in range(-10, SIZE + 11, 8)]
        draw.line(points, fill=55, width=8)
    return image


def smoke_drift() -> Image.Image:
    rng = random.Random(2004)
    small = Image.new("L", (32, 32))
    small.putdata([rng.randrange(20, 246) for _ in range(32 * 32)])
    image = small.resize((SIZE, SIZE), Image.Resampling.BICUBIC)
    image = image.filter(ImageFilter.GaussianBlur(20.0))
    lo, hi = image.getextrema()
    return image.point(lambda value: int((value - lo) * 255 / max(hi - lo, 1)))


def chain_link() -> Image.Image:
    image, draw = canvas(255)
    for offset in range(-SIZE, SIZE * 2, 72):
        draw.line((offset, 0, offset - SIZE, SIZE), fill=0, width=11)
        draw.line((offset, 0, offset + SIZE, SIZE), fill=0, width=11)
    return image


def industrial_grate() -> Image.Image:
    image, draw = canvas(255)
    for x in range(0, SIZE + 1, 64):
        draw.rectangle((x - 9, 0, x + 9, SIZE), fill=0)
    for y in range(0, SIZE + 1, 64):
        draw.rectangle((0, y - 9, SIZE, y + 9), fill=0)
    for offset in range(-SIZE, SIZE * 2, 128):
        draw.line((offset, 0, offset - SIZE, SIZE), fill=70, width=6)
    return image


def rotating_fan() -> Image.Image:
    image, draw = canvas(255)
    centre = (SIZE // 2, SIZE // 2)
    draw.ellipse((36, 36, SIZE - 36, SIZE - 36), outline=0, width=18)
    for blade in range(5):
        angle = blade * math.tau / 5.0
        points = [centre]
        for radius, delta in ((62, -0.28), (205, -0.48), (218, 0.08), (72, 0.32)):
            points.append((centre[0] + radius * math.cos(angle + delta),
                           centre[1] + radius * math.sin(angle + delta)))
        draw.polygon(points, fill=0)
    draw.ellipse((218, 218, 294, 294), fill=0)
    return image


def neon_sign() -> Image.Image:
    image, draw = canvas(0)
    draw.rounded_rectangle((58, 142, 454, 370), radius=28, outline=235, width=13)
    try:
        font = ImageFont.truetype("DejaVuSans-Bold.ttf", 94)
    except OSError:
        font = ImageFont.load_default()
    text = "NEON"
    box = draw.textbbox((0, 0), text, font=font, stroke_width=2)
    x = (SIZE - (box[2] - box[0])) // 2
    y = (SIZE - (box[3] - box[1])) // 2 - box[1]
    draw.text((x, y), text, fill=245, font=font, stroke_width=2, stroke_fill=245)
    return image.filter(ImageFilter.GaussianBlur(0.7))


GENERATORS = {
    "arch_window": arched_window,
    "french_door": french_door,
    "curtain_edge": curtain_edge,
    "stairwell_rail": stairwell_rail,
    "door_crack": door_crack,
    "dense_foliage": dense_foliage,
    "palm_dapple": palm_dapple,
    "water_caustics": water_caustics,
    "cucoloris": cucoloris,
    "fine_celo": fine_celo,
    "scrim_wave": scrim_wave,
    "smoke_drift": smoke_drift,
    "chain_link": chain_link,
    "industrial_grate": industrial_grate,
    "rotating_fan": rotating_fan,
    "neon_sign": neon_sign,
}


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for stem in LEGACY:
        source = Image.open(OUTPUT / f"gobo_{stem}.png").convert("L")
        source.filter(ImageFilter.GaussianBlur(5.0)).save(
            OUTPUT / f"gobo_{stem}_medium.png", optimize=True)
        source.filter(ImageFilter.GaussianBlur(14.0)).save(
            OUTPUT / f"gobo_{stem}_heavy.png", optimize=True)
    for stem, generator in GENERATORS.items():
        save_family(stem, generator())


if __name__ == "__main__":
    main()
