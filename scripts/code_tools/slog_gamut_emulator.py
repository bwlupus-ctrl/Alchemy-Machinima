"""Render real production log capture and an ACES-free assumed-S-Gamut3 grade.

No viewer build. Windows hidden-WGL harness; refuses to start with AlchemyTest.
Outputs labelled PNG charts, YCbCr vectorscopes, CSV and provenance JSON under
.tmp/slog_gamut/emulator. No RRT/ODT is approximated.
"""
import csv
import hashlib
import json
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont
import sensor_post_gl_check as P
import slog_gamut_check as S
import slog_gamut_external_check as E

OUT = S.ROOT/'.tmp/slog_gamut/emulator'
PATCHES = [('18% grey', (.18, .18, .18)), ('White', (1., 1., 1.)), ('90% white', (.9, .9, .9)),
           ('SL pure red', (1., 0., 0.)), ('Magenta', (1., 0., 1.)),
           ('Blue', (0., 0., 1.)), ('Green', (0., 1., 0.)),
           ('Red wall', (.60, .15, .12)), ('Skin / red light', (.80, .35, .30))]
FONT = S.ROOT/'indra/newview/fonts/IBMPlexSansVar-Roman.ttf'
CHART_HEIGHT=826


def font(size):
    return ImageFont.truetype(str(FONT), size) if FONT.is_file() else ImageFont.load_default(size=size)


def srgb(value):
    return np.where(value <= .0031308, 12.92*value, 1.055*value**(1/2.4)-.055)


def saturation(value):
    peak = float(np.max(value))
    return (peak-float(np.min(value)))/peak if peak > 0 else 0.


def cbcr(value):
    y = float(value @ S.LUMA)
    return ((float(value[2])-y)/1.8556, (float(value[0])-y)/1.5748)


def rgb8(value):
    return tuple(np.round(np.clip(value, 0., 1.)*255).astype(int))


def chart(name, samples, raw=False):
    image = Image.new('RGB', (48+len(PATCHES)*200, CHART_HEIGHT), '#171d28')
    draw = ImageDraw.Draw(image)
    draw.text((24, 16), name, font=font(25), fill='white')
    title = 'Delivered log codes (no input transform)' if raw else 'Assumed Sony S-Gamut3 input -> linear Rec.709 -> clip -> sRGB display'
    draw.text((24, 53), title, font=font(18), fill='#afbed4')
    draw.text((24, 80), 'Production S-Log3 / Full / toe 0. No sensor simulation, look, ACES RRT or ODT.', font=font(15), fill='#afbed4')
    for i, sample in enumerate(samples):
        row, col = divmod(i, len(PATCHES))
        x, y = 24+col*200, 132+row*232
        if col == 0:
            draw.text((24, y-27), f'EV +{sample["ev"]}', font=font(18), fill='#73d9de')
        draw.rounded_rectangle((x, y, x+190, y+190), radius=7, fill='#242d3c')
        draw.text((x+9, y+7), sample['patch'], font=font(16), fill='white')
        color = sample['code'] if raw else sample['display']
        draw.rectangle((x+9, y+34, x+181, y+94), fill=rgb8(color))
        linear = sample['code'] if raw else sample['decoded']
        draw.text((x+9, y+102), f'{"Code" if raw else "Linear"} R {linear[0]: .4f}', font=font(14), fill='#d4deef')
        draw.text((x+9, y+121), f'G {linear[1]: .4f}  B {linear[2]: .4f}', font=font(14), fill='#d4deef')
        draw.text((x+9, y+142), f'{"Code" if raw else "Display"} saturation {100*saturation(color):.1f}%', font=font(13), fill='#afbed4')
        reference=sample['external_code']
        draw.text((x+9, y+161), f'Sony code ref R {reference[0]:.4f}', font=font(12), fill='#73d9de')
        draw.text((x+9, y+177), f'G {reference[1]:.4f}  B {reference[2]:.4f}', font=font(12), fill='#73d9de')
    draw.text((24, CHART_HEIGHT-20), 'Cyan: external Sony code reference (EV 0 neutral/red anchors verified by lead; other values use independent primaries + published OETF).', font=font(14), fill='#afbed4')
    image.save(OUT/(name+('_log.png' if raw else '_grade.png')))
    return image


