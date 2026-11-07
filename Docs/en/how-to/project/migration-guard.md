---
layout: default
title: Guard Identity Migrations
locale: en
document_id: project-migration-guard
permalink: /Docs/en/how-to/project/migration-guard.html
---

# Guard Identity Migrations

Saved entities and authored content refer to prototypes, property names and enum values by name. Renaming or deleting one without a `///@ MigrationRule` leaves stored data pointing at nothing, and declaring a retired name again gives old data a meaning it was never written with. `BuildTools/migration_guard.py` catches both in review: it compares the identities a project declares with a committed manifest, requires a rule for every identity that left, and refuses every reuse. Run it in the change that renames, removes or moves an identity, and as a CI gate.

## Source paths inspected

- `BuildTools/migration_guard.py`
- `BuildTools/fo_formats.py`
- `BuildTools/tests/test_migration_guard.py`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/codegen.py`
- `Source/Tools/MetadataBaker.cpp`
- `Source/Tools/ProtoBaker.cpp`
- `Source/Common/EngineBase.cpp`
- `Source/Common/Properties.cpp`
- `Source/Common/PropertiesSerializer.cpp`
- `Source/Common/EntityProperties.h`
- `Source/Common/Common.h`
- `Docs/en/how-to/project/data-migrations.md`

## What the guard checks

| Identity | Declared by | A removal is covered by | Reuse means |
| --- | --- | --- | --- |
| `proto <Type> <Name>` | a prototype section of a `Proto` baker pack (`$Name` or file stem), or a configured file identity | `MigrationRule Proto <Type> Rename`, `Remove` or `Transform` | the name is retired, or a `Rename`/`Remove` source; a `Transform` source may stay only if it was already live |
| `property <Owner> <Name>` | `///@ Property` in script or native sources, `FO_ENTITY_PROPERTY` in native sources | `MigrationRule Property <Owner> Rename` or `Remove` | the name is retired, or a `Rename`/`Remove` source |
| `enum <Enum> <Value>` | `///@ Enum` in script or native sources, `///@ ExportEnum` classes in native sources | `MigrationRule Enum <Enum> <Old> <New>` | the value is retired, or an `Enum` rule source |

Rules are read as `MetadataBaker` reads them: from any `.fos` or `.cs` file of a `Metadata` baker pack, so AngelScript and Managed projects are checked the same way. Quoted sources (`"Local knowledge"`), dotted component names (`Grid.ToMap`) and `Namespace::Function` or `Class.Function` migrators are supported. A rule written for a RefType's fields registrar (`<RefType>RefType`) counts for the RefType. The rule grammar and runtime behavior are described in [Migrate Saved Data and Content](data-migrations.md#declare-a-rule-for-every-rename-removal-and-move).

The guard also reports what the bake would reject anyway (duplicate prototype ids, names with whitespace, malformed rules), because such content makes the identity set wrong. A rename chain `A -> B -> C` is valid when `C` exists: the Engine follows rename chains to their end.

## Scope

By default the scope is what the bakers see in the main config:

- prototype files: the inputs of every resource pack whose `Bakers` include `Proto`, filtered by its patterns and `Baking.ProtoFileExtensions`, walked with the Engine's skip rules (see [Read Engine Content Formats](reading-engine-formats.md#rules-the-library-reproduces));
- script sources: the `.fos` and `.cs` inputs of every pack whose `Bakers` include `Metadata`;
- a `sub_config` applies first, so its `Baking.IgnoreInputDirs` takes effect.

Native sources are opt-in: the guard reads them only when `native_sources` lists them. List the Engine's and the project's native declarations when the project wants an Engine update to show its identity changes in review. Files git ignores are skipped by default, so a local run sees the tree a clean CI checkout sees; paths inside a submodule are asked of that submodule.

`--content-dir`, `--proto-extension` and `--script-dir` (or `content_dirs`, `proto_extensions`, `script_dirs`) replace the pack-derived scope, for a project whose config cannot be read on the guard host.

## Configure it

The configuration is a JSON object; every key is optional except that a manifest path must be given here or with `--manifest`. Relative paths resolve against `--root` (default: the current directory). A complete example:

