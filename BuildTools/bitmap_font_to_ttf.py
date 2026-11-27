#!/usr/bin/env python3
"""Convert a bitmap font (FOnline `.fofnt` or binary BMFont v3 `.fnt` with its page image) into a TrueType face.

The engine renders text only from TrueType faces. This tool keeps a pixel-art font's exact look and makes it scale:

- One bitmap pixel is 100 font units; the native size (in pixels) is the font's line step, LineHeight + YAdvance for
  `.fofnt`, so binding the face at its native size reproduces the bitmap's layout metrics.
- A single-colour font embeds the bitmap itself as an 8-bit `EBDT`/`EBLC` strike at the native size, so FreeType draws
  it pixel for pixel in every raster and hinting mode, and carries outlines traced from the same glyphs (coverage
  upscaled bilinearly, thresholded at half and traced with potrace) for every other size.
- A multi-colour font keeps its colours as `COLR` v0 layers over a `CPAL` palette: one layer per distinct pixel colour,
  made of whole-pixel rectangles, so the native size is exact and other sizes scale the pixel grid. Its base glyphs are
  the traced silhouettes, used by renderers without colour support.
- A no-op `prep` program stops FreeType from auto-hinting the face, which distorts outlines traced from a pixel design.

The output is byte-identical between runs. Requires fontTools, Pillow, numpy and potracer.

Usage:
    python3 BuildTools/bitmap_font_to_ttf.py SOURCE.fofnt OUTPUT.ttf --family "Name" [--style Regular] [--check]

`--check` converts in memory and fails when OUTPUT is missing or differs, instead of writing it.

The original bitmap sources of the faces the Engine ships are not part of the Engine: they are kept in the
`fonline-historical-stuff` repository, folder `FontSources`.
"""

import argparse
import io
import re
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
import potrace
from fontTools.colorLib.builder import buildCOLR, buildCPAL
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.reverseContourPen import ReverseContourPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import newTable
from fontTools.ttLib.tables.E_B_D_T_ import ebdt_bitmap_format_1
from fontTools.ttLib.tables.E_B_L_C_ import SbitLineMetrics, SmallGlyphMetrics, Strike, eblc_index_sub_table_1
from fontTools.ttLib.tables.ttProgram import Program
from PIL import Image

PIXEL = 100
UPSCALE = 16
TRACE_PADDING = 1
TRACE_ALPHAMAX = 1.0
TRACE_OPTTOLERANCE = 0.2
CURVE_MAX_ERROR = 4
# The engine's neutral glyph grey: the font shader doubles the vertex colour, so 128 draws the text colour unchanged
NEUTRAL_GREY = (128, 128, 128)
# Control codes are layout commands for the engine and C1 codes are not text; neither becomes a glyph
SKIPPED_CODEPOINTS = set(range(0x00, 0x20)) | set(range(0x7F, 0xA0))
# One SVTCA[x] instruction, the default state: FreeType treats the face as hinted and moves nothing
NOOP_PREP = bytes([0x01])


@dataclass
class Glyph:
    codepoint: int
    pixels: Image.Image | None  # RGBA crop of the glyph box, None for an empty glyph
    left: int  # screen column of the box's first column, relative to the pen position
    top: int  # screen row of the box's first row, relative to the line top
    advance: int


@dataclass
class BitmapFont:
    line_height: int
    line_gap: int
    grey: bool  # the bitmap is a coverage mask: every pixel is drawn in the neutral grey
    glyphs: dict = field(default_factory=dict)


