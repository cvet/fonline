---
layout: default
title: Generated Content Workflow
locale: en
document_id: generated-content-workflow
permalink: /Docs/en/how-to/build/generated-content.html
---

# Generated Content Workflow

This guide explains what to regenerate after changing Engine or game sources, what is authoritative, and how to review generated output without editing it by hand.

## Source paths inspected

- `BuildTools/Init.cmake`
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/codegen.py`
- `BuildTools/docs_metadata.py`
- `BuildTools/docs_contract_diff.py`
- `BuildTools/docs_validate.py`
- `Source/Tools/MetadataBaker.cpp`
- `Examples/MinimalProject/CMakeLists.txt`
- `Examples/MinimalMultiplayer/CMakeLists.txt`

## Classify the output first

FOnline has three distinct generated layers:

| Layer | Typical output | Owning input |
|---|---|---|
| Configure/code generation | build-tree `GeneratedSource/`, generated native bindings and internal config | CMake project interface, C++ tags/templates, project options |
| Resource baking | `Baking/`, `Resources/`, `ServerResources/`, `PlatformBinaries/`, `Cache/` | `.fomain` resource packs, scripts, prototypes, maps, assets, metadata tags |
| Documentation generation | `Docs/generated/`, `_data/docs-site.json`, search/AI artifacts | source-backed interface models and `Docs/documentation-manifest.json` |

Generated output is evidence, not an editing surface. Fix the source annotation, interface model, project config, generator, or authored asset, then regenerate.

<figure class="docs-diagram">
<picture>
<source media="(max-width: 700px)" srcset="../../../assets/diagrams/generated-content-pipeline-mobile.svg">
<img src="../../../assets/diagrams/generated-content-pipeline.svg" alt="Pipeline diagram with four columns. Authoritative engine and game inputs feed CMake configuration and code generation, then native and script compilation plus resource baking, then runtime and contract validation, and finally generated documentation, search, AI delivery, and the reviewed release diff." loading="lazy">
</picture>
<figcaption>Regenerate from left to right. Delivery artifacts consume earlier generated models and hashes, so a green final gate is meaningful only when configure, compile, bake, and focused validation have already succeeded.</figcaption>
</figure>

## Configure and generate native sources

The embedding project calls the staged BuildTools pipeline:

```cmake
StartProjectGeneration()
RegisterProjectOptions()
AddThirdPartyLibraries()
RegisterEngineSources()
SetupCodeGeneration()
BuildCoreLibraries()
BuildApplications()
SetupScriptsAndBaking()
BuildPackages()
FinalizeProjectGeneration()
```

`SetupCodeGeneration()` consumes Engine and project native sources, code-generation tags, templates, and project options. `ForceCodeGeneration` is the dependency used by script compilation and baking targets, so stale native metadata cannot be hidden behind an unrelated incremental resource bake.

Reconfigure after changing CMake options, source registration, stage hooks, generated templates, or the Engine pin. Build the smallest target that compiles the affected generated source.

## Compile scripts

For AngelScript projects:

```bash
cmake --build <build-dir> --config RelWithDebInfo --target CompileAngelScript
```

Format authored scripts through the wrapper described in [AngelScript Style and Refactoring](../scripting/style-and-refactoring.md) before compilation. A generated `.fos` failure is fixed in its owning metadata, generator, or authored source and then regenerated; the derived file is not a manual edit target.

BuildTools invokes the generated ASCompiler with:

```text
-ApplyConfig <project .fomain> -ApplySubConfig NONE
```

This validates the master project contract instead of a convenient development overlay. A project may add focused script/test targets, but should keep the master compile route green.

## Bake resources

Run the normal incremental route first:

```bash
cmake --build <build-dir> --config RelWithDebInfo --target BakeResources
```

Use the forced route when the input graph changed:

```bash
cmake --build <build-dir> --config RelWithDebInfo --target ForceBakeResources
```

A forced bake is appropriate after changing:

- resource-pack directories, explicit files, include/exclude patterns, recipients, or baker lists;
- baker behavior or a baked binary schema;
- language set/order or text fallback policy;
- prototype/map migrations or identity rules;
- generated metadata tags, entity/property layouts, remotes, enums, fixed/value/ref types;
- output paths or platform binary composition;
- an incremental-cache bug or missing dependency.

Do not routinely delete the whole workspace. Preserve logs and failed outputs long enough to diagnose ownership, then remove only documented disposable directories.

## Understand metadata outputs

`MetadataBaker` parses project script tags and emits side-specific metadata such as:

```text
Baking/Metadata/Metadata.fometa-server
Baking/Metadata/Metadata.fometa-client
```

The pair is consumed by runtime dynamic metadata registration and can also generate a project-owned remote-call catalog:

```bash
python Engine/BuildTools/docs_metadata.py \
  --metadata Baking/Metadata/Metadata.fometa-server \
  --metadata Baking/Metadata/Metadata.fometa-client \
  --write
