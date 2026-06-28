# Smart Pointers

> Engine-owned documentation. This page defines the native C++ pointer vocabulary in `Source/Essentials/SmartPointers.h`: ownership, nullability, migration rules, and validation expectations.

## Purpose

The engine uses small pointer wrappers to make pointer contracts visible at the type level. The long-term convention matches the script-side nullability model: absence must be explicit. A non-null type means `null` is not a normal state; a nullable type uses the `n*` spelling.

The migration from the older `raw_ptr` name is intentionally staged. `raw_ptr<T>` and `nullable_raw_ptr<T>` are removed from the engine-owned API; use `ptr<T>` and `nptr<T>` instead. `nptr<T>`, `unique_nptr<T>`, and `refcount_nptr<T>` are separate nullable wrapper types. The default build enables the strict non-null contract for ordinary `ptr<T>`, `unique_ptr<T>`, and `refcount_ptr<T>` spellings; nullable state belongs in the matching `n*` type.

## Pointer vocabulary

| Meaning | Non-null spelling | Nullable spelling |
| --- | --- | --- |
| Non-owning borrowed pointer | `ptr<T>` | `nptr<T>` |
| Unique owning pointer | `unique_ptr<T>` | `unique_nptr<T>` |
| Intrusive refcount owning pointer | `refcount_ptr<T>` | `refcount_nptr<T>` |
| Unique array pointer | `unique_arr_ptr<T>` | Future `unique_arr_nptr<T>` if needed |
| Custom-deleter unique pointer | `unique_del_ptr<T>` | `unique_del_nptr<T>` |

`raw_ptr<T>` / `nullable_raw_ptr<T>` are legacy spellings. New engine-owned code must use `ptr<T>` or `nptr<T>`.

## Contracts

`ptr<T>` is a borrowed, non-owning pointer. In the final strict contract it is non-null in usable state, has no normal `nullptr` assignment/check path, and must not be used to model optional state.

`nptr<T>` is a borrowed nullable pointer. Use it for lookups, current/active/selected state, backend handles that can be absent, and API results where absence is a normal outcome.

Use `nptr<T>::as_ptr()` after a local null check when a nullable borrowed value is explicitly narrowed to a required borrowed `ptr<T>`. The method asserts the non-null invariant and keeps the narrowing visible at the call site.

**Narrowing names.** A pointer's name form follows its *type*, so a raw value, a nullable wrapper, and a checked non-null view never collide in one scope. The clean domain name is reserved for the non-null `ptr<T>` — the value the body actually works with; the `raw_` / `nullable_` prefixes mark boundary values that exist only to be checked and narrowed:

| Type | local / parameter (`snake_case`) | data member (`_camelCase`) |
|---|---|---|
| raw `T*` | `raw_target` | `_rawTarget` |
| `nptr<T>` (nullable wrapper) | `nullable_target` | `_nullableTarget` |
| `ptr<T>` (non-null) | `target` | `_target` |

```cpp
// nullable wrapper -> checked non-null
nptr<Critter> nullable_target = engine->GetCritter(id);
if (!nullable_target) {
    return;
}
ptr<Critter> target = nullable_target.as_ptr();   // body works with `target`

// raw C-ABI pointer -> checked non-null
static void Cleanup(sentry_options_t* raw_options) noexcept {
    if (raw_options != nullptr) {                  // check the raw pointer directly
        ptr<sentry_options_t> options = raw_options;   // clean name for the non-null ptr
        sentry_options_free(options.get());
    }
}
```

Do **not** invent per-call disambiguators such as `_lookup`, `_ref`, `_ptr`, or `_begin`, and **never copy an already-named nullable or raw pointer into a `nullable_`/`raw_` intermediate just to wear the prefix** — check and narrow it directly. The `nullable_` / `raw_` name belongs where the value is first produced (a parameter, lookup, cast, or C-API result), and the non-null `ptr<T>` narrowed from it takes the clean name. Bind a raw pointer straight to `ptr<T>` after the `!= nullptr` check — do not route it through an `nptr<T>` intermediate just to null-check it.

