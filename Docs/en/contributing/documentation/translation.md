---
layout: default
title: Documentation Translation Workflow
locale: en
document_id: documentation-translation-workflow
permalink: /Docs/en/contributing/documentation/translation.html
---

# Documentation Translation Workflow

FOnline documentation uses English as the canonical source and Russian as a whole-document mirror. This guide defines the translation metadata, glossary, freshness gate, link policy, and production transition.

## Current migration state

The locale migration is complete. Canonical human documentation lives under
paired `Docs/en/...` and `Docs/ru/...` routes, while former flat paths remain
durable Markdown pointers. Repository and subsystem README pairs follow the
explicit entrypoint mapping below. Every public human page has:

- one stable document ID;
- one planned `Docs/en/...` destination;
- one derived `Docs/ru/...` mirror path, or an explicit README pair;
- one canonical English content hash in the generated translation-status model.

All 197 required Russian counterparts are present, and
`localization.enforcement` is `complete`. Every page must remain complete,
hash-current, code-preserving, and correctly paired. The generated report is
the authoritative inventory; adding a new translation-required English page
without its Russian counterpart fails validation immediately.

The current machine report is [translation-status.json](../../../generated/translation-status.json).

This 197/197 result proves physical page parity. Generated Russian pages can
also contain reader-facing prose supplied by machine models rather than by the
Markdown template. That semantic layer has a separate catalog and gate,
described below; physical parity must not be reported as complete semantic
translation of generated content.

## Directory convention

Use lowercase locale directories, which are conventional for documentation sites:

```text
Docs/
|-- en/
|   |-- index.md
|   |-- tutorials/
|   |-- how-to/
|   |-- reference/
|   |-- explanation/
|   |-- troubleshooting/
|   `-- contributing/
`-- ru/
    `-- <the same relative paths>
