# FOnline Engine — Agent Instructions

This is the reusable Engine's maintainer entry point. Read it fully before
Engine work, then open the owning guide. Human entry: [README.md](README.md).
Documentation: [English index](Docs/en/index.md), [Russian mirror](Docs/ru/index.md).
Keep this file concise; detailed contracts belong to their existing owners.

## Working agreement

1. Confirm checkout/branch/diff/scope; preserve unrelated work. Re-anchor from
   source/plan/diff after handoff. Reports are evidence, not authority.
2. Read owners, validators and callers: source proves current behavior, owner
   requirements define intended behavior. Keep game policy/content/tooling outside
   Engine; fix reusable defects here instead of adding project workarounds.
3. Finish authorized verification; ask for material decisions or missing access.
   Establish every required source before multi-source synthesis.
4. No stage/commit/push/publication/deployment without action/scope authority. Past
   permission does not cover later tasks. Requested history integration may create
   its necessary merge. Stay on the requested branch; no generic-workflow branch churn.

**Language:** commits/prepared messages, PRs/issues, comments, diagnostics, plans and
release notes are English. Russian docs translate English; chat language does not
change this. Continue existing documents in their own language. No agent signatures.

**Artifacts:** task screenshots/logs/scripts/downloads/drafts/worker materials use
`Workspace/<task-id>/`; follow an embedding project's explicit artifact root.
Standard tool outputs retain their locations. No credentials/private raw sessions
in versioned docs; sanitize evidence.

## History, compatibility and documentation

- **Rebase unpublished commits, preserve published history.** Fetch `origin/master`
  before an authorized update, then rebase only unpublished local work. A branch's
  upstream or earlier pushes do not make its newer commits published. Merge only
  diverged published histories. No reset/amend/rebase of pushed commits, force-push
  or remote branch deletion. Before an authorized push, verify
  `git merge-base --is-ancestor <destination-remote-tip> HEAD`.
