---
layout: default
title: AngelScript Style and Refactoring
locale: en
document_id: angelscript-style
permalink: /Docs/en/how-to/scripting/style-and-refactoring.html
---

# AngelScript Style and Refactoring

> Engine-owned documentation. This guide defines the reusable source, formatting, module, and refactoring contract supported by the current FOnline compiler, formatter, CoreScripts, and tests. An embedding game owns its domain vocabulary, module catalog, generated project formats, gameplay architecture, and migration policy.

## Contract status

This is a `current-revision` guide, not a promise that every historical FOnline project already follows these rules. Normative claims are derived from the current Engine source and tests. `Source/Scripting/AngelScript/CoreScripts` supplies maintained examples, while Last Frontier and FOnline TLA are comparison evidence only.

Use the [scripting runtime explanation](../../explanation/scripting-runtime/), [lifecycle and concurrency guide](lifecycle-and-concurrency.md), [nullability contract](../../contributing/coding-contracts/nullability.md), and [generated content workflow](../build/generated-content.md) for their deeper owning contracts. This page owns the route from an authored `.fos` change to a reviewable, behavior-preserving result.

## Scope and ownership

Use this guide when adding, moving, formatting, or refactoring Engine or project AngelScript. It separates four concerns:

1. source layout and formatting that Engine tools can enforce;
2. module, side, attribute, mutable-state, and nullability rules enforced by compilation or validation;
3. generated or compatibility-sensitive names whose owner must be changed first;
4. project policy that must not be presented as a universal Engine rule.

The Engine can define the compiler and formatter contract. It cannot choose a game's comment language, gameplay terminology, service decomposition, persistent schema, content keys, test scenes, or acceptance thresholds.

## Fast convention

Before changing reusable or project script code:

- give each authored file one primary namespace matching the file stem, such as `Time.fos` and `namespace Time`;
- keep first-line `Sort N` source ordering separate from runtime `[[ModuleInit(priority)]]` ordering;
- keep one clear responsibility in that namespace and call other modules explicitly as `Namespace::Function()`;
- isolate target-specific declarations with `#if SERVER`, `#if CLIENT`, or `#if MAPPER`; for example, the server module has `SERVER=1`, while `#ifdef SERVER` is true on every side because all side macros are always defined as `0` or `1`;
- keep authoritative mutation on its owning side and mutable state on an owning engine or entity object;
- use the exact function and declaration attributes required by the dispatcher or generator;
- enter dispatcher-owned attributed functions through the dispatcher route;
- keep module globals const-only by default: `Script.MutableGlobalsAllowedNamespaces` is a narrow compatibility escape hatch for explicitly owned legacy namespaces, not permission for unowned mutable state;
- write nullable handles as `T?`, narrow before use, and use a non-null type when absence is not part of the contract;
- edit authored inputs and regenerate derived `.fos` files instead of patching generated output;
- classify the change as mechanical, structural, behavioral, or contract work; use small, narrow batches and add migration/compatibility proof for contract work;
- run `python BuildTools/buildtools.py format-source`, compile every affected side without warnings, and run the narrowest test that observes the behavior;
- keep the module catalog, comment language, game vocabulary and architecture, project-generated formats, persistence migrations, fixtures, and gameplay acceptance policy in the embedding project. The Engine does not own a game's module catalog merely because it supplies the compiler or formatter.

A complete Engine/project boundary summary must state both rules that are easy
to lose in compression: dispatcher-owned attributed functions are entered only
through their dispatcher route, and generated project formats, persistence
migrations, plus gameplay acceptance remain project policy.

The namespace-to-file rule is an Engine convention and retrieval aid, not AngelScript grammar. A generated or compatibility-owned exception should be pinned in its generator or validator rather than weakening the default.

## How scripts become a module

### File discovery and ordering

The backend receives the configured script files, reads each file's first line, and looks for `Sort N`. Missing directives use sort value `0`. It then performs a stable ascending sort by numeric value and, for equal values, by filename stem. The generated root source includes every resulting file.