def read_fofnt(path):
    """Parse a `.fofnt` the way the engine's former loader did: the last definition of a letter wins."""
    text = path.read_text(encoding="utf-8").replace("\r\n", "\n")
    header = dict(re.findall(r"^(Version|Image|LineHeight|YAdvance)\s+(\S+)", text, re.M))
    image_name = header["Image"]
    grey = image_name.endswith("*")
    atlas = Image.open(path.parent / image_name.rstrip("*")).convert("RGBA")
    letters = {}

    for block in re.split(r"\n(?=Letter )", text)[1:]:
        first, rest = block.split("\n", 1)
        char = first[len("Letter '"):-1]

        if len(char) != 1:
            raise ValueError(f"{path}: letter {char!r} is not a single codepoint")

        letters[ord(char)] = {key: int(value) for key, value in re.findall(r"^\s+(\w+)\s+(-?\d+)", rest, re.M)}

    line_height = int(header.get("LineHeight", 0)) or max(fields.get("Height", 0) for fields in letters.values())
    font = BitmapFont(line_height=line_height, line_gap=int(header.get("YAdvance", 0)), grey=grey)

    for codepoint, fields in letters.items():
        width, height = fields.get("Width", 0), fields.get("Height", 0)
        x, y = fields.get("PositionX", 0), fields.get("PositionY", 0)
        pixels = atlas.crop((x, y, x + width, y + height)) if width > 0 and height > 0 else None
        font.glyphs[codepoint] = Glyph(codepoint, pixels, -fields.get("OffsetX", 0), -fields.get("OffsetY", 0),
                                       fields.get("XAdvance", 0))

    return font


def read_bmfont(path):
    """Parse a binary BMFont v3 `.fnt` with one page, placing glyphs as the engine's former loader did."""
    data = path.read_bytes()

    if data[:4] != b"BMF\x03":
        raise ValueError(f"{path}: not a binary BMFont v3 file")

    blocks = {}
    position = 4

    while position < len(data):
        block_type, length = data[position], struct.unpack_from("<I", data, position + 1)[0]
        blocks[block_type] = data[position + 5:position + 5 + length]
        position += 5 + length

    line_height, base = struct.unpack_from("<HH", blocks[2], 0)
    pages = struct.unpack_from("<H", blocks[2], 8)[0]

    if pages != 1:
        raise ValueError(f"{path}: the font must have exactly one page")

    atlas = Image.open(path.parent / blocks[3].split(b"\0")[0].decode("utf-8")).convert("RGBA")
    records = blocks[4]
    font = BitmapFont(line_height=0, line_gap=0, grey=True)

    for index in range(len(records) // 20):
        cid, x, y, w, h, ox, oy, xa = struct.unpack_from("<IHHHHHHH", records, index * 20)
        # The engine dropped the one-pixel padding BMFont puts around every glyph and read the offsets unsigned
        inner = atlas.crop((x + 1, y + 1, x + w - 1, y + h - 1)) if w > 2 and h > 2 else None
        font.glyphs[cid] = Glyph(cid, inner, ox, oy - (line_height - base), xa + 1)

    w_glyph = font.glyphs.get(ord("W"))
    font.line_height = w_glyph.pixels.height if w_glyph and w_glyph.pixels else base
    font.line_gap = font.line_height // 2
    return font


def read_bitmap_font(path):
    return read_bmfont(path) if path.suffix.lower() == ".fnt" else read_fofnt(path)


def ink_colors(font):
    colors = set()

    for glyph in font.glyphs.values():
        if glyph.pixels is not None:
            colors.update((r, g, b) for r, g, b, a in glyph.pixels.getdata() if a != 0)

    return colors


def baseline_rows(font):
    """Rows above the baseline: the most common bottom of the ink of Latin capitals and digits."""
    bottoms = {}

    for codepoint in list(range(ord("A"), ord("Z") + 1)) + list(range(ord("0"), ord("9") + 1)):
        glyph = font.glyphs.get(codepoint)

        if glyph is None or glyph.pixels is None:
            continue

        alpha = np.array(glyph.pixels.getchannel("A"))
        rows = np.nonzero(alpha.any(axis=1))[0]

        if rows.size:
            bottom = glyph.top + int(rows[-1]) + 1
            bottoms[bottom] = bottoms.get(bottom, 0) + 1

    return max(bottoms, key=lambda bottom: (bottoms[bottom], bottom)) if bottoms else font.line_height


def trace_contours(glyph, ascent):
    coverage = glyph.pixels.getchannel("A")
    canvas = Image.new("L", (coverage.width + 2 * TRACE_PADDING, coverage.height + 2 * TRACE_PADDING), 0)
    canvas.paste(coverage, (TRACE_PADDING, TRACE_PADDING))
    upscaled = canvas.resize((canvas.width * UPSCALE, canvas.height * UPSCALE), Image.BILINEAR)
    filled = np.array(upscaled) >= 128

    if not filled.any():
        return []

    # potracer treats False as ink
    paths = potrace.Bitmap(~filled).trace(turdsize=2, alphamax=TRACE_ALPHAMAX, opticurve=True,
                                          opttolerance=TRACE_OPTTOLERANCE)

    def to_font(point):
        x = point.x / UPSCALE - TRACE_PADDING + glyph.left
        y = point.y / UPSCALE - TRACE_PADDING + glyph.top
        return (round(x * PIXEL), round((ascent - y) * PIXEL))

    contours = []

    for curve in paths:
        operations = [("moveTo", to_font(curve.start_point))]

        for segment in curve.segments:
            if segment.is_corner:
                operations.append(("lineTo", to_font(segment.c)))
                operations.append(("lineTo", to_font(segment.end_point)))
            else:
                operations.append(("curveTo", to_font(segment.c1), to_font(segment.c2), to_font(segment.end_point)))

        contours.append(operations)

    return contours


def signed_area(points):
    return sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(points, points[1:] + points[:1])) / 2


