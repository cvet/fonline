# Text And Localization

This guide documents the reusable FOnline Engine contract for raw `.fotxt`
files, prototype-localized `$Text` fields, language baking, runtime lookup, and
renderer-owned inline color tags. Use the generated [text-format
reference](generated/text-format/index.md) and [canonical JSON
model](generated/text-format.json) for the exact current-revision rule set.
Bitmap-font descriptors, slot binding, glyph coverage, measurement, wrapping,
and the complete renderer flag contract are owned by [FontFormat.md](FontFormat.md).

An embedding game owns its language priorities, pack catalog, semantic key
names, translation workflow, and any formatter layered on top of retrieved
strings. Engine documentation must not depend on one project's packs or lexems.

## Text pack model

Every text value is stored under a `TextPackKey`:

```text
Collection + Key1 + Key2 + Key3
```

`Collection` is a `TextPackName`; the remaining fields are `hstring` values.
Language is deliberately not part of the key. The same key may therefore exist
in each baked language pack.

The backing container is a multimap. Multiple values under one complete key are
variants. `Game.GetTextCount(key)` reports their count and
`Game.GetText(key, index)` performs zero-based indexed selection.

## Raw `.fotxt` files

Name every source:

```text
<TextPack>.<Language>.fotxt
```

The basename must contain exactly those two dot-separated segments. The Engine
treats the language suffix as an opaque string and does not require a
four-character locale code. Projects may adopt a naming convention, but it is
not part of the reusable parser contract.

Each logical entry has three brace-delimited fields:

```text
{Key1}{Key2}{Text}
```

For example:

```text
{Welcome}{}{Welcome to the wasteland.}
{QuestName}{Short}{A difficult choice}
{LongMessage}{}{First line
Second line}
```

The filename supplies `Collection`; raw entries supply `Key1`, `Key2`, and the
value. `Key3` remains empty. `Collection` and `Key1` must be non-empty. `Key2`
and the text value may be empty.

Only the third field may continue across physical lines. The parser appends
newline characters until it finds the first closing brace. There is no escape
for a literal closing brace inside the value, and trailing data after the third
field is not part of the entry.

### Comment boundary

Raw text parsing has no comment-token grammar. A physical line is skipped only
when the parser cannot find the first opening brace. A project may use
brace-free headings or `#` lines for readability, but a supposed comment that
contains brace groups can become an entry or make the bake fail.

Treat this as a content-review rule:

- keep comments and headings free of `{` and `}`;
- demonstrate key shapes in fenced documentation, not inside `.fotxt` comments;
- reject editor tooling that inserts annotation braces into text-pack sources.

### Variants

Duplicate complete keys remain separate variants:

```text
{Ambient}{Dust}{The wind scrapes across the road.}
{Ambient}{Dust}{A sheet of dust hides the horizon.}
```

The script API does not choose randomly by default. The default
`skipCount = 0` selects the first variant. For deliberate random selection,
query `Game.GetTextCount(key)`, select an index in project code, and pass that
index to `Game.GetText(key, index)`.

Do not assume that every language has the same number or ordering of variants.
Language normalization aligns key presence, not duplicate cardinality.

## Language normalization

`Baking.BakeLanguages` is ordered and must not be empty. Its first entry is the
base language. The Engine default is `engl`; embedding projects normally
override this in their `.fomain`.

For a changed raw pack, `TextBaker` gathers all configured language files for
that pack. The base-language source must be present. Unsupported filename
suffixes are warned and skipped.

`TextPack::FixPacks` then normalizes every non-base language:

1. remove languages not listed in `Baking.BakeLanguages`;
2. add missing configured languages;
3. remove packs that do not exist in the base language;
4. copy packs missing from a non-base language;
5. add keys missing from a non-base pack using base-language values;
6. remove keys that are absent from the base pack.

This fallback is completed during baking. Runtime lookup does not consult the
base language after a binary pack is loaded.

The binary name is:

```text
<ResourcePack>.<TextPack>.<Language>.fotxt-bin
```

`TextPack::LoadFromResources` requires exactly those three basename segments.

## Prototype `$Text` fields

Prototype sections can author localized values without separate raw text files:

```ini
[ProtoItem]
$Name = LaserRifle
$Text engl Name = Laser rifle
$Text engl Desc Short = Compact description
$Text russ Name = Localized name
```

The grammar is:

```text
$Text [Language] [Key2] [Key3] = Value
```

There may be at most four key tokens including `$Text`. The prototype id
becomes `Key1`. When `Language` is omitted, the field uses the first
`Baking.BakeLanguages` entry. Values pass through
`StringEscaping::DecodeString`, so sequences such as `\n` become actual
newlines.

Parent `$Text` fields are collected recursively before child fields. A child
inherits missing exact keys and replaces an inherited value when it defines the
same `$Text Language Key2 Key3` key.

`ProtoTextBaker` emits five packs for every configured language:

| Prototype type | Generated pack |
|---|---|
| Item | `Items` |
| Critter | `Critters` |
| Map | `Maps` |
| Location | `Locations` |
| Other non-exported entity or fixed type with `HasProtos` | `Protos` |

All five outputs exist even when some are empty. Unsupported `$Text` languages
are warned and omitted. If multiple prototype sources would generate the same
complete key in one pack and language, baking fails instead of depending on
iteration order.

## Runtime script API

The generated [method reference](generated/api/methods.md) owns exact exported
signatures. The important behavioral contract is:

| API | Sides | Behavior |
|---|---|---|
| `Game.GetLanguage()` | server, client, mapper | Return the current `Language` setting as `LanguageName`. |
| `Game.GetText(key, skipCount = 0)` | client, mapper | Return the indexed variant from the current language. Missing or out-of-range returns an empty string; negative index throws. |
| `Game.GetText(langName, key)` | client, mapper | Use the current pack when `langName` is empty or current; otherwise load/cache that language and return its first variant. No runtime fallback is applied for an absent non-empty language. |
| `Game.GetTextCount(key)` | server, client, mapper | Return variant count, or zero when absent. |
| `Game.IsTextPresent(key)` | server, client, mapper | Return whether at least one variant exists. |
| `Game.ChangeLanguage(langName)` | client, mapper | Replace the current pack and write `Client.Language`. |

The client loads `Client.Language` during startup. `Game.ChangeLanguage` does
not validate the identifier and does not invoke a game GUI refresh callback.
An embedding project owns its allowed-language selector, persistence policy,
and refresh/rebuild sequence.

The server loads one pack for `Settings.Language`. It exposes only presence and
count queries to scripts; there is no server-side script `Game.GetText`
overload in the Engine contract.

## Engine and project formatting boundary

`TextPack` stores opaque strings. The Engine does not define `@pname@`,
`@nname@`, `@sex@`, `@rnd@`, `@arg@`, `@text@`, variant separators, named
argument serialization, or dialog-specific lexem expansion. Those features,
when present, belong to the embedding project's scripts and documentation.

The client font renderer does own inline color tags. The exact byte-order and
flag interaction is also pinned in the generated [font rendering
reference](generated/font-format/rendering.md):

```text
@color:BBGGRR@
@color:AABBGGRR@
@color:0xBBGGRR@
@color:0xAABBGGRR@
@color@
```

Six hex digits set a color without an explicit alpha byte; eight include alpha.
The empty `@color@` tag restores the previous color. Tags are stripped during
font formatting. With `FontFlag.NoColorize`, valid tags are still stripped but
all text uses the base color.

If a project adds another formatting pass, document and test:

- which side performs it;
- whether it runs before or after text retrieval;
- whether nested lookups are allowed;
- escaping and malformed-input behavior;
- random-selection ownership;
- interaction with renderer color tags.

## Authoring workflow

1. Choose the owning text pack and a semantic complete key.
2. Author the base language first.
3. Add configured translations without inventing keys absent from the base.
4. Use duplicate keys only when the caller deliberately handles variants.
5. Keep raw `.fotxt` comments brace-free.
6. Use prototype `$Text` for prototype-owned names and descriptions.
7. Bake resources and treat every parser, missing-base, or intersection error
   as a source-content failure.
8. Exercise language switching and every project formatter in a visible client.

## Validation workflow

Engine maintainers changing `TextPack`, `TextBaker`, `ProtoTextBaker`, language
settings, script text methods, or inline color parsing must update
`BuildTools/TextFormatInterface.json`, this guide, and generated outputs in the
same change:

```powershell
python BuildTools\docs_text_format.py --write
python BuildTools\docs_text_format.py --check
python -m unittest BuildTools.tests.test_docs_text_format
```

Run focused native tests for `TextPack`, `TextBaker`, and `ProtoTextBaker` when
their behavior changes. Validate color-parser changes through the nearest
rendering tests and a visible client. Then rebake an embedding project.
Project-owned pack catalogs, translation guards, lexem formatters, and GUI
refresh behavior require project tests rather than Engine fixtures.

## Maintenance routing

- raw syntax, key identity, variants, binary loading, or normalization:
  `Source/Common/TextPack.*`;
- filename selection, incremental pack completion, or raw output:
  `Source/Tools/TextBaker.cpp`;
- prototype `$Text`, inheritance, pack routing, or intersections:
  `Source/Tools/ProtoTextBaker.cpp`;
- script lookup and switching: `Source/Scripting/*GlobalScriptMethods.cpp` plus
  `Source/Client/Client.cpp` or `Source/Server/Server.cpp`;
- inline color tags and all font layout/rendering behavior:
  `Source/Client/FontManager.*` and [FontFormat.md](FontFormat.md);
- project pack names, language order, translations, lexems, or GUI refresh:
  the embedding project.