```json
{
  "main_config": "MyGame.fomain",
  "sub_config": "Validation",
  "manifest": "Tools/MigrationGuard/identities.tsv",
  "skip_git_ignored": true,
  "native_sources": ["Engine/Source/**/*.h", "SourceExt/**/*.h"],
  "exclude": ["Engine/Source/Tests/**", "SourceExt/ThirdParty/**"],
  "file_identities": [{"type": "Dialog", "patterns": ["Dialogs/**/*.fodlg"]}]
}
```

| Key | Meaning |
| --- | --- |
| `main_config` | Main configuration file. Without it and without explicit directories, the single `*.fomain` in the root is used. |
| `sub_config` | `[SubConfig]` name applied before the resource packs are read. |
| `manifest` | The committed manifest file. |
| `skip_git_ignored` | `true` (default) skips git-ignored files and requires git; set `false` outside a repository. |
| `native_sources` | Root-relative globs of native headers and sources. Start each pattern with a literal directory: only that directory is walked. |
| `exclude` | Root-relative globs removed from every scope, for test fixtures and third-party code. |
| `file_identities` | Project identities stored by file stem, each with the prototype type its rules use. |
| `content_dirs`, `proto_extensions`, `script_dirs` | Explicit scope instead of the main config's packs. |

An unknown key, a wrong value type or an unreadable manifest is a configuration error.

## Run it

Create the manifest once, commit it, then check every change.

Windows:

```powershell
py -3 Engine\BuildTools\migration_guard.py --config Tools\MigrationGuard\migration-guard.json --update-manifest
py -3 Engine\BuildTools\migration_guard.py --config Tools\MigrationGuard\migration-guard.json --check
```

Linux:

```bash
python3 Engine/BuildTools/migration_guard.py --config Tools/MigrationGuard/migration-guard.json --update-manifest
python3 Engine/BuildTools/migration_guard.py --config Tools/MigrationGuard/migration-guard.json --check
```

On a standalone Engine checkout, `py -3 BuildTools\migration_guard.py --main-config Examples\MinimalMultiplayer\FOnlineMinimalMultiplayer.fomain --manifest <path> --update-manifest` exercises the tool on an example. macOS is unobserved.

| Exit code | Meaning |
| --- | --- |
| `0` | `--check`: the tree matches the manifest. `--update-manifest`: the manifest was written. |
| `1` | Violations, a missing manifest for `--check`, or an update refused because of a blocking violation. |
| `2` | Usage or configuration error, unreadable input, or git unavailable while `skip_git_ignored` is set. |

## Read the diagnostics

Each violation is one tab-separated line: code, kind, owner, name, message and, where known, `path:line`.

```text
MIGRATION GUARD: violations found:
  MISSING_RULE	proto	Item	OldGun	prototype id left the tree without a rule; add ///@ MigrationRule Proto Item Rename OldGun <New>, Remove OldGun or Transform OldGun <Function>
  STALE_MANIFEST	proto	Item	NewGun	prototype id is declared but not recorded live; run --update-manifest	Items/Guns.fopro:5
```

| Code | Cause | Fix |
| --- | --- | --- |
| `MISSING_RULE` | A live identity left the tree with no covering rule. | Add the rule shown, or restore the identity. |
| `REUSED_PROTO_ID`, `REUSED_PROPERTY_NAME`, `REUSED_ENUM_VALUE` | A declared identity is retired or is the source of a retiring rule. | Give the new meaning a new name. |
| `DANGLING_TARGET` | A `Proto Rename` chain ends at a prototype that does not exist. | Point the rule at a live prototype. |
| `INVALID_RULE`, `INVALID_TAG`, `INVALID_CONTENT` | A malformed rule or tag, or content the baker rejects. | Fix the source the location names. |
| `STALE_MANIFEST` | The manifest does not record a new live identity or a newly retired one. | Run `--update-manifest` in the same change. |

`--update-manifest` writes only when no blocking violation remains: it never records a missing rule or a reuse, also not on the first run.

## Keep the manifest

The manifest is a sorted UTF-8 text file with one `<kind>\t<status>\t<owner>\t<name>` line per identity; `status` is `live` or `retired`.

```text
# FOnline migration guard manifest v1: kind, status, owner, name
enum	live	QuestState	Failed
enum	retired	QuestState	Aborted
property	live	Critter	QuestStage
property	retired	Critter	OldQuestStage
proto	live	Item	Pistol
proto	retired	Item	OldPistol
```

A retired identity never leaves the manifest, even after its rule is deleted under the project's retention policy, so a name stays reserved after the tombstone is gone. The guard has no exemption list: an identity that must be reused is a design decision that belongs in a data migration with a new name. Review the manifest diff with the change; it is the record of what the change retired.