When a value is **provably non-null** — `std::string::data()` / `string_view::data()` (never null), `&object`, or an already-non-null result — bind it **straight to `ptr<T>` with no check and no `nptr<T>` intermediate**; adding a `!= nullptr` test there is dead code. And do not introduce *any* wrapper for a method-local pointer that never escapes the function and is used only for local address arithmetic — a `ptr<T>` (or a plain raw pointer for pure pointer math) is enough:

```cpp
ptr<const char> view_begin = _sv.data();        // data() is never null — no nptr, no check
ptr<const char> storage_begin = _s.data();
ptr<const char> storage_end = storage_begin.get() + _s.size();
if (view_begin < storage_begin || !(view_begin < storage_end)) { ... }
```

One exception: an **exported/ABI parameter whose name is fixed by an external contract** (or pinned in the smart-pointer-audit allowlist) keeps its name; narrow it to a `<name>_ptr` local rather than renaming the parameter.

The same naming applies to every narrowing form (`refcount_nptr<T>::as_ptr()`, `unique_nptr<T>::take_not_null()`, `shared_ptr<T>::as_ptr()`, …).

`unique_ptr<T>` owns one object in usable state. A moved-from object may be empty only until destruction or reassignment. If empty state is part of the normal object model, use `unique_nptr<T>`.

`unique_nptr<T>` owns zero or one object. Use it for lazily-created resources, optional owned state, and objects that are created after the owner is constructed or cleared before the owner is destroyed.

Use `unique_nptr<T>::take_not_null()` when a nullable owner has been checked or otherwise proved present and ownership must move into a `unique_ptr<T>`. The method asserts the non-null invariant and makes the narrowing visually reviewable.

Use `take_not_null(unique_del_nptr<T>&)` for the matching custom-deleter case when a checked nullable custom-deleter owner must move into `unique_del_ptr<T>`. The helper keeps the stored deleter with the transferred owner, so call sites do not need to manually release raw storage or rebuild deleters.

In strict builds, `unique_ptr<T>::release()` is rvalue-only and returns `ptr<T>` instead of raw `T*`. `unique_nptr<T>::release()` returns `nptr<T>`. Unwrap with `.get()` only at an explicit ABI, allocator, or adoption boundary.

Helpers that can fail a type cast or lookup while transferring unique ownership must return `unique_nptr<T>`. For example, `dynamic_ptr_cast<T>(unique_ptr<U>&&)` returns `unique_nptr<T>` because a failed cast is normal absence, not a valid `unique_ptr<T>` state.

`unique_del_ptr<T>` / `unique_del_nptr<T>` are custom-deleter owners used at external cleanup boundaries. Use `unique_del_ptr<T>` only after a `ptr<T>` or equivalent assertion proves the custom-owned object is present; it is a strict move-only non-null owner and rejects default/null construction in strict builds. Use the `unique_del_nptr<T>` spelling for stored state that can be empty, lazily initialized, moved-from, or reset.

`refcount_ptr<T>` owns an intrusive reference and is non-null in usable state. Copying increments the reference count; destruction decrements it.

`refcount_nptr<T>` owns an intrusive reference when present and can be empty. Use it for optional entity/view/current state and lookup results that need to keep the object alive when found.

Use `refcount_nptr<T>::as_ptr()` after a local null check when a nullable intrusive owner is explicitly narrowed to a required borrowed `ptr<T>` without changing ownership. Use `refcount_nptr<T>::take_not_null()` when a nullable intrusive owner has been checked or otherwise proved present and ownership must move into a `refcount_ptr<T>`. The ownership-narrowing method asserts the non-null invariant and adopts the already-held reference without adding another one.

Do not wrap ordinary nullable intrusive ownership in `optional<refcount_ptr<T>>`; keep normal absence in `refcount_nptr<T>`. Reserve `optional<refcount_ptr<T>>` for APIs that intentionally encode a second state beyond pointer absence, such as load-result contracts with a separate error flag. Handle those optional contracts at their owning boundary instead of using them as generic nullable-owner adapters, including script-ownership release helpers. The same rule applies to other wrappers: avoid `optional<ptr<T>>`, `optional<nptr<T>>`, `optional<unique_ptr<T>>`, `optional<unique_nptr<T>>`, and `optional<refcount_nptr<T>>`; use the direct wrapper vocabulary or a named domain result type.

