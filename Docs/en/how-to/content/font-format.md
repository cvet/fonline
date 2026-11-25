---
layout: default
title: Font Formats And Text Layout
locale: en
document_id: font-format-guide
permalink: /Docs/en/how-to/content/font-format.html
---

# Font Formats And Text Layout

FOnline draws text from TrueType faces only. The client rasterizes the faces at
runtime with FreeType and binds them to `FontType` slots; faces are copied into
baked resources unchanged and need no image. The Engine ships a default font
that a bare executable can draw with, and a converter that turns authored bitmap
fonts (`.fofnt` or binary BMFont) into TrueType faces. The client loads no
bitmap font descriptor.

Use this guide for authoring and integration decisions. Use the generated
[font-format reference](../../reference/font-format/index.md), its focused
[format](../../reference/font-format/formats.md),
[binding](../../reference/font-format/binding.md),
[authoring](../../reference/font-format/authoring.md),
[layout](../../reference/font-format/layout.md),
[rendering](../../reference/font-format/rendering.md), and
[validation](../../reference/font-format/validation.md) pages, plus the
[canonical JSON model](../../../generated/font-format.json), for the exact
current-revision contract.

## Scope and authority

The owning sources are:

- `Source/Client/FontManager.cpp` and `.h` for face binding, the default font,
  glyph sheets, text layout, drawing, and the short-lived format cache;
- `Source/Client/FontRasterizer.cpp` and `.h` for the FreeType wrapper that
  opens faces and rasterizes letters, including colored faces (the library is
  vendored in `ThirdParty/freetype`);
- `Source/Scripting/ClientGlobalScriptMethods.cpp` for `Game.BindFont`,
  `Game.GetTextInfo`, `Game.GetTextLines`, and `Game.DrawText`;
- `Source/Common/ResourceDefaults.h` for the default font path and pixel size;
- `Source/Common/Settings.inc` and `Source/Tools/RawCopyBaker.cpp` for raw-copy
  selection and delivery;
- `BuildTools/bitmap_font_to_ttf.py` with `Resources/FontSources/` for the
  bitmap-font converter and its sources;
- `Resources/Embedded/Fonts/` and `Resources/Core/Fonts/` for the shipped faces.

`BuildTools/FontFormatInterface.json` is the source-backed structured contract.
`BuildTools/docs_font_format.py` derives the face-list overload, raw-copy
defaults, the default font binding, the Embedded capacity default, colored-face
load flags, converter constants, font slots and flags, atlas, cache lifetime,
and the shipped face inventory. It rejects source or manifest drift and renders
the generated reference.

This page is reusable Engine documentation. An embedding project owns its font
files, `FontType` extensions, GUI assignments, typography, language coverage,
licensing, backend screenshots, and acceptance thresholds. Project docs may
link here but must not redefine the binding contract.

## Supported resources