This ordering can affect preprocessing and declaration visibility. Keep `Sort N` on the first line when it is needed, treat an existing value as behavior-bearing, and do not use it as a substitute for explicit lifecycle ordering. `[[ModuleInit(priority)]]` owns runtime module-initializer order; the [lifecycle guide](lifecycle-and-concurrency.md) owns that contract.

Projects normally configure script inputs through their Engine integration. Authored modules do not need a hand-maintained include graph. A manual include or sort change is therefore a structural change until every affected side compiles and the relevant startup path passes.

### Side-specific compilation

FOnline preprocesses and compiles a separate module for each requested side. All three side macros exist in each compile and have values `0` or `1`:

- server: `SERVER=1`, `CLIENT=0`, `MAPPER=0`;
- client: `SERVER=0`, `CLIENT=1`, `MAPPER=0`;
- mapper: `SERVER=0`, `CLIENT=0`, `MAPPER=1`.

Use value tests such as `#if SERVER`. `#ifdef SERVER` is true on every side and does not isolate server code. A balanced preprocessor guard proves only lexical structure; compiling each affected side proves that its declarations, attributes, and calls are valid.

### Namespace and file ownership

The maintained CoreScripts use one top-level namespace matching each `.fos` stem:

```angelscript
// Time.fos
namespace Time
{

timespan Seconds(int value)
{
    return timespan(value, SecondsPlace);
}

}
```

This gives contributors, diagnostics, search, and retrieval systems the same route from `Time::Seconds` to `Time.fos`. A file may contain target guards or private implementation helpers, but its primary public ownership should remain obvious.

### Dependency and compatibility boundaries

Cross-namespace calls should name the owner. Move a helper only when its behavior and callers belong to the receiving module, not merely to satisfy a size or ordering preference.

Treat the following as contracts until the owning source and tests prove otherwise:

- first-line sort directives and side guards;
- function attributes and dispatcher signatures;
- `///@` declaration metadata;
- remote-call subsystem and method names;
- reflection strings, invoke names, property names, enum values, serialized identifiers, and content keys;
- generated filenames and generator inputs.

A local rename is mechanical only when none of these surfaces can observe it.

## Formatter contract

### Supported command and version

`Source/Scripting/AngelScript/CoreScripts/.clang-format` is the Engine-owned layout definition. The current contract uses clang-format 20, four spaces, no tabs, a 160-column limit, next-line function braces, same-line control braces, inserted braces for control-flow bodies, no indentation for the outer namespace body, no include sorting, and no comment reflow.

From the Engine repository run:

```bash
python BuildTools/buildtools.py format-source
git diff --exit-code
```

`format-source` formats supported files under Engine `Source`; it does not discover an embedding project's separate script tree. A project must use its documented formatter for project-owned `.fos` files and run the Engine command separately when the submodule source changed.

BuildTools uses `FO_CLANG_FORMAT` when set, otherwise searches for `clang-format-20` and then `clang-format`, and rejects a binary whose major version is not 20. Engine CI reruns the wrapper and requires an empty diff.

### What the wrapper repairs

Raw clang-format parses `.fos` as C++ and can separate AngelScript-specific tokens. The wrapper masks strings, character literals, line comments, and block comments, then repairs at least these source forms after formatting:

- nullable declarations such as `Critter? target`;
- nullable parameters, return types, casts, and template arguments;
- nullable array elements such as `Item?[]`;
- named arguments such as `Create(count: 2)`.

Use the wrapper rather than raw clang-format. A project formatter may cover additional authored formats, but its `.fos` path must preserve these semantics or delegate to equivalent Engine-backed logic.

### Encoding, line endings, and EOF

The wrapper reads and writes UTF-8, removes a UTF-8 BOM, preserves whether an existing file uses LF or CRLF, and leaves exactly one line terminator at EOF. Line-ending normalization alone does not count as a semantic formatting difference.

Do not impose a project-wide line-ending rule through this guide. Repository attributes and the project formatter own that policy. The Engine guarantee is preservation of the input convention for a formatted file.

### What formatting does not prove

