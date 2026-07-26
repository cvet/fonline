# Font Formats And Text Layout

FOnline renders bitmap fonts described by either the Engine text format
`.fofnt` or the binary BMFont v3 format `.fnt`. Descriptors are copied into
baked resources unchanged, their referenced images follow the normal image
baking pipeline, and client scripts bind descriptor paths to `FontType` slots.

Use this guide for authoring and integration decisions. Use the generated
[font-format reference](generated/font-format/index.md), its focused
[format](generated/font-format/formats.md),
[FOFNT](generated/font-format/fofnt.md),
[BMFont](generated/font-format/bmfont.md),
[binding](generated/font-format/binding.md),
[layout](generated/font-format/layout.md),
[rendering](generated/font-format/rendering.md), and
[validation](generated/font-format/validation.md) pages, plus the
[canonical JSON model](generated/font-format.json), for the exact
current-revision contract.

## Scope and authority

The owning sources are:

- `Source/Client/FontManager.cpp` and `.h` for descriptor parsing, glyph
  metrics, bind-time scaling, atlas preparation, text layout, drawing, and the
  short-lived format cache;
- `Source/Scripting/ClientGlobalScriptMethods.cpp` for `Game.BindFont`,
  `Game.GetTextInfo`, `Game.GetTextLines`, and `Game.DrawText`;
- `Source/Common/Settings.inc` and `Source/Tools/RawCopyBaker.cpp` for raw-copy
  selection and delivery;
- `Source/Client/Updater.cpp` for the built-in default-font dependency;
- `Resources/Core/Fonts/` for shipped examples of both runtime descriptor
  formats and BMFont authoring sidecars.

`BuildTools/FontFormatInterface.json` is the source-backed structured contract.
`BuildTools/docs_font_format.py` derives the live extension dispatch, raw-copy
defaults, FOFNT keys and maximum version, BMFont binary constants and signed
fields, font slots and flags, scale range, atlas, cache lifetime, updater path,
and bundled descriptor inventory. It rejects source or manifest drift and
renders the generated reference.

This page is reusable Engine documentation. An embedding project owns its font
files, `FontType` extensions, GUI assignments, typography, language coverage,
licensing, backend screenshots, and acceptance thresholds. Project docs may
link here but must not redefine the parser contract.

## Supported resources

The runtime accepts exactly two case-sensitive suffixes through
`Game.BindFont`:

| Suffix | Runtime role | Notes |
| --- | --- | --- |
| `.fofnt` | Engine text descriptor | Explicit image, line, and glyph records. |
| `.fnt` | Binary BMFont v3 descriptor | Binary only, one texture page, one-pixel padding. |
| `.bmfc` | None | BMFont authoring configuration; raw-copied by default but never parsed by `Game.BindFont`. |

The client does not load BMFont text/XML descriptors, TTF, OTF, or other vector
fonts at runtime. Convert or rasterize those sources before shipping them.

Descriptor and image delivery are separate:

1. Keep `fofnt` and `fnt` in `Baking.RawCopyFileExtensions`. `RawCopyBaker`
   preserves each descriptor's bytes and resource path.
2. Put the referenced bitmap in a resource pack. The image is baked and loaded
   through the [image and sprite format](ImageFormat.md) pipeline.
3. Preserve the relative relationship between descriptor and image. Both
   loaders combine the image filename with the descriptor directory.
4. Bind the slot on the client before any code measures or draws with it.

A successful raw-copy bake proves only that the descriptor was delivered. It
does not prove that its image, glyph rectangles, language coverage, borders, or
GUI composition are correct.

## Minimal FOFNT

FOFNT is a whitespace-token parser. The first parsed key must be `Version`.
Current resources should author version 2; the client rejects values greater
than 2. A small descriptor looks like this:

```text
Version 2
Image Example.png*
LineHeight 14
YAdvance 2

Letter ' '
  PositionX 1
  PositionY 1
  Width 1
  Height 1
  OffsetX 0
  OffsetY 0
  XAdvance 5

Letter 'A'
  PositionX 4
  PositionY 1
  Width 9
  Height 12
  OffsetX 0
  OffsetY 0
  XAdvance 10

End
```