```

Repository and subsystem entry points use explicit pairs instead:

```text
README.md
README.ru.md
BuildTools/README.md
BuildTools/README.ru.md
```

Stable document IDs join languages. Translated titles and headings never define identity.

## Translate one document

1. Find the record in `Docs/generated/translation-status.json`.
2. Read the owning sources and canonical English page; do not translate stale behavior.
3. Use `Docs/translation-glossary.json` for shared terminology.
4. Create the exact `russian_path`.
5. Put a one-line metadata comment near the top:

   ```text
   <!-- docs-translation: {"document_id":"getting-started","locale":"ru","source_path":"Docs/en/tutorials/getting-started.md","source_sha256":"<current hash>"} -->
   ```

6. Translate prose, headings, table labels, alt text, and reader-facing warnings.
7. Preserve identifiers, file paths, commands, code, metadata tags, enum values, and fenced code blocks exactly.
8. Link to the Russian counterpart. The complete-parity gate rejects a missing target rather than allowing a production fallback.
9. Run the snippet gate against the canonical English fences, then run the localization generator and focused tests.

The helper can print the exact hash through the generated status model. Do not compute or edit hashes independently in a second format.

## Freshness and parity

`BuildTools/docs_localization.py` normalizes line endings and computes SHA-256 over the complete canonical English source. For every existing Russian page it requires:

- exact document ID;
- locale `ru`;
- exact canonical source path;
- current normalized source hash;
- identical ordered fenced code blocks;
- language-preserving links whenever the Russian target exists.

Run:

```bash
python BuildTools/docs_localization.py --write
python BuildTools/tests/test_docs_localization.py
python BuildTools/docs_localization.py --check --enforce-complete
```

The manifest also enables this production release gate by default; the explicit
flag keeps local and CI intent visible:

```bash
python BuildTools/docs_localization.py --check --enforce-complete
```

Changing English invalidates the Russian hash immediately. Update both in the same change or leave the branch failing; do not refresh the hash without reviewing the translation.

## Generated model prose

`Docs/description-translations.ru.json` is the reviewed Russian overlay for
reader-facing prose stored in generated contract models. Each record uses a
domain-local stable locator derived from an owning `id`, `name`, or another
unique source key, plus a normalized hash of the exact English value. Array
positions are forbidden identities. Reordering source records therefore keeps
translations attached, while changing English text makes the owning entry
stale immediately.

`BuildTools/docs_description_translations.py` inventories 20 generated models,
rejects duplicate, unknown, stale, type-changing, and inline-code-changing
records, and writes
[description-translation-status.json](../../../generated/description-translation-status.json).
Generators apply the overlay to a deep copy of the model: canonical JSON and
English Markdown remain unchanged, while Russian Markdown receives translated
descriptions before its fixed labels and headings are localized.

The catalog uses `complete`: all 4,918 reader-facing values in all 20 generated
domains, including the native API, have current reviewed records. CI and local
validation reject any missing, unknown, stale, type-changing, list-shape-changing,
or inline-code-changing entry. When a generator exposes another reader-facing
value, add its reviewed translation in the same change; an incomplete catalog
is not an accepted intermediate state.

Exact-source translation memory avoids duplicating reviewed prose when one
generated contract projects another. A missing locator may reuse a catalog
translation from the same domain only when both the normalized SHA-256 and the
complete source value match. Every donor for that source must agree on the
translation; otherwise the locator remains missing and `complete` fails. The
status record names the donor as `translation_source_locator`. Generated
`*Property` enum values use this path to share the owning property's reviewed
translation without creating a second maintenance copy.

Run:

```bash
python BuildTools/docs_description_translations.py --write --enforce-complete
python BuildTools/tests/test_docs_description_translations.py
python BuildTools/docs_description_translations.py --check --enforce-complete
```

For a translated value, preserve stable IDs, signatures, paths, enum values,
commands, and inline code exactly. Use `preserve_source: true` only when the
complete reader-visible value is intentionally language-neutral, such as
`WebGL 2`; it may not excuse untranslated prose.

## Glossary policy

Each glossary record uses one policy:

- `preserve` - keep the English/product spelling;
- `translate` - use the reviewed Russian term in prose;
- `contextual` - translate prose but preserve exact identifiers, paths, target names, or established technical spelling.

Add terms when two plausible translations could alter meaning, ownership, stability, or support claims. The glossary is not a dictionary of every common word.

API identifiers remain untranslated. Reader-facing API descriptions use the
generated-description catalog above; stable IDs and signatures remain
byte-identical.

## Link policy

Within a Russian page:

- link to the Russian counterpart when it exists;
- retain the same anchor semantics;
- use the canonical English page only as an explicit temporary fallback;
- never construct locale paths from translated titles;
- keep external source links pinned or rolling according to the owning English page.

The site language switcher must resolve by stable document ID and fall back visibly when a counterpart is still missing. It must not silently send a Russian reader to an unrelated index.

Search is locale-scoped. English pages load `assets/docs-search.json`; Russian
pages load `assets/docs-search.ru.json`. Each index has the same fail-closed
byte limit, contains only documents available in that locale, and must return
locale-preserving result URLs. This keeps the complete future Russian mirror
bounded independently from the English corpus.

## Review checklist

- Behavior still matches current Engine source and tests.
- No requirement, warning, limitation, recovery step, table row, or alt text was omitted.
- Commands and fenced code are unchanged and pass the same snippet harness as English; claimed semantic outcomes still pass their owning compile/bake/smoke test.
- Identifiers, paths, settings, tags, and extensions are unchanged.
- Terminology follows the glossary.
- Links preserve language where counterparts exist.
- The English hash matches.
- A native Russian reviewer approved meaning and tone.
- Search finds the page by Russian task vocabulary and technical identifiers.
- Mobile layout, tables, code blocks, and long identifiers remain readable.
- The `zoom-200` browser profile keeps the Russian page readable at a 640 x
  512 CSS-pixel viewport with device scale factor 2, and the retained
  1280 x 1024 screenshot has been visually reviewed.

Machine checks protect structure and freshness; they cannot approve translation quality.

## Migrating canonical paths

Migrate pages in reviewed groups:

1. create the canonical `Docs/en/...` page;
2. create and review the paired `Docs/ru/...` page;
3. retain the old Markdown path as a durable pointer;
4. update the manifest source path while preserving the stable ID;
5. regenerate routes, navigation, search, localization status, and AI outputs;
6. inspect the rendered `_site` artifact and both language routes;
7. verify old links and anchors;
8. merge only with translation parity green.

Do not bulk-move all English files before link, redirect, and language-switch behavior is proven on a small complete group.

The two linked getting-started tutorials are that proof group. Their English
and Russian canonical routes, old pointer routes and anchors, front-matter
locale, language switch, and Russian search are required regression fixtures
for every subsequent batch.

## Production transition

The bilingual launch requires:

1. one-to-one Russian coverage for every `translation: required` current human page;
2. reviewed glossary and complete generated-description catalog;
3. `README.ru.md` and paired subsystem/example READMEs;
4. complete enforcement in the manifest and CI;
5. bilingual navigation/search and language-preserving links;
6. native-speaker review plus the automated Russian 200-percent reflow profile
   and representative assistive-technology review;
7. rendered GitHub Pages artifact verification;
8. removal of the temporary translation-pending allowance.

After launch, Engine and embedding-project updates must include translation impact in the same documentation audit. Existing translations may never remain green against an older source hash.
