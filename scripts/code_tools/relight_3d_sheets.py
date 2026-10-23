"""Contact sheets: ground truth | production | difference, one row per quality config."""
import numpy as np
from PIL import Image, ImageDraw, ImageFont

LIGHT_COLORS = [(1.0, 0.86, 0.62), (0.70, 0.80, 1.0), (0.8, 1.0, 0.8), (1.0, 0.8, 0.9)]


def _font(size):
    try:
        return ImageFont.load_default(size=size)
    except TypeError:  # pragma: no cover
        return ImageFont.load_default()


def shade(gt, vis, albedo_table):
    """Display-referred shading of the grid: albedo * (ambient + sum_l L0_l * vis_l * colour_l)."""
    surf = gt['surf']
    alb = np.zeros(surf.shape + (3,), np.float32)
    sky = surf == 255
    for i, a in enumerate(albedo_table):
        alb[surf == i] = a
    lum = np.full(surf.shape + (3,), 0.10, np.float32)
    for li, L in enumerate(gt['lights']):
        lum += (L['L0'] * vis[li] * L['cand'])[..., None] * np.array(LIGHT_COLORS[li % 4], np.float32) * 1.8
    col = alb * lum
    col = col / (1.0 + col)
    col = np.power(np.clip(col, 0, 1), 1 / 2.2)
    col[sky] = (0.05, 0.06, 0.10)
    return col


def diff_image(gt, prod, base):
    """Context (dim ground-truth shading) with leaks red (orange when the blocker was off-screen/hidden) and
    false-dark pixels blue. Intensity = lighting-weighted visibility error."""
    out = base * 0.35
    for li, L in enumerate(gt['lights']):
        d = (prod[li] - L['vis']) * L['L0'] * L['cand']
        mag = np.clip(np.abs(d) * 5.0, 0, 1)[..., None]
        expected = ((L['cls'] != 0) & (L['cls'] != 255))[..., None]
        leak = np.where(expected, np.array([1.0, 0.62, 0.1]), np.array([1.0, 0.1, 0.1]))
        dark = np.array([0.15, 0.45, 1.0])
        tint = np.where((d > 0)[..., None], leak, dark)
        out = out * (1 - mag) + tint * mag
    return np.clip(out, 0, 1)


def to_image(arr):
    return Image.fromarray((np.clip(arr[::-1], 0, 1) * 255).astype(np.uint8))  # rows are bottom-up


def contact_sheet(path, title, subtitle, gt, rows, albedo_table):
    """rows: list of (config name, prod (L,Hg,Wg) visibility, one-line metrics text)."""
    Hg, Wg = gt['surf'].shape
    gap, head, strip = 6, 54, 20
    sheet = Image.new('RGB', (3 * Wg + 4 * gap, head + len(rows) * (Hg + strip + gap) + gap), (18, 18, 20))
    dr = ImageDraw.Draw(sheet)
    f1, f2 = _font(15), _font(12)
    dr.text((gap, 6), title, fill=(240, 240, 240), font=f1)
    dr.text((gap, 28), subtitle, fill=(190, 190, 200), font=f2)
    gt_vis = [L['vis'] * L['cand'] + (~L['cand']) for L in gt['lights']]
    gt_img = shade(gt, gt_vis, albedo_table)
    pic_gt = to_image(gt_img)
    y = head
    for name, prod, text in rows:
        dr.text((gap, y + 3), '%s   %s' % (name, text), fill=(235, 235, 190), font=f2)
        y0 = y + strip
        prod_img = shade(gt, prod, albedo_table)
        sheet.paste(pic_gt, (gap, y0))
        sheet.paste(to_image(prod_img), (2 * gap + Wg, y0))
        sheet.paste(to_image(diff_image(gt, prod, gt_img)), (3 * gap + 2 * Wg, y0))
        y += Hg + strip + gap
    dr.text((gap, head - 14), 'ground truth (ray traced)', fill=(150, 200, 150), font=f2)
    dr.text((2 * gap + Wg, head - 14), 'production screen-space', fill=(150, 200, 150), font=f2)
    dr.text((3 * gap + 2 * Wg, head - 14), 'red leak / orange leak (blocker off-screen, expected) / blue false-dark', fill=(150, 200, 150), font=f2)
    sheet.save(path, optimize=False, compress_level=3)


def thumb(img):
    """Float (Hg,Wg,3) display image -> 2x reduced uint8 (rows still bottom-up)."""
    h, w = img.shape[:2]
    a = img[:h // 2 * 2, :w // 2 * 2].reshape(h // 2, 2, w // 2, 2, 3).mean((1, 3))
    return (np.clip(a, 0, 1) * 255).astype(np.uint8)


def overview_sheet(path, title, tiles, cols=4):
    """tiles: list of (label, gt_thumb, prod_thumb) uint8 arrays from thumb(); shown as GT|production pairs."""
    if not tiles:
        return
    th, tw = tiles[0][1].shape[:2]
    pad, lab = 4, 16
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * (2 * tw + 2 * pad), 28 + rows * (th + lab + pad)), (18, 18, 20))
    dr = ImageDraw.Draw(sheet)
    dr.text((pad, 6), title, fill=(240, 240, 240), font=_font(14))
    f = _font(11)
    for k, (label, gi, pi) in enumerate(tiles):
        x = (k % cols) * (2 * tw + 2 * pad) + pad
        y = 28 + (k // cols) * (th + lab + pad)
        dr.text((x, y), label, fill=(220, 220, 170), font=f)
        sheet.paste(Image.fromarray(gi[::-1]), (x, y + lab))
        sheet.paste(Image.fromarray(pi[::-1]), (x + tw, y + lab))
    sheet.save(path, compress_level=3)
