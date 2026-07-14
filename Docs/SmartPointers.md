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

**Keep a checked nullable as `nptr<T>` and dereference it directly** — do not introduce a `nullable_x` copy and narrow it to `ptr<T>` just to call through it. `nptr<T>` has `operator->` / `operator*`, an unchecked deref exactly like `ptr<T>`, so after a guard the direct deref is free and the clean domain name stays on the `nptr<T>` itself:

```cpp
nptr<Critter> target = engine->GetCritter(id);   // clean domain name stays on the nptr
if (!target) {
    return;                                       // guard proves non-null for the rest of the scope
}
target->Foo();                                    // deref the nptr directly — no as_ptr(), no copy
```

The guard may be an early `if (!target) { <recover>; return; }`, a positive `if (target) { ... }` / short-circuit `target && target->Foo()`, an `if (nptr<T> target = expr) { target->... }` if-init, a `for (nptr<T> n = ...; n; ...)` loop condition, or an exit-on-fail assertion `FO_VERIFY_AND_THROW(target, "...")` / `FO_STRONG_ASSERT(target, "...")` — each proves non-null, so an `as_ptr()` there would only add a redundant assert. In verify/assert conditions, write pointer truthiness directly (`target`, `size == 0 || data`), not through a double-negation expression; when an invariant really compares presence to another boolean, compute that boolean explicitly (`static_cast<bool>(target)` for wrappers, `raw != nullptr` for raw pointers). This is **enforced** across all strict engine / `SourceExt` scopes (not only script bindings): the smart-pointer audit's `NullableLocalDereference` rule is guard-aware and flags any `nptr` / `unique_nptr` / `refcount_nptr` local dereferenced with no preceding null check. The rule has no baseline — a hit is fixed at the source (add the guard/`verify`, or narrow the producer to `ptr<T>`), never quarantined.

**A checked `nptr<T>` narrows to `ptr<T>` implicitly — do not spell `.as_ptr()` at a `ptr<T>` site.** `nptr<T>` converts to `ptr<T>` through an implicit converting constructor that asserts non-null at the conversion point (the same always-on assert `.as_ptr()` runs), so a guard-checked nptr flows straight into a `ptr<T>` parameter, member, or return with no ceremony:

```cpp
nptr<Map> map = EntityMngr.GetMap(map_id);       // clean domain name on the nptr
if (!map) {
    return;                                       // guard proves non-null
}
FindPath(map, cr, from_hex, to_hex);              // FindPath takes ptr<Map> — implicit narrow, no .as_ptr()
```

The conversion is const-propagating and mutability-preserving (a `const nptr<T>` yields only `ptr<const T>`, never a mutable `ptr<T>`). When a nullable source is named in a deduced local and the following code requires presence, keep the nullable wrapper itself, then check it explicitly before deref or implicit `ptr<T>` use. Do not rely on a hidden `.as_ptr()` assertion for this:

```cpp
auto target = engine->GetCritter(id);
FO_VERIFY_AND_THROW(target, "Target critter not found");
target->Foo();                                    // checked nptr, direct deref
UseTarget(target);                                // UseTarget takes ptr<Critter>
```

Explicit `.as_ptr()` / `.as_nptr()` conversions are also valid when they make the borrow visible, resolve overload or template deduction, materialize a value for use after an owner is moved, or create a copyable lambda capture. Implicit conversion remains available when the destination `ptr<T>` / `nptr<T>` type is already unambiguous.

When the source is a raw `T*`, use the global helper and keep the local declaration deduced:

```cpp
auto target = make_ptr(raw_target);               // raw T* -> ptr<T>, asserts non-null
auto maybe_target = make_nptr(raw_target);        // raw T* -> nptr<T>, preserves null
```