| Suffix | Runtime role | Notes |
| --- | --- | --- |
| `.ttf` | TrueType face | The only runtime text format; bound through the face-list overload of `Game.BindFont`. |
| `.fofnt`, `.fnt` | None | Authoring sources of the [converter](#converting-bitmap-fonts); never loaded by the client and not raw-copied by default. |
| `.bmfc` | None | BMFont tool configuration; not a runtime or converter format. |

The client does not load bitmap font descriptors, BMFont text/XML descriptors,
OpenType fonts with CFF outlines, or WOFF files. Convert those sources before
shipping them. Handing a `.fofnt` or `.fnt` path to the face list fails the bind:
the file is missing from baked resources unless the project still raw-copies the
suffix, and a delivered file cannot be opened by FreeType as a face.

Face delivery is a raw copy:

1. Keep `ttf` in `Baking.RawCopyFileExtensions`; its default is `ttf ogv json
   ini`. `RawCopyBaker` preserves each face's bytes and resource path.
2. Put the faces in a resource pack under the path the script binds.
3. Bind the slot on the client before any code measures or draws with it.

A successful raw-copy bake proves only that the face was delivered. It does not
prove glyph coverage, hinting, language support, or GUI composition.

## Default font

The client, the Mapper, and the built-in updater bind `Fonts/FOnline-Regular.ttf`
to `FontType::Default` at pixel size 10 before any script runs, with normal
hinting, a one-pixel outline for `FontFlag::Bordered`, and preload ranges
`32-126`, `160-255`, and `1024-1279` (Latin, Latin-1, Cyrillic). The binding
skips a slot that is already bound, so a bare executable can draw text
immediately and a script that binds `Default` itself simply replaces the font.

The face is the converted FOnline pixel font and lives in the **Embedded**
resource pack (`Resources/Embedded/Fonts/`), which is linked into the executable;
its compressed size is the reason `FO_EMBEDDED_DATA_CAPACITY` now defaults to
`400000`. A project that sets its own capacity must make sure the compressed
Embedded pack still fits.

To change the default look:

- bind other faces to `FontType::Default` from a client script with `Game.BindFont`
  (a later bind rebuilds the slot); or
- ship your own `Fonts/FOnline-Regular.ttf` in a resource pack that shadows the
  Engine's, which keeps the pre-script startup font and the updater consistent.

The face must carry the space letter and the codepoints your texts use; a face
without a space letter fails the bind, and a missing resource fails startup.

## Binding font slots

The Engine declares one slot:

```angelscript
enum FontType
{
    Default = 0
}
```

An embedding project may extend `FontType` through its codegen enum annotation
and bind each extra slot during client initialization:

```angelscript
void BindFonts()
{
    int[] ranges = {32, 126, 160, 255, 1024, 1279};

    string[] fat = {"Fonts/FOnline-Fat.ttf"};
    Game.BindFont(FontType::Fat, fat, 16, FontOptions(), ranges);

    string[] numbers = {"Fonts/FOnline-Numbers.ttf"};
    Game.BindFont(FontType::Numbers, numbers, 17, FontOptions(), ranges);
}
```

The exact enum-extension syntax belongs to the embedding project's generated
API setup. Slots are integer indices, not path aliases. Measuring or drawing an
unloaded, negative, or out-of-range slot throws.

`Game.BindFont` has one overload: a face list, a pixel size, `FontOptions`, and
preload ranges. It binds to `AtlasType::IfaceSprites`. Rebinding a slot replaces
its font, releases the previous glyph sheet, and clears cached layouts. The
engine default font occupies `FontType::Default` until a script rebinds it.

There is no bind-time scale. Pick the pixel size you want to see: a TrueType face
scales through its outlines, and a face converted from a bitmap font is exact at
its native size (see [Converting bitmap fonts](#converting-bitmap-fonts)).

## TrueType fonts

`Game.BindFont(font, faces, pixelSize, options, preloadRanges)` binds a slot to
TrueType faces rasterized by FreeType. Calling it again on the same slot
rebuilds the font at another size, with other faces or options, and releases
the previous glyph sheet from the atlas. The caller re-lays out any text it
cached, because measurements change with the font.

- **Faces** form a fallback chain of font file paths. A letter comes from the
  first face that carries it. A letter no face carries is drawn as the first
  face's missing-glyph box, and all such letters share one cell, so a coverage
  gap stays visible instead of silently shortening the line.
  `FontManager::HaveLetter` answers from the faces and never reports the box.
- **Letters rasterize on first use.** `preloadRanges` (inclusive codepoint
  pairs) are rasterized at bind time so the first frames that show them do not
  stall; codepoints in a range that no face carries are skipped. Every other
  letter is rasterized when a text containing it is first measured or drawn,
  which is why the measuring queries (`GetTextInfo`, `GetLinesCount`,
  `GetLinesHeight`) are not const. Control characters are layout commands and
  never become glyphs.
- **One texture per font.** Glyph cells are packed in shelves into one sheet per
  font, with a second sheet for the outlined variant. A full sheet doubles,
  keeps every cell in place, is uploaded as a new atlas sprite, and the old one
  is released after queued text is flushed. A sheet is capped at 2048 by 2048
  pixels.
- **Metrics stay integer and identical on every platform.** FreeType works in
  26.6 fixed point; advances and kerning are rounded to whole pixels, and the
  line height is the face's ascender plus descender at the pixel size. A Windows
  client and a web client therefore wrap the same text at the same place.
- **Wrapping makes progress in a narrow field.** An oversized first glyph
  occupies its own line, and measurement reports its actual width so callers can
  detect overflow. Explicit truncation flags still discard text that exceeds the
  field.
- **`FontOptions`** (the zero value is light hinting, antialiased, no kerning,
  no outline): `Hinting` (`Light` is automatic vertical-only hinting, `Normal`
  is full hinting with the font's own instructions where it has them,
  `Unhinted` keeps the outlines as drawn), `Monochrome` (binary coverage, 0 or
  255), `ForceAutoHint` (FreeType's automatic hinter instead of native
  instructions; `Unhinted` disables both), `Embolden` (synthetic bold: grow the
  raster coverage one pixel to the right and add one pixel to each advance,
  including spaces; prefer an authored Bold face), `Kerning` (pair kerning from
  the `kern` table or simple GPOS pairs, applied between two neighbouring
  letters of the same face, identically in measurement and drawing),
  `OutlineWidth` (pixels of the dark outline `FontFlag::Bordered` draws; zero
  draws the plain glyphs for that flag), `LineSpacing` (pixels added to the
  face's line gap; may be negative, but the line step must stay positive).

There is no text shaping: text is laid out one glyph per codepoint, left to
right. Latin, Cyrillic, Greek, Armenian, Georgian, CJK, and symbols render
correctly; scripts that need shaping or bidirectional layout (Arabic, Hebrew,
Indic, Thai), combining diacritics, and ligatures do not.

The FreeType build compiles only the TrueType driver, the SFNT loader, the
automatic hinter, and the grey and monochrome rasterizers, with TrueType color
layers (`COLR` version 0 over `CPAL`) enabled. OpenType fonts with CFF outlines,
WOFF files, and variable-font instances other than the default are not
supported, and the build reads no `FREETYPE_PROPERTIES` environment. The pixel
size is limited to 256 and the outline to 8 pixels. The engine unit tests
`Test_FontRasterizer` and `Test_FontManager` rasterize fixture fonts generated
from rectangles by `BuildTools/generate_test_fonts.py`, so every asserted metric
follows from the numbers in that script.

## Colored fonts

A face with `COLR` v0 color layers over a `CPAL` palette draws every glyph in its
own colors. The rasterizer loads such a face with `FT_LOAD_COLOR` and no hinting
and renders it antialiased into a color bitmap, so the `Monochrome`, `Hinting`,
`ForceAutoHint`, and `Embolden` options do not apply to it. The glyph sheet keeps
the straight-alpha glyph colors, the font shader multiplies them by the text
color, and the outlined variant keeps the glyph colors under the outline.

The font shader computes glyph color times text color times two, and an ordinary
glyph is stored as neutral grey 128, so it is drawn in the text color unchanged.
A text color of neutral grey (128, 128, 128) therefore shows a colored face as
authored; any other text color tints or brightens it. The shipped number faces
`FOnline-Numbers.ttf`, `FOnline-BigNumbers.ttf`, and `FOnline-SandNumbers.ttf`
are colored.

## Converting bitmap fonts

`BuildTools/bitmap_font_to_ttf.py` turns an authored bitmap font into a TrueType
face. It reads a `.fofnt` text descriptor with its image, or a binary BMFont v3
`.fnt` with its single page, and needs `fontTools`, Pillow, `numpy`, and
`potracer`; neither the runtime nor the documentation gates need them.

```powershell
python BuildTools\bitmap_font_to_ttf.py SOURCE.fofnt OUTPUT.ttf --family "Name" --style Regular
python BuildTools\bitmap_font_to_ttf.py --manifest Resources\FontSources\Fonts.json
python BuildTools\bitmap_font_to_ttf.py --manifest Resources\FontSources\Fonts.json --check
```

- **Metrics.** One bitmap pixel is 100 font units, and the native size in pixels
  is the font's line step: `LineHeight + YAdvance` for `.fofnt`. Bind the
  converted face at that size to reproduce the bitmap's layout. A native size
  outside 1..255 is rejected. The face has no glyphs for control codes; give the
  source an explicit space letter, because the client refuses a face without one.
- **Single-color fonts** embed the bitmap as an 8-bit `EBDT`/`EBLC` strike at the
  native size, which FreeType draws pixel for pixel in every raster and hinting
  mode, and carry outlines traced from the glyph coverage for every other size.
  A no-op `prep` program stops FreeType from auto-hinting the face.
- **Multi-color fonts** (a source with colors beyond the neutral grey) become
  `COLR` v0 layers over a `CPAL` palette: one layer per distinct pixel color,
  built from whole-pixel rectangles, exact at the native size and a scaled pixel
  grid at other sizes. The traced silhouettes are the base glyphs.
- **Deterministic output.** Timestamps are fixed, so a second run writes the same
  bytes. `--manifest` converts every entry of a JSON file
  (`{"fonts": [{"source", "output", "family", "style"}]}`, paths relative to the
  manifest) and `--check` fails when a committed face differs from a fresh
  conversion.

The Engine keeps its sources in `Resources/FontSources/` (`.fofnt` files, their
images, and `Fonts.json`), which no resource pack includes, and ships only the
generated faces. A project that owns a bitmap font does the same: keep the source
and the manifest in the project, commit the generated faces, and run the `--check`
mode in its validation.

## Shipped faces

| Pack | Face | Native size | Replaces |
| --- | --- | --- | --- |
| Embedded | `Fonts/FOnline-Regular.ttf` | 10 | `Fonts/Default.fofnt` |
| Core | `Fonts/FOnline-Big.ttf` | 26 | `Fonts/Big.fofnt` |
| Core | `Fonts/FOnline-Fat.ttf` | 20 | `Fonts/Fat.fofnt` |
| Core | `Fonts/FOnline-Thin.ttf` | 28 | `Fonts/Thin.fofnt` |
| Core | `Fonts/FOnline-Special.ttf` | 12 | `Fonts/Special.fofnt` |
| Core | `Fonts/FOnline-Old.ttf` | 12 | `Fonts/OldDefault.fofnt` |
| Core | `Fonts/FOnline-Numbers.ttf` (colored) | 17 | `Fonts/Numbers.fofnt` |
| Core | `Fonts/FOnline-BigNumbers.ttf` (colored) | 24 | `Fonts/BigNumbers.fofnt` |
| Core | `Fonts/FOnline-SandNumbers.ttf` (colored) | 13 | `Fonts/SandNumbers.fofnt` |

The former `CourierNewSmall` and `DefaultExt` BMFont files were removed without
conversion. The faces carry the Engine's pixel designs; a project that needs
other typography ships its own TrueType faces.

## TextFormat and layout

`TextFormat` contains `Font`, a `FontFlag` bitmask, and nonnegative `SkipLines`.
The generated [layout reference](../../reference/font-format/layout.md) lists exact
flag values. The important interactions are:

- default finite-width layout wraps at the latest space or tab; an overlong
  token gets a line break inserted at the overflow point;
- `NoWrap` truncates drawing at the first width overflow. It is draw-mode-only:
  `Game.GetTextInfo` still follows ordinary wrapping, so do not use measurement
  to infer the final truncated substring;
- `TruncateLine` removes overflowing glyphs through the next authored newline;
- `CenterX` and `AlignRight` position each line independently;
- `CenterY` and `AlignBottom` position the visible text block vertically;
- `SkipLines` removes leading lines normally and trailing lines when
  `AlignBottom` is set;
- `KeepTail` removes leading overflow so the newest fitting lines remain;
- `Justify` distributes remaining width over spaces on wrapped lines. Tabs stay
  fixed at four space widths;
- `Bordered` selects the generated outlined texture.

A zero layout width or height is treated as unbounded in the corresponding
dimension by the formatter. Public line-count helpers reject nonpositive sizes,
so use explicit positive GUI rectangles for portable measurement behavior.

The formatter decodes UTF-8 codepoints. Invalid sequences have zero advance and
produce no glyph. A valid letter that no bound face carries is drawn as the first
face's missing-glyph box, so a coverage gap stays visible. Glyph coverage must
still be an explicit project gate.

## Measurement and drawing

Use the same font slot, flags, width, and height when measuring and drawing:

```angelscript
TextFormat format;
format.Font = FontType::Default;
format.Flags = FontFlag::CenterX | FontFlag::CenterY;

isize resultSize;
int resultLines;
Game.GetTextInfo(text, boxSize, format, resultSize, resultLines);
Game.DrawText(text, boxPos, boxSize, color, format);
```

`Game.DrawText` is available only during the interface-render event. Negative
draw width or height mirrors the rectangle origin adjustment into a positive
size before layout. A clear color selects the Engine default text white.

Except for draw-only `NoWrap`, measurement and drawing share the same formatter,
line metrics, skips, and glyph advances. `GetTextInfo` returns the maximum
line width, visible block height, and visible line count. Cache entries are keyed
by text, slot, flags, skips, rectangle dimensions, color, and formatting mode;
they expire after three unused frames and are invalidated by font replacement.
Never depend on cache identity or lifetime.

## Color and effects

The font starts with the Engine shared font effect. A project may replace the
shared effect or select a per-slot `EffectType::Font` subtype. Passing a null
per-slot override returns that slot to the current shared font effect. Effect
syntax and backend validation belong to [Effect Format](effect-format.md).

Colored faces draw their own colors, multiplied by the text color (see
[Colored fonts](#colored-fonts)). Inline renderer tags use the Engine's packed `BBGGRR` / `AABBGGRR` order, with
an optional `0x` prefix; `@color@` restores the previous color. Valid tags are
removed before wrapping. With `NoColorize`, valid tags are still stripped but
their colors are not applied. Malformed tags remain ordinary text. See
[Text And Localization](text-and-localization.md#engine-and-project-formatting-boundary)
for the exact forms and shared string-authoring boundary; do not duplicate
these tags in a project-specific localization grammar.

## Recommended project practice

Keep typography data-driven and small:

1. Define semantic slots such as body, heading, compact numbers, and debug text
   instead of binding a separate slot for every widget.
2. Bind each slot at the pixel size you reviewed; a converted pixel face is exact
   at its native size. Do not expect layout-time scaling.
3. Include all source-language and fallback-language codepoints, punctuation,
   digits, and symbols used by gameplay, chat, console, and updater paths.
4. Put the face, its bitmap source and manifest if it is converted, the
   license/provenance note, the slot binding, and a visual test scene in the
   same review scope.
5. Measure dynamic labels with the actual localized string and font slot; never
   size a panel from an English placeholder or character count.
6. Keep at least one screenshot matrix covering normal/bordered rendering,
   every bound pixel size, narrow wrapping, all alignments in use, and longest
   localized labels on every supported backend.

## Validation workflow

For an Engine face-binding, metric, layout, converter, or script-binding change:

```powershell
python BuildTools\docs_font_format.py --write
python -m unittest BuildTools.tests.test_docs_font_format
python -m unittest BuildTools.tests.test_bitmap_font_to_ttf
python BuildTools\bitmap_font_to_ttf.py --manifest Resources\FontSources\Fonts.json --check
python BuildTools\docs_contract_diff.py --check
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

For an embedding-project font change:

1. Regenerate project code when `FontType` changes.
2. Bake the faces and, if the project converts bitmap fonts, run the converter's
   `--check` first.
3. Run focused text-measurement and GUI-layout tests for every changed slot or
   pixel size.
4. Launch a visible client and inspect regular, bordered, colored, wrapped,
   aligned, and localized strings.
5. Check logs for face, atlas, effect, and script exceptions.
6. Update the project-owned font catalog and GUI/localization docs in the same
   change.

The focused generated checks prove the documented source contract. Only the
embedding project can prove its glyph coverage, typography, UI fit, rendering
backend behavior, and asset rights.
