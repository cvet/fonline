#!/usr/bin/env python3
"""Generate the TrueType fonts the engine unit tests rasterize.

All fonts are drawn here from rectangles, so the fixture carries no third-party font data and every metric a
test asserts follows from the numbers below. Output: Source/Tests/Test_FontFixtures.h. Requires fontTools.
"""

import argparse
import io
import pathlib

from fontTools.feaLib.builder import addOpenTypeFeaturesFromString
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont, newTable
from fontTools.ttLib.tables.ttProgram import Program

UNITS_PER_EM = 1000
ASCENDER = 800
DESCENDER = -200
# A fixed date keeps the output byte-identical between runs; fontTools stamps the current time otherwise
FIXED_TIMESTAMP = 3_600_000_000

# Codepoints the main font carries in bulk: more cells than the first glyph sheet the engine sizes for a font holds,
# so rasterizing all of them makes the sheet grow at least once
BULK_FIRST = 0x0100
BULK_COUNT = 320


def rectangles_glyph(rects, holes=()):
    pen = TTGlyphPen(None)

    # TrueType fills by non-zero winding: outer contours run clockwise, holes the other way
    for x0, y0, x1, y1 in rects:
        pen.moveTo((x0, y0))
        pen.lineTo((x0, y1))
        pen.lineTo((x1, y1))
        pen.lineTo((x1, y0))
        pen.closePath()

    for x0, y0, x1, y1 in holes:
        pen.moveTo((x0, y0))
        pen.lineTo((x1, y0))
        pen.lineTo((x1, y1))
        pen.lineTo((x0, y1))
        pen.closePath()

    return pen.glyph()


def empty_glyph():
    return TTGlyphPen(None).glyph()


def build_font(family, glyph_specs, features):
    glyph_order = [".notdef"] + [name for name, _, _, _ in glyph_specs]
    cmap = {}
    glyphs = {".notdef": rectangles_glyph([(50, 0, 450, 700)], holes=[(100, 50, 400, 650)])}
    metrics = {".notdef": (500, 50)}

    for name, codepoint, advance, rects in glyph_specs:
        cmap[codepoint] = name
        glyphs[name] = rectangles_glyph(rects) if rects else empty_glyph()
        metrics[name] = (advance, min((rect[0] for rect in rects), default=0))

    builder = FontBuilder(UNITS_PER_EM, isTTF=True)
    builder.setupGlyphOrder(glyph_order)
    builder.setupCharacterMap(cmap)
    builder.setupGlyf(glyphs)
    builder.setupHorizontalMetrics(metrics)
    builder.setupHorizontalHeader(ascent=ASCENDER, descent=DESCENDER)
    builder.setupNameTable({"familyName": family, "styleName": "Regular"})
    builder.setupOS2(sTypoAscender=ASCENDER, sTypoDescender=DESCENDER, sTypoLineGap=0, usWinAscent=ASCENDER, usWinDescent=-DESCENDER)
    builder.setupPost()
    builder.updateHead(created=FIXED_TIMESTAMP, modified=FIXED_TIMESTAMP)

    if features:
        addOpenTypeFeaturesFromString(builder.font, features)

    output = io.BytesIO()
    builder.save(output)
    return output.getvalue()


def main_font_specs():
    specs = [
        ("space", 0x20, 300, []),
        ("period", 0x2E, 250, [(50, 0, 200, 150)]),
        ("A", 0x41, 600, [(50, 0, 550, 700)]),
        ("V", 0x56, 600, [(50, 0, 550, 700)]),
        ("W", 0x57, 900, [(50, 0, 850, 700)]),
        ("a", 0x61, 500, [(50, 0, 450, 500)]),
        ("b", 0x62, 500, [(50, 0, 450, 750)]),
        ("p", 0x70, 500, [(50, -200, 450, 500)]),
        ("Zhe", 0x416, 800, [(50, 0, 750, 700)]),
    ]

    for index in range(BULK_COUNT):
        width = 500 + (index % 7) * 50
        specs.append((f"bulk{index}", BULK_FIRST + index, width + 100, [(50, 0, 50 + width, 700)]))

    return specs


def fallback_font_specs():
    return [
        ("space", 0x20, 300, []),
        ("Omega", 0x3A9, 700, [(50, 0, 650, 700)]),
        ("Zhong", 0x4E2D, 1000, [(50, -100, 950, 750)]),
    ]


def format_array(name, data):
    lines = [f"    inline constexpr std::array<uint8_t, {len(data)}> {name} = {{"]

    # Match the engine's formatter so regeneration does not reflow the existing fixture arrays.
    for byte in data:
        lines.append(f"        0x{byte:02X},")

    lines.append("    };")
    return "\n".join(lines)


def hinted_font():
    """Move A one pixel up in native bytecode; the auto-hinter must ignore that instruction."""
    data = build_font("FOnline Native Hint Test", [("space", 0x20, 300, []), ("A", 0x41, 600, [(50, 0, 550, 700)])], "")
    font = TTFont(io.BytesIO(data))
    program = Program()
    program.fromAssembly(['SVTCA[0]', 'PUSHB[ ]', '4', 'SLOOP[ ]', 'PUSHB[ ]', '0', '1', '2', '3', '64', 'SHPIX[ ]'])
    font['glyf']['A'].program = program
    # FreeType treats a font without fpgm and with a tiny/missing prep as unhinted and auto-hints it.
    # A harmless font program lets this fixture exercise the native interpreter instead.
    font_program = newTable('fpgm')
    font_program.program = Program()
    font_program.program.fromAssembly(['PUSHB[ ]', '0', 'POP[ ]'])
    font['fpgm'] = font_program
    font['maxp'].maxStackElements = 16
    font['maxp'].maxSizeOfInstructions = len(program.getBytecode())
    font['head'].created = FIXED_TIMESTAMP
    font['head'].modified = FIXED_TIMESTAMP
    font.recalcTimestamp = False
    output = io.BytesIO()
    font.save(output)
    return output.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default=str(pathlib.Path(__file__).resolve().parent.parent / "Source" / "Tests" / "Test_FontFixtures.h"))
    args = parser.parse_args()

    main_font = build_font("FOnline Test Sans", main_font_specs(), "languagesystem DFLT dflt;\nfeature kern { pos A V -150; } kern;\n")
    fallback_font = build_font("FOnline Test Fallback", fallback_font_specs(), None)

    output = pathlib.Path(args.output)
    header = output.read_text(encoding="utf-8").split("#pragma once", 1)[0] if output.exists() else ""
    body = "\n".join([
        "#pragma once",
        "",
        "#include \"Common.h\"",
        "",
        "FO_BEGIN_NAMESPACE",
        "",
        "// Generated by BuildTools/generate_test_fonts.py, which documents every glyph and metric; do not edit by hand",
        "namespace FontTests",
        "{",
        f"    inline constexpr uint32_t BulkFirstLetter = 0x{BULK_FIRST:04X};",
        f"    inline constexpr uint32_t BulkLetterCount = {BULK_COUNT};",
        "",
        format_array("MainFont", main_font),
        "",
        format_array("FallbackFont", fallback_font),
        "",
        format_array("HintedFont", hinted_font()),
        "}",
        "",
        "FO_END_NAMESPACE",
        "",
    ])
    output.write_text(header + body, encoding="utf-8", newline="\n")
    print(f"Wrote {output} ({len(main_font)} + {len(fallback_font)} font bytes)")


if __name__ == "__main__":
    main()