Owning wrappers borrow the same way: `refcount_ptr<T>` / `refcount_nptr<T>`, `unique_ptr<T>` / `unique_nptr<T>`, `unique_del_ptr<T>` / `unique_del_nptr<T>`, `unique_arr_ptr<T>`, and `shared_ptr<T>` all flow implicitly to `ptr<T>` / `nptr<T>` borrow sites through `get()`. The `ptr<T>` conversion asserts non-null at the conversion point; the `nptr<T>` conversion preserves absence. These conversions never transfer ownership. At call/typed-return/member sites with a known destination type, pass the owner directly. For local access, use the source owner itself or an `auto&` alias when no borrow type is required. When deduction, owner movement, or a lambda capture genuinely requires a borrowed value, materialize it as `auto borrow = owner.as_ptr();` / `auto maybe_borrow = owner.as_nptr();` and validate nullable state before dereference. If a nullable owner was just assigned from a factory with a reviewed non-null result and only a few member accesses follow, work with the source owner directly instead of creating a temporary borrowed local and re-checking it.

When a dynamic cast is the only reason for a temporary borrow, call `dyn_cast<T>()` on the owner directly: write `_views[i].dyn_cast<EditorAssetView>()`, not `_views[i].as_ptr().dyn_cast<EditorAssetView>()`. `unique_ptr<T>` / `unique_nptr<T>` owner casts return a borrowed `nptr<U>`. `refcount_ptr<T>` / `refcount_nptr<T>` keep the existing owning result (`refcount_nptr<U>`) when `U` is itself intrusive-refcountable, and return borrowed `nptr<U>` for non-refcountable mixin/interface targets such as view interfaces. If ownership is intentionally acquired from a borrow, keep spelling that explicitly with `hold_ref()` / `try_hold_ref()` or `require_refcount_ptr(...)`.

Reverse transitions from borrowed wrappers to owners remain explicit and reviewed: use `hold_ref()` / `try_hold_ref()` for intrusive refs, `adopt_unique_ptr(ptr<T>)` for scalar unique adoption, `make_unique_del_ptr(...)` for custom-deleter adoption, `take_not_null()` for nullable-owner ownership narrowing, and `SafeAlloc::MakeShared(...)` / domain factories for shared ownership. There is no implicit `ptr<T>` / `nptr<T>` → owner conversion.

When a narrowed `ptr<T>` genuinely coexists with the nullable in one scope, name them by role so the two never collide — the clean domain name goes to the value the body works with, and boundary values exist only to be checked or narrowed:

| Type | local / parameter (`snake_case`) | data member (`_camelCase`) |
|---|---|---|
| raw `T*` | `raw_target` | `_rawTarget` |
| `nptr<T>` narrowed away to a coexisting `ptr<T>` | `maybe_target` / domain name | `_maybeTarget` / domain name |
| `ptr<T>` (non-null, narrowed) | `target` | `_target` |

```cpp
// raw C-ABI pointer -> checked non-null
static void Cleanup(sentry_options_t* raw_options) noexcept {
    if (raw_options != nullptr) {                  // check the raw pointer directly
        ptr<sentry_options_t> options = raw_options;   // clean name for the non-null ptr
        sentry_options_free(options.get());
    }
}
```

Because the common case now keeps the clean name on the checked `nptr<T>` itself, do **not** introduce `nullable_` locals as a narrowing ritual. Do **not** invent per-call disambiguators such as `_lookup`, `_ref`, `_ptr`, or `_begin`, and **never copy an already-named nullable or raw pointer into an intermediate just to wear a prefix**. Bind a raw pointer straight to `ptr<T>` after the `!= nullptr` check — do not route it through an `nptr<T>` intermediate just to null-check it.

When a value is **provably non-null** — `std::string::data()` / `string_view::data()` (never null), `&object`, or an already-non-null result — bind it **straight to `ptr<T>` with no check and no `nptr<T>` intermediate**; adding a `!= nullptr` test there is dead code. And do not introduce *any* wrapper for a method-local pointer that never escapes the function and is used only for local address arithmetic — a `ptr<T>` (or a plain raw pointer for pure pointer math) is enough:

```cpp
ptr<const char> view_begin = _sv.data();        // data() is never null — no nptr, no check
ptr<const char> storage_begin = _s.data();
ptr<const char> storage_end = storage_begin.get() + _s.size();
if (view_begin < storage_begin || !(view_begin < storage_end)) { ... }
```

