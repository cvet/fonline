---
layout: default
title: Read Engine Content Formats from Project Tools
locale: en
document_id: project-reading-engine-formats
permalink: /Docs/en/how-to/project/reading-engine-formats.html
---

# Read Engine Content Formats from Project Tools

A game project's audits, linters and generators read the same files the bakers read: the main configuration, resource packs, prototypes and text packs. `BuildTools/fo_formats.py` reads them the way the Engine parsers do, so a project tool reports the effective value a bake would produce instead of what a regular expression guesses. Use it for every project tool that needs a configuration value, a pack's input files, an inherited prototype field or a text entry, and keep only the project's policy in the project.

## Source paths inspected

- `BuildTools/fo_formats.py`
- `BuildTools/foconfig.py`
- `BuildTools/tests/test_fo_formats.py`
- `BuildTools/tests/data/fo_formats/expected.json`
- `BuildTools/tests/data/fo_formats/Project/Golden.fomain`
- `BuildTools/HelperCliInterface.json`
- `Source/Common/ConfigFile.cpp`
- `Source/Common/Settings.cpp`
- `Source/Common/Settings.inc`
- `Source/Common/FileSystem.cpp`
- `Source/Common/DataSource-Directory.cpp`
- `Source/Essentials/DiskFileSystem.cpp`
- `Source/Essentials/StringUtils.cpp`
- `Source/Common/AnyData.cpp`
- `Source/Common/TextPack.cpp`
- `Source/Common/EngineBase.cpp`
- `Source/Tools/ProtoBaker.cpp`
- `Source/Tools/ProtoTextBaker.cpp`
- `Source/Tools/TextBaker.cpp`
- `Source/Tools/MetadataBaker.cpp`
- `Source/Tests/Test_ConfigFile.cpp`
- `Source/Tests/Test_ProtoBaker.cpp`
- `Source/Tests/Test_ProtoTextBaker.cpp`
- `Source/Tests/Test_TextPack.cpp`
- `Docs/generated/prototype-format.json`
- `Docs/generated/text-format.json`

## What the library reads

| Format | Engine owner | Entry points |
| --- | --- | --- |
| Configuration grammar (`.fomain`, sub-configs, any `[Section]` file) | `ConfigFile.cpp` | `parse_config_text`, `read_config_file`, `parse_config_key_value_line` |
| Settings, `$ENV{}` and `$FILE{}` directives, `[ResourcePack]`, `[SubConfig]`, `Baking.IgnoreInputDirs` | `Settings.cpp` | `load_project_config`, `resolve_setting_value`, `ProjectConfig.setting`, `setting_list`, `setting_bool` |
| Resource pack input files | `DataSource-Directory.cpp`, `DiskFileSystem.cpp`, `FileSystem::FilterFiles` | `pack_files`, `project_files`, `match_resource_glob` |
| Prototype sections, `$Name`, `$Parent`, `$Text` | `ProtoBaker.cpp`, `ProtoTextBaker.cpp` | `collect_prototypes`, `resolve_prototypes`, `load_project_prototypes`, `prototype_text_entries`, `decode_string` |
| `.fotxt` text packs and language fallback | `TextPack.cpp`, `TextBaker.cpp` | `parse_text_pack`, `read_text_pack`, `parse_bake_languages`, `normalize_language_packs` |
| `///@` tags and `MigrationRule` | `MetadataBaker.cpp`, `EngineBase.cpp` | `iter_metadata_tags`, `parse_migration_rule`, `MigrationRules` |

The module is standard-library Python 3.12+. It reports and never writes content. `BuildTools/foconfig.py`, which packaging uses, parses through the same grammar.

## Print effective values from the command line

The command line prints JSON. Run it from the project root; in an embedding project the Engine is at `Engine/`.

Windows:

```powershell
py -3 Engine\BuildTools\fo_formats.py config MyGame.fomain --sub-config LocalTest --files
py -3 Engine\BuildTools\fo_formats.py protos MyGame.fomain --type Item --name Pistol
py -3 Engine\BuildTools\fo_formats.py text Texts\Dialogs.engl.fotxt
```