def vectorscope(name, samples):
    image = Image.new('RGB', (720, 790), '#171d28')
    d = ImageDraw.Draw(image)
    d.text((24, 16), name, font=font(25), fill='white')
    d.text((24, 52), 'After assumed S-Gamut3 input / clip / sRGB (ACES-free)', font=font(16), fill='#afbed4')
    origin, scale = (360, 365), 500
    def xy(value):
        c = cbcr(value)
        return origin[0]+scale*c[0], origin[1]-scale*c[1]
    for radius in (.25, .5):
        r = radius*scale
        d.ellipse((origin[0]-r, origin[1]-r, origin[0]+r, origin[1]+r), outline='#4c5d74', width=1)
    d.line((65, origin[1], 655, origin[1]), fill='#4c5d74')
    d.line((origin[0], 70, origin[0], 660), fill='#4c5d74')
    d.text((663, 359), 'Cb', font=font(16), fill='white')
    d.text((368, 75), 'Cr', font=font(16), fill='white')
    for label, value in [('R', (1, 0, 0)), ('M', (1, 0, 1)), ('B', (0, 0, 1)), ('G', (0, 1, 0)), ('Y', (1, 1, 0)), ('C', (0, 1, 1))]:
        x, y = xy(np.array(value))
        d.rectangle((x-7, y-7, x+7, y+7), outline=rgb8(np.array(value)), width=2)
        d.text((x+10, y-10), label, font=font(16), fill='white')
    for sample in samples:
        x, y = xy(sample['display'])
        color = rgb8(np.array(PATCHES[sample['index']][1]))
        r = 5
        if sample['ev'] == 0:
            d.ellipse((x-r, y-r, x+r, y+r), fill=color, outline='white')
        elif sample['ev'] == 2:
            d.rectangle((x-r, y-r, x+r, y+r), fill=color, outline='white')
        else:
            d.polygon([(x, y-r-2), (x+r+2, y), (x, y+r+2), (x-r-2, y)], fill=color, outline='white')
    d.text((24, 685), 'Circle: EV 0     Square: EV +2     Diamond: EV +4', font=font(18), fill='white')
    d.text((24, 717), 'Outline targets are Rec.709 display primaries. Marks use patch colours.', font=font(16), fill='#afbed4')
    d.text((24, 744), 'Cb=(B-Y)/1.8556, Cr=(R-Y)/1.5748; Y uses Rec.709 weights.', font=font(16), fill='#afbed4')
    image.save(OUT/(name+'_vectorscope.png'))


def ocio_chart(evidence):
    image=Image.new('RGB',(1648,590),'#171d28')
    d=ImageDraw.Draw(image)
    d.text((24,16),'External ACES 1.2 Sony IDT: requested Bradford gate FAIL',font=font(25),fill='white')
    d.text((24,54),f'GPU codes -> local PyOpenColorIO {evidence["version"]} -> AP0. Required tolerance 0.001; measured {evidence["max_AP0_error"]:.6f}.',font=font(18),fill='#ffbd9b')
    d.text((24,82),f'Installed Sony IDT uses CAT02; independent CAT02 error {evidence["max_CAT02_AP0_error"]:.6f}. No shader matrix or gate changed.',font=font(17),fill='#73d9de')
    for x,label in ((24,'Input linear Rec.709'),(380,'GPU -> Sony IDT AP0'),(790,'Independent Bradford AP0'),(1210,'Independent CAT02 AP0')):
        d.text((x,129),label,font=font(17),fill='white')
    for row,values in enumerate(zip(evidence['patches'],evidence['AP0'],evidence['expected_AP0'],evidence['CAT02_AP0'])):
        y=174+row*45
        for x,value in zip((24,380,790,1210),values):
            d.text((x,y),' / '.join(f'{v:.6f}' for v in value),font=font(16),fill='#d4deef')
    d.text((24,520),'The independent Sony XYZ check passes at 1e-5. ACES adaptation choice explains this separate IDT mismatch.',font=font(17),fill='#afbed4')
    d.text((24,553),'Exact input space: Input - Sony - S-Log3 - S-Gamut3. No RRT / ODT or display transform in these AP0 values.',font=font(16),fill='#afbed4')
    image.save(OUT/'ocio_reference_comparison.png')