The trailing `*` on `Image` requests grayscale normalization. Omit it when the
bitmap's authored RGB must remain. `Image` is mandatory and relative to the
descriptor.

`Letter` decodes one UTF-8 codepoint beginning after the first apostrophe. Its
following metric keys modify that current glyph until another `Letter` appears.
A duplicate codepoint replaces the earlier record. Unknown keys are ignored;
this makes typos especially dangerous because the descriptor may bind with
zero/default metrics. Treat warnings, missing glyphs, and visual displacement
as asset failures.

`#` and `;` are stripped only when found in the current whitespace-delimited
key token. Do not rely on them as a full line-comment grammar. `End` stops
parsing and should terminate every authored descriptor.

## FOFNT metrics

Each glyph has a visible rectangle and cursor metrics:

- `PositionX`, `PositionY`: top-left pixel of the visible rectangle;
- `Width`, `Height`: visible dimensions, excluding the one-pixel sampling
  border;
- `OffsetX`, `OffsetY`: signed Engine bearings. Drawing starts at cursor minus
  the offset, so positive values move the bitmap left/up and negative values
  move it right/down;
- `XAdvance`: signed horizontal cursor advance after the codepoint;
- `LineHeight`: visible line height. Zero or omission derives the maximum glyph
  height after optional scaling;
- `YAdvance`: additional gap between lines.

Author an explicit space glyph. Its `XAdvance` becomes `SpaceWidth`; without
one, spaces and tabs can have zero width. A tab advances by four `SpaceWidth`
units. Kerning pairs are not represented or applied.

Leave at least one transparent pixel around every visible glyph and around the
image edge. The renderer expands texture coordinates and geometry by one pixel
on all sides. That border carries antialiased edge pixels and gives the optional
outline generator room to dilate without bleeding into neighboring glyphs.

## Binary BMFont

Use a BMFont exporter with these settings:

- binary format, version 3;
- exactly one texture page;
- padding top/right/bottom/left = `1/1/1/1`;
- Info, Common, Pages, and Chars blocks in standard order;
- page image filename relative to the `.fnt` file.

The client expects 20-byte character records. `xoffset`, `yoffset`, and
`xadvance` are signed little-endian 16-bit fields. Negative bearings are normal
and occur in the bundled fonts; converting them to unsigned values moves glyphs
by tens of thousands of pixels.

The loader removes the exporter's padding from each record: it shifts X/Y by
one, subtracts two from width/height, negates bearings into the Engine offset
convention, and adds one to X advance. BMFont Common `lineHeight` participates
in vertical-bearing conversion but is not copied directly. Engine `LineHeight`
uses the visible `W` glyph height when `W` exists, otherwise Common `base`, and
`YAdvance` becomes half of that result.

Every binary BMFont binding is grayscale-normalized and gets a bordered atlas
copy. The runtime ignores kerning and supports neither multiple texture pages
nor alternate block ordering. Review the generated
[BMFont contract](generated/font-format/bmfont.md) before changing exporter
settings.

## Binding font slots

The Engine declares one slot:

```angelscript
enum FontType
{
    Default = 0
}
```

An embedding project may extend `FontType` through its codegen enum annotation
and bind each slot during client initialization:

```angelscript
Game.BindFont(FontType::Default, "Fonts/Default.fofnt");
Game.BindFont(FontType::Big, "Fonts/Big.fofnt", 0.8f);
Game.BindFont(FontType::Numbers, "Fonts/Numbers.fofnt");
```

The exact enum-extension syntax belongs to the embedding project's generated
API setup. Slots are integer indices, not path aliases. Measuring or drawing an
unloaded, negative, or out-of-range slot throws.

Both descriptor paths bind to `AtlasType::IfaceSprites`. Rebinding a slot
replaces its font, rebuilds texture data, and clears cached layouts. The built-in
updater separately attempts to bind `FontType::Default` from
`Fonts/Default.fofnt` with skip-if-already-loaded behavior, so that resource is
part of the stock host contract.

