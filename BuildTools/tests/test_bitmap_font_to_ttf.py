from __future__ import annotations

import contextlib
import io
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
FONT_MANIFEST = ENGINE_ROOT / "Resources" / "FontSources" / "Fonts.json"
CONVERTER = BUILDTOOLS_DIR / "bitmap_font_to_ttf.py"
sys.path.insert(0, str(BUILDTOOLS_DIR))

try:
    import bitmap_font_to_ttf  # noqa: E402
    from fontTools.ttLib import TTFont  # noqa: E402
    from PIL import Image  # noqa: E402

    SKIP_REASON = ""
except ImportError as exception:
    SKIP_REASON = (
        "the bitmap-font converter needs fontTools, Pillow, numpy and potracer "
        f"(python -m pip install fonttools pillow numpy potracer): {exception}"
    )

# Glyph rows: '#' is an inked pixel, 'r' and 'g' are the red and green ink of the colored font
GLYPH_A = ("..#..", ".#.#.", "#...#", "#####", "#...#", "#...#", "#...#")
GLYPH_B = ("####.", "#...#", "####.", "#...#", "#...#", "#...#", "####.")
SPACE_ADVANCE = 3
LETTER_ADVANCE = 6
LINE_HEIGHT = 9
LINE_GAP = 1
NATIVE_SIZE = LINE_HEIGHT + LINE_GAP
TOP_ROW = 1


def _paint(image: "Image.Image", left: int, rows: tuple[str, ...], palette: dict[str, tuple[int, int, int, int]]) -> None:
    for y, row in enumerate(rows):
        for x, mark in enumerate(row):
            if mark in palette:
                image.putpixel((left + x, 1 + y), palette[mark])


def _write_source(directory: Path, name: str, *, colored: bool) -> Path:
    """Write a two-letter bitmap font plus a space; the colored one paints A in red and B in green."""
    atlas = Image.new("RGBA", (16, 10), (0, 0, 0, 0))
    ink = (255, 255, 255, 255)
    first = {"#": (255, 0, 0, 255)} if colored else {"#": ink}
    second = {"#": (0, 255, 0, 255)} if colored else {"#": ink}
    _paint(atlas, 1, GLYPH_A, first)
    _paint(atlas, 8, GLYPH_B, second)
    image_name = f"{name}.png"
    atlas.save(directory / image_name)

    # A grey source ends the image name with an asterisk: the pixels are a coverage mask drawn in the text color
    header_image = image_name if colored else f"{image_name}*"
    lines = [
        "Version 2",
        f"Image {header_image}",
        f"LineHeight {LINE_HEIGHT}",
        f"YAdvance {LINE_GAP}",
        "",
        "Letter ' '",
        "  PositionX 0",
        "  PositionY 0",
        "  Width 0",
        "  Height 0",
        f"  XAdvance {SPACE_ADVANCE}",
    ]

    for letter, left in (("A", 1), ("B", 8)):
        lines.extend(
            [
                "",
                f"Letter '{letter}'",
                f"  PositionX {left}",
                "  PositionY 1",
                "  Width 5",
                "  Height 7",
                "  OffsetX 0",
                f"  OffsetY {-TOP_ROW}",
                f"  XAdvance {LETTER_ADVANCE}",
            ]
        )

    lines.extend(["", "End", ""])
    source = directory / f"{name}.fofnt"
    source.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    return source


def _convert(source: Path, output: Path, *extra: str) -> int:
    arguments = [str(source), str(output), "--family", "Synthetic", *extra]
    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
        return bitmap_font_to_ttf.main(arguments)