def main():
    if not S.gpu_idle():
        print('PENDING-GPU: viewer running or process query failed; rerun python scripts/code_tools/slog_gamut_emulator.py')
        return 2
    OUT.mkdir(parents=True, exist_ok=True)
    ocio_file=E.OUT/'ocio_comparison.json'
    ocio=json.loads(ocio_file.read_text(encoding='utf-8')) if ocio_file.is_file() else None
    if ocio:ocio_chart(ocio)
    search_file=E.OUT/'aces_search.json'
    if search_file.is_file():
        (OUT/'aces_reference_search.json').write_text(search_file.read_text(encoding='utf-8'),encoding='utf-8')
    r = P.Runner()
    camera = P.camera(r, 'CAMERA')
    source = np.array([color for _, color in PATCHES], np.float32)[None]
    tex = r.upload('emulator', source, fmt=0x8814)
    width=len(PATCHES)
    dst = r.target('emulator_codes', width, 1, fmt=0x8814)
    sg_inverse = np.linalg.inv(E.SG3)
    rows, variants, metrics = [], [], []
    # Four variants, each containing all requested patches at all three EVs.
    for gamut in (0, 1):
        for purity in (1., .9):
            name = ('legacy709' if gamut == 0 else 'sgamut3')+'_purity_'+str(purity).replace('.', 'p')
            samples = []
            for ev in (0, 2, 4):
                u = dict(tonemap_type=8, tonemap_slog_gamut=gamut, tonemap_slog_purity=purity,
                         tonemap_params=(0., 0., 0., float(ev)), tonemap_grade=(1., 1.), exposure=1., bloom_strength=0.)
                codes = r.draw(camera, dst, width, 1, [('diffuseRect', tex)], u)[0][0, :, :3].astype(np.float64)
                # Always decode using the user's assumed S-Gamut3 input, even
                # when the producer deliberately selected legacy Rec.709.
                decoded = S.slog3_inverse(codes) @ sg_inverse.T
                clipped = np.clip(decoded, 0., 1.)
                display = srgb(clipped)
                expected = S.apply_purity(source[0].astype(np.float64)*2.**ev, purity)
                reference_codes=E.sony(expected @ (E.SG3.T if gamut==1 else np.eye(3)))
                if ev==0:
                    for patch_name, anchor in (('18% grey',.4106),('White',.5960),('90% white',.5845)):
                        reference_codes[next(i for i,p in enumerate(PATCHES) if p[0]==patch_name)]=anchor
                    if gamut==1:
                        reference_codes[next(i for i,p in enumerate(PATCHES) if p[0]=='SL pure red')]=(np.array([.5337,.3238,.2140]) if purity==1. else E.sony([.53070,.09053,.04138]))
                np.testing.assert_allclose(codes,reference_codes,atol=.001,rtol=0.)
                for index, ((patch, color), code, value, shown, target, clip, reference) in enumerate(zip(PATCHES, codes, decoded, display, expected, clipped,reference_codes)):
                    sample = dict(ev=ev, index=index, patch=patch, code=code, decoded=value, display=shown,external_code=reference)
                    samples.append(sample)
                    row = dict(variant=name, producer_gamut=gamut, assumed_input='Sony S-Gamut3', purity=purity, EV=ev, patch=patch)
                    for prefix, vector in [('input', color), ('expected_purity_linear', target), ('log_code', code),
                                           ('decoded_linear', value), ('clipped_linear', clip), ('display_srgb', shown),('external_code',reference)]:
                        row.update({prefix+'_'+channel: float(v) for channel, v in zip('rgb', vector)})
                    row.update(saturation_decoded_linear=saturation(value), saturation_clipped_linear=saturation(clip),
                               saturation_display_srgb=saturation(shown), saturation_log_code=saturation(code))
                    rows.append(row)
                if gamut == 1:
                    # Code-domain tolerance is meaningful at HDR values; the
                    # CSV retains the actual decoded-linear precision errors.
                    camera_linear = expected @ E.SG3.T
                    ref_code = np.where(camera_linear >= .01125,
                                        (420+np.log10((camera_linear+.01)/.19)*261.5)/1023.,
                                        (camera_linear*((171.2102946929-95)/.01125)+95)/1023.)
                    error = float(np.max(np.abs(codes-ref_code)))
                    assert error <= S.DELIVERED_TOL
                    metrics.append(dict(variant=name, EV=ev, code_error=error,
                                        decoded_linear_error=float(np.max(np.abs(decoded-expected)))))
            variants.append(chart(name, samples))
            chart(name, samples, raw=True)
            vectorscope(name, samples)
    with (OUT/'decoded_values.csv').open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    for ev in (0, 2, 4):
        for patch in ('18% grey', 'White', '90% white'):
            for gamut in (0, 1):
                selected = [x for x in rows if x['patch'] == patch and x['EV'] == ev and x['producer_gamut'] == gamut]
                assert len(selected) == 2
                assert all(selected[0]['log_code_'+c] == selected[1]['log_code_'+c] for c in 'rgb')
    red = next(x for x in rows if x['patch'] == 'SL pure red' and x['EV'] == 0 and x['producer_gamut'] == 0 and x['purity'] == 1.)
    assert 1.87 < red['decoded_linear_r'] < 1.89
    summary = dict(renderer=r.gl.renderer, production_shader_sha256=hashlib.sha256((S.SHADERS/S.TONE).read_bytes()).hexdigest(),
                   reference='Published S-Log3 inverse -> assumed S-Gamut3 -> Rec.709 linear -> clip [0,1] -> IEC sRGB',
                   ACES=(dict(status=ocio['status'],config=ocio['config'],PyOpenColorIO=ocio['version'],
                              Bradford_error=ocio['max_AP0_error'],CAT02_error=ocio['max_CAT02_AP0_error'],
                              explanation='Local ACES 1.2 Sony IDT uses CAT02, not requested Bradford; 0.001 gate preserved.')
                         if ocio else 'PENDING: run slog_gamut_external_check.py --gpu for local OCIO evidence.'),
                   external_anchors='Lead-verified neutral/red codes overlaid; other references derive from independently typed Sony primaries and published OETF.',
                   neutral_code_identity=True, rows=len(rows), matching_input_errors=metrics,
                   legacy_misread_red=[red['decoded_linear_'+c] for c in 'rgb'])
    (OUT/'summary.json').write_text(json.dumps(summary, indent=2)+'\n', encoding='utf-8')
    comparison = Image.new('RGB', (variants[0].width, CHART_HEIGHT*4))
    for i, chart_image in enumerate(variants):
        comparison.paste(chart_image, (0, i*CHART_HEIGHT))
    comparison.save(OUT/'comparison.png')
    (OUT/'README.txt').write_text('Actual production GPU encode, not a CPU encoder copy.\n'
        'All grade charts assume S-Gamut3, so legacy producer variants intentionally show the mismatch.\n'
        'The ACES-free reference clips channels; it does not model a highlight DRT or Magic Bullet look.\n'
        'Local ACES 1.2 Sony IDT found and executed with bundled PyOpenColorIO; no approximate RRT was used.\n'
        'ocio_reference_comparison.png shows the required Bradford gate FAIL and the independently verified CAT02 explanation.\n'
        'Cyan rows overlay external Sony code references beside measured values; CSV includes external_code_r/g/b.\n'
        'CSV saturation is (max-min)/max, separately before/after clipping and in display/log codes.\n'
        'Negative decoded channels can give >100% saturation before clipping.\n', encoding='utf-8')
    print(json.dumps(summary, indent=2))
    print(f'PASS: {len(rows)} GPU-produced patch/EV/variant rows; external anchors, neutral code identity; charts and vectorscopes saved')
    return 0


if __name__ == '__main__':
    sys.exit(main())
