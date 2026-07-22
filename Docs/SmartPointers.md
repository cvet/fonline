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

After a local null check, use an `nptr<T>` directly (`nptr<T>` has `operator->` / `operator*`): `if (cr) { cr->Foo(); }`, the short-circuit `cr && cr->Foo()`, or after an early `if (!cr) { return; }`. The guard already proves non-null, so copying the value through `as_ptr()` only adds a redundant local and assertion. Use `as_ptr()` only when an API specifically requires `ptr<T>` or a non-null borrow must escape the guarded scope. In script bindings (`FO_SCRIPT_API` and `SourceExt/` script impls) this is **enforced**: the smart-pointer audit's `ScriptNullableLocalDereference` rule is guard-aware — it accepts the guarded forms above but flags any `nptr` local dereferenced with no preceding null test.

**Pointer names describe the domain value, not its nullability.** Do not add `nullable_` / `_nullable` solely because a value uses an `n*` wrapper; the type already carries that contract. Keep the clean domain name before and after the guard:

| Type | local / parameter (`snake_case`) | data member (`_camelCase`) |
|---|---|---|
| raw `T*` | `raw_target` | `_rawTarget` |
| `nptr<T>` (nullable wrapper) | `target` | `_target` |
| `ptr<T>` (non-null) | `target` | `_target` |

```cpp
// Guard the nullable wrapper, then use it directly
nptr<Critter> target = engine->GetCritter(id);
if (!target) {
    return;
}
target->Foo();

// raw C-ABI pointer -> checked non-null
static void Cleanup(sentry_options_t* raw_options) noexcept {
    if (raw_options != nullptr) {                  // check the raw pointer directly
        ptr<sentry_options_t> options = raw_options;   // clean name for the non-null ptr
        sentry_options_free(options.get());
    }
}
```

Do **not** invent per-call disambiguators such as `_lookup`, `_ref`, `_ptr`, `_begin`, or `nullable_`. Check an `n*` value under its domain name and use it directly while the guard holds. Bind a raw pointer straight to `ptr<T>` after the `!= nullptr` check — do not route it through an `nptr<T>` intermediate just to null-check it.

When a value is **provably non-null** — `std::string::data()` / `string_view::data()` (never null), `&object`, or an already-non-null result — bind it **straight to `ptr<T>` with no check and no `nptr<T>` intermediate**; adding a `!= nullptr` test there is dead code. And do not introduce *any* wrapper for a method-local pointer that never escapes the function and is used only for local address arithmetic — a `ptr<T>` (or a plain raw pointer for pure pointer math) is enough:

```cpp
ptr<const char> view_begin = _sv.data();        // data() is never null — no nptr, no check
ptr<const char> storage_begin = _s.data();
ptr<const char> storage_end = storage_begin.get() + _s.size();
if (view_begin < storage_begin || !(view_begin < storage_end)) { ... }
```

One exception: an **exported/ABI parameter whose name is fixed by an external contract** (or pinned in the smart-pointer-audit allowlist) keeps its name; narrow it to a `<name>_ptr` local rather than renaming the parameter.

The same domain naming applies to every wrapper form (`refcount_nptr<T>`, `unique_nptr<T>`, `shared_ptr<T>`, …). Ownership transfers such as `unique_nptr<T>::take_not_null()` remain explicit because they change the owner type rather than creating a redundant local borrow.

`unique_ptr<T>` owns one object in usable state. A moved-from object may be empty only until destruction or reassignment. If empty state is part of the normal object model, use `unique_nptr<T>`.

`unique_nptr<T>` owns zero or one object. Use it for lazily-created resources, optional owned state, and objects that are created after the owner is constructed or cleared before the owner is destroyed.

Use `unique_nptr<T>::take_not_null()` when a nullable owner has been checked or otherwise proved present and ownership must move into a `unique_ptr<T>`. The method asserts the non-null invariant and makes the narrowing visually reviewable.

Use `take_not_null(unique_del_nptr<T>&)` for the matching custom-deleter case when a checked nullable custom-deleter owner must move into `unique_del_ptr<T>`. The helper keeps the stored deleter with the transferred owner, so call sites do not need to manually release raw storage or rebuild deleters.

In strict builds, `unique_ptr<T>::release()` is rvalue-only and returns `ptr<T>` instead of raw `T*`. `unique_nptr<T>::release()` returns `nptr<T>`. Unwrap with `.get()` only at an explicit ABI, allocator, or adoption boundary.

Helpers that can fail a type cast or lookup while transferring unique ownership must return `unique_nptr<T>`, because a failed cast is normal absence, not a valid `unique_ptr<T>` state.