When renaming or removing an identity:

1. Rename or remove it in content, scripts and settings.
2. Add the `MigrationRule` in the project's migration module.
3. Run `--check`; only `STALE_MANIFEST` lines should remain.
4. Run `--update-manifest` and commit the manifest with the change.

## Wire it into a local task and CI

The guard reads sources only, so it runs before baking and needs no build. A local task can call it with the same arguments as CI, for example as a VS Code process task:

```json
{
  "label": "Validate :: Migration Guard",
  "type": "process",
  "command": "py",
  "args": ["-3", "Engine/BuildTools/migration_guard.py", "--config", "Tools/MigrationGuard/migration-guard.json", "--check"],
  "problemMatcher": []
}
```

In CI, run `--check` in the validation job on a full checkout of the project (the Engine submodule included when native sources are configured):

```text
- name: Guard identity migrations
  run: python3 Engine/BuildTools/migration_guard.py --config Tools/MigrationGuard/migration-guard.json --check
```

Never run `--update-manifest` in CI; the manifest changes only in a reviewed commit.

## Extend it in a project

Keep a thin project wrapper that fixes the root and configuration, so developers and CI run one command:

```python
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Engine' / 'BuildTools'))

import migration_guard  # noqa: E402


def main() -> int:
    mode = sys.argv[1:] or ['--check']
    return migration_guard.main(['--root', str(ROOT), '--config', 'Tools/MigrationGuard/migration-guard.json', *mode])


if __name__ == '__main__':
    raise SystemExit(main())
```

- Put project identity registries beside the guard as separate project checks: catalog ids stored inside property values, text keys copied into saves, collection names. Use `fo_formats` to read the content they need, and give each its own manifest or baseline with a written reason per entry.
- Add identities that are stored by file name through `file_identities` instead of a second tool.
- Keep the project's migration module, retention policy and identity ledger decisions in project documentation; the guard enforces them but does not choose them.

A project that kept its own earlier guard migrates by carrying over its retired history and letting the Engine guard rebuild the live set. If the earlier copy kept retired names as `retired<TAB><Owner><TAB><Name>` lines, prefix the kind.

Linux:

```bash
{
  printf '# FOnline migration guard manifest v1: kind, status, owner, name\n'
  awk -F'\t' '$1 == "retired" { print "property\t" $0 }' Tools/MigrationGuard/property_name_manifest.txt
  awk -F'\t' '$1 == "retired" { print "enum\t" $0 }' Tools/MigrationGuard/enum_value_manifest.txt
} > Tools/MigrationGuard/identities.tsv
```

Windows:

```powershell
$rows = @('# FOnline migration guard manifest v1: kind, status, owner, name')
$rows += Get-Content Tools\MigrationGuard\property_name_manifest.txt -Encoding utf8 | Where-Object { $_ -like "retired`t*" } | ForEach-Object { "property`t$_" }
$rows += Get-Content Tools\MigrationGuard\enum_value_manifest.txt -Encoding utf8 | Where-Object { $_ -like "retired`t*" } | ForEach-Object { "enum`t$_" }
[System.IO.File]::WriteAllText("$PWD\Tools\MigrationGuard\identities.tsv", ($rows -join "`n") + "`n")
```

Then run `--update-manifest` on a tree where the earlier guard passed, review the diff, delete the earlier tool and its manifests, and switch the CI step. Expect three differences from a hand-written copy: prototype ids are exactly the baker's (a fully named file's stem and an empty file are not ids, FixedType sections are), removed properties and enum values now need a rule like prototypes, and retired prototype ids are recorded in the manifest. A `DANGLING_TARGET` that appears only with the Engine guard means stored data would migrate to a prototype the bake does not have.

## Validation checklist

- The manifest is committed and `--check` passes on the default branch.
- The configuration names the main config (and sub-config if one excludes test inputs), the manifest and the project's native sources.
- Every rename, removal or move in a change comes with its rule and the manifest diff in the same commit.
- CI runs `--check` only; nobody runs `--update-manifest` to make a failure disappear.
- Project identity registries and their reasons live in the project beside the guard.
- `py -3 -m pytest Engine/BuildTools/tests/test_migration_guard.py -q -p no:cacheprovider` passes on the pinned Engine revision.
