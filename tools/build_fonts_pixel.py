"""Replace the 12 px and 16 px glyph slices of main/generated/home_font.c with the
TRMNL12-Bold and TRMNL16-Bold pixel fonts (OFL-1.1, Heavyweight Digital Type
Foundry for TRMNL, v1.002), rendered at their native size so every stroke is a
whole pixel. Every other slice (Atkinson 10/22/30/44/48/64 px, CJK) is copied
byte for byte from the existing file.
"""
import re
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / 'firmware' / 'main' / 'generated'
FONTS = {12: ROOT / 'fonts' / 'TRMNL12-Bold.ttf', 16: ROOT / 'fonts' / 'TRMNL16-Bold.ttf'}


def parse(src):
    bits = bytes(int(v, 16) for v in re.findall(r'0x([0-9a-f]{2})', src.split('home_font_bits[]')[1].split('};')[0]))
    rows = [tuple(map(int, r)) for r in re.findall(r'\{(\d+),(\d+),(\d+),(\d+),(-?\d+),(-?\d+),(\d+)\}', src.split('home_glyphs[]')[1])]
    cjk = [tuple(map(int, m)) for m in re.findall(r'\{(\d+),(\d+),(\d+)\},', src.split('home_cjk_fonts[')[1].split('};')[0])]
    fonts = [tuple(map(int, m)) for m in re.findall(r'\{(\d+),(\d+),(\d+)\},', src.split('home_fonts[8]')[1])]
    assert len(fonts) == 8 and len(rows) == sum(f[2] for f in fonts) + sum(f[2] for f in cjk)
    return bits, rows, cjk, fonts


def render(font, cp, size, blob):
    ch = chr(cp)
    mask = Image.new('1', (size * 4 + 16, size * 4 + 16))
    origin = size + 8
    ImageDraw.Draw(mask).text((origin, origin * 2), ch, font=font, fill=1, anchor='ls')
    bb = mask.getbbox()
    offset = len(blob)
    if bb:
        crop = mask.crop(bb)
        width, height = crop.size
        for y in range(height):
            for x in range(0, width, 8):
                blob.append(sum((1 << (7 - bit)) for bit in range(8) if x + bit < width and crop.getpixel((x + bit, y))))
        left, top = bb[0] - origin, bb[1] - origin * 2
    else:
        width = height = left = top = 0
    return (cp, offset, width, height, left, top, round(font.getlength(ch, mode='1')))


def main():
    src = (GEN / 'home_font.c').read_text()
    bits, rows, cjk, fonts = parse(src)
    blob, out = bytearray(), []
    for size, first, count in fonts + cjk:
        table = rows[first:first + count]
        if (size, first) in [(f[0], f[1]) for f in fonts] and size in FONTS:
            font = ImageFont.truetype(str(FONTS[size]), size)
            new = [render(font, cp, size, blob) for cp, *_ in table]
            missing = [cp for cp, o, w, h, l, t, a in new if not w and cp > 32]
            worst = min(t + size for _, _, _, _, _, t, _ in new)
            assert worst >= 0, f'{size}px: glyph above the line box by {-worst}'
            print(f'{size} px: {len(new)} glyphs from {FONTS[size].name}, empty {len(missing)}: {[hex(c) for c in missing][:12]}')
            out += new
        else:
            for cp, offset, w, h, left, top, adv in table:
                n = ((w + 7) // 8) * h
                new_offset = len(blob)
                blob += bits[offset:offset + n]
                out.append((cp, new_offset, w, h, left, top, adv))
    text = ['/* Generated bitmap glyphs: Atkinson Hyperlegible Next (OFL-1.1) at 10/22/30/44/48/64 px,',
            ' * TRMNL12-Bold and TRMNL16-Bold pixel fonts (OFL-1.1) at 12/16 px, Noto Sans CJK SC (OFL-1.1)',
            ' * for Simplified Chinese; tools/build_fonts_cjk.py and tools/build_fonts_pixel.py. See home_font.h. */',
            '#include "home_font.h"', 'const uint8_t home_font_bits[] = {']
    for i in range(0, len(blob), 24):
        text.append(','.join(f'0x{b:02x}' for b in blob[i:i + 24]) + ',')
    text += ['};', 'const home_glyph_t home_glyphs[] = {'] + ['{%d,%d,%d,%d,%d,%d,%d},' % r for r in out] + ['};']
    text += ['/* Same glyph array and bit blob: one slice per pixel size that has CJK glyphs. */',
             'const home_font_t home_cjk_fonts[%d] = {' % len(cjk)] + ['{%d,%d,%d},' % f for f in cjk] + ['};']
    text += ['const home_font_t home_fonts[8] = {'] + ['{%d,%d,%d},' % f for f in fonts] + ['};']
    (GEN / 'home_font.c').write_text('\n'.join(text) + '\n')
    print(f'bits {len(bits)} -> {len(blob)} B')


if __name__ == '__main__':
    main()