Formatting does not prove namespace ownership, side authority, balanced behavior across roles, attribute use, callback routing, mutable-global policy, generated ownership, nullability flow, serialization compatibility, or runtime behavior. Review formatter output before treating the mechanical phase as complete.

## Source layout

### Module ordering inside a namespace

Keep related declarations together and follow the surrounding module's order. Put public ownership ahead of cosmetic uniformity: do not reorder initialization, registration, callbacks, or declaration metadata without checking whether the consumer observes source order.

Avoid a universal gameplay-helper order in Engine documentation. CoreScripts are a reusable baseline, while a game may group domain declarations differently and pin that structure in project checks.

### Names and comments

The reusable baseline is intentionally small:

- namespace and type names use `PascalCase`;
- public function names follow the surrounding Engine script API;
- local names expose intent and follow the surrounding module;
- compatibility names change only through their owning API or migration process;
- comments explain intent, invariants, ownership, or a non-obvious constraint instead of restating the next statement.

The Engine does not mandate a natural language for game comments, a file-header template, one vocabulary for NPC or item variables, or a universal maximum module size.

### Mutable state and globals

After module build, the backend rejects every mutable module-level global whose namespace does not match a configured prefix in `Script.MutableGlobalsAllowedNamespaces`. Const globals are accepted. The default empty list therefore enforces const-only module globals.

Prefer state owned by the engine instance, entity, or an explicit lifecycle object. If a legacy project temporarily needs mutable globals, allow only the narrowest namespace prefix, record the owner and removal condition, and test startup on every side. Prefix matching means a broad entry can admit more namespaces than its author intended.

An allowlist is a compatibility escape hatch, not evidence that a global cache or service is correctly scoped. Module initialization and the global freeze boundary remain owned by the [lifecycle guide](lifecycle-and-concurrency.md).

## Nullability and invariants

Use `T?` only when absence is part of the contract. Bind or guard the nullable value before dereferencing it. Use a normal branch for expected absence and `verify(...)` for a violated invariant; keep the message a fixed description and pass dynamic values as context arguments.

```angelscript
Critter? target = Game.GetCritter(targetId);
if (target == null) {
    return;
}

ApplyEffect(target);
```

Do not scatter defensive checks around non-null values or freshly supplied entity arguments. Revalidate retained entity handles after an actual lifetime boundary such as `Yield`, a callback that can destroy or detach the entity, or storage beyond the current call. The [nullability contract](../../contributing/coding-contracts/nullability.md) owns narrowing details; the [lifecycle guide](lifecycle-and-concurrency.md) owns suspension and synchronization.

## Attributes and callback ownership

Attributes are compiler and dispatcher contracts, not decorative labels. The pipeline preprocesses source, extracts and binds attributes, validates their use and special forms, validates callback and remote-call contracts, and only then emits bytecode.

### Direct-call blockers

The built-in direct-call-blocking set is `Event`, `TimeEvent`, `AnimCallback`, `PropertyGetter`, `PropertySetter`, `ServerRemoteCall`, `ClientRemoteCall`, `AdminRemoteCall`, `ItemTrigger`, `ItemStatic`, `ModuleInit`, and `InvokeEntry`. A script function bearing one of these attributes must be entered through its owning dispatcher or API, not called as an ordinary helper.

Projects may add dispatcher-owned attributes through `Script.ExtraDirectCallBlockingAttributes`. `Script.AttributedFunctionDirectCallAllowedNamespaces` can exempt caller namespaces by prefix for compatibility. Keep either list narrow and transitional; extracting a normal helper is preferable when behavior genuinely needs both direct and dispatched entry.

The validator also checks callback API ownership, including event subscription, time-event APIs, animation callbacks, and property getter/setter registration. An attribute with the right spelling but the wrong route still fails the contract.

### Marker propagation

Any function attribute not classified as direct-call-blocking is treated as a marker by the call validator. If a function calls a marked function, the caller must carry the same marker. `[[Async]]` is the common example: propagation makes the transitive suspension boundary visible instead of hiding it inside a helper.

Do not remove or add a marker as formatting cleanup. Compile all callers and follow the owning lifecycle or attribute documentation before changing the call graph.