@unittest.skipIf(SKIP_REASON, SKIP_REASON)
class BitmapFontConverterTests(unittest.TestCase):
    def setUp(self) -> None:
        self._directory = tempfile.TemporaryDirectory()
        self.addCleanup(self._directory.cleanup)
        self.directory = Path(self._directory.name)

    def _convert_font(self, name: str, *, colored: bool) -> tuple[Path, "TTFont"]:
        source = _write_source(self.directory, name, colored=colored)
        output = self.directory / f"{name}.ttf"
        self.assertEqual(_convert(source, output), 0)
        return output, TTFont(output)

    def test_single_color_font_embeds_a_native_size_strike(self) -> None:
        _, font = self._convert_font("Single", colored=False)

        self.assertEqual(font["head"].unitsPerEm, NATIVE_SIZE * 100)
        self.assertEqual(
            sorted(font.getBestCmap()), [ord(" "), ord("A"), ord("B")]
        )
        advances = {
            codepoint: font["hmtx"][name][0]
            for codepoint, name in font.getBestCmap().items()
        }
        self.assertEqual(
            advances,
            {
                ord(" "): SPACE_ADVANCE * 100,
                ord("A"): LETTER_ADVANCE * 100,
                ord("B"): LETTER_ADVANCE * 100,
            },
        )

        # Capitals end on row 8, so the baseline sits eight rows below the line top
        ascent = TOP_ROW + len(GLYPH_A)
        self.assertEqual(font["hhea"].ascent, ascent * 100)
        self.assertEqual(font["hhea"].descent, -(LINE_HEIGHT - ascent) * 100)
        self.assertEqual(font["hhea"].lineGap, LINE_GAP * 100)

        self.assertNotIn("COLR", font)
        self.assertNotIn("CPAL", font)
        strikes = font["EBLC"].strikes
        self.assertEqual(len(strikes), 1)
        size = strikes[0].bitmapSizeTable
        self.assertEqual((size.ppemX, size.ppemY, size.bitDepth), (NATIVE_SIZE, NATIVE_SIZE, 8))

        bitmap = font["EBDT"].strikeData[0][font.getBestCmap()[ord("A")]]
        self.assertEqual((bitmap.metrics.width, bitmap.metrics.height), (5, 7))
        self.assertEqual(bitmap.metrics.BearingX, 0)
        self.assertEqual(bitmap.metrics.BearingY, ascent - TOP_ROW)
        self.assertEqual(bitmap.metrics.Advance, LETTER_ADVANCE)
        expected = bytes(255 if mark == "#" else 0 for row in GLYPH_A for mark in row)
        self.assertEqual(bytes(bitmap.imageData), expected)

        outline = font["glyf"][font.getBestCmap()[ord("A")]]
        self.assertGreater(outline.numberOfContours, 0)

    def test_color_font_carries_color_layers_instead_of_a_strike(self) -> None:
        _, font = self._convert_font("Colored", colored=True)

        self.assertIn("COLR", font)
        self.assertIn("CPAL", font)
        self.assertNotIn("EBLC", font)
        self.assertNotIn("EBDT", font)
        self.assertEqual(font["head"].unitsPerEm, NATIVE_SIZE * 100)

        palette = font["CPAL"].palettes[0]
        colors = {(round(c.red), round(c.green), round(c.blue), round(c.alpha)) for c in palette}
        self.assertEqual(colors, {(255, 0, 0, 255), (0, 255, 0, 255)})

        cmap = font.getBestCmap()
        layers = font["COLR"].ColorLayers
        for letter, expected in (("A", (255, 0, 0, 255)), ("B", (0, 255, 0, 255))):
            letter_layers = layers[cmap[ord(letter)]]
            self.assertEqual(len(letter_layers), 1)
            color = palette[letter_layers[0].colorID]
            self.assertEqual(
                (round(color.red), round(color.green), round(color.blue), round(color.alpha)),
                expected,
            )

        # The base glyph keeps the traced silhouette for renderers without color support
        self.assertGreater(font["glyf"][cmap[ord("A")]].numberOfContours, 0)
        self.assertEqual(font["hmtx"][cmap[ord("A")]][0], LETTER_ADVANCE * 100)

    def test_output_is_deterministic_and_check_detects_a_stale_face(self) -> None:
        source = _write_source(self.directory, "Stable", colored=False)
        first = self.directory / "first.ttf"
        second = self.directory / "second.ttf"

        self.assertEqual(_convert(source, first), 0)
        self.assertEqual(_convert(source, second), 0)
        self.assertEqual(first.read_bytes(), second.read_bytes())
        self.assertEqual(_convert(source, first, "--check"), 0)

        stale = bytearray(first.read_bytes())
        stale[-1] ^= 0xFF
        first.write_bytes(bytes(stale))
        self.assertEqual(_convert(source, first, "--check"), 1)
        self.assertEqual(_convert(source, self.directory / "missing.ttf", "--check"), 1)

    def test_manifest_mode_writes_and_checks_relative_outputs(self) -> None:
        _write_source(self.directory, "Grey", colored=False)
        _write_source(self.directory, "Paint", colored=True)
        manifest = self.directory / "Fonts.json"
        manifest.write_text(
            json.dumps(
                {
                    "fonts": [
                        {"source": "Grey.fofnt", "output": "out/Grey.ttf", "family": "Grey"},
                        {"source": "Paint.fofnt", "output": "out/Paint.ttf", "family": "Paint", "style": "Bold"},
                    ]
                }
            ),
            encoding="utf-8",
        )

        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(bitmap_font_to_ttf.main(["--manifest", str(manifest)]), 0)
            self.assertEqual(bitmap_font_to_ttf.main(["--manifest", str(manifest), "--check"]), 0)

        paint = TTFont(self.directory / "out" / "Paint.ttf")
        self.assertEqual(paint["name"].getDebugName(2), "Bold")
        self.assertEqual(TTFont(self.directory / "out" / "Grey.ttf")["name"].getDebugName(1), "Grey")

        (self.directory / "out" / "Grey.ttf").write_bytes(b"stale")
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(bitmap_font_to_ttf.main(["--manifest", str(manifest), "--check"]), 1)

    def test_native_size_outside_the_strike_range_is_rejected(self) -> None:
        source = _write_source(self.directory, "Huge", colored=False)
        text = source.read_text(encoding="utf-8").replace(
            f"YAdvance {LINE_GAP}", "YAdvance 300"
        )
        source.write_text(text, encoding="utf-8", newline="\n")

        with self.assertRaisesRegex(ValueError, "out of the 1..255 range"):
            with contextlib.redirect_stdout(io.StringIO()):
                bitmap_font_to_ttf.main([str(source), str(self.directory / "huge.ttf"), "--family", "Huge"])

    def test_repository_manifest_faces_are_current(self) -> None:
        manifest = json.loads(FONT_MANIFEST.read_text(encoding="utf-8"))["fonts"]
        self.assertEqual(len(manifest), 9)

        # The committed faces must equal a fresh conversion of their sources; this is the slow gate of the file
        result = subprocess.run(
            [sys.executable, "-W", "ignore::DeprecationWarning", str(CONVERTER), "--manifest", str(FONT_MANIFEST), "--check"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=900,
            check=False,
        )

        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout.count(": current"), len(manifest))


if __name__ == "__main__":
    unittest.main()