One exception: an **exported/ABI parameter whose name is fixed by an external contract** (or pinned in the smart-pointer-audit allowlist) keeps its name; narrow it to a `<name>_ptr` local rather than renaming the parameter.

The same naming applies to every narrowing form (`unique_nptr<T>::take_not_null()`, `refcount_nptr<T>::take_not_null()`, custom-deleter `take_not_null(...)`, …).

`unique_ptr<T>` owns one object in usable state. A moved-from object may be empty only until destruction or reassignment. If empty state is part of the normal object model, use `unique_nptr<T>`.

`unique_nptr<T>` owns zero or one object. Use it for lazily-created resources, optional owned state, and objects that are created after the owner is constructed or cleared before the owner is destroyed.

Use `unique_nptr<T>::take_not_null()` when a nullable owner has been checked or otherwise proved present and ownership must move into a `unique_ptr<T>`. The method asserts the non-null invariant and makes the narrowing visually reviewable.

Use `take_not_null(unique_del_nptr<T>&)` for the matching custom-deleter case when a checked nullable custom-deleter owner must move into `unique_del_ptr<T>`. The helper keeps the stored deleter with the transferred owner, so call sites do not need to manually release raw storage or rebuild deleters.

In strict builds, `unique_ptr<T>::release()` and `unique_del_ptr<T>::release()` return `ptr<T>` instead of raw `T*`. `unique_nptr<T>::release()` and `unique_del_nptr<T>::release()` return `nptr<T>` or raw ABI storage for the custom-deleter nullable owner. Unwrap with `.get()` only at an explicit ABI, allocator, or adoption boundary.

Helpers that can fail a type cast or lookup while transferring unique ownership must return `unique_nptr<T>`, because a failed cast is normal absence, not a valid `unique_ptr<T>` state.

`unique_del_ptr<T>` / `unique_del_nptr<T>` are custom-deleter owners used at external cleanup boundaries. Use `unique_del_ptr<T>` only after a `ptr<T>` or equivalent assertion proves the custom-owned object is present; it is a strict move-only non-null owner and rejects default/null construction in strict builds. Use the `unique_del_nptr<T>` spelling for stored state that can be empty, lazily initialized, moved-from, or reset. When the custom deleter owns a typed C array or buffer, index the owner directly (`owner[index]`) instead of creating a borrowed-pointer temporary; `void` owners deliberately do not expose indexing. Opaque C resources declared as `void` may be owned directly as `unique_del_ptr<void>` / `unique_del_nptr<void>` when their cleanup function accepts the corresponding `void*`; do not introduce a C++ holder object merely to carry such a handle.

`refcount_ptr<T>` owns an intrusive reference and is non-null in usable state. Copying increments the reference count; destruction decrements it.

`refcount_nptr<T>` owns an intrusive reference when present and can be empty. Use it for optional entity/view/current state and lookup results that need to keep the object alive when found.

Borrow `refcount_ptr<T>` and `refcount_nptr<T>` through the implicit owner→`ptr<T>` / owner→`nptr<T>` conversions. Use `refcount_nptr<T>::take_not_null()` when a nullable intrusive owner has been checked or otherwise proved present and ownership must move into a `refcount_ptr<T>`. The ownership-narrowing method asserts the non-null invariant and adopts the already-held reference without adding another one.

Do not wrap ordinary nullable intrusive ownership in `optional<refcount_ptr<T>>`; keep normal absence in `refcount_nptr<T>`. A load-result contract that must distinguish "absent" from "error" returns `refcount_nptr<T>` **plus a separate error flag** (e.g. a `bool& is_error` out-parameter), not `optional<refcount_ptr<T>>`. The same rule applies to other wrappers: avoid `optional<ptr<T>>`, `optional<nptr<T>>`, `optional<unique_ptr<T>>`, `optional<unique_nptr<T>>`, and `optional<refcount_nptr<T>>`; use the direct wrapper vocabulary or a named domain result type.