### Generated declarations

Declaration comments such as `///@ Event`, `///@ RemoteCall`, `///@ Property`, and `///@ Enum` feed generated metadata and script declarations. Change the authored declaration, regenerate in dependency order, inspect generated diffs, and then compile the affected sides.

Function attributes and `///@` declarations solve different parts of the pipeline. Do not replace one with the other because their names appear related.

## Generated script ownership

Every generated `.fos` file needs an upstream owner: Engine metadata/codegen, a project GUI generator, a content generator, or another declared tool. Fix that owner and regenerate. A hand edit to derived output is not a completed change because the next generation pass will erase it.

Before editing an unfamiliar file, check repository instructions, generated headers, build tasks, and the [generated content workflow](../build/generated-content.md). When a generated failure is visible only in derived code, retain enough source-located evidence to repair the input or generator.

## Refactoring classification

Classify the change before editing. The highest-risk touched surface determines the proof required for the batch.

### Mechanical changes

Examples: formatter output, comment correction, and a local rename with no reflected or serialized use.

Minimum proof: run the owning formatter, inspect the diff, and compile the affected scripts without warnings.

### Structural changes

Examples: helper extraction, function move, namespace move, file split, side-guard regrouping, or sort change.

Minimum proof: compile every affected side and run focused tests for module initialization, callbacks, and the moved call paths. Verify that generated and reflected ownership did not change accidentally.

### Behavioral changes

Examples: condition, ordering, state mutation, callback result, authority decision, or lifetime behavior.

Minimum proof: focused runtime or gameplay tests plus the relevant integration path. Describe the behavior change separately from cleanup.

### Contract changes

Examples: attribute, metadata name or type, remote call, property, enum, persisted identifier, content key, or generated schema.

Minimum proof: regeneration, compatibility classification, migration or disposition when required, compile and bake gates, and runtime tests on both ends of the contract.

## Safe batch workflow

### Establish ownership

Read the nearest repository instructions and owning docs. Identify whether each touched file is authored or generated, which side owns the behavior, and whether names cross process, persistence, reflection, or content boundaries.

### Capture the baseline

Run the narrowest existing compile or test before a broad change. Record known failures rather than silently treating them as refactor output. For a migration, inventory temporary allowlists, generated files, and compatibility names before changing them.

### Change one class

Keep mechanical cleanup separate from structural, behavioral, and contract changes when practical. Small batches make a failed compile, changed callback order, missing side symbol, or compatibility drift attributable to one decision.

Do not bulk-delete commented code or rename strings, reflection tokens, metadata identifiers, serialized fields, or content keys without determining ownership first. Version control preserves old text; it does not prove that disabled code is obsolete or that a name is non-contractual.

### Regenerate and format

Run every owner generator whose input changed, then the correct formatter for each authored tree. Never format a generated file as a substitute for fixing its generator unless the generator contract explicitly includes that formatting pass.

### Compile each side

Compile SERVER, CLIENT, and MAPPER wherever the changed file or metadata can reach them. Treat warnings as failures. A common-only edit is not proven by compiling one convenient role.

### Prove behavior

Run the narrowest Engine unit, project script test, scene, or integration route that observes the changed behavior. When a cleanup reveals a probable bug, either prove and fix it as a separately described behavioral change or record a precise follow-up.

## Validation matrix

| Change | Required evidence |
| --- | --- |
| Engine CoreScript source | Engine `format-source`, clean diff, affected script compiles, focused Engine test |
| Project-authored `.fos` | Project formatter, `CompileAngelScript` or equivalent for all roles, focused project test |
| Side guard or sort directive | Compilation of every affected side, startup or initialization test where order matters |
| Mutable global policy | Startup compile/bake with exact namespace settings, lifecycle test, documented removal condition for any exception |
| Function attribute or callback route | Attribute validation, owning dispatcher path, direct-call or propagation regression where relevant |
| `///@` metadata or generated declaration | Regeneration, generated diff, compile/bake, compatibility review |
| Persisted, reflected, remote, or content identifier | Contract disposition or migration plus producer/consumer runtime tests |
| Broad refactor | Repeat the applicable gates for each small batch; finish with the project's aggregate validation |