`unique_del_ptr<T>` / `unique_del_nptr<T>` are custom-deleter owners used at external cleanup boundaries. Use `unique_del_ptr<T>` only after a `ptr<T>` or equivalent assertion proves the custom-owned object is present; it is a strict move-only non-null owner and rejects default/null construction in strict builds. Use the `unique_del_nptr<T>` spelling for stored state that can be empty, lazily initialized, moved-from, or reset.

`refcount_ptr<T>` owns an intrusive reference and is non-null in usable state. Copying increments the reference count; destruction decrements it.

`refcount_nptr<T>` owns an intrusive reference when present and can be empty. Use it for optional entity/view/current state and lookup results that need to keep the object alive when found.

Use `refcount_nptr<T>::as_ptr()` after a local null check when a nullable intrusive owner is explicitly narrowed to a required borrowed `ptr<T>` without changing ownership. Use `refcount_nptr<T>::take_not_null()` when a nullable intrusive owner has been checked or otherwise proved present and ownership must move into a `refcount_ptr<T>`. The ownership-narrowing method asserts the non-null invariant and adopts the already-held reference without adding another one.

Do not wrap ordinary nullable intrusive ownership in `optional<refcount_ptr<T>>`; keep normal absence in `refcount_nptr<T>`. A load-result contract that must distinguish "absent" from "error" returns `refcount_nptr<T>` **plus a separate error flag** (e.g. a `bool& is_error` out-parameter), not `optional<refcount_ptr<T>>`. The same rule applies to other wrappers: avoid `optional<ptr<T>>`, `optional<nptr<T>>`, `optional<unique_ptr<T>>`, `optional<unique_nptr<T>>`, and `optional<refcount_nptr<T>>`; use the direct wrapper vocabulary or a named domain result type.

`shared_ptr<T>` and `weak_ptr<T>` are engine-own shared-ownership types (no `std::shared_ptr` inside): an atomic control block owns the object through the strong count and the block itself through the weak count, the object is embedded in the same allocation by `SafeAlloc::MakeShared()` (which also honors the OOM backup pool), and destruction goes through a virtual hook so holders never need the complete pointee type. Types that need `shared_from_this()` / `weak_from_this()` derive from the engine-own `enable_shared_from_this<T>`; the factory wires the embedded weak reference right after construction, so like `std::enable_shared_from_this` it is not usable inside the constructor. Casts are member methods: `shared_ptr<U>::dyn_cast<T>()` (dynamic cast sharing the same control block, empty on failure) and `shared_ptr<const T>::cast_no_const()` (const-stripping escape hatch, the shared-owner sibling of `get_no_const()`). Both types can export explicit local borrows through `as_ptr()` after a guard/assert or `as_nptr()` when absence remains part of the current path. These methods do not change shared ownership; keep a shared owner alive for the full borrowed use. `unique_arr_ptr<T>` and `unique_del_nptr<T>` are likewise engine-own owners (array `delete[]` and type-erased `function<void(T*)>` deleter respectively); `unique_del_nptr<T>::get_deleter()` exposes the stored deleter for reviewed ownership handoffs such as `take_not_null()`.

Class and struct state must not store C++ reference members (`T& _member` or `const T& _member`). Constructor parameters and short local aliases may still use references when that is the clearest borrow, but stored required borrowed dependencies must be reviewed non-null `ptr<T>` members. Stored optional dependencies should use the matching nullable wrapper (`nptr<T>`, `unique_nptr<T>`, or `refcount_nptr<T>`). Use value members only for data the object actually owns by value, not as a workaround for a borrowed dependency.

Process/runtime command-line arguments enter through platform or DLL ABI `argc` / `argv` signatures, then immediately move into the wrapper vocabulary. Use `CommandLineArg` / `CommandLineArgs` for internal argument views; build any temporary `char**` array only at a final ABI handoff such as the client-runtime export call.

When a reviewed raw cleanup boundary must adopt object storage, route it through a named helper instead of constructing owners directly from `.get()`. Use `adopt_unique_ptr(ptr<T>)` for scalar object cleanup and `make_unique_del_ptr(ptr<T>, deleter)` / `make_unique_del_ptr(nptr<T>, deleter)` for custom-deleter holders; the `ptr<T>` overload returns `unique_del_ptr<T>`, and the `nptr<T>` overload returns `unique_del_nptr<T>`. Domain-specific helpers may wrap these primitives, but call sites should not spell `unique_ptr<T> {value.get()}` or `unique_del_ptr<T> {value.get(), deleter}` directly.