```

Both sides must agree on every paired remote call, including its `MaxBytes` and `MaxCollectionSize` structural limits. Every record carries a mandatory `Limits` trailer, with zeroes when limits are omitted. Do not reconstruct that catalog by parsing `.fos` with a second grammar; the baked metadata is authoritative.

Metadata changes can affect persistence, network synchronization, script bindings, content validation, and save compatibility even when native C++ compiles. Review the generated model and run a real bake plus the narrow runtime/test route.

## Regenerate documentation contracts

Each checked interface owns its generator. Run the affected generator with `--write`, then verify all outputs:

```bash
python BuildTools/docs_diagrams.py --write
python BuildTools/docs_screenshots.py --write
python BuildTools/docs_reference.py --write
python BuildTools/docs_snippets.py --write --external
python BuildTools/docs_description_translations.py --write
python BuildTools/docs_localization.py --write
python BuildTools/docs_site.py --write
python BuildTools/docs_ai_eval.py --write
python BuildTools/docs_ai_delivery.py --write
python BuildTools/docs_validate.py
```

Focused format/CLI/CMake generators are listed in [Generated API and Metadata](../../reference/metadata/index.md). `docs_validate.py` checks byte-for-byte freshness; it is not a replacement for the focused semantic test.

For a source/API change, compare against the base revision:

```bash
python BuildTools/docs_contract_diff.py \
  --baseline-git-ref <base> \
  --current-dir Docs/generated \
  --dispositions Docs/contract-change-dispositions.json \
  --write \
  --enforce
```

Complete the required owner, migration, release-note, and compatibility dispositions. Never edit generated JSON merely to silence the comparator.

## Dependency order

Use this order when a change crosses layers:

1. update source contracts, project configuration, authored content, and tests;
2. reconfigure and regenerate native sources;
3. compile native targets and scripts;
4. bake resources and side-specific metadata;
5. run focused native/content/runtime tests;
6. regenerate canonical documentation models, source-owned diagrams and
   screenshot catalogs, and Markdown projections;
7. regenerate example and snippet inventories, localization status, then route,
   site, search, AI-evaluation, and AI-delivery artifacts;
8. run aggregate documentation validation and contract diff;
9. inspect the final diff for unexpected generated churn.

Later steps may consume hashes or inventories from earlier ones. Running site/AI generation before canonical pages are current can produce internally consistent but stale delivery artifacts.

## Review generated changes

Review the input and output together:

- generated symbols should trace to a source tag/template;
- generated settings/options should trace to their runtime-consumed interface;
- baked files should trace to exactly one resource pack and baker;
- side-specific metadata should agree where a contract is paired;
- removed IDs need migration and compatibility review;
- unrelated mass churn usually indicates a path, ordering, line-ending, toolchain, or non-determinism problem.

Generated artifacts should be deterministic for the same inputs. Run the generator twice or use its `--check` mode to prove this before committing.

## Recovery

| Failure | Recovery |
|---|---|
| Generated source does not compile | Fix the source tag/template or project registration, reconfigure, then rebuild |
| Script compiler and runtime disagree | Ensure both use the same config, Engine revision, and fresh generated metadata |
| Formatter changes `T?`, a cast/template form, or a named argument | Use the Engine-aware wrapper from [AngelScript Style and Refactoring](../scripting/style-and-refactoring.md), not raw clang-format |
| Metadata sides disagree | Fix paired declarations and rebake both sides |
| Incremental resources stay stale | Run `ForceBakeResources`; inspect pack selection and baker dependency tracking |
| Documentation `--check` fails | Run the named generator with `--write`, then inspect why source changed |
| Contract diff reports a break | Restore compatibility or add the exact reviewed disposition; do not hide the model delta |
| Site/search/AI output changes unexpectedly | Regenerate canonical pages first, then delivery artifacts in dependency order |

## Update discipline

Every Engine or embedding-project update is a generation-bearing change. Record old/new revisions, audit the complete range, identify affected generated layers, regenerate in dependency order, and update owning docs/tests in the same work. A green native build alone is not evidence that scripts, resources, metadata, documentation, or compatibility outputs are current.