Linux:

```bash
python3 Engine/BuildTools/fo_formats.py config MyGame.fomain --sub-config LocalTest --files
python3 Engine/BuildTools/fo_formats.py protos MyGame.fomain --type Item --name Pistol
python3 Engine/BuildTools/fo_formats.py text Texts/Dialogs.engl.fotxt
```

On a standalone Engine checkout, `py -3 BuildTools\fo_formats.py protos Examples\MinimalMultiplayer\FOnlineMinimalMultiplayer.fomain` prints the example's prototypes. macOS is unobserved.

- `config` prints the settings the files assign after directive resolution, the resource packs in effect, the sub-configs and the directive warnings. `--files` adds each pack's input files.
- `protos` collects every prototype of the `Proto` baker packs, applies the project's `MigrationRule` declarations from the `Metadata` packs and prints effective `properties` and `texts` per `<Type> <Name>`.
- `text` prints the entries of one `<TextPack>.<Language>.fotxt` file.

| Exit code | Meaning |
| --- | --- |
| `0` | Read without findings. |
| `1` | `protos` or `text` found input the bakers reject; the findings are in `issues`. |
| `2` | Usage error, unreadable file, or a configuration the Engine refuses to load. |

The full argument list is in the generated [helper command reference](../../reference/helper-cli/index.md).

## Use it from a project tool

Import the module from the Engine checkout. This audit fails when an item ends up without an effective `Weight`, wherever in its parent chain the value would come from:

```python
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Engine' / 'BuildTools'))

import fo_formats  # noqa: E402

config = fo_formats.load_project_config(ROOT / 'MyGame.fomain')
prototypes, resolved, issues = fo_formats.load_project_prototypes(config)

for issue in issues:
    print(issue)

missing = [proto for (type_name, _), proto in sorted(resolved.items()) if type_name == 'Item' and 'Weight' not in proto.properties]

for proto in missing:
    print(f'{proto.source.path}:{proto.source.line}: item {proto.source.name} has no effective Weight')

sys.exit(1 if issues or missing else 0)
```

`ResolvedPrototype.property_sources` and `text_sources` name the prototype whose own section supplied each value, which is what a "redundant override" or "field defined on the wrong layer" audit needs. `PrototypeSource.key_lines` gives the authored line span of every key.

## Rules the library reproduces

These are the corner cases where hand-written parsers usually disagree with the bake. The tests pin each one to the Engine source or `Source/Tests` case it comes from.

Configuration files:

- `#` starts a comment unless it is escaped with `\` or inside double quotes; the quotes stay in the value.
- `Key += value` appends with one space; into a missing key it starts the value without a leading space; an empty append changes nothing.
- A repeated key replaces the earlier value. Repeated sections stay separate and keep file order.
- A trailing `\` after a space or tab continues the line, including on the last line of a file.
- A header without `]` or with an empty name is ignored, and its keys stay in the previous section.
- A byte order mark is not stripped, so a first `[Section]` behind one is not a header.
- Section names containing `/` are nested sections; prototype readers skip them with their keys.
- `$FILE{path}` is relative to the directory of the declaring config and its content is trimmed. An unknown `$ENV{}` variable or missing file keeps the directive text and produces a warning. `$TARGET_ENV{}` and `$TARGET_FILE{}` stay verbatim in baking mode.
- A setting value starting with `+` appends to the earlier value. Sub-config parents merge left to right before the section's own keys.

Resource pack input files:

- Input directories are walked recursively. Names starting with `.` or `~` and directories starting with `_` are skipped.
- A resource path present in several input directories of one pack comes from the first directory.
- `*` and `?` stay within one path segment, `**` crosses segments and `**/` also matches no directory. `ExcludePatterns` win over `IncludePatterns`.
- Archive inputs (`InputFiles`) are not opened; `pack_files` raises unless the caller passes `skip_archives=True`.

Prototypes:

- The section selects the type: `[Proto<Type>]` or a FixedType name. Without a FixedType list every other section is taken as a FixedType; pass `fixed_types` to reject unknown sections as the baker does.
- `$Name` defaults to the file name without its last extension. A file whose every section sets `$Name` declares no prototype under its stem, and an empty file declares nothing.
- Names with whitespace, `/` or `$` are rejected. Identities and parent names pass the `Proto` migration rules, which follow rename chains to their end, before duplicate detection.
- Parents apply left to right, each after its own ancestors, and the child applies last, so the rightmost parent wins. A later write replaces the whole value; lists are not merged.
- An ancestor reached through two parents contributes only where it is first reached. With `Baking.AllowRepeatedProtoParents = False` it is a `REPEATED_PARENT` finding.
- A parent cycle, a missing parent and a parent of another type fail the prototype under every setting.
- `$Text [Language] [Key2] [Key3] = Value` keys inherit like properties. The value is decoded: `\n`, `\"` and `\\` decode, `\r` is dropped, other escapes stay literal, and a value starting with `"` must end with its closing quote.

Text packs:

- Each entry is `{Key1}{Key2}{Text}`. A line without `{` is skipped; there is no comment syntax, so a comment containing brace fields is an entry.
- Only the text continues over lines and keeps the newlines, including carriage returns of a CRLF file. The first `}` ends it; there is no escape for `}`.
- A line whose first field never closes is skipped silently. A missing second field, an unclosed text or an empty first key is a finding and makes the baker reject the file; valid entries are still returned.
- Duplicate keys are ordered variants. `normalize_language_packs` gives a language without a key the first variant of its fallback language and drops keys the fallback lacks.

## Limits

- Values are text. Property types, enum names and references are not validated; that needs the baked metadata.
- `Proto<Type>` entity types are not checked against the metadata, and only the settings the module uses have built-in Engine defaults (`ENGINE_SETTING_DEFAULTS`).
- A `+` append is modeled as text joined with one space, which is the Engine behavior for string and list settings.
- Only one main config is read; command-line `--ApplyConfig` layering is not modeled.

## Extend it in a project

- Keep project policy in a project module that imports `fo_formats`: naming conventions, required fields per type, content-root conventions, allowed language sets. The library answers "what does the bake see"; the project module answers "is that acceptable for this game".
- Put a project check beside the Engine module, not inside it. If a check needs a value the library does not expose, read it through `parse_config_text` or `ResolvedPrototype` rather than parsing the file again.
- Keep the reason next to every exemption a project check allows, in the project's own exemption file, so a reviewer sees why a prototype or text key is skipped.
- A project that kept its own parser copy migrates in three steps. First, run the old tool and the library over the same tree and compare their output (for example the effective properties per prototype); every difference is a defect in one of them, usually the copy. Second, replace the copy's parsing with imports from `fo_formats` and delete the copy. Third, keep the comparison as a project test until the project's checks pass on the library alone.
- Report a disagreement between the library and a bake as an Engine defect with the smallest input that shows it; do not patch around it in the project.

## Wire it into tasks and CI

Project tools built on the library run in the project's ordinary validation task and CI job, before baking, because they read sources only. Run them with the interpreter the project already resolves for BuildTools; see [Write portable steps](continuous-integration.md#write-portable-steps). A tool that only prints JSON (`fo_formats.py config`) belongs in diagnostics, not in a gate.

## Validation checklist

- Every project tool that reads `.fomain`, prototype or `.fotxt` files imports `fo_formats` instead of its own parser.
- A tool that reports prototype values uses `resolve_prototypes` effective values, with `Baking.AllowRepeatedProtoParents` from the main config.
- A tool that enumerates content uses the resource packs of the main config, not a hard-coded directory list.
- Project policy and exemptions with reasons live in the project; the comparison against any earlier parser copy is clean.
- `py -3 -m pytest Engine/BuildTools/tests/test_fo_formats.py -q -p no:cacheprovider` passes on the Engine revision the project pins.

See also: [Guard Identity Migrations](migration-guard.md), [prototype file syntax](../../reference/prototype-format/syntax.md), [text pack syntax](../../reference/text-format/syntax.md), [Prototype Format](../content/prototype-format.md), [Keep a Game Project Healthy](quality-gates.md).