def contains(point, polygon):
    x, y = point
    inside = False

    for (x0, y0), (x1, y1) in zip(polygon, polygon[1:] + polygon[:1]):
        if (y0 > y) != (y1 > y) and x < (x1 - x0) * (y - y0) / (y1 - y0) + x0:
            inside = not inside

    return inside


def reversed_operations(operations):
    recording = RecordingPen()

    for operation in operations:
        getattr(recording, operation[0])(*operation[1:])

    recording.closePath()
    reversed_recording = RecordingPen()
    recording.replay(ReverseContourPen(reversed_recording))
    return [(name,) + tuple(arguments) for name, arguments in reversed_recording.value if name != "closePath"]


def draw_contours(contours, pen):
    # TrueType fills by the non-zero rule: outer contours run clockwise, holes counter-clockwise, by nesting depth
    polygons = [[operation[-1] for operation in contour] for contour in contours]

    for index, operations in enumerate(contours):
        depth = sum(1 for other, polygon in enumerate(polygons) if other != index and contains(polygons[index][0], polygon))

        if (signed_area(polygons[index]) < 0) != (depth % 2 == 0):
            operations = reversed_operations(operations)

        for operation in operations:
            getattr(pen, operation[0])(*operation[1:])

        pen.closePath()


def draw_rectangles(rectangles, pen):
    # Clockwise in y-up font coordinates, as outer contours must be
    for x0, y0, x1, y1 in rectangles:
        pen.moveTo((x0, y0))
        pen.lineTo((x0, y1))
        pen.lineTo((x1, y1))
        pen.lineTo((x1, y0))
        pen.closePath()


def color_layers(glyph, ascent):
    """Whole-pixel rectangles per RGBA colour, as row runs, in font units."""
    layers = {}

    for row in range(glyph.pixels.height):
        top = (ascent - (glyph.top + row)) * PIXEL
        column = 0

        while column < glyph.pixels.width:
            color = glyph.pixels.getpixel((column, row))

            if color[3] == 0:
                column += 1
                continue

            end = column

            while end < glyph.pixels.width and glyph.pixels.getpixel((end, row)) == color:
                end += 1

            left = (glyph.left + column) * PIXEL
            right = (glyph.left + end) * PIXEL
            layers.setdefault(color, []).append((left, top - PIXEL, right, top))
            column = end

    return layers


