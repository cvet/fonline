---
layout: default
title: Local Variables
locale: en
document_id: local-variables
permalink: /Docs/en/contributing/coding-contracts/local-variables.html
---

# Local Variables

This document owns the narrow first-party C++ rules for local type spelling,
redundant top-level `const`, and use-after-move diagnostics.

There is no engine-specific immutability-by-default rule. Local variables and
function or method parameters follow normal C++ mutation semantics and require
no annotation when they are written.

## Redundant local `const`

Do not add top-level `const` to an automatic local when removing it leaves the
type contract unchanged:

```cpp
int32_t count = GetCount();
item_ptr item = FindItem();
```

Preserve constness that is part of the value's actual type or semantics:

```cpp
const Item* item = FindItem();       // const pointee
const Item& item = GetItem();        // const referent
const int32_t values[] = {1, 2};     // const elements
constexpr int32_t limit = 10;        // required by constexpr
```

`int32_t* const pointer` has removable top-level const and is diagnosed.
Parameters are not part of this check.

The rare intentional top-level local qualifier can be suppressed on the
diagnostic line or the preceding line:

```cpp
const int32_t value = SelectOverload(); // FO_REDUNDANT_CONST_SUPPRESS: const is required for overload selection
```

The reason is mandatory and must contain at least eight characters.

## Explicit simple local types

Replace local `auto` only when all of these are true:

- Clang resolves one accessible, unqualified `snake_case` type name.
- The type has no visible template arguments.
- The type is not nested or namespace-qualified.
- Writing the explicit type does not introduce or hide a conversion.
- The declaration is not a structured binding, lambda, or dependent deduction.

When Clang exposes only a canonical template type, an unqualified alias is
eligible if that alias is explicitly spelled in the initializer, is the only
simple alias for the exact same desugared type, and is accessible at the
declaration. This covers calls such as `glm::translate(mat44 {...}, offset)`
without admitting library-internal names such as `iterator` or `value_type`.

Examples:

```cpp
int32_t count = GetCount();
float32_t factor = GetFactor();
hstring name = GetName();
```

Keep `auto` for cases such as:

```cpp
auto values = vector<int32_t> {};
auto iter = values.begin();                  // nested iterator type
auto handle = MakeHandle<void>();            // template spelling
auto pointer = GetRawPointer();               // explicit target may convert
auto [left, right] = Split(value);            // structured binding
```

When an eligible deduced numeric primitive is written explicitly, use the
engine's sized spelling:

| Deduced type | Explicit spelling |
|---|---|
| `short` | `int16_t` |
| `int` | `int32_t` |
| `float` | `float32_t` |
| `double` | `float64_t` |

Existing fixed-width signed and unsigned types keep their spelling.

See [Enforcement](#enforcement) for how this rule is checked.

## Use after move

Moving from a local does not make it mutable and needs no annotation. The
independent `bugprone-use-after-move` gate rejects subsequent use until a valid
reinitialization:

```cpp
value_type value = BuildValue();
Consume(std::move(value));
```

Intentional inspection of a moved-from object must state the contract:

```cpp
CHECK(value.empty()); // FO_USE_AFTER_MOVE_SUPPRESS: test verifies the moved-from container contract
```

The reason is mandatory and must contain at least eight characters.

## Enforcement

These rules are intended for a Clang 20+ `compile_commands.json` analyzer:
an explicit-type checker plus `clang-query` / `clang-tidy` checks for redundant
local constness and use after move. The engine owns only the rules above and the
`FO_REDUNDANT_CONST_SUPPRESS` / `FO_USE_AFTER_MOVE_SUPPRESS` markers. An
embedding project owns analyzer implementation, compile-database generation,
scope selection, and CI policy.

Run all three checks over changed engine sources before publication. A project
may extend the same gate to its native extensions, but project paths, task
names, and workflow jobs are not part of this reusable contract.

Both `clang-query` and `clang-tidy` must be present on the host running the
gate, and the compile database must support a real parse. Generated headers
reachable from the engine's common include chain must exist before analysis;
otherwise translation units fail on missing includes before any rule is
evaluated.

Prune the compilation database to one command per translation unit before passing it to Clang tooling. CMake emits one entry for every target that compiles a file; an Engine source may therefore appear dozens of times. Clang runs its action once per entry, and `clang-query` retains every built AST, so duplicates multiply both wall time and memory. One Engine AST is roughly half a gigabyte; substantially larger usage usually indicates an unpruned database rather than an inherent analyzer cost.