`shared_ptr<T>` and `weak_ptr<T>` are `propagate_const` aliases around the standard shared ownership types. They can export explicit local borrows through `as_ptr()` after a guard/assert or `as_nptr()` when absence remains part of the current path. These methods do not change shared ownership; keep a shared owner alive for the full borrowed use.

Class and struct state must not store C++ reference members (`T& _member` or `const T& _member`). Constructor parameters and short local aliases may still use references when that is the clearest borrow, but stored required borrowed dependencies must be reviewed non-null `ptr<T>` members. Stored optional dependencies should use the matching nullable wrapper (`nptr<T>`, `unique_nptr<T>`, or `refcount_nptr<T>`). Use value members only for data the object actually owns by value, not as a workaround for a borrowed dependency.

Process/runtime command-line arguments enter through platform or DLL ABI `argc` / `argv` signatures, then immediately move into the wrapper vocabulary. Use `CommandLineArg` / `CommandLineArgs` for internal argument views; build any temporary `char**` array only at a final ABI handoff such as the client-runtime export call.

When a reviewed raw cleanup boundary must adopt object storage, route it through a named helper instead of constructing owners directly from `.get()`. Use `adopt_unique_ptr(ptr<T>)` for scalar object cleanup and `make_unique_del_ptr(ptr<T>, deleter)` / `make_unique_del_ptr(nptr<T>, deleter)` for custom-deleter holders; the `ptr<T>` overload returns `unique_del_ptr<T>`, and the `nptr<T>` overload returns `unique_del_nptr<T>`. Domain-specific helpers may wrap these primitives, but call sites should not spell `unique_ptr<T> {value.get()}` or `unique_del_ptr<T> {value.get(), deleter}` directly.

For a low-level byte/ABI reinterpret of the pointee type (`ucolor` <-> `uint8_t`, `void` -> `T`, etc.), use `ptr<T>::reinterpret_as<U>()` / `nptr<T>::reinterpret_as<U>()` rather than the raw `cast_from_void<U*>(cast_to_void(p.get()))` roundtrip. It returns `ptr<U>` / `nptr<U>`, propagates the source pointee's `const` (so it can never silently strip `const`), and accepts a `ptr<void>` source (as `GetPtrAs<U>()` does). Reserve the bare `cast_from_void` / `cast_to_void` helpers for what the method cannot express — a deliberate const-stripping reinterpret (off a `get_no_const()`), or a non-wrapper raw operand.

## Refcount bridges

Raw or borrowed pointer to intrusive-refcount ownership must be explicit. The target API is:

```cpp
refcount_ptr<Entity> held = entity_ptr.hold_ref();
refcount_nptr<Entity> maybe_held = maybe_entity.try_hold_ref();

ptr<Entity> borrowed = held.as_ptr();
nptr<Entity> maybe_borrowed = maybe_held.as_nptr();
```

When a raw pointer is unavoidable at an ABI, atomic, or allocator boundary, use the named refcount factories instead of direct construction:

```cpp
refcount_ptr<Entity> held_from_raw = refcount_ptr<Entity>::from_add_ref(raw_entity);
refcount_nptr<Entity> maybe_held_from_raw = refcount_ptr<Entity>::try_from_add_ref(raw_entity);
refcount_ptr<Entity> adopted = refcount_ptr<Entity>::from_adopted_ref(raw_entity_with_existing_ref);
```

Direct `refcount_ptr<T>(T*)`, `operator=(T*)`, and public `adopt_tag` construction are disabled by default through `FO_STRICT_REFCOUNT_EXPLICIT`. Temporary compatibility builds may override the flag to `0`, but engine/project code should use the named methods so raw/refcount transitions are visually reviewable.

## Strict Flags

The strict migration flags are mandatorily defined by the build system (`FO_STRICT_*` CMake options in `BuildTools/cmake/stages/Init.cmake`, default `ON`), never through a header fallback; `SmartPointers.h` only gates on them with `#if FO_STRICT_*`. They are enabled by default and can be overridden explicitly when a temporary compatibility build is needed:

- `FO_STRICT_REFCOUNT_EXPLICIT` disables direct raw `refcount_ptr<T>(T*)`, `operator=(T*)`, and public `adopt_tag` construction. Use the named factories above.
- `FO_STRICT_PTR_NONNULL` disables `ptr<T>` default/null construction, `nullptr` assignment/comparison, `operator bool`, `get_pp()`, and `reset()` without a replacement pointer.
- `FO_STRICT_OWNING_NONNULL` disables `unique_ptr<T>` / `refcount_ptr<T>` default/null construction, `nullptr` assignment/comparison, `operator bool`, default-null `reset()`, and lvalue `release()` / `release_ownership()`; rvalue `unique_ptr<T>::release()` returns `ptr<T>` under the strict contract.

The embedding project CI should keep a unit-test build that exercises the default strict configuration. Temporary compatibility builds may set strict options to `OFF` only for focused migration or bisect work; new code must be valid under the strict defaults.

## Raw pointer allowlist

Do not force wrappers into places where raw pointers are the clearer ABI or low-level representation:

- `void*` and byte buffers with an adjacent size.
- Allocator internals, placement new/delete, and pointer arithmetic.
- C, OS, graphics, COM-style, and third-party ABI surfaces.
- Process/runtime `argc` / `argv` entrypoints and final `char**` handoffs to compatible runtime modules.
- AngelScript generic API plumbing, generated registration strings, and script-visible container element types (`vector<T*>` / `readonly_vector<T*>` and `FO_ENTITY_EVENT` payloads). See "Script binding boundary" below.
- `///@ EngineHook` callbacks invoked by the engine with raw pointers (do not convert hook definitions).
- `std::atomic<T*>` until a dedicated atomic nullable wrapper exists.

For engine-owned APIs outside these categories, prefer `ptr<T>` or `nptr<T>` for borrowed values and `unique_*` / `refcount_*` for owned values.

## Script binding boundary (`FO_SCRIPT_API`)

`///@ ExportMethod` script bindings are generated by `BuildTools/codegen.py`, which emits `ptr<T>` / `nptr<T>` for the scalar pointer surface of each export and constructs the wrapper inside the generated `NativeDataCaller::NativeCall` glue:

- The engine/entity **receiver** (first parameter) is always non-null, so it is `ptr<EngineType>` / `ptr<EntityType>`.
- A non-null entity/ref argument or return is `ptr<T>`; a nullable one is `nptr<T>`, replacing the older bare `T*` spelling (nullability is now carried by the wrapper type).
- The script-facing AngelScript registration is unchanged, so no `.fos`, bytecode, or save migration is required; only the C++ glue type changes. The client/server compatibility hash deliberately excludes the wrapper spelling.

These script-binding shapes intentionally stay raw at the ABI edge:

- Script-visible **container element types** (`vector<T*>` / `readonly_vector<T*>`, `FO_ENTITY_EVENT` payloads): the AngelScript generator owns the raw-handle vector ABI. Build contents from wrapper locals and unwrap only at the final handoff.
- Generic AngelScript plumbing, generated registration strings, handle slots, and `char*` / `char**` buffers and process argv.
- `///@ EngineHook` callbacks: the engine **calls** these with raw pointers, so their definitions keep the raw signature.

Inside an export body, bind any remaining raw input to a wrapper before ordinary engine work and unwrap (`.get()`) only at the final ABI/handoff line.


## Migration rules