For a low-level byte/ABI reinterpret of the pointee type (`ucolor` <-> `uint8_t`, `void` -> `T`, etc.), use `ptr<T>::reinterpret_as<U>()` / `nptr<T>::reinterpret_as<U>()` rather than the raw `cast_from_void<U*>(cast_to_void(p.get()))` roundtrip. It returns `ptr<U>` / `nptr<U>`, propagates the source pointee's `const` (so it can never silently strip `const`), and accepts a `ptr<void>` source (as `GetPtrAs<U>()` does). Reserve the bare `cast_from_void` / `cast_to_void` helpers for what the method cannot express — a deliberate const-stripping reinterpret (off a `get_no_const()`), or a non-wrapper raw operand.

`cast_from_void<T>(void_ptr)` recovers a **concrete** typed pointer from a `void`-based pointer with `static_cast` (keeping const-correctness); its `T` must have a non-void ultimate pointee. A **void-of-void** indirection (`void**`, `const void* const*`, ...) — the AngelScript handle-slot plumbing in `ScriptSystem.h` and the script-call bridges — is not a concrete recovery, so reinterpret the *wrapper* with `ptr<void>::reinterpret_as<void*>()` / `nptr<const void>::reinterpret_as<const void*>()` (dereference with `*` to read the stored handle). A concrete-to-concrete storage reinterpret (e.g. a `unique_del_ptr<uint8_t>` holding a typed object) likewise goes through the wrapper's `.as_ptr().reinterpret_as<T>()`.

For reinterpreting a mutable **byte span** as a typed value span, use the shared `bytes_to_objects<T>(span<uint8_t>)` helper (`CommonHelpers.h`) — it reinterprets the whole byte span (asserting the size is a whole multiple of `sizeof(T)`) into a `span<T>` through `reinterpret_as`; take a subrange with `span::first`/`subspan` on the input or result when fewer elements are wanted. The inverse `object_to_bytes<T>(T&)` exposes one object as its `span<uint8_t>`.

To build a typed **element span** from a borrowed pointer and a length, use `make_span(ptr<T>, length) -> span<T>` / `make_const_span(ptr<T>, length) -> const_span<T>` (`CommonHelpers.h`) instead of constructing `span<T>{p.get(), length}` — they keep the `.get()` unwrap inside the helper. `make_const_span` also has a raw `const T*` overload for container-data sources (`make_const_span(vec.data(), n)`). (The other `make_span(...)` overloads take a *byte* size and return a `span<uint8_t>` view; the `ptr<T>` overload counts elements.) For a `ptr<char>` / `ptr<const char>` holding text, `as_str(size_t len) -> string_view` builds the view directly rather than `string_view{p.get(), len}`.

### Always-on non-null enforcement

The non-null invariant is enforced by an **always-on** runtime check, in every build configuration — not a debug-only `assert` that release strips. Constructing a `ptr<T>` from a null raw pointer (the `ptr(T*)` ctor), and every nullable→non-null narrowing (`nptr<T>::as_ptr()`, `refcount_nptr<T>::as_ptr()` / `take_not_null()`, `unique_nptr<T>::take_not_null()`, `shared_ptr<T>::as_ptr()`, …), assert the invariant and, on violation, produce the same `StrongAssertationException` report and process exit as `FO_STRONG_ASSERT`.

The check is spelled `FO_BASIC_STRONG_ASSERT(expr)`, declared in `Essentials/BasicCore.h` (next to `ExitApp`) and implemented in `Essentials/ExceptionHandling.cpp`. It exists because `SmartPointers.h` sits **above** `ExceptionHandling.h` in the `Essentials.h` include order and therefore cannot use `FO_STRONG_ASSERT` directly; the primitive is a forward-declared `[[noreturn]] ReportStrongAssertAndExit(message, file, line)` (dependency inversion — declared low, implemented high, no include-layer violation). It is a `noexcept` function, so it is safe to call from the `noexcept` smart-pointer members (the throw/catch that builds the report is fully contained inside it). Any Essentials module above `ExceptionHandling` that needs an always-on assertion uses `FO_BASIC_STRONG_ASSERT`.

Consequence: a null wrapped in `ptr<T>` fails fast at the construction site even in release. The two common sources are (1) an empty container's `.data()` — `std::vector` (MSVC) / `std::string_view` / `std::span` return null when empty (`std::string` / `std::array` never do), and (2) a raw API that returns null on "not found" (`GetProcAddress`, `dlsym`, `GetModuleHandle(nullptr)` for the main module, …). Keep genuinely-nullable values in `nptr<T>`, or — for a transient buffer pointer used once — pass `X.data()` straight into a consumer that takes `nptr`/raw instead of building an intermediate `ptr<T>`.

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

Direct `refcount_ptr<T>(T*)`, `operator=(T*)`, and public `adopt_tag` construction are unavailable; engine/project code uses the named methods so raw/refcount transitions are visually reviewable.

## Non-null and explicit-bridge contract

These rules are the unconditional behavior of the smart-pointer types. (They were previously gated by the `FO_STRICT_*` migration flags; once the migration completed the flags were removed and the strict behavior became the only behavior — there is no permissive mode.)

