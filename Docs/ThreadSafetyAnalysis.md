# Thread Safety Analysis

The engine annotates its conventional mutexes with [Clang Thread Safety Analysis](https://clang.llvm.org/docs/ThreadSafetyAnalysis.html)
(TSA) so that lock misuse — touching guarded state without the lock, forgetting a required capability, returning while
still holding a lock — is a **compile-time error** on every Clang toolchain. It is static, zero-cost, and complements
(does not replace) runtime checks and concurrency tests.

> TSA is a defense-in-depth layer for **conventional, lexically-scoped mutexes**. It deliberately does **not** model
> cooperative / dynamically-acquired lock schemes (see *Exemptions*).

> For exception-safety of lock acquisition — why an `EntityLock` post-grant invariant uses `FO_STRONG_ASSERT` (a
> waiter that woke non-aborted must be `GRANTED` and own the lock, or the process must stop) — see
> [ExceptionSafety.md](ExceptionSafety.md).

## Toolchain & enforcement

- Enabled in `BuildTools/cmake/stages/Init.cmake` for every Clang compiler id (native `clang`, `clang-cl`, AppleClang,
  Emscripten, Android NDK) as `-Wthread-safety -Werror=thread-safety` (routed through `/clang:` under the cl-style
  `clang-cl` driver). MSVC and GCC do not implement TSA, so the analysis never runs there — the **Clang build is the
  authoritative gate**.
- The `FO_TSA_*` macros expand to `__attribute__((...))` only under `__clang__`; on every other compiler they are no-ops,
  so annotated code builds unchanged on MSVC/GCC.
- Third-party libraries are silenced by `DisableLibWarnings` (`-w` plus `-Wno-error=` for the Clang-20+ legacy-C
  default-errors), so TSA only ever evaluates first-party engine + embedding-project code.

## Why the `fo::` mutex wrappers exist

The project links the platform STL (libc++ is disabled), and **neither MS STL nor libstdc++ annotates `std::mutex` /
`std::shared_mutex` as capabilities**. `FO_TSA_GUARDED_BY(std_mutex_member)` would therefore emit
`'guarded_by' attribute requires arguments whose type is annotated with 'capability'` and check nothing. So guarded
state must be protected with the engine's own annotated primitives (defined in `Source/Essentials/Threading.h`).
They are **drop-in replacements for the std analogues** — same names in snake_case, same method names — so a lock site
is a plain `std::` → `fo::` swap:

| Type | Wraps / mirrors | Use for |
|------|-----------------|---------|
| `fo::mutex` | `std::mutex` | exclusive-only state |
| `fo::shared_mutex` | `std::shared_mutex` | reader/writer state |
| `fo::scoped_lock<T>` | `std::scoped_lock` / `std::lock_guard` | exclusive RAII guard for `mutex` *or* `shared_mutex` (CTAD: `scoped_lock lk {m}`) |
| `fo::shared_lock<T>` | `std::shared_lock` | shared (reader) RAII guard for `shared_mutex` |
| `fo::unique_lock<T>` | `std::unique_lock` | exclusive guard with manual `lock()`/`unlock()`, usable with `std::condition_variable_any` |

`std::scoped_lock` / `std::unique_lock` / `std::shared_lock` are opaque to the analyzer under the platform STL, so the
`fo::` guards must be used at lock sites for the analysis to track them. Engine code lives inside `FO_BEGIN_NAMESPACE`,
so the names are used unqualified (`mutex`, `scoped_lock`, …). Include `Threading.h` where you use them (it sits low in
the Essentials layer, just above `HashedString`, so even low-layer headers can include it).

Condition variables use `std::condition_variable_any` (it accepts any lockable, including `fo::unique_lock`); pass the
guard directly to `wait(...)`:

```cpp
unique_lock lock {_dataLocker};
_workSignal.wait(lock, [this]() FO_TSA_REQUIRES(_dataLocker) { return _ready; });
```

## Annotating a mutex

1. Make the mutex a `mutex` / `shared_mutex`, declared **before** the fields it guards.
2. `FO_TSA_GUARDED_BY(_locker)` on every data member protected by it.
3. At lock sites (a plain `std::` → `fo::` swap): `std::scoped_lock`/`std::lock_guard` → `scoped_lock`;
   `std::shared_lock` → `shared_lock`; a plain exclusive `std::unique_lock` → `scoped_lock`; a `std::unique_lock` used
   with a condition variable or manual relock → `unique_lock` (and switch the cv to `std::condition_variable_any`).
   Declare guards with brace initialization uniformly: `scoped_lock lock {mutex};`, `shared_lock lock {mutex};`.
4. Private helpers that assume the lock is already held: `FO_TSA_REQUIRES(_locker)` (exclusive) or
   `FO_TSA_REQUIRES_SHARED(_locker)` (read) on the declaration.
5. Hand-rolled RAII guards: mark the class `FO_TSA_SCOPED_CAPABILITY`, the ctor `FO_TSA_ACQUIRE(mutex)`, the dtor
   `FO_TSA_RELEASE()`; a move ctor (if any) needs `FO_TSA_NO_ANALYSIS`.

> Attribute placement: never put an attribute between `()` and a trailing `-> type`. Use a leading return type on
> annotated methods (e.g. `bool try_lock() FO_TSA_TRY_ACQUIRE(true)`).
>
> `fo::thread` (the pool task handle, `threading::run_thread`'s return) also lives in `Threading.h` in the `fo`
> namespace.

## Macro vocabulary

`FO_TSA_CAPABILITY(name)`, `FO_TSA_SCOPED_CAPABILITY`, `FO_TSA_GUARDED_BY(x)`, `FO_TSA_PT_GUARDED_BY(x)`,
`FO_TSA_ACQUIRED_BEFORE/AFTER(...)`, `FO_TSA_REQUIRES(...)`, `FO_TSA_REQUIRES_SHARED(...)`, `FO_TSA_ACQUIRE(...)`,
`FO_TSA_ACQUIRE_SHARED(...)`, `FO_TSA_RELEASE(...)`, `FO_TSA_RELEASE_SHARED(...)`, `FO_TSA_RELEASE_GENERIC(...)`,
`FO_TSA_TRY_ACQUIRE(...)`, `FO_TSA_TRY_ACQUIRE_SHARED(...)`, `FO_TSA_EXCLUDES(...)`, `FO_TSA_ASSERT_CAPABILITY(x)`,
`FO_TSA_ASSERT_SHARED_CAPABILITY(x)`, `FO_TSA_RETURN_CAPABILITY(x)`, `FO_TSA_NO_ANALYSIS`.

## Exemptions (what TSA does NOT cover)

- **`std::recursive_mutex`** — re-entrant acquisition cannot be modeled. Recursive locks stay raw `std::recursive_mutex`
  with no `GUARDED_BY`; mark them `// recursive: not modelable by TSA`.
- **Single-threaded init/teardown** that sweeps guarded state while re-entering the locking code (so it cannot hold the
  lock) — mark the function (and any inner lambda separately, as a lambda is its own analysis scope) `FO_TSA_NO_ANALYSIS`
  with a comment stating why it is single-threaded. Use sparingly; never to hide a real race.
- **Cooperative / dynamically-acquired lock schemes** whose held set is data-dependent and acquired non-lexically cannot
  be expressed; leave them unannotated (they reference no capability, so they need no escape hatch) and document the
  exemption in the owning subsystem doc.

Embedding projects document their own guarded-field inventory and project-specific exemptions in their threading docs;
this file owns only the reusable mechanism.