1. Replace legacy `raw_ptr<T>` call sites with `ptr<T>` without changing behavior.
2. Replace legacy `nullable_raw_ptr<T>` call sites with `nptr<T>`.
3. Convert already-empty `ptr<T> ... {}` declarations to `nptr<T>`.
4. Convert already-empty `unique_ptr<T> ... {}` declarations to `unique_nptr<T>`.
5. Convert already-empty `refcount_ptr<T> ... {}` declarations to `refcount_nptr<T>`.
6. Review lookup APIs, current-state fields, and script/third-party boundary adapters before changing stricter contracts.
7. Use `nptr<T>::as_ptr()` when optional borrowed state is explicitly narrowed back to `ptr<T>` after a null check.
8. Use `unique_nptr<T>::take_not_null()` when optional unique ownership is explicitly narrowed back to `unique_ptr<T>`; use `take_not_null(unique_del_nptr<T>&)` for the same checked narrowing into `unique_del_ptr<T>`.
9. Use `refcount_nptr<T>::as_ptr()` for checked borrowed access and `refcount_nptr<T>::take_not_null()` when optional intrusive-refcount ownership is explicitly narrowed back to `refcount_ptr<T>`.
10. Change functions that return `nullptr` or `{}` from `ptr<T>`, `unique_ptr<T>`, or `refcount_ptr<T>` to the matching nullable return type unless absence is made impossible.
11. Enable strict non-null behavior only after the affected subsystem no longer relies on nullable operations through the non-null spelling.

## Validation

Use grep gates during the staged migration:

```powershell
rg "\braw_ptr<" Source SourceExt
rg "^\s*(mutable\s+)?ptr<[^\n;]+>\s+\w+[^;{}]*\{\}\s*;" Source SourceExt
rg "^\s*(mutable\s+)?unique_ptr<[^\n;]+>\s+\w+[^;{}]*\{\}\s*;" Source SourceExt
rg "^\s*(mutable\s+)?refcount_ptr<[^\n;]+>\s+\w+[^;{}]*\{\}\s*;" Source SourceExt
```

Run `LF_UnitTests` / `RunUnitTests` after pointer-layer changes. If native script bindings or exported method signatures are touched, also run the script compilation/baking validation used by the embedding project.

Embedding projects can keep lightweight migration guards in source control. The current companion tooling is:

```bash
python3 Tools/SmartPointerAudit/smart_pointer_audit.py \
  --fail-on error \
  --raw-pointer-baseline Tools/SmartPointerAudit/raw_pointer_baseline.tsv \
  --raw-pointer-abi-baseline Tools/SmartPointerAudit/raw_pointer_abi_baseline.tsv \
  --raw-pointer-container-baseline Tools/SmartPointerAudit/raw_pointer_container_baseline.tsv \
  --raw-pointer-container-abi-baseline Tools/SmartPointerAudit/raw_pointer_container_abi_baseline.tsv \
  --unique-nptr-baseline Tools/SmartPointerAudit/unique_nptr_baseline.tsv \
  --unique-del-nptr-baseline Tools/SmartPointerAudit/unique_del_nptr_baseline.tsv \
  --nonnull-member-allowlist Tools/SmartPointerAudit/nonnull_member_allowlist.tsv \
  --reference-member-allowlist Tools/SmartPointerAudit/reference_member_allowlist.tsv \
  --raw-pointer-low-level-allowlist Tools/SmartPointerAudit/raw_pointer_low_level_allowlist.tsv \
  --raw-pointer-abi-allowlist Tools/SmartPointerAudit/raw_pointer_abi_allowlist.tsv \
  --raw-pointer-container-abi-allowlist Tools/SmartPointerAudit/raw_pointer_container_abi_allowlist.tsv \
  --raw-pointer-header-abi-allowlist Tools/SmartPointerAudit/raw_pointer_header_abi_allowlist.tsv \
  --unique-nptr-allowlist Tools/SmartPointerAudit/unique_nptr_allowlist.tsv \
  --unique-del-nptr-allowlist Tools/SmartPointerAudit/unique_del_nptr_allowlist.tsv
python3 Tools/SmartPointerAudit/smart_pointer_clang_query.py --diff-base origin/main --require-tooling
```

The first command is the regular textual non-regression audit: raw pointer, reviewed ABI/container, and nullable unique-owner counts may shrink freely but must not grow without an intentional baseline update, while line-level allowlists keep reviewed reference-member, raw ABI, low-level raw, and nullable owner quarantine rows from drifting. The second command is the optional AST-backed clang-query gate for newly added raw pointer declarations in checked scopes after a build has produced `compile_commands.json`.