- Revision updates follow [reconciliation](Docs/en/contributing/documentation/index.md#revision-update-reconciliation):
  old/new SHAs, complete incoming range, owners/models and validation before dropping
  safety stashes. Check the embedding gitlink and Engine HEAD separately.
- Every master update follows [versioning](Docs/en/how-to/release/versioning.md#every-master-update):
  derive the next minor from the latest published master, current UTC year,
  `YEAR.MAJOR.MINOR-dev`, dated EN/RU changes and exhaustive migration notes.
  All change types count. Release branches need an owner command; their cut freezes
  and advances patch. Local preparation does not authorize publication/commits.
- **Current API, preserved meaning:** valid inputs/outputs/units/IDs/defaults/side
  effects retain meaning. Replacements get distinct names/types/keys/formats and
  remove old surfaces. No deprecated aliases, shims, permissive parsing or masking;
  unmigrated consumers fail explicitly. [ADR-0002](Docs/en/contributing/decisions/0002-public-api-stability-contract.md)
  and [migration records](Docs/en/how-to/release/versioning.md#exhaustive-migration-record) own the details.
- Serialized changes update `///@ MigrationRule`; network/runtime compatibility
  changes bump the current central marker in `Source/Common/Common.h`. Never copy
  a historical number or accept mixed metadata bakes. Business migrations stay project-owned.
- Temporary old-build/platform/dependency restrictions use one ID/date across
  `FO_TEMPORARY_COMPAT`, `[TemporaryCompat]` or CMake `TemporaryCompat`; run
  `BuildTools/temporary_compat.py`. This never authorizes an old API bridge.
- Update owning docs and EN/RU/generated parity in the same change. Engine docs
  stand alone; projects may be examples, never normative validation dependencies.
  Follow [documentation maintenance](Docs/en/contributing/documentation/).

## Native review checklist

- **Module structure:** a standalone named native module is a `.h` + `.cpp` pair,
  both registered in its owning source list, even if it currently contains only
  passive data. Pure template/constexpr/umbrella headers are exceptions. Keep
  engine-owned source inventories unconditional; self-guarded modules own their
  `#if FO_*` and are included normally. Third-party feature dependencies may be gated.
- **Types/names:** passive structs have no user constructors, methods, invariants or
  `final`; behavioral types are encapsulated classes. Static-only utilities are
  namespaces unless class identity is required (`friend class safe_alloc`). Essentials
  uses snake_case, higher layers PascalCase; preserve documented exceptions/module filenames.
  Namespace helpers follow declaration order; moving a static-only class to a
  namespace may require forward declarations, and namespaces cannot be friends.
- **Layout:** class declarations/forward declarations precede implementations;
  high-level orchestration precedes helpers. Within a class: construction/destruction,
  `[[nodiscard]]` informational accessors, then mutators ordered from initialization to teardown.
  Keep existing dedicated families together. Do not hoist unrelated declarations
  or put implementation into a forward-declaration block.
- **Spacing:** separate semantic stages inside functions and lambdas with blank lines.
  Validation, resource acquisition, ownership transfer, conversion, callbacks and
  result handling form separate groups. Keep a declaration with its immediate
  initialization/validation; a formatter cannot infer these groups.
  Surround complete control-flow statements with blank lines; adjacent statements
  of the same construct may form a run. Preprocessor directives do not change this
  spacing. Put a blank line before a comment and none between it and its following
  block. Edited source ends with
  exactly one trailing blank line. Reuse MIT headers and `#pragma once`.
  Manually review changed semantic groups and control-flow blocks in the final diff;
  neither a formatter nor `git diff --check` verifies these spacing rules.
- **Comments:** explain only what code cannot express: intent, units, ownership,
  lifetime or a non-obvious constraint. One line normally, two maximum; longer
  explanations belong in the owning doc, with a short link. No task/issue/PR IDs,
  dates, version/release markers, author/history/benchmark bookkeeping, temporary-plan
  links or narration. Non-code links go to the owning maintained document.
  Only the concluding sentence omits its period; earlier sentences keep punctuation.
  Preserve licenses and commented code. Short structural headings may aid navigation;
  capitalize prose without changing symbols. Do not narrate signatures, parameters
  or visible steps. Enum trailing descriptions and `// field: ...` value-layout
  metadata follow [metadata ownership](Docs/en/reference/metadata/index.md#source-owned-symbol-descriptions).
- **Locals:** fixed-width numeric aliases and `size_t`; explicit types for primitives,
  `string`, `hstring` and other simple values. No redundant top-level local `const`;
  pointee/referent constness and `constexpr` remain. Use `numeric_cast` for numeric
  conversions. See [local variables](Docs/en/contributing/coding-contracts/local-variables.md)
  for justified suppression and use-after-move checks.
- **Pointers:** Engine `ptr`/`nptr` borrows, `unique_*`/`refcount_*` owners and own
  `shared_ptr`/`weak_ptr`; no `std::` owners. Entities use pointers, not references.
  Informal “raw pointer” means a borrow; literal `T*` needs an explicit documented
  ABI/low-level boundary. Wrap ingress, unwrap at final handoff. No `.as_ptr()` where
  implicit conversion applies; deduction/capture/owner-move may need it. Full rules:
  [smart pointers](Docs/en/contributing/coding-contracts/smart-pointers.md).
- **Memory/callables/platform:** use Engine containers, strings, `safe_alloc`,
  move-only `function` (copyable only for a real copy contract), `random_generator`,
  `coarse_sleep`/`precise_sleep` and OS wrappers. No `std::` substitutes or bare C
  allocation. [Essentials](Docs/en/reference/native/essentials.md) owns interop
  (`make_stream_string`, `fs::make_path`, unqualified `getline`) and the narrow
  low-layer/foreign-ABI exceptions, including the `StackTrace.h` `std::function` hook.
- **Layering:** `Essentials.h` defines dependency order, including implementation
  ownership, not just includes. A module may depend only on earlier modules; do
  not reorder the aggregate or hide a later implementation behind an early declaration.
  Consumers outside Essentials include `Common.h`. STL includes belong only in
  `BasicCore.h`. Check `BuildTools/tests/test_essentials_layering.py`.
- **Helpers/macros:** static file-local functions, no unnamed namespaces. Put a
  module-prefixed public free surface in its namespace. No redundant function `extern`;
  preserve variable declarations/`extern "C"`. Owned macros use `#if FO_*` values.
  `ignore_unused` takes objects; ignored calls use `(void)Call()`.
  Move a module's whole free surface, including types, into its namespace; vocabulary
  that needs no module prefix stays bare. Do not rename `logging::` to colliding `log`.
- **Profiling:** first-statement `FO_TRACE_ZONE(Category)` only for meaningful work
  units, variable cost or blocking boundaries. No zones in headers, lambdas, tests,
  trivial hot helpers, profiler internals, `FO_SCRIPT_API` exports or export-only
  helpers; generated bindings already open their Script zone. Use the subsystem
  category and [profiling policy](Docs/en/how-to/quality/profiling.md#placing-zones).

## Invariants, synchronization and lifetime

- Fix the failing owner; do not hide wrong lower-layer results with caller guards,
  swallowed exceptions or fabricated defaults. Invariant checks are predicates:
  perform reads/writes/acquisitions/out-parameter work first into named values.
  Use fixed meaningful exception/verification messages plus context arguments.
- Allocation terminates on OOM; never build allocation-only rollback. Derive the
  function's actual exception guarantee. Validate before observable mutation,
  commit authoritative state before derived caches, and use RAII/no-throw rollback
  for genuine recoverable partial changes. `None` is a defect, not an accepted tier.
- Post-mutation corruption-only checks use `FO_STRONG_ASSERT`, not a swallowable
  throw. Entity create/destroy is throw-as-signal: events may legitimately destroy
  or relocate an entity while leaving valid state. Do not impose blanket rollback.
- Throw only `std::exception`-derived types. A catch-all beside ordinary exception
  recovery is `FO_UNKNOWN_EXCEPTION()`; no log-and-continue. Documented no-throw
  teardown/C-ABI/reporting boundaries have their own narrow handling rules.
- `NoThrow` classification does not justify adding `noexcept`. Use it only for a
  deliberate no-throw contract such as moves/swap, unwind callables, C-ABI callbacks
  or documented primitives. Do not repeat implicit destructor `noexcept`; write
  explicit `noexcept(false)` only where throwing destruction is the contract.
  Full owner: [exception safety](Docs/en/contributing/coding-contracts/exception-safety.md).
- Per-engine state belongs to owning instances, not singletons or mutable statics.
  Compile-time constants and pure file-local functions are permitted. A thread-local
  slot requires proved engine-partitioned threads; a process-wide atomic requires
  no cross-engine semantic identity. An existing global is not a precedent.
- Ordinary server script calls supply complete native entity cover, including
  transitive existing dependencies. `ValidateEntityAccess` checks it;
  `EnsureEntitySynced` may retain an already-covered entity's own lock idempotently,
  without releasing caller cover or acquiring an unrelated missing dependency.
  Fresh-parentless capture is a private registration boundary. Native callees must
  not repair missing cover with `SyncEntity`/`SyncEntities`. Explicit script sync
  primitives and the `Server.SingleThreadedLogic` opt-out retain their documented
  contracts. Read [lifecycle/concurrency](Docs/en/how-to/scripting/lifecycle-and-concurrency.md)
  and [Managed C#](Docs/en/how-to/scripting/managed-csharp.md) before changing this path.
- Events can re-enter and destroy entities synchronously. Snapshot iteration with
  `copy_hold_ref`, revalidate retained entities after callbacks, and fire events only
  after non-revocable state is consistent. An async boundary also requires renewed
  lifetime/identity/cover proof; a convenient sample caller is not the owning policy.
- Settings are immutable and addressed as `Group.Name`; runtime state lives on the
  owning subsystem. Test override APIs are test-only. Optional entity capabilities
  with related fields should use [property components](Docs/en/how-to/content/prototype-format.md#property-components),
  with full stored-name migration when moving persisted fields.
- Managed sources use their established file-scoped namespace/using order and
  PascalCase members, including private fields; generated internals and native value
  types retain documented spellings. `[CallableByEngine]` methods are `internal`.
  Cover attributes do not lock; apply the owning analyzer and dispatcher contract.
  Preserve checked arithmetic and restrict `unchecked` to intentional wrapping.

## Validation Routing

Tool owners: [Mapper interactive manual](Docs/en/how-to/tools/mapper-interactive.md),
[Mapper automation](Docs/en/how-to/tools/mapper.md), and
[Animation and Particle Viewers](Docs/en/how-to/tools/animation-particle-viewers.md).

Build and codegen finish warning-free; fix the source instead of suppressing a
diagnostic or weakening a gate. Use the embedding project's authoritative targets
and [Engine testing](Docs/en/contributing/testing/) for the actual configuration.

| Change | Required route |
| --- | --- |
| Native implementation/binding | Build/run affected generated `<ProjectDevName>_UnitTests` / `RunUnitTests`; cover changed behavior and negative controls. Run applicable pointer/allocator/exception/local-variable audits supplied by the integration. |
| Build/CLI/package/formats/bakers/extensions | Update the owning `BuildTools/*Interface.json` and guide, regenerate/check model/reference, run focused tests and [aggregate contract diff](Docs/en/contributing/contract-change-management.md), then exercise the affected real project/preset/bake/package. |
| Scripts/runtime/lifetime/analyzers | Follow the [AngelScript style and refactoring guide](Docs/en/how-to/scripting/style-and-refactoring.md) for authored `.fos` changes. Run affected backend/native tests and compile every changed backend/role; keep project-only policy outside Engine. |
| Presentation | Visible/audible interaction on every claimed backend/platform after native tests and bake. High-level GUI libraries/layouts/generators remain project-owned. |
| Documentation/instructions | `python BuildTools/docs_prepare.py`, focused owning doc tests, `python BuildTools/tests/test_docs_validate.py`, `python BuildTools/docs_validate.py`, `git diff --check`. Public route/site changes also follow the [site publication gates](Docs/en/contributing/documentation/site-publication.md). |

Use [generated-content ordering](Docs/en/how-to/build/generated-content.md).
Generated site/AI-delivery indexes and reports are ignored outputs, not authored
instructions or files to stage. Never hand-edit generated public contract models,
reference pages or delivery output in place of their owner/generator.

Track owned processes and output directories. Do not overlap writers/consumers of
one bake; wait for other tasks rather than killing by image name or deleting lock
files. Stop your own process tree and verify exit/cleanup. A command start, partial
pass count or background run is not a completed gate. Keep runtime, full-suite,
performance, platform and publication evidence separate; finish the promised scope
and record actual limitations. Update the plan and owning docs before reporting done.

Find additional owners through [the source tree](Docs/en/contributing/source-tree/)
and [public contracts](Docs/en/reference/public-contract/index.md). Verify exact
current names; do not guess from an old file or neighboring backend. Report the
final diff/checks/blockers and leave local edits unless publication was authorized.