`shared_ptr<T>` and `weak_ptr<T>` are engine-own shared-ownership types (no `std::shared_ptr` inside): an atomic control block owns the object through the strong count and the block itself through the weak count, the object is embedded in the same allocation by `SafeAlloc::MakeShared()` (which also honors the OOM backup pool), and destruction goes through a virtual hook so holders never need the complete pointee type. Types that need `shared_from_this()` / `weak_from_this()` derive from the engine-own `enable_shared_from_this<T>`; the factory wires the embedded weak reference right after construction, so like `std::enable_shared_from_this` it is not usable inside the constructor. Casts are member methods: `shared_ptr<U>::dyn_cast<T>()` (dynamic cast sharing the same control block, empty on failure) and `shared_ptr<const T>::cast_no_const()` (const-stripping escape hatch, the shared-owner sibling of `get_no_const()`). A present `shared_ptr<T>` borrows implicitly to `ptr<T>`; a possibly-empty one borrows implicitly to `nptr<T>`. These borrows do not change shared ownership; keep a shared owner alive for the full borrowed use. `unique_arr_ptr<T>` and `unique_del_nptr<T>` are likewise engine-own owners (array `delete[]` and type-erased `function<void(T*)>` deleter respectively); `unique_del_nptr<T>::get_deleter()` exposes the stored deleter for reviewed ownership handoffs such as `take_not_null()`.

Class and struct state must not store C++ reference members (`T& _member` or `const T& _member`). Constructor parameters and short local aliases may still use references when that is the clearest borrow, but stored required borrowed dependencies must be reviewed non-null `ptr<T>` members. Stored optional dependencies should use the matching nullable wrapper (`nptr<T>`, `unique_nptr<T>`, or `refcount_nptr<T>`). Use value members only for data the object actually owns by value, not as a workaround for a borrowed dependency.

Process/runtime command-line arguments enter through platform or DLL ABI `argc` / `argv` signatures, then immediately move into the wrapper vocabulary. Use `CommandLineArg` / `CommandLineArgs` for internal argument views; build any temporary `char**` array only at a final ABI handoff such as the client-runtime export call.

When a reviewed raw cleanup boundary must adopt object storage, route it through a named helper instead of constructing owners directly from `.get()`. Use `adopt_unique_ptr(ptr<T>)` for scalar object cleanup and `make_unique_del_ptr(ptr<T>, deleter)` / `make_unique_del_ptr(nptr<T>, deleter)` for custom-deleter holders; the `ptr<T>` overload returns `unique_del_ptr<T>`, and the `nptr<T>` overload returns `unique_del_nptr<T>`. Domain-specific helpers may wrap these primitives, but call sites should not spell `unique_ptr<T> {value.get()}` or `unique_del_ptr<T> {value.get(), deleter}` directly.

For a low-level byte/ABI reinterpret of the pointee type (`ucolor` <-> `uint8_t`, `void` -> `T`, etc.), use `ptr<T>::reinterpret_as<U>()` / `nptr<T>::reinterpret_as<U>()` rather than a raw `void*` roundtrip. It returns `ptr<U>` / `nptr<U>`, propagates the source pointee's `const` (so it can never silently strip `const`), and accepts a `ptr<void>` source (as `GetPtrAs<U>()` does).

`ptr<T>::void_cast()` / `nptr<T>::void_cast()` are one-way handoff helpers for C/ABI surfaces: they return a raw `void*` directly. Recover a **concrete** typed nullable borrow from that opaque pointer with `cast_from_void<T*>(void_ptr)`, which returns `nptr<T>` and uses `static_cast` internally. Its `T*` spelling must have a non-void ultimate pointee. A **void-of-void** indirection (`void**`, `const void* const*`, ...) — the AngelScript handle-slot plumbing in `ScriptSystem.h` and the script-call bridges — is not a concrete recovery, so reinterpret the *wrapper* with `ptr<void>::reinterpret_as<void*>()` / `nptr<const void>::reinterpret_as<const void*>()` (dereference with `*` to read the stored handle). If the source is already a wrapper, call `reinterpret_as<T>()` directly on it rather than wrapping it again (`mem.reinterpret_as<T>()`, not `ptr<void> {mem}.reinterpret_as<T>()`). A concrete-to-concrete storage reinterpret (e.g. a `unique_del_ptr<uint8_t>` holding a typed object) likewise goes through an explicit deduced borrow: `auto storage = owner.as_ptr(); auto object = storage.reinterpret_as<T>();`. If the source is a raw pointer rather than a wrapper, use `auto storage = make_ptr(raw_storage);` / `auto storage = make_nptr(raw_storage);`.

