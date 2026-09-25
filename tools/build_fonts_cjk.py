"""Append Simplified Chinese glyphs (Noto Sans CJK SC, OFL-1.1) to the generated
Atkinson tables in main/generated/home_font.c.

The Atkinson bytes are taken from the existing file, never re-rendered, so every
Latin frame stays bit-identical. CJK glyphs go into the same bit blob and glyph
array; home_cjk_fonts[] maps a pixel size to its slice. Codepoints: the whole of GB 2312
(level 1 and 2, 6 763 hanzi) plus CJK punctuation and full-width forms. Level 2 came in 0.5.1
after a reader in China reported missing characters: news headlines carry level-2 names and
places all the time.

Running it again on a file that already has CJK is fine: the old CJK slice is cut first, so the
Atkinson and TRMNL bytes stay exactly as they were.
"""
import hashlib
import re
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
# NotoSansCJKsc-Medium.otf from https://github.com/notofonts/noto-cjk (Sans/OTF/SimplifiedChinese), passed as argv[1].
FONT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'fonts' / 'NotoSansCJKsc-Medium.otf'
GEN = ROOT / 'firmware' / 'main' / 'generated'
SIZES = (30, 22, 16, 12)  # poster sizes a headline or note falls back through


def charset():
    cps = set()
    for hi in range(0xB0, 0xF8):  # level 1 (B0-D7) and level 2 (D8-F7)
        for lo in range(0xA1, 0xFF):
            try:
                cps.add(ord(bytes([hi, lo]).decode('gb2312')))
            except UnicodeDecodeError:
                pass
    cps |= {0x3000, 0x3001, 0x3002, 0x3008, 0x3009, 0x300A, 0x300B, 0x300C, 0x300D, 0x300E,
            0x300F, 0x3010, 0x3011, 0x3014, 0x3015, 0x301C, 0x30FB}
    cps |= set(range(0xFF01, 0xFF5F))  # full-width ASCII
    return sorted(cps)


def parse_existing(src):
    bits = bytes(int(v, 16) for v in re.findall(r'0x([0-9a-f]{2})', src.split('home_font_bits[]')[1].split('};')[0]))
    rows = [tuple(map(int, r)) for r in re.findall(r'\{(\d+),(\d+),(\d+),(\d+),(-?\d+),(-?\d+),(\d+)\}', src.split('home_glyphs[]')[1])]
    fonts = [tuple(map(int, m)) for m in re.findall(r'\{(\d+),(\d+),(\d+)\},', src.split('home_fonts[8]')[1])]
    assert len(fonts) == 8, len(fonts)
    latin = sum(f[2] for f in fonts)
    if len(rows) > latin:  # a CJK slice from an earlier run: cut it, keep every Latin byte
        cut = min(r[1] for r in rows[latin:])
        rows, bits = rows[:latin], bits[:cut]
    assert len(rows) == latin, (len(rows), latin)
    return bits, rows, fonts


def main():
    src = (GEN / 'home_font.c').read_text()
    bits, rows, fonts = parse_existing(src)
    blob, records, cjk_fonts = bytearray(bits), list(rows), []
    cps = charset()
    for size in SIZES:
        font = ImageFont.truetype(str(FONT), size)
        first = len(records)
        for cp in cps:
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
                        blob.append(sum((1 << (7 - bit)) for bit in range(8)
                                        if x + bit < width and crop.getpixel((x + bit, y))))
                left, top = bb[0] - origin, bb[1] - origin * 2
            else:
                width = height = left = top = 0
            advance = round(font.getlength(ch, mode='1'))
            assert 0 <= width < 256 and 0 <= height < 256 and -128 <= left < 128 and -128 <= top < 128 and advance < 256
            records.append((cp, offset, width, height, left, top, advance))
        # Keep every glyph inside the line box (top >= -size): shift the whole size
        # down by the same whole pixels, so the CJK baseline stays consistent.
        shift = max(0, -min(r[5] + size for r in records[first:]))
        worst = max(r[5] + r[3] - size for r in records[first:])
        records[first:] = [(cp, o, w, h, l, t + shift, a) for cp, o, w, h, l, t, a in records[first:]]
        print(f'size {size}: shift {shift} px, deepest descent {worst} px below the baseline')
        cjk_fonts.append((size, first, len(cps)))
    out = ['/* Generated Atkinson glyph subset (OFL-1.1) with Simplified Chinese glyphs from',
           ' * Noto Sans CJK SC Medium (OFL-1.1) appended by tools/build_fonts_cjk.py; see home_font.h. */',
           '#include "home_font.h"', 'const uint8_t home_font_bits[] = {']
    for i in range(0, len(blob), 24):
        out.append(','.join(f'0x{b:02x}' for b in blob[i:i + 24]) + ',')
    out.append('};')
    out.append('const home_glyph_t home_glyphs[] = {')
    out += ['{%d,%d,%d,%d,%d,%d,%d},' % r for r in records]
    out.append('};')
    out.append('/* Same glyph array and bit blob: one slice per pixel size that has CJK glyphs. */')
    out.append('const home_font_t home_cjk_fonts[%d] = {' % len(cjk_fonts))
    out += ['{%d,%d,%d},' % f for f in cjk_fonts]
    out.append('};')
    out.append('const home_font_t home_fonts[8] = {')
    out += ['{%d,%d,%d},' % f for f in fonts]
    out.append('};')
    (GEN / 'home_font.c').write_text('\n'.join(out) + '\n')
    header = (GEN / 'home_font.h').read_text()
    if 'home_cjk_fonts' not in header:
        header = header.replace('extern const home_font_t home_fonts[8];',
                                'extern const home_font_t home_fonts[8];\n'
                                '/* Simplified Chinese (Noto Sans CJK SC, OFL-1.1): GB 2312 level 1 and 2 + punctuation, sizes 30/22/16/12. */\n'
                                'extern const home_font_t home_cjk_fonts[%d];' % len(cjk_fonts))
        header = header.replace('/* Generated Atkinson bitmap glyph subset. Atkinson font: OFL-1.1.',
                                '/* Generated bitmap glyphs: Atkinson Hyperlegible Next and Noto Sans CJK SC, both OFL-1.1.')
        (GEN / 'home_font.h').write_text(header)
    print(f'chars {len(cps)} sizes {SIZES} bits +{len(blob) - len(bits)} B, glyphs +{len(records) - len(rows)}, '
          f'font sha256 {hashlib.sha256(FONT.read_bytes()).hexdigest()[:16]}')


if __name__ == '__main__':
    main()
