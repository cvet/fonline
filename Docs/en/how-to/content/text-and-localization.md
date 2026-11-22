---
layout: default
title: Text and Localization
locale: en
document_id: text-and-localization-guide
permalink: /Docs/en/how-to/content/text-and-localization.html
---

# Text And Localization

Engine owns raw `.fotxt`, prototype `$Text`, baking, lookup and inline color tags.
Exact rules: [text-format reference](../../reference/text-format/index.md) and
[JSON](../../../generated/text-format.json). [Font Formats And Text Layout](font-format.md)
owns bitmap descriptors, slots, glyphs, measurement, wrapping and render flags.
Projects own languages, catalogs, keys, translations and formatters; this contract
does not depend on a project's packs or lexems.

## Text pack model

Every text value is stored under a `TextPackKey`:

```text
Collection + Key1 + Key2 + Key3
```

`Collection` is a `TextPackName`; the remaining fields are `hstring` values.
Language is deliberately not part of the key. The same key may therefore exist
in each baked language pack.

A complete-key-sorted vector stores adjacent variants. Successful loads, merges
and fallback repair restore order; reads verify it and binary-search without repairing shared
state. `Game.GetTextCount(key)` counts variants; `Game.GetText(key, index)` indexes from zero.

Native `TextPack::LoadFromBinaryData` takes `const_span<uint8_t>` by value. It reads
the buffer during the call and stores owned strings; it does not retain the view.
Vectors, arrays and slices of larger buffers can supply the encoded bytes.

## Raw `.fotxt` files

Name every source:

```text
<TextPack>.<Language>.fotxt
```

Require exactly two basename segments. Language is opaque, with no four-character
restriction; additional naming conventions are project-owned.

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

Only Text spans lines: append newlines until its first closing brace. There is no
closing-brace escape; data after the third field is ignored.

### Comment boundary

There is no comment-token grammar. Only lines without an opening brace are skipped.
Use brace-free headings/comments; brace groups in a supposed comment can create
an entry or fail baking. Keep key examples in fenced docs and reject tools that
insert annotation braces into `.fotxt` comments.

### Variants

Duplicate complete keys remain separate variants:

```text
{Ambient}{Dust}{The wind scrapes across the road.}
{Ambient}{Dust}{A sheet of dust hides the horizon.}
```

`Game.GetText(key)` selects a uniformly random variant; explicit-language lookup
does so too. Use `Game.GetText(key, index)` for stable zero-based selection; query
`Game.GetTextCount(key)` before choosing a project-owned index.

Do not assume that every language has the same number or ordering of variants.
Language normalization aligns key presence, not duplicate cardinality.

## Language normalization

`Baking.BakeLanguages` is ordered and must not be empty. Each entry is either a
bare `language` or `child:parent`. The first entry is the default base language;
an explicit parent must appear earlier, and language names must be unique. A
bare later language falls back to the first language. For example,
`russ engl ru18:russ en18:engl` creates two adult-overlay fallback chains. The
Engine default is `engl`; embedding projects normally override it in their
`.fomain`.

For a changed raw pack, `TextBaker` gathers all configured language files for
that pack. The base-language source must be present. Unsupported filename
suffixes are warned and skipped.

`TextPack::FixPacks` then normalizes every non-base language against its explicit
parent, or against the first language when no parent is declared:

1. remove languages not listed in `Baking.BakeLanguages`;
2. add missing configured languages;
3. remove packs that do not exist in the selected fallback language;
4. copy packs missing from the child language;
5. add keys missing from a child pack using fallback-language values;
6. remove keys that are absent from the fallback pack.

This fallback is completed during baking. Runtime lookup does not consult the
base language after a binary pack is loaded.

The binary name is:

```text
<ResourcePack>.<TextPack>.<Language>.fotxt-bin
```

`TextPack::LoadFromResources` requires those three basename segments. It appends
all language-matching files before one stable sort; duplicates retain existing,
file and entry order. Empty language selects every language without fallback.
Storage grows geometrically. Counts/key/value lengths are bounded before
allocation. Decode errors raise `DataReadingException`. An explicit final
`EnsureSorted()` sorts only completed loads; failures neither sort nor roll back. Dirty reads reject;
a successful retry sorts retained entries. Sort errors propagate normally.
Empty packs and unused trailing bytes remain accepted.

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

The generated [method reference](../../../generated/api/methods.md) owns exact exported
signatures. The important behavioral contract is:

| API | Sides | Behavior |
|---|---|---|
| `Game.GetLanguage()` | server, client, mapper | Return the current `Language` setting as `LanguageName`. |
| `Game.GetText(key)` | client, mapper | Return a uniformly random variant from the current language. A missing key returns an empty string. |
| `Game.GetText(key, index)` | client, mapper | Return the zero-based indexed variant from the current language. Missing or out-of-range returns an empty string; negative index throws. |
| `Game.GetText(langName, key)` | client, mapper | Use the current pack when `langName` is empty or current; otherwise load/cache that language and select uniformly among its variants. No runtime fallback is applied for an absent non-empty language. |
| `Game.GetTextCount(key)` | server, client, mapper | Return variant count, or zero when absent. |
| `Game.IsTextPresent(key)` | server, client, mapper | Return whether at least one variant exists. |
| `Game.ChangeLanguage(langName)` | client, mapper | Replace the current runtime pack; the immutable startup setting is unchanged. |

Startup `Client.Language` initializes live `Game.CurrentLanguage`/`GetLanguage()`
state. `ChangeLanguage` validates no identifier and calls no GUI refresh callback.
Projects own selection, persistence and GUI refresh/rebuild.

The server initializes the same language state from `Client.Language`; scripts
expose presence/count only, with no server-side `Game.GetText` overload.

## Engine and project formatting boundary

`TextPack` stores opaque strings. Engine does not define `@pname@`, `@nname@`,
`@sex@`, `@rnd@`, `@arg@`, `@text@`, variant separators, named-argument serialization
or dialog lexems; those belong to project scripts/docs.

Client font rendering owns inline colors; see byte order/flags in the
[font rendering reference](../../reference/font-format/rendering.md):

```text
@color:BBGGRR@
@color:AABBGGRR@
@color:0xBBGGRR@
@color:0xAABBGGRR@
@color@
```

Six hex digits omit alpha, eight include it; `@color@` restores the prior color.
Formatting strips tags. `FontFlag.NoColorize` still strips valid tags but uses the base color.

For project formatting, document/test its side, order relative to retrieval,
nested lookups, escaping/invalid input, random-choice owner and color-tag interaction.

## Authoring workflow

1. Choose the owning pack and semantic complete key.
2. Author the base language, then configured translations using its keys.
3. Use duplicates only for deliberate variants; keep `.fotxt` comments brace-free.
4. Use `$Text` for prototype-owned names/descriptions.
5. Bake; repair parser, missing-base and intersection failures in sources.
6. Visually check language switching and every project formatter.

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
  `Source/Client/FontManager.*` and [Font Formats And Text Layout](font-format.md);
- project pack names, language order, translations, lexems, or GUI refresh:
  the embedding project.