For reinterpreting a mutable **byte span** as a typed value span, use the shared `bytes_to_objects<T>(span<uint8_t>)` helper (`CommonHelpers.h`) — it reinterprets the whole byte span (asserting the size is a whole multiple of `sizeof(T)`) into a `span<T>` through `reinterpret_as`; take a subrange with `span::first`/`subspan` on the input or result when fewer elements are wanted. The inverse `object_to_bytes<T>(T&)` exposes one object as its `span<uint8_t>`.

To build a typed **element span** from a borrowed pointer and a length, use `make_span(ptr<T>, length) -> span<T>` / `make_const_span(ptr<T>, length) -> const_span<T>` (`CommonHelpers.h`) instead of constructing `span<T>{p.get(), length}` — they keep the `.get()` unwrap inside the helper. `make_const_span` also has a raw `const T*` overload for container-data sources (`make_const_span(vec.data(), n)`). (The other `make_span(...)` overloads take a *byte* size and return a `span<uint8_t>` view; the `ptr<T>` overload counts elements.) For a `ptr<char>` / `ptr<const char>` holding text, `as_str(size_t len) -> string_view` builds the view directly rather than `string_view{p.get(), len}`.

### Always-on non-null enforcement

The non-null invariant is enforced by an **always-on** runtime check, in every build configuration — not a debug-only `assert` that release strips. Constructing a `ptr<T>` from a null raw pointer (the `ptr(T*)` ctor), every nullable/owner→`ptr<T>` borrow (`nptr<T>`, `refcount_nptr<T>`, `unique_nptr<T>`, `unique_del_nptr<T>`, `shared_ptr<T>`, …), and every ownership narrow (`unique_nptr<T>::take_not_null()`, `refcount_nptr<T>::take_not_null()`, custom-deleter `take_not_null(...)`, …) assert the invariant and, on violation, produce the same `StrongAssertationException` report and process exit as `FO_STRONG_ASSERT`.

The check is spelled `FO_BASIC_STRONG_ASSERT(expr)`, declared in `Essentials/BasicCore.h` (next to `ExitApp`) and implemented in `Essentials/ExceptionHandling.cpp`. It exists because `SmartPointers.h` sits **above** `ExceptionHandling.h` in the `Essentials.h` include order and therefore cannot use `FO_STRONG_ASSERT` directly; the primitive is a forward-declared `[[noreturn]] ReportStrongAssertAndExit(message, file, line)` (dependency inversion — declared low, implemented high, no include-layer violation). It is a `noexcept` function, so it is safe to call from the `noexcept` smart-pointer members (the throw/catch that builds the report is fully contained inside it). Any Essentials module above `ExceptionHandling` that needs an always-on assertion uses `FO_BASIC_STRONG_ASSERT`.

Consequence: a null wrapped in `ptr<T>` fails fast at the construction site even in release. The two common sources are (1) an empty container's `.data()` — `std::vector` (MSVC) / `std::string_view` / `std::span` return null when empty (`std::string` / `std::array` never do), and (2) a raw API that returns null on "not found" (`GetProcAddress`, `dlsym`, `GetModuleHandle(nullptr)` for the main module, …). Keep genuinely-nullable values in `nptr<T>`, or — for a transient buffer pointer used once — pass `X.data()` straight into a consumer that takes `nptr`/raw instead of building an intermediate `ptr<T>`.

## Refcount bridges

Raw or borrowed pointer to intrusive-refcount ownership must be explicit. The target API is:

```cpp
refcount_ptr<Entity> held = entity_ptr.hold_ref();
refcount_nptr<Entity> maybe_held = maybe_entity.try_hold_ref();

ptr<Entity> borrowed = held;
nptr<Entity> maybe_borrowed = maybe_held;
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
- `unique_ptr<T>` / `unique_del_ptr<T>` / `refcount_ptr<T>` have no default/null construction, `nullptr` assignment/comparison, `operator bool`, or default-null `reset()`; `unique_ptr<T>::release()` and `unique_del_ptr<T>::release()` return `ptr<T>`, while nullable-owner `release()` methods keep their nullable/raw ABI return shape.

Any two wrapper values (borrow or owner, same or different kind) compare directly with `==` / `!=` through a heterogeneous free `operator==` that forwards to the stored pointers — write `item == other_item` rather than unwrapping both sides with `.get()`. The smart-pointer audit's `DirectWrapperGetComparison` rule flags `a.get() == b.get()`.

New code must be valid under this contract; the nullable siblings (`nptr<T>`, `unique_nptr<T>`, `refcount_nptr<T>`) exist for the genuinely-optional cases.

## Raw pointer allowlist

Do not force wrappers into places where raw pointers are the clearer ABI or low-level representation:

- `void*` and byte buffers with an adjacent size.
- Allocator internals, placement new/delete, and pointer arithmetic.
- C, OS, graphics, COM-style, and third-party ABI surfaces.
- Process/runtime `argc` / `argv` entrypoints and final `char**` handoffs to compatible runtime modules.
- AngelScript generic API plumbing, generated registration strings, handle-slot storage, and low-level `char*` / `char**` buffers and process argv. Script-visible export/event/hook handle signatures use `ptr<T>` / `nptr<T>`; see "Script binding boundary" below.
- `std::atomic<T*>` until a dedicated atomic nullable wrapper exists.

For engine-owned APIs outside these categories, prefer `ptr<T>` or `nptr<T>` for borrowed values and `unique_*` / `refcount_*` for owned values.

## Script binding boundary (`FO_SCRIPT_API`)

`///@ ExportMethod` script bindings are generated by `BuildTools/codegen.py`, and every script-visible handle pointer in the generated ABI is spelled with `ptr<T>` / `nptr<T>` (including `///@ ExportEvent`, `FO_ENTITY_EVENT`, `///@ ExportRefType`, and `///@ EngineHook` declarations). Codegen rejects bare raw handle pointers in exported/event signatures, and `NativeDataProvider` / `NativeDataCaller` static-assert if a raw handle pointer reaches the native marshalling templates.

- The engine/entity **receiver** (first parameter) is always non-null, so it is `ptr<EngineType>` / `ptr<EntityType>`.
- A non-null entity/ref argument or return is `ptr<T>`; a nullable one is `nptr<T>`. Bare `T*` no longer carries a nullable contract at the export/event boundary; spell the contract explicitly with the wrapper.
- **Container element types** use the wrapper vocabulary too: returns such as `vector<ptr<T>>` and parameters such as `readonly_vector<ptr<T>>` / `readonly_vector<nptr<T>>` are the supported handle-container shapes. Codegen reduces the element to the raw handle only for the AngelScript metadata (the registered `T@[]` signature and the compatibility hash are unchanged), while the generated extern and `NativeCall` cast keep the wrapper element type so the glue matches the C++ signature.
- `///@ ExportRefType` methods (e.g. the dialog accessors) likewise use `ptr<T>` / `nptr<T>` for handle returns and generated `ptr<RefType>` receivers. The script `.Ret` metadata still reduces to the handle type, so the registered script signature and compatibility hash are unchanged.
- The script-facing AngelScript registration is unchanged, so no `.fos`, bytecode, or save migration is required; only the C++ glue type changes. The client/server compatibility hash deliberately excludes the wrapper spelling.

These script-binding shapes intentionally stay raw at the ABI edge:

- Generic AngelScript plumbing, generated registration strings, handle slots, and `char*` / `char**` buffers and process argv.
- Third-party/native C callback surfaces that are not script handle contracts. Bind those raw inputs to `ptr<T>` / `nptr<T>` at the first engine-owned line with `make_ptr` / `make_nptr`.