def line_metrics(ascent, descent):
    metrics = SbitLineMetrics()

    for name, value in (("ascender", ascent), ("descender", -descent), ("widthMax", 0), ("caretSlopeNumerator", 0),
                        ("caretSlopeDenominator", 0), ("caretOffset", 0), ("minOriginSB", 0), ("minAdvanceSB", 0),
                        ("maxBeforeBL", 0), ("minAfterBL", 0), ("pad1", 0), ("pad2", 0)):
        setattr(metrics, name, value)

    return metrics


def strike_bitmap(glyph, ascent, font):
    metrics = SmallGlyphMetrics()
    bitmap = ebdt_bitmap_format_1(None, font)

    if glyph.pixels is None:
        metrics.height, metrics.width, metrics.BearingX, metrics.BearingY = 1, 1, 0, 0
        bitmap.imageData = bytes([0])
    else:
        metrics.width, metrics.height = glyph.pixels.size
        metrics.BearingX = glyph.left
        metrics.BearingY = ascent - glyph.top
        bitmap.imageData = glyph.pixels.getchannel("A").tobytes()

    metrics.Advance = glyph.advance
    bitmap.metrics = metrics
    return bitmap


def build_ttf(font, family, style):
    native = font.line_height + font.line_gap

    if native <= 0 or native > 255:
        raise ValueError(f"native size {native} px is out of the 1..255 range a bitmap strike can carry")

    ascent = baseline_rows(font)
    descent = font.line_height - ascent
    units_per_em = native * PIXEL
    colors = ink_colors(font)
    is_color = not font.grey and not colors <= {NEUTRAL_GREY}

    glyph_order = [".notdef"]
    glyphs = {}
    advances = {".notdef": max(native // 2, 1) * PIXEL}
    notdef = TTGlyphPen(None)
    draw_rectangles([(PIXEL // 2, 0, advances[".notdef"] - PIXEL // 2, ascent * PIXEL)], notdef)
    glyphs[".notdef"] = notdef.glyph()
    cmap = {}
    sources = {}
    layers = {}

    for codepoint in sorted(font.glyphs):
        if codepoint in SKIPPED_CODEPOINTS:
            continue

        glyph = font.glyphs[codepoint]
        name = "uni%04X" % codepoint if codepoint <= 0xFFFF else "u%05X" % codepoint
        pen = TTGlyphPen(None)

        if glyph.pixels is not None:
            draw_contours(trace_contours(glyph, ascent), Cu2QuPen(pen, max_err=CURVE_MAX_ERROR))

        glyph_order.append(name)
        glyphs[name] = pen.glyph()
        advances[name] = glyph.advance * PIXEL
        cmap[codepoint] = name
        sources[name] = glyph

        if is_color and glyph.pixels is not None:
            layers[name] = color_layers(glyph, ascent)

    palette = []
    color_glyphs = {}

    if is_color:
        palette_index = {}

        for name, by_color in layers.items():
            entries = []

            for layer_number, (color, rectangles) in enumerate(sorted(by_color.items())):
                if color not in palette_index:
                    palette_index[color] = len(palette)
                    palette.append(color)

                layer_name = f"{name}.layer{layer_number}"
                pen = TTGlyphPen(None)
                draw_rectangles(rectangles, pen)
                glyph_order.append(layer_name)
                glyphs[layer_name] = pen.glyph()
                advances[layer_name] = advances[name]
                entries.append((layer_name, palette_index[color]))

            color_glyphs[name] = entries

    builder = FontBuilder(units_per_em, isTTF=True)
    builder.setupGlyphOrder(glyph_order)
    builder.setupCharacterMap(cmap)
    builder.setupGlyf(glyphs)

    # The left side bearing must equal the outline's xMin: FreeType shifts the outline to match the bearing
    glyf = builder.font["glyf"]
    metrics = {}

    for name in glyph_order:
        outline = glyf[name]
        outline.recalcBounds(glyf)
        metrics[name] = (advances[name], outline.xMin if outline.numberOfContours else 0)

    builder.setupHorizontalMetrics(metrics)
    builder.setupHorizontalHeader(ascent=ascent * PIXEL, descent=-descent * PIXEL, lineGap=font.line_gap * PIXEL)
    builder.setupNameTable({"familyName": family, "styleName": style})
    builder.setupOS2(sTypoAscender=ascent * PIXEL, sTypoDescender=-descent * PIXEL, sTypoLineGap=font.line_gap * PIXEL,
                     usWinAscent=ascent * PIXEL, usWinDescent=descent * PIXEL, fsType=0,
                     usWeightClass=700 if "Bold" in style else 400)
    builder.setupPost()

    prep = newTable("prep")
    prep.program = Program()
    prep.program.fromBytecode(NOOP_PREP)
    builder.font["prep"] = prep

    if is_color:
        builder.font["COLR"] = buildCOLR(color_glyphs, version=0)
        builder.font["CPAL"] = buildCPAL([[(r / 255, g / 255, b / 255, a / 255) for r, g, b, a in palette]])
    else:
        ebdt = newTable("EBDT")
        ebdt.version = 2.0
        strike_glyphs = glyph_order[1:]
        ebdt.strikeData = [{name: strike_bitmap(sources[name], ascent, builder.font) for name in strike_glyphs}]
        eblc = newTable("EBLC")
        eblc.version = 2.0
        strike = Strike()
        size = strike.bitmapSizeTable
        size.hori = line_metrics(ascent, descent)
        size.vert = line_metrics(ascent, descent)
        size.colorRef, size.ppemX, size.ppemY, size.bitDepth, size.flags = 0, native, native, 8, 1
        size.startGlyphIndex, size.endGlyphIndex = 1, len(glyph_order) - 1
        index_table = eblc_index_sub_table_1(None, builder.font)
        index_table.indexFormat, index_table.imageFormat = 1, 1
        index_table.names = strike_glyphs
        strike.indexSubTables = [index_table]
        eblc.strikes = [strike]
        builder.font["EBDT"] = ebdt
        builder.font["EBLC"] = eblc

    # Fixed timestamps keep the output byte-identical between runs
    builder.font["head"].created = 0
    builder.font["head"].modified = 0

    buffer = io.BytesIO()
    builder.font.save(buffer)
    summary = {"native": native, "ascent": ascent, "descent": descent, "lineGap": font.line_gap,
               "glyphs": len(cmap), "colors": len(palette)}
    return buffer.getvalue(), summary


def convert(source, output, family, style, check):
    data, summary = build_ttf(read_bitmap_font(source), family, style)
    kind = f"{summary['colors']} colours" if summary["colors"] else "8-bit strike"
    description = (f"{output}: {summary['glyphs']} glyphs, native {summary['native']} px "
                   f"(ascent {summary['ascent']}, descent {summary['descent']}, gap {summary['lineGap']}), {kind}")

    if check:
        if not output.is_file() or output.read_bytes() != data:
            print(f"{output} is stale: run python3 BuildTools/bitmap_font_to_ttf.py", file=sys.stderr)
            return False

        print(f"{description}: current")
        return True

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data)
    print(f"Wrote {description}, {len(data)} bytes")
    return True


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("source", type=Path, help="bitmap font: .fofnt or binary BMFont .fnt")
    parser.add_argument("output", type=Path, help="TrueType face to write")
    parser.add_argument("--family", required=True, help="font family name written into the face")
    parser.add_argument("--style", default="Regular")
    parser.add_argument("--check", action="store_true", help="fail when OUTPUT differs from a fresh conversion")
    arguments = parser.parse_args(argv)

    return 0 if convert(arguments.source, arguments.output, arguments.family, arguments.style, arguments.check) else 1


if __name__ == "__main__":
    sys.exit(main())