## Bind-time scale

`Game.BindFont` accepts `defaultScale`, defaulting to `1.0`. It must be finite
and in `(0, 1]`. The client deliberately does not upscale a bitmap font; author
a larger source atlas and downscale it for smaller slots.

Scaling happens once while the font is bound:

1. Every glyph is area-average resampled within its own rectangle using
   alpha-weighted color.
2. The original rectangle is cleared and the smaller bitmap is written at the
   same top-left position, so neighboring glyphs cannot bleed into it.
3. glyph size, bearings, advance, line height, space width, and line gap are
   rounded to integer target metrics;
4. grayscale normalization and border dilation run on the scaled result.

There is no independent per-widget font scale in `TextFormat`. Bind separate
slots when a project needs several sizes. Validate every scale with
`Game.GetTextInfo` and visible text because integer rounding can change wrapping
and baseline fit.

## TextFormat and layout

`TextFormat` contains `Font`, a `FontFlag` bitmask, and nonnegative `SkipLines`.
The generated [layout reference](generated/font-format/layout.md) lists exact
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

The formatter decodes UTF-8 codepoints. Invalid sequences and codepoints absent
from the selected font have zero advance and produce no fallback glyph. A font
that lacks a required language character can therefore collapse words without
a hard runtime error. Glyph coverage must be an explicit project gate.

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
line metrics, skips, scale, and glyph advances. `GetTextInfo` returns the maximum
line width, visible block height, and visible line count. Cache entries are keyed
by text, slot, flags, skips, rectangle dimensions, color, and formatting mode;
they expire after three unused frames and are invalidated by font replacement.
Never depend on cache identity or lifetime.

## Color and effects

The font starts with the Engine shared font effect. A project may replace the
shared effect or select a per-slot `EffectType::Font` subtype. Passing a null
per-slot override returns that slot to the current shared font effect. Effect
syntax and backend validation belong to [Effect Format](EffectFormat.md).

Inline renderer tags use the Engine's packed `BBGGRR` / `AABBGGRR` order, with
an optional `0x` prefix; `@color@` restores the previous color. Valid tags are
removed before wrapping. With `NoColorize`, valid tags are still stripped but
their colors are not applied. Malformed tags remain ordinary text. See
[Text And Localization](TextAndLocalization.md#engine-and-project-formatting-boundary)
for the exact forms and shared string-authoring boundary; do not duplicate
these tags in a project-specific localization grammar.

## Recommended project practice

Keep typography data-driven and small:

1. Define semantic slots such as body, heading, compact numbers, and debug text
   instead of binding a separate slot for every widget.
2. Author the largest bitmap needed for a family and bind reviewed downscaled
   variants. Do not expect layout-time scaling.
3. Include all source-language and fallback-language codepoints, punctuation,
   digits, and symbols used by gameplay, chat, console, and updater paths.
4. Put descriptor, bitmap, license/provenance note, slot binding, and visual
   test scene in the same review scope.
5. Measure dynamic labels with the actual localized string and font slot; never
   size a panel from an English placeholder or character count.
6. Keep at least one screenshot matrix covering normal/bordered rendering,
   every bound scale, narrow wrapping, all alignments in use, and longest
   localized labels on every supported backend.

## Validation workflow

For an Engine parser, metric, layout, or script-binding change:

```powershell
python BuildTools\docs_font_format.py --write
python -m unittest BuildTools.tests.test_docs_font_format
python BuildTools\docs_contract_diff.py --check
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

For an embedding-project font change:

1. Regenerate project code when `FontType` changes.
2. Bake descriptor and image resources together.
3. Run focused text-measurement and GUI-layout tests for every changed slot or
   scale.
4. Launch a visible client and inspect regular, bordered, colored, wrapped,
   aligned, and localized strings.
5. Check logs for descriptor, image, atlas, effect, and script exceptions.
6. Update the project-owned font catalog and GUI/localization docs in the same
   change.

The focused generated checks prove the documented source contract. Only the
embedding project can prove its glyph coverage, typography, UI fit, rendering
backend behavior, and asset rights.