Engine CI runs `buildtools.py format-source` and `git diff --exit-code`. `BuildTools/tests/test_docs_angelscript_style.py` pins the documentation route, compiler ordering and side macros, mutable-global and attribute settings, CoreScripts namespace/guard/encoding rules, formatter repairs, external evidence, localization, and workflow inclusion.

## Failure diagnosis

| Symptom | First boundary to inspect |
| --- | --- |
| Server declaration appears on client or mapper | Replace `#ifdef SIDE` with `#if SIDE`; inspect guard nesting |
| Symbol appears or disappears after file move | First-line `Sort N`, equal-sort filename ordering, namespace qualification |
| Mutable global rejected after module build | Owning state object and the exact `MutableGlobalsAllowedNamespaces` prefix; do not broaden blindly |
| Direct call to attributed function rejected | Enter through the owning dispatcher or extract a normal helper |
| Caller is missing `[[Async]]` or another marker | Propagate the marker through the real call chain and review the lifecycle boundary |
| Formatter separates `?` or named-argument `:` | Use the Engine-aware wrapper and confirm clang-format major 20 |
| Generated `.fos` change disappears | Edit the generator input or generator, then regenerate |
| One role compiles while another fails | Compile the failing side with its actual `0`/`1` macros and generated declarations |
| Refactor changes startup behavior | Separate file sort, module-init priority, callback registration, and runtime event order |

## Project policy boundary

An embedding game must document the parts the Engine cannot choose:

- module and domain catalog, gameplay architecture, and authority decisions;
- comment language, terminology, headers, and local naming additions;
- generated script names, generator commands, and project-only authored formats;
- serialized identifiers, migration approvals, and compatibility windows;
- test harness, fixtures, launch profiles, and gameplay acceptance;
- formatter coverage outside Engine `Source`;
- temporary namespace exceptions and their removal plan;
- file-size, commented-code, or other quality-ratchet thresholds.

Last Frontier's narrow test-only global exceptions and project formatter are useful current evidence. TLA's broad production mutable-global allowlist and ongoing refactoring log are migration evidence, not an Engine recommendation. Neither project is a normative dependency of this guide.

## Maintenance triggers

Re-audit this page in the same change when any of these owners changes:

- script discovery, first-line sorting, side macros, module construction, or bytecode pipeline in `AngelScriptBackend.cpp`;
- direct-call blockers, marker propagation, callback validation, or special attributes in `AngelScriptAttributes.*`;
- `Script.MutableGlobalsAllowedNamespaces`, `Script.AttributedFunctionDirectCallAllowedNamespaces`, or `Script.ExtraDirectCallBlockingAttributes`;
- `.fos` patterns, clang-format discovery/version checks, repair logic, encoding, line-ending, or EOF behavior in `BuildTools/buildtools.py`;
- the CoreScripts `.clang-format` contract or maintained CoreScript layout;
- generated declaration ownership or the lifecycle/nullability contracts linked from this guide;
- external project evidence used to separate reusable rules from project policy.

Run the focused documentation test, localization check, snippet check, site generation check, and standalone documentation validator. If Engine `.fos` behavior changed, also run the formatter, compile every affected side, and execute the owning native/script tests.

## Source paths inspected

- `BuildTools/buildtools.py`
- `.github/workflows/validate.yml`
- `Source/Common/Settings.inc`
- `Source/Common/ScriptSystem.cpp`
- `Source/Scripting/AngelScript/CoreScripts/.clang-format`
- `Source/Scripting/AngelScript/CoreScripts/*.fos`
- `Source/Scripting/AngelScript/AngelScriptBackend.cpp`
- `Source/Scripting/AngelScript/AngelScriptAttributes.cpp`
- `Source/Scripting/AngelScript/AngelScriptAttributes.h`
- `Source/Tests/Test_AngelScriptAttributes.cpp`
- `Source/Tests/Test_AngelScriptBaker.cpp`
- `BuildTools/tests/test_docs_angelscript_style.py`