- Direct raw `refcount_ptr<T>(T*)`, `operator=(T*)`, and public `adopt_tag` construction are unavailable. Use the named factories above.
- `ptr<T>` has no default/null construction, `nullptr` assignment/comparison, `operator bool`, `get_pp()`, or `reset()` without a replacement pointer.
- `unique_ptr<T>` / `refcount_ptr<T>` have no default/null construction, `nullptr` assignment/comparison, `operator bool`, default-null `reset()`, or lvalue `release()` / `release_ownership()`; rvalue `unique_ptr<T>::release()` returns `ptr<T>`.

Any two wrapper values (borrow or owner, same or different kind) compare directly with `==` / `!=` through a heterogeneous free `operator==` that forwards to the stored pointers — write `item == other_item` rather than unwrapping both sides with `.get()`. The smart-pointer audit's `DirectWrapperGetComparison` rule flags `a.get() == b.get()`.

New code must be valid under this contract; the nullable siblings (`nptr<T>`, `unique_nptr<T>`, `refcount_nptr<T>`) exist for the genuinely-optional cases.

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
- **Container element types** may use the wrapper vocabulary too: a return `vector<ptr<T>>` and a parameter `readonly_vector<ptr<T>>` / `readonly_vector<nptr<T>>` are supported. Codegen reduces the element to the raw handle for the AngelScript metadata (the registered `T@[]` signature and the compatibility hash are unchanged), and — for **parameters** — re-spells the generated extern and `NativeCall` cast with the element wrapper so the glue matches the C++-mangled parameter (`container_element_wrapper` / `apply_container_element_wrapper` in `codegen.py`). Returns need no such match: C++ return types are not part of the mangled symbol, so a `vector<ptr<T>>` body links against the raw extern by layout-compat. The vector unmarshaller reads each element as its `value_type`, so wrapped elements work by pointer layout-compatibility.
- `///@ ExportRefType` methods (e.g. the dialog accessors) may likewise return `ptr<T>` / `nptr<T>` instead of raw `T*`. Codegen records the return wrapper (`RefTypeMethod.ret_wrapper`) and appends `.get()` inside the generated `Wrapped::Call` body so the raw-`T*` glue return still matches; the script `.Ret` metadata stays the reduced handle type, so the registered signature and compatibility hash are unchanged.
- The script-facing AngelScript registration is unchanged, so no `.fos`, bytecode, or save migration is required; only the C++ glue type changes. The client/server compatibility hash deliberately excludes the wrapper spelling.

These script-binding shapes intentionally stay raw at the ABI edge:

- Script-visible **container element types** stay raw `vector<T*>` only where a wrapper cannot carry the contract: the raw-handle **bridge helpers** in `ScriptSystem.h` (`MakeScriptHandleVector*`), **ownership-transfer returns** that release `refcount_ptr` to the script (`ReleaseScriptOwnershipVector` — a `vector<ptr<T>>` would drop the ref → use-after-free), **const-strip returns** whose `const_cast` belongs inside the helper (`MakeMutableScriptHandleVector`, `MakeScriptRefHandleVectorAs`), and `FO_ENTITY_EVENT` payloads. Everywhere else, prefer the wrapper spelling above and build contents from wrapper locals.
- Generic AngelScript plumbing, generated registration strings, handle slots, and `char*` / `char**` buffers and process argv.
- `///@ EngineHook` callbacks: the engine **calls** these with raw pointers, so their definitions keep the raw signature.

Inside an export body, bind any remaining raw input to a wrapper before ordinary engine work and unwrap (`.get()`) only at the final ABI/handoff line. When a function's declared **return type is itself a raw pointer** (a `void*`/`T*` handed to a C or script API — SDL/ImGui/Spine allocators, script element accessors, generic readers), `return wrapper.get();` is allowed: the audit reads the function's return type (raw pointer, deduced `auto`, or a bare template parameter `-> T`) and treats such a `.get()` return as a genuine raw-pointer boundary rather than a wrapper unwrap. A `.get()` return whose enclosing function returns a **wrapper** (`nptr<T>`/`ptr<T>`) is still flagged — return the wrapper (`return x;`) or bind a named wrapper local. The mutable `get_no_const()` return keeps its stricter rule (only inside a `Return*` / `BorrowScript*`-named boundary). The `GetAddressOfReturnLocation()` placement-new return likewise routes through a `Return*`-named boundary (e.g. `ReturnGenericPointer`).


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

As an embedding-project example, Last Frontier runs the full audit invocation in CI on every push and exposes the same command locally as the `Analyze :: Smart Pointer Audit` VS Code task (bundled into its `Analyze All` and pre-commit validation tasks).