Inside an export body, bind any remaining raw input to a wrapper before ordinary engine work and unwrap (`.get()`) only at the final ABI/handoff line. When a function's declared **return type is itself a raw pointer** (a `void*`/`T*` handed to a C or script API — SDL/ImGui/Spine allocators, script element accessors, generic readers), `return wrapper.get();` is allowed: the audit reads the function's return type (raw pointer, deduced `auto`, or a bare template parameter `-> T`) and treats such a `.get()` return as a genuine raw-pointer boundary rather than a wrapper unwrap. A `.get()` return whose enclosing function returns a **wrapper** (`nptr<T>`/`ptr<T>`) is still flagged — return the wrapper (`return x;`) or bind a named wrapper local. Direct `get_no_const()` is likewise allowed at the final mutable script ABI edge; no one-line `ScriptMutablePtr` / `Return*` bridge is required. A direct placement-new pointer write to `GetAddressOfReturnLocation()` is also a final AngelScript ABI handoff.


## Migration rules

1. Replace legacy `raw_ptr<T>` call sites with `ptr<T>` without changing behavior.
2. Replace legacy `nullable_raw_ptr<T>` call sites with `nptr<T>`.
3. Convert already-empty `ptr<T> ... {}` declarations to `nptr<T>`.
4. Convert already-empty `unique_ptr<T> ... {}` declarations to `unique_nptr<T>`.
5. Convert already-empty `refcount_ptr<T> ... {}` declarations to `refcount_nptr<T>`.
6. Review lookup APIs, current-state fields, and script/third-party boundary adapters before changing stricter contracts.
7. Let checked `nptr<T>` values flow directly to `ptr<T>` sites; use `auto borrow = value.as_ptr()` only when a distinct borrowed value is genuinely needed.
8. Use `unique_nptr<T>::take_not_null()` when optional unique ownership is explicitly narrowed back to `unique_ptr<T>`; use `take_not_null(unique_del_nptr<T>&)` for the same checked narrowing into `unique_del_ptr<T>`.
9. Let `refcount_ptr<T>` / `refcount_nptr<T>` owners borrow implicitly to `ptr<T>` / `nptr<T>`; use `refcount_nptr<T>::take_not_null()` only when optional intrusive-refcount ownership is explicitly narrowed back to `refcount_ptr<T>`.
10. Call owner `dyn_cast<T>()` directly instead of spelling an intermediate `.as_ptr().dyn_cast<T>()`; use explicit ownership helpers only when the cast result must acquire/retain ownership.
11. Change functions that return `nullptr` or `{}` from `ptr<T>`, `unique_ptr<T>`, or `refcount_ptr<T>` to the matching nullable return type unless absence is made impossible.
12. Enable strict non-null behavior only after the affected subsystem no longer relies on nullable operations through the non-null spelling.

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
  --raw-pointer-low-level-allowlist Tools/SmartPointerAudit/raw_pointer_low_level_allowlist.tsv \
  --raw-pointer-abi-allowlist Tools/SmartPointerAudit/raw_pointer_abi_allowlist.tsv \
  --raw-pointer-container-abi-allowlist Tools/SmartPointerAudit/raw_pointer_container_abi_allowlist.tsv \
  --raw-pointer-header-abi-allowlist Tools/SmartPointerAudit/raw_pointer_header_abi_allowlist.tsv
python3 Tools/SmartPointerAudit/smart_pointer_clang_query.py --diff-base origin/main --require-tooling
```

The first command is the regular textual non-regression audit. Exact line-level allowlists keep reviewed raw ABI and low-level raw rows from drifting; count budgets are not used, and class reference members fail directly without an allowlist. Nullable owners such as `unique_nptr<T>` and `unique_del_nptr<T>` are counted for inventory only, not quarantined. The `NullableLocalDereference` gate (a checked nullable local dereferenced with no preceding null check) is guard-aware, covers all strict engine / `SourceExt` scopes, and every hit is fixed at the source. The second command is the optional AST-backed clang-query gate for newly added raw pointer declarations in checked scopes after a build has produced `compile_commands.json`.

As an embedding-project example, Last Frontier runs the full audit invocation in CI on every push and exposes the same command locally as the `Analyze :: Smart Pointer Audit` VS Code task (bundled into its `Analyze All` and pre-commit validation tasks).
