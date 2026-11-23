---
layout: default
title: Nullability
locale: en
document_id: nullability
permalink: /Docs/en/contributing/coding-contracts/nullability.html
---

# Nullability

> Engine-owned documentation. This page defines the reusable compiler, runtime, and native-boundary contract. Project-side analyzers may enforce stricter authoring policy, but they are not part of the engine contract.

Convention and runtime enforcement for nullable values across AngelScript, Managed C#, and the native engine boundary. For the broader scripting runtime, see [Scripting](../../explanation/scripting-runtime/); for exported native method ownership, see [Script Methods Map](../../reference/script-api/method-ownership.md).

## Core principle

> Better to not pass `null` at all than to defensively check inside and bail out.
>
> A parameter or return may be marked nullable **only when the function meaningfully handles both null and non-null cases**. Early-exit-on-null guards are a code smell — the contract should be non-null and the caller fixed instead.

It applies symmetrically on both sides of the script-engine boundary.

## Managed C# side

Managed generation enables nullable reference types and maps the metadata bit to C# `?` on entity, string, and ref-type references; value types never carry it. `ptr<T>` is non-null; `nptr<T>`, a nullable property flag, or a nullable event/remote-call tag becomes `T?`. Generated methods, properties, events, delegates, and remote-call callers preserve the spelling so Roslyn can diagnose unchecked dereferences and incompatible assignments at compile time.

C# annotations do not guarantee runtime ownership. A managed wrapper stays borrowed, may point to an entity destroyed after an `await`, and must be re-resolved and revalidated with server cover. Narrow expected absence with an `is null` / `is not null` branch. For invariants, use `Game.VerifyNotNull(value, message)` or `Game.Verify(...)` and keep the narrowed value; use `!` to suppress a warning only when an external API hides the proof from the compiler.

Metadata declarations stay authoritative across backends: a nullable `///@ Event` or `///@ RemoteCall` argument generates a nullable C# parameter, and the attributed `[Event]`, `[ServerRemoteCall]`, or `[ClientRemoteCall]` handler must preserve that contract. Managed validation compiles the generated project with nullable diagnostics enabled and configured Roslyn analyzers, plus a runtime callback/serialization test with legitimate null. See [Managed C# Scripting](../../how-to/scripting/managed-csharp.md) for generation, async lifetime, analyzers, and packaging.

## AngelScript side: `T?` suffix

AngelScript marks nullability with a Kotlin/C#-style `?` type suffix; default is **non-nullable**.

```angelscript
// Return may be null
Location? GetCritterLocation(Critter cr)
{
    if (cr.MapId == ZERO_IDENT) {
        return null;
    }
    Map map = cr.GetMap();
    return map != null ? map.GetLocation() : null;
}

// Parameter may be null — body handles both cases
void ResolveTargetHex(Critter cr, Critter? target, mpos fallbackHex)
{
    mpos resolvedTargetHex = target != null ? target.Hex : fallbackHex;
    // ...
}
```

The **AngelScript front-end itself** parses `?` (see `ParseType` in [../ThirdParty/AngelScript/sdk/angelscript/source/as_parser.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_parser.cpp), `MakeNullable`/`isNullable` in [../ThirdParty/AngelScript/sdk/angelscript/source/as_datatype.h](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_datatype.h), and `CreateDataTypeFromNode` in [../ThirdParty/AngelScript/sdk/angelscript/source/as_builder.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_builder.cpp)). The preprocessor no longer rewrites it: `StripNullableTypeSuffix` was removed when the engine learned the suffix directly. Misplaced markers (e.g. `int?`) produce a compile-time error: *"Nullable marker '?' is only allowed on handle types"*.

The AS parser distinguishes type-suffix `?` from ternary `?` by consuming it only in `ParseType`; expressions keep `cond ? a : b` as the conditional operator.

### `///@ Event` and `///@ RemoteCall` declarations

The same `?` suffix works in `///@ Event` and `///@ RemoteCall` tag declarations; [`MetadataBaker`](../../../../Source/Tools/MetadataBaker.cpp) propagates the per-arg bit into baked metadata (`ArgDesc::Nullable` on `EntityEventDesc::Args` / `RemoteCallDesc::Args`).

```angelscript
///@ Event Server Game OnCritterDamaged(Critter cr, Critter? attacker, int32 damage)
///@ Event Server Game OnCritterDead(Critter critter, Critter? killer)
///@ RemoteCall Server SwitchCharacter(Critter? newCritter)
```

The declaration is the contract: every `[[Event]]` subscriber and every `[[ServerRemoteCall]]` / `[[ClientRemoteCall]]` implementation matching the event/call name must use the same `?` marker per argument. The baker and side-specific binding enforce that for remote calls; see [Remote Calls](../../reference/scripting/remote-calls.md). `[[AdminRemoteCall]]` is a separate command entry point, not a `///@ RemoteCall` target.

```angelscript
// Matches the OnCritterDamaged declaration above.
[[Event]]
void OnCritterDamaged(Critter cr, Critter? attacker, int32 damage) { ... }

// Violates declaration parity: declaration has `Critter?`, handler drops `?`.
[[Event]]
void OnCritterDamaged(Critter cr, Critter attacker, int32 damage) { ... }
```

Because the front-end tracks the per-type nullable bit, the AS engine enforces null contracts on handle writes at runtime (see "Runtime enforcement" below). Event and remote-call declaration parity remains project-side static analysis: the engine cannot know two unrelated declarations share a contract.

## Engine side: `ptr<T>` / `nptr<T>` and raw-pointer nullability

Native `///@ ExportMethod` declarations in [../Source/Scripting/](../../../../Source/Scripting/), exported events, `FO_ENTITY_EVENT` payloads, `///@ ExportRefType` methods, and `///@ EngineHook` declarations use smart pointers: non-null entity/engine/ref pointers are `ptr<T>`, nullable ones `nptr<T>` (see [SmartPointers.md](smart-pointers.md), "Script binding boundary"). Codegen rejects raw handle pointers (`T*`) in exports/events; native marshalling templates static-assert if one reaches the ABI. Raw pointers in AngelScript plumbing, generated registration strings, handle slots, process argv, or external C callbacks are low-level ABI values; wrap them with `make_ptr(raw)` / `make_nptr(raw)` before engine work. Owning files are mapped in [Script Methods Map](../../reference/script-api/method-ownership.md). (The empty `FO_NULLABLE` macro is gone; the `ptr<T>` / `nptr<T>` spelling carries nullability.)

Outside exported signatures, native C++ uses the [SmartPointers.md](smart-pointers.md) vocabulary: `ptr<T>` for borrowed non-null values, `nptr<T>` for borrowed nullable values, and matching `unique_*` / `refcount_*` owning forms. `nptr<T>`, `unique_nptr<T>`, and `refcount_nptr<T>` are separate nullable wrappers; migration still tightens nullable operations on non-null spellings. For a borrow used only for a dynamic cast, call `owner.dyn_cast<T>()` directly, not through an intermediate `owner.as_ptr().dyn_cast<T>()`.

Stored native `ScriptFunc` pointer spellings must match the callback declaration argument by argument: `Item?` stores and looks up as `nptr<Item>`, `Item` as `ptr<Item>`. Otherwise a legitimate `null` can cross the exported boundary successfully, then fail when the callback wrapper implicitly narrows it.

**Prefer non-null; use `nptr<T>` only when absence is real and handled.** Drop nullable wrappers when callers always pass non-null, a function never returns null, or a member is always set before use; use `ptr<T>` / `unique_ptr<T>` / `refcount_ptr<T>`. Do **not** make a raw `pointer + size` buffer nullable for an empty `nullptr`; use non-null `const_span<uint8_t>` / `span<uint8_t>` (the engine's byte-buffer vocabulary) and `.empty()`. This removes spurious nullability and the `nptr<T> x = nullptr; if (!c.empty()) x = c.data(); f(x, c.size())` boilerplate at every call site (`f(c)`). Keep `nptr<T>` for legitimate fallible casts/lookups, transient-null data members between construction and assignment, or defensive boundary helpers deliberately accepting and asserting a nullable. **After the guard, dereference the checked `nptr<T>` directly** (`if (!x) { ... return; }` / `FO_VERIFY_AND_THROW(x, ...)` then `x->`), not through a `nullable_x` narrowing alias. The checked `nptr<T>` flows **implicitly** into any `ptr<T>` parameter, member, or return, the `nptr<T>`→`ptr<T>` conversion asserting non-null. Owning wrappers (`refcount_ptr`/`refcount_nptr`, `unique_ptr`/`unique_nptr`, `unique_del_ptr`/`unique_del_nptr`, `unique_arr_ptr`, and `shared_ptr`) also borrow implicitly to `ptr<T>` / `nptr<T>`; ownership acquisition and nullable-owner ownership narrowing stay explicit (`hold_ref`, `adopt_unique_ptr`, `make_unique_del_ptr`, `take_not_null`, `safe_alloc::make_shared`, or a domain factory). Direct owner dynamic casts (`owner.dyn_cast<T>()`) avoid an intermediate borrow; freshly assigned non-null factory results can use the source owner for a few member accesses. Nullable/nullable-owner locals needing presence bind with `auto`, check with `FO_VERIFY_AND_THROW(local, ...)` or `FO_STRONG_ASSERT(local, ...)` inside `noexcept`, then dereference; do not double-negate the local in these guards. Explicit `.as_ptr()` / `.as_nptr()` calls serve borrow clarity or overload/template deduction; implicit conversion suffices when unambiguous. Raw pointers entering native code use `make_ptr(raw_value)` / `make_nptr(raw_value)`. The audit enforces guarded nullable dereference via `NullableLocalDereference`; see [SmartPointers.md](smart-pointers.md).

```cpp
///@ ExportMethod
FO_SCRIPT_API nptr<Map> Server_Critter_GetMap(ptr<Critter> self)
{
    return self->GetEngine()->EntityMngr.GetMap(self->GetMapId());
}

///@ ExportMethod
FO_SCRIPT_API void Server_Player_SwitchCritter(ptr<Player> self, nptr<Critter> cr)
{
    self->GetEngine()->SwitchPlayerCritter(self, cr);
}
```

The `self` first parameter (`this` receiver) and implicit `engine` parameter of global methods are **never** marked; AS validates `this` before dispatch.

An exported pointer argument defaulting to `nullptr` must be `nptr<T>`; codegen records it as script `null`, keeping native-call validation aligned with the callable signature. A non-null `ptr<T>` argument cannot default to `nullptr`.

### Component accessors are non-nullable and throw; probe with `Has<Component>`

An entity component getter (`item.Weapon`, `item.MapExit`, `cr.DialogContext`, `item.Locker`, … — every property declared `///@ Property <Entity> ... <Name> Component`) is **non-nullable** and **throws** when absent. `Entity_GetComponent` in [../Source/Scripting/AngelScript/AngelScriptEntity.cpp](../../../../Source/Scripting/AngelScript/AngelScriptEntity.cpp) raises `ScriptException` unless the presence bool is set, so the getter registers as `{}@ get_{}() const` without `?`. A `bool get_Has<Component>() const` accessor beside each getter probes presence.

```angelscript
// item.Weapon is ItemWeaponComponent (non-nullable) — access directly
int dist = item.Weapon.MaxDist;        // OK; throws iff the item is not a weapon

// probe presence with Has<Component>, never `== null`
if (item.HasWeapon) {
    int d = item.Weapon.MaxDist;        // guarded
}
verify(item.HasWeapon, "Item must be a weapon");
```

Like the throwing getters (`Chosen` / `CurMap` / …), a missing component where code assumes presence is an **invariant violation**, not a recoverable null. Never write `item.Weapon == null` / `!= null`; the getter throws. Use `!item.HasWeapon` / `item.HasWeapon`. All entity flavors (concrete / `Abstract` / `Proto` / `Static`) and fixed types register the same probes, so `proto.HasWeapon`, `abstractItem.HasWeapon`, and similar probes exist. (Name clash: `Ammo` is an Item component and a nullable *property* of the weapon component; `item.Weapon.Ammo` is the loaded ammo item and a normal nullable `== null` check.)

### Throwing global getters: `Chosen` / `CurMap` / `CurLocation` / `CurPlayer`

The client global getters `Chosen`, `CurMap`, `CurLocation`, `CurPlayer` (registered in [../Source/Scripting/ClientGlobalScriptMethods.cpp](../../../../Source/Scripting/ClientGlobalScriptMethods.cpp)) are **non-nullable** and **throw** when absent (e.g. "No chosen critter"). Deliberately, most client code runs only where they exist (the in-game UI is disabled without `Chosen`), so read `Chosen.X` directly without a null dance; where absence is valid, use the matching bool predicate:

```angelscript
if (!HasChosen) {
    return;        // no chosen critter right now - handle it
}
Critter cr = Chosen;   // OK - non-nullable, guaranteed present here
```

`HasChosen` / `HasCurMap` / `HasCurLocation` / `HasCurPlayer` are presence checks. Do **not** write `Chosen == null` / `Chosen != null`: comparing a non-nullable handle to null trips redundant-comparison warning (#5) and *throws* when `Chosen` is absent — doubly wrong. Use `!HasChosen` / `HasChosen`.

### `Game` during shutdown: `IsGameDestroying`

`Game` is a **non-nullable** global handle (`GameSingleton@`), so read `Game.X` directly. But the game engine is genuinely absent in one window: **script-object destructors that run while the scripting backend is being torn down**. The AngelScript GC runs them inside `~AngelScriptBackend` *after* the engine pointer resets ([../Source/Scripting/AngelScript/AngelScriptBackend.cpp](../../../../Source/Scripting/AngelScript/AngelScriptBackend.cpp)); `get_Game` is a **throwing getter** raising *"Game engine is not available"* without an engine. A `~T()` touching `Game.*` then throws, and `Game != null` cannot guard it (same double-wrong as `Chosen != null`: redundant-comparison warning #5 plus `Game` throwing when evaluated).

The `bool IsGameDestroying` global getter (registered next to `get_Game()` in [../Source/Scripting/AngelScript/AngelScriptGlobals.cpp](../../../../Source/Scripting/AngelScript/AngelScriptGlobals.cpp)) probes this case: `true` precisely when `Game` is unavailable (backend `HasGameEngine()` false, the same condition under which `get_Game` throws), and reading it never evaluates `Game`. Use it to skip engine-dependent destructor cleanup:

```angelscript
~Sprite()
{
    // Game engine may already be gone during shutdown; freeing the sprite then is both impossible and unnecessary
    if (!IsGameDestroying) {
        Unload();   // calls Game.FreeSprite(...)
    }
}
```

Use `IsGameDestroying` only in destructors or other teardown paths during backend destruction; elsewhere `Game` is guaranteed present, so read it directly.

### Throwing proto getters: `Game.GetProtoItem/Critter/Map/Location` + `CheckProtoX`

`Game.GetProtoItem`, `GetProtoCritter`, `GetProtoMap`, `GetProtoLocation` (in [../Source/Scripting/CommonGlobalScriptMethods.cpp](../../../../Source/Scripting/CommonGlobalScriptMethods.cpp)) are **non-nullable** and **throw** when no proto with that id exists (e.g. "Item proto not found"). Each has a matching `Game.CheckProtoItem/Critter/Map/Location(pid)` bool predicate for legitimately missing ids (stale checkpoint/map data, user-supplied ids, …).

```angelscript
// id known to exist - read directly:
ProtoItem proto = Game.GetProtoItem(Content::Item::Dynamite);

// id may be missing - probe first, or keep a nullable local via a guarded ternary:
if (!Game.CheckProtoMap(mapPid)) {
    return;            // unknown map proto - handle it
}
ProtoMap proto = Game.GetProtoMap(mapPid);

ProtoLocation? loc = Game.CheckProtoLocation(locPid) ? Game.GetProtoLocation(locPid) : null;
if (loc == null) { /* recover */ }
```

Do **not** write `Game.GetProtoX(pid) == null` / `!= null`; it throws on a missing proto before the comparison. Use `!Game.CheckProtoX(pid)` / `Game.CheckProtoX(pid)`.

The same applies to **codegen-generated proto/fixed-type getters** for custom entities (`Game.GetProtoModifier`, `Game.GetProtoFaction`, `Game.GetEncounterProfileData`, `Game.GetWeatherType`, `Game.GetItemBag`, …): they are non-nullable and **throw** on unknown ids, with a matching `Game.Check<Name>(pid)` predicate. They register in `register_entity_protos` / `register_fixed_type` (`Game_GetProtoCustomEntity` / `Game_CheckProtoCustomEntity`) in [AngelScriptEntity.cpp](../../../../Source/Scripting/AngelScript/AngelScriptEntity.cpp). Read directly for known ids (`instance.ProtoId`, an authored content id); probe with `Check<Name>` only where absence is legitimate.

## Runtime enforcement

There are now two complementary runtime gates:

Managed indexed interop applies the same contract to its packed ABI.
Scalar and plain fixed-value data are non-nullable value slots. Entity,
prototype, fixed entity, and native reference arguments use pointer-sized
handle slots with nullable bits from the generated `ManagedInteropAbi`
manifest. ABI binding rejects a generated/native mismatch before scripts
start, while the generated wrapper performs the required null check at
the call boundary. Do not encode optional references as zero-valued
fixed data or relax metadata to work around binding failures.

### Script-side: `asBC_RefCpyChk` on handle assignments

The compiler emits new `asBC_RefCpyChk` bytecode (defined in [../ThirdParty/AngelScript/sdk/angelscript/include/angelscript.h](../../../../ThirdParty/AngelScript/sdk/angelscript/include/angelscript.h), handler in [../ThirdParty/AngelScript/sdk/angelscript/source/as_context.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_context.cpp)) when a handle write targets a **non-nullable**, **user-declared** destination (`T` without `?`, not a compiler temporary). This drop-in REFCPY variant raises a *Null assignment to non-nullable handle* exception when the source handle is null. `PerformAssignment` and `CompileInitializationWithAssignment` in [../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp) emit it; `T?` declarations use `asBC_REFCPY` and accept null silently.

The non-nullable test uses the **declared** destination type, not a smart-cast view: a narrowed declared-nullable local or `&` parameter (see [Smart-cast](#smart-cast-flow-sensitive-narrowing)) remains a `REFCPY` destination, so `x = null;` inside the guard is legal *un-narrowing*, not a null write to a non-nullable slot. `PerformAssignment` restores declared nullability before choosing the instruction, then invalidates narrowing, so the next read sees `T?`. (Previously the narrowed type chose the instruction, making the branch's `asBC_RefCpyChk` always throw; the *"Null assignment to non-nullable handle"* ScriptExceptions from `Combat::DeferredAttackHit` and cursor handling were this defect.)

The temporary guard matters because `PerformAssignment` also serves argument slots typed from a native parameter (no `?` on the AS side). Native-boundary validation owns their nullability; skipping temporaries keeps `func(null)` working for nullable (`nptr<T>`) natives.

Thus **script-to-script assignments** of null to a non-nullable handle still throw, even when the value comes from another script function rather than a native call.

### Native-boundary: codegen-emitted arg/return checks

Native validation still flows through codegen-generated `MethodDesc::Call` lambdas, **not** the AS-to-native bridge. [../BuildTools/codegen.py](../../../../BuildTools/codegen.py) emits per-method `NativeDataProvider::CheckArgNotNull` / `CheckReturnNotNull` calls (defined in [../Source/Common/ScriptSystem.h](../../../../Source/Common/ScriptSystem.h)) immediately before and after invocation:

```text
MethodDesc::Call(call)
  → NativeDataProvider::CheckArgNotNull(call, i, "Server_Player_SetCritter", "cr", "Critter")   // for each non-nullable entity arg
  → native invocation
  → NativeDataProvider::CheckReturnNotNull(call, "...", "...")                                  // for non-nullable entity return
```

Array parameters and returns are not blanket-scanned here. Script arrays may intentionally hold null handle cells (sparse grids, nullable-element arrays, caller-owned filtering), and export metadata has no per-element nullability bit distinguishing `array<T>` from `array<T?>` for generated C++ wrappers. If an API requires non-null elements, enforce and document that invariant in its API-specific producer/consumer; do not rely on a global codegen check that would reject legitimate nullable arrays.

Scalar checks at the `MethodDesc::Call` boundary cover **every** `///@ ExportMethod` caller: the AS-to-native bridge, native test harnesses, future Mono-backend dispatch, anyone. Each costs one pointer compare.

Violations surface as `ScriptException` propagated to the calling AngelScript context, with three distinct messages:

- *"Null assignment to non-nullable handle"* — `asBC_RefCpyChk`, the new AS-side bare-handle write check. Distinct from the generic null-deref, so stack traces point at the bad assignment rather than a downstream call.
- *"Null pointer access"* — the original AS message from `asBC_CHKREF` / `asBC_ChkRefS` / `asBC_ChkNullV` / `asBC_ChkNullS` for null-handle dereferences, indexing, and method calls.
- Native-boundary scalar checks emit the method name, parameter name when applicable, and type via the codegen-generated `NativeDataProvider::CheckArgNotNull` / `CheckReturnNotNull`.

### Compile-time guarantees

Besides runtime `asBC_RefCpyChk`, the front-end raises two compile-time errors on null-unsafe assignments before emitting bytecode:

1. **Bare `null` to a non-nullable handle is always rejected.** Always on, with no engine property, it emits:
   *"Cannot assign 'null' to a non-nullable handle of type 'T' (use 'T?' to allow null)"*.
   It catches `T x = null;`, `someField = null;`, and implicit `T x;` null initialization.

2. **A nullable handle source to a non-nullable destination is rejected when `asEP_DISALLOW_NULLABLE_TO_NON_NULLABLE` is enabled.** FOnline enables it in [../Source/Scripting/AngelScript/AngelScriptBackend.cpp](../../../../Source/Scripting/AngelScript/AngelScriptBackend.cpp). It emits:
   *"Cannot assign nullable 'T?' to non-nullable 'T' without a null-check (add `if (src != null)` or change the destination to 'T?')"*.

3. **Redundant `?` on local initializer is warned about.** When destination `T?` has a statically non-nullable initializer (the source type cannot produce `null`), the front-end emits:
   *"Redundant '?': initializer of type 'T' cannot be null; declare destination as 'T' instead"*.
   This catches stale `T?` after API return-type tightening (e.g. `nptr<Critter>` → `ptr<Critter>`) and the usual `if (cr == null)` dead branch. `CompileInitialization` in [../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp) emits it as a *warning*, so scripts keep building during cleanup; the message names both types, so the fix is one character. Like deref (#4) and redundant-comparison (#5), it **trusts static source non-nullability**, including non-const handle refs: an `arr[i]` cell of a non-nullable array, or a non-nullable `T` field/local by reference, aliases storage whose writes enforce the invariant, so `T? x = arr[i]` is flagged as `arr[i] != null` by #5. Two source shapes are exempt because static non-nullability cannot guarantee a non-null value:
   - `cast<T>(...)` and `cond ? a : b` — failed casts yield null and branch types may differ (use `cast<T?>` when failure is possible).
   - a **const handle reference `T@const&`** — `dict.get(key, default)` substitutes a possibly-null default, yielding null on a missing key, so the `?` on `T? v = someDict.get(k, null)` is needed, not redundant.

4. **Dereferencing an un-narrowed nullable handle is warned about.** Dereferencing a `T?` (`x.Member`, `x[i]`, `x.Method()`) without narrowing to non-null makes the front-end emit:
   *"Dereference of nullable handle 'T?' without a null-check; narrow it first (e.g. `if (x == null) return;`) so it becomes 'T'"*.
   `ttDot` in `CompileExpressionPostOp` ([../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp)) emits it. This *warning*, not error, leaves the runtime null-deref check (`asBC_CHKREF` etc.) as the safety net, so authors clean up at their pace. Narrowing tracks only **named locals/params**: `x.Method() == null || x.Method().Foo` still warns on the second call because each getter call is fresh; bind a local (`auto v = x.Method(); if (v == null) return; v.Foo`) to narrow.

5. **Redundant null comparison against a non-nullable handle is warned about.** Comparing `==` / `!=` with `null` when the other operand is statically non-nullable — **a local/param *or* a temporary** such as a property/getter result — is constant, so the guarded branch is dead. The front-end emits:
   *"Redundant null comparison: 'T' is a non-nullable handle and can never be null; remove the check (or make the source nullable if it actually can be null)"*.
   `CompileOperatorOnHandles` in [../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp) emits it; with deref warning (#4), it pushes every nullable value to one null-check where introduced. Like #3/#4, it is a *warning*. Its "or make the source nullable" hint matters: a hit usually means the **source** is mis-modeled non-nullable, so spell it nullable instead of deleting the check. Cases:
   - A **throwing getter** (`Chosen`, `CurMap`, a `item.Weapon` getter, `Game.GetProtoModifier(...)`) returns non-null and throws when absent; if absent, `getter() != null` throws before comparison. Use the matching `Has*` / `Check*` probe (`HasChosen`, `item.HasWeapon`, `Game.CheckProtoModifier(...)`).
   - A **fallible reference cast** `cast<T>(x)` is non-null-typed but may yield null; `cast<T?>(x)` gives `T?` and legitimate `!= null` (see [Reference casts](#reference-casts)).
   - A **proto / fixed-type property handle** that may be unset (`UsableOn.TargetItem`, `Harvested.SmallBag`, `Weapon.Ammo`) needs its `///@ Property` tagged `Nullable` so the getter registers `@?` (see [Nullable property handles](#nullable-property-handles)).
   - A null-capable native return spelled non-null `ptr<T>` should be `nptr<T>`; remove a truly dead check on a non-null source.

   Both named operands and temporaries are checked: a local binding does **not** silence the warning; only a nullable source spelling (`cast<T?>`, `Nullable`, `nptr<T>`) does. The *type* must tell the truth, not a hidden runtime-null.

**Conditional expressions propagate nullability.** `cond ? a : b` is `T?` when either branch is statically nullable or `null`. `CompileCondition` (FOnline patch in [../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp)) captures this before branch-type unification can drop the flag, then reapplies it to the result. Assignment (#2), deref (#4), and redundant-comparison (#5) checks can therefore flag `Entity x = cond ? a? : b` as nullable at compile time, not only through runtime `asBC_RefCpyChk`. The redundant-`?` exemption for #3 ternaries remains: statically non-null branches may still yield null through a failed reference cast, so `T? x = cond ? a : b` is not reported as redundant `?`. (Diagnostics only; bytecode is unchanged because runtime `asBC_RefCpyChk` keys off the assignment *destination*, so no compatibility-version bump is needed.)

Runtime `asBC_RefCpyChk` remains the safety net beneath both errors: even when compile-time checking accepts an assignment (e.g. `dict<K, V>.get(key, null)` returning a statically non-nullable reference bound at runtime to null), bytecode still throws on null write.

### Identity comparison: `==` only, never `is` / `!is`

Project convention **bans `is` and `!is` in `.fos`**. Use `==` and `!=` for null checks and handle-identity comparison. A FOnline patch on `CompileOperator` ([../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp)) falls back to `asBC_CmpPtr` (handle identity) when no `opEquals` is registered for the ref type, so both forms produce the same bytecode:

| Operands | `==` / `!=` behavior |
|----------|-----|
| Both sides are entity types (`Critter`, `Item`, `Map`, ...) with codegen-emitted `opEquals` | id-based equality (compares entity id) |
| Either side is `null` | null-handle compare |
| Both sides are ref types **without** `opEquals` (`Gui::Screen`, `Object`, custom classes, funcdef handles) | handle-identity (`asBC_CmpPtr`) - same as `is` |
| One side is a reference and the other is a handle | implicit conversion to handle, then handle-identity |

The *type*, not the operator, therefore determines "id-based" versus "pointer-based" behavior; the operator adds no information, and `is` is pure cognitive noise. Projects adopting the `==` / `!=` convention should enforce it with a read-only source check in CI.

### Smart-cast (flow-sensitive narrowing)

Kotlin-style smart-cast narrows a `T?` local to `T` in a provably non-null region, so no explicit cast is needed. A per-scope `smartCasts` stack in [../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp) drives `DetectNullCheckPattern`, `GetNarrowedTypeForLocal`, and `CompileIfStatement` (guards), `CompileCondition` (ternaries), and `CompilePostFixExpression` (`&&` / `||` short-circuit operands). The `!=` / `==` result context is tagged in `CompileOperatorOnHandles` with a **list** of narrowed locals (`nullCheckNarrowList` on `asCExprContext`); chained `&&` (keeps `!=`-tags) / `||` (keeps `==`-tags) **accumulates all** matching-polarity narrowings from both operands, so `a != null && b != null && a.X && b.Y` narrows both `a` and `b` in later operands, not just the nearest check. When a tagged operand reaches the postfix evaluation stack, `ShortCircuitNarrowsRightOperand` scans ahead to confirm it is the *left* operand of a matching short circuit and, if so, pushes every list narrowing for that operator's **entire right operand** (tracked by expr-stack index and popped at the operator). Thus `x != null && x.Prop == y` narrows `x.Prop`, not only the `&&`-adjacent term. Each tag stores the local's stack offset; parameter offsets are **negative**, so `nullCheckNarrowValid`, not the sign, marks "tag present"; otherwise every parameter would be skipped. Then/else guards that "exit without falling through" (enabling complementary narrowing afterwards) include `return`/`throw` and `break` / `continue` (`StatementAbortsFallThrough`), so `if (x == null) continue;` in a loop narrows `x` for the rest like an early `return`. This narrowing-only signal never feeds function-must-return-value analysis, which still ignores `break`/`continue`.

Supported patterns:

```angelscript
// 1) `if (x != null) { ... }` narrows in the then-branch
Item? maybeItem = GetMaybeItem();
if (maybeItem != null) {
    Item item = maybeItem;        // OK — compiler treats maybeItem as Item here
    item.Use();
}

// 2) `if (x == null) { <recover>; return; } <code>` — early-exit narrows after the if
Critter? maybeCr = GetMaybeCr();
if (maybeCr == null) {
    Logging::Warning("CharacterRoster", "main_critter_missing player=" + player.Name);
    return null;
}
Critter cr = maybeCr;             // OK — the early return rules out null

// 2b) break / continue guards narrow the same way (loop bodies)
for (int i = 0; i < ids.length(); i++) {
    Critter? probe = Game.GetCritter(ids[i]);
    if (probe == null) {
        continue;                 // bails this iteration
    }
    probe.Use();                  // OK — narrowed for the rest of the loop body
}

// 3) Compound `&&` / `||` shapes narrow every recognised atom
if (a != null && b != null && c != null) {
    Item ai = a; Item bi = b; Item ci = c;   // OK — all three narrowed
}
if (a == null || b == null) {
    return;
}
Item ai = a; Item bi = b;                    // OK — both narrowed after early return

// 4) Assignment invalidates the narrowing on that local. The write itself goes
//    through the DECLARED type — narrowing is a read-time refinement only — so
//    `x = null;` inside the guard is a legal un-narrowing write (plain REFCPY),
//    not a null write into a non-nullable slot.
if (x != null) {
    x = GetMaybeNull();           // x becomes nullable again
    Item y = x;                   // compile-time error here
}
if (x != null && x.IsBroken()) {
    x = null;                     // OK — drops the narrowed view, x is `Item?` again
}

// 5) `&&` / `||` short-circuit narrows every later operand in the chain (any
//    expression, not just an `if` condition). `&&` consumes a `!=` check (the
//    rest of the chain runs only when the check was true); `||` consumes an
//    `==` check. The check may sit anywhere in the chain, and works for locals
//    and parameters alike.
bool ready = maybeItem != null && maybeItem.IsReady();              // narrowed in RHS
bool ok    = maybeItem == null || maybeItem.IsReady();              // narrowed in RHS
bool both  = Other() && maybeItem != null && maybeItem.IsReady();   // narrowed after the check
bool tail  = maybeItem != null && Other() && maybeItem.IsReady();   // still narrowed at the tail
// the narrowing covers the WHOLE right operand, not just an adjacent term:
bool cmp   = maybeItem != null && maybeItem.Id == wanted;           // maybeItem.Id narrowed
if (maybeItem != null && maybeItem.Id > 0 && Other()) { ... }       // narrowed across the compound
// every checked local in the chain narrows in the later operands, not just the nearest:
if (a != null && b != null && a.Id == b.Id) { ... }                 // both a and b narrowed
if (a == null || b == null || a.Id != b.Id) { return; }             // both narrowed past the ||s

// 6) Ternary branches narrow when the condition is a null-check
int n = maybeItem != null ? maybeItem.Id : 0;            // then-branch narrowed
int m = maybeItem == null ? 0 : maybeItem.Id;            // else-branch narrowed
```

Smart-cast deliberately does **not** narrow:
- Class fields or globals (`obj.Field`, `g_Var`); snapshot to a local first.
- Method return values (`GetMaybe()`); bind to a local.
- `cast<T>(...)` results. A bare `cast<T>` is non-null-typed but can fail at runtime — see [Reference casts](#reference-casts) for `cast<T?>` and local binding.
- Conditions mixing `&&` / `||` at one precedence level in an `if`; split the `if`.
- A local **reassigned** inside a chain operand (rare): chain narrowing follows the immediate-narrowing contract and does not re-track the new value.

When smart-cast cannot see through a shape, the established fallbacks are to bind the expression to a local, change the destination to `T?`, or wrap the use in `if (x == null) { <recover>; return ...; }`.

<a id="reference-casts"></a>

### Reference casts: `cast<T?>(x)`

A reference cast can fail at runtime: `cast<T>(expr)` yields `null` when `expr` is not a `T`. Spell the fallible form **`cast<T?>(x)`**, making the result `T?` so the engine treats it honestly:
- `cast<T?>(x) != null` / `== null` is legitimate (no redundant-comparison warning, #5).
- `cast<T?>(x).Member` requires narrowing first: bind a local, then `if (v == null) ...`.

A bare `cast<T>(x)` keeps the non-nullable "this cast is known to succeed" contract, like `static_cast`: chain `cast<T>(x).Member` directly, but `cast<T>(x) != null` is flagged redundant (#5), so switch to `cast<T?>(x)`. The `?` is honored by `CompileConversion` in [../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp), which applies `to.IsNullable()` to the cast result, including the early-return path where an implicit conversion already produced the target type.

`cast<T?>(x)` also covers a **same-type** read that is statically non-null but can be null: a legacy/sparse `array<T>` field keeps element type `T`, yet `resize`/grid growth can leave empty cells **null**. Reading one into non-nullable `T` (`T v = field[i];`) throws `Null pointer access`; `T? v = field[i];` is flagged a redundant widening (#3) because the element type is non-null. Prefer `array<T?>` / `T?[]` storage when holes are allowed; with an existing `array<T>` sparse container, read via `cast<T?>(field[i])` so the source type tells the truth.

For API parameters, native returns, and sync/lock scopes, null-element policy belongs to that API's contract. Use nullable-element arrays when null is meaningful; otherwise validate at the producer or API entry point that owns the invariant.

### Nullable property handles

A `///@ Property` of proto or fixed-type **handle** (`ProtoItem`, `ItemBag`, `ProtoMap`, …) is non-nullable by default but returns `null` when unset. When "unset → null" is legitimate, add **`Nullable`** to the tag so the AS getter registers as `@?`:

```angelscript
///@ Property Item Server ProtoItem UsableOn.TargetItem Nullable
///@ Property Item Common ItemBag Harvested.SmallBag Nullable
///@ Property Critter Server ProtoItem StartWeapon Nullable
```

[../Source/Common/Properties.cpp](../../../../Source/Common/Properties.cpp) parses the flag (`_isNullable`); entity getter registration in [../Source/Scripting/AngelScript/AngelScriptEntity.cpp](../../../../Source/Scripting/AngelScript/AngelScriptEntity.cpp) consumes it (`@?`), and [`MetadataBaker`](../../../../Source/Tools/MetadataBaker.cpp) accepts `Nullable` only on FixedType / Proto entity properties. Without it, `proto.Field != null` is flagged redundant (#5) even though the field can legitimately be unset, so add the flag instead of deleting the check. (`ItemBag?` in the tag does **not** work: stored-property nullability is a flag, not a `?` suffix.)

For **`Mutable`** nullable handle properties, the **setter** parameter is also nullable (`@?+`), matching the getter; see `set_handle_str` in [AngelScriptEntity.cpp](../../../../Source/Scripting/AngelScript/AngelScriptEntity.cpp). This is load-bearing: AngelScript derives a virtual property's static type from the **setter parameter** whenever one exists (only getter-only / read-only properties use the getter's return type — see `FindPropertyAccessor` in `as_compiler.cpp`). A non-nullable setter with an `@?` getter makes `T? local = obj.MutableNullableProp` read as *non-nullable* and wrongly trip redundant-`?` warning (#3), while `T local = obj.MutableNullableProp` (no `?`) still errors through the getter's nullable return, leaving no warning-free spelling. A nullable setter resolves both.

### Invariant helpers

The Engine no longer ships the former AngelScript `Core.fos` library. An embedding project keeping the traditional `verify(cond, message, ...)` variadic macro owns its definition, visibility, and tests:

```text
#define verify(cond, ...) if (!(cond)) throw(__VA_ARGS__)
```

When provided, it states an **invariant**: a condition that holds whenever project server and client logic behaves correctly. A failure is a bug, so it throws, and the project must keep it enabled in every configuration, not treat it as a debug-only assertion.

Managed C# has Engine-owned equivalents `Game.Verify`, `Game.VerifyNotNull`, and `Game.Unreachable` in `Source/Scripting/Managed/CoreScripts/Verify.cs`; [Managed C# Scripting](../../how-to/scripting/managed-csharp.md) documents their nullable-flow annotations and throwing behavior.

#### Verify vs. graceful recovery

Choose by *what a failure represents*. **Scripts never call `throw` directly** - every failure path goes through `verify`:

| Shape | Use when |
|-------|----------|
| `verify(cond, "...")` | `cond` is an **invariant** - it must hold whenever our system behaves correctly. A violation is a bug (or a tampered client; see below). |
| `verify(false, "...")` | an **unconditional** failure with no single guard condition: an unreachable branch (`switch` default, post-loop "not found"), or a guard whose body does more than fail (e.g. log-then-fail). Always throws - the replacement for what used to be a bare `throw(...)`. It is **no-return**, so no `return` / `break` is needed after it (the compiler treats a constant-true `if` whose body terminates as terminating). |
| `if (x == null) { <recover>; return ...; }` | the state is an **expected, recoverable** runtime outcome - log and fall back, do not throw. |

#### Client input: transport validation and script invariants

Treat every client-originated payload as untrusted at the server boundary. The Engine validates remote-call framing, payload size, collection bounds, encoded value shapes, and complete buffer consumption before synchronization and handler dispatch; see [Remote Calls](../../reference/scripting/remote-calls.md). The project handler still owns and must validate authorization, object ownership, ranges, state transitions, and other domain rules before its first mutation.

Inside script code, `verify` is the always-on rejection mechanism when malformed or forbidden input violates that handler contract. It catches both a bug in the project's normal client and a tampered request, but complements rather than replaces the native transport checks and handler-specific semantic validation. Where rejection is an expected gameplay outcome rather than an invariant failure, use an ordinary recovery branch instead.

#### Narrowing and arguments

`throw(message, ...)` is the exception-raising **global function** (registered in [AngelScriptGlobals.cpp](../../../../Source/Scripting/AngelScript/AngelScriptGlobals.cpp); AngelScript lacks a `throw` keyword, so the name is free), but only the **primitive the `verify` macro expands to**. Scripts never call `throw`; write `verify(false, message, ...)` for unconditional failure. Because `throw` is noreturn, a passing `verify(x != null, "...")` **narrows** `x` for the rest of the scope like an `if (x == null) return;` guard: `if (!(x != null)) throw(...)` has a noreturn then-branch. A verify documents the invariant and removes the following dereference warning.

The macro is **variadic**: `verify(cond, message, ctx...)` forwards `message` and trailing context values to `throw`, matching a bare `throw`'s diagnostic context (e.g. `verify(trader != null, "Barter target not exists", player, traderId)`). Author `message` as a readable sentence (`"No current player"`, not `"Expected: CurPlayer != null"`), passing wanted entities / ids / values as extra arguments.

**Scope of enforcement:** codegen validates every **script handle** crossing the script ↔ native boundary. It emits the check when the meta-type is one of:
- a `///@ ExportEntity` name (`Critter`, `Item`, `Map`, `Location`, `Player`, `Game`, `ImGui`) or the generic `Entity`,
- an entity relative (`Abstract<Entity>`, `Proto<Entity>`, `Static<Entity>` — currently `AbstractItem`, `ProtoCritter`, `ProtoItem`, `ProtoLocation`, `ProtoMap`, `StaticItem`),
- a `///@ ExportRefType` class (`MovingContext`, `MapSpriteHolder`, `SpritePattern`, `VideoPlayback`, `ScriptImGui`).

On the C++ engine side, all matching pointer spellings (`ptr<Critter>` / `nptr<Critter>`, `ptr<CritterView>` / `nptr<CritterView>`, `ptr<Map>` / `nptr<Map>`, `ptr<ProtoItem>`, `ptr<StaticItem>`, `ptr<MovingContext>`, …) are in scope. The membership test lives in `is_validated_pointer_meta_type(...)` in [../BuildTools/codegen.py](../../../../BuildTools/codegen.py), now the sole arbiter: without an `FO_NULLABLE` marker, codegen treats exported `ptr<T>` handles as non-null and `nptr<T>` handles as nullable. Bare raw handle pointers are rejected before registration.

Marking `?` on a primitive value type (`int`, `bool`, `mpos`, `hstring`, …) is rejected because those types have no `null` representation.

**Out of scope:** script-to-script parameter passing. `asBC_RefCpyChk` covers handle **assignments** and **initializations**; AS argument passing typically uses copy-on-call patterns that bypass REFCPY. A project-side declaration analyzer plus the native-boundary check can cover the common case of a script value flowing through an engine call.

### Migration note

The change is **source-incompatible** for any AS code that assigned `null`, or a runtime value that might be null, to a bare handle. Such scripts must now mark the destination `T?`. Inline test scripts in `Source/Tests/Test_*.cpp` were updated in lock-step and provide engine-owned regression coverage.

Projects that completed the nullable migration should enable the strict compile-time variant (`asEP_DISALLOW_NULLABLE_TO_NON_NULLABLE`), which catches more shapes at build time. Any remaining runtime *"Null assignment to non-nullable handle"* indicates a missing marker or invalid non-null contract. Shapes the smart-cast or compile-time checker cannot see through, requiring an explicit `T?` destination or guarding recovery path, include `dict<K,V>.get(key, null)`, output-reference parameters (`Map& out`), and class fields assigned across method boundaries.

FOnline runs with `asEP_ALLOW_IMPLICIT_HANDLE_TYPES`, so script code uses implicit-handle syntax (`Critter`, `Item?`, `array<Critter>`) instead of explicit `@` syntax (`Critter@`, `Item@?`, `array<Critter@>`) for ref-class types. When compiling a user-authored section, the builder rejects explicit `@` on `asOBJ_IMPLICIT_HANDLE` types with *"Explicit handle '@' is not allowed on implicit-handle type 'X'"*. Funcdef parameters keep `Type@+` (auto-add-ref handle) because the trailing `+` is semantically required and bare `Type+` is not a valid AS signature.

Native API registrations (`RegisterObjectType`, `RegisterFuncdef`, `RegisterObjectMethod`, `GetFunctionByDecl`) and engine-generated declaration strings still accept the explicit `@`. Rejection fires only while a script module is being built (`module != 0`) and the builder is not in silent lookup mode — see `CreateDataTypeFromNode` in [../ThirdParty/AngelScript/sdk/angelscript/source/as_builder.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_builder.cpp).

## Project-side tooling

The engine owns compiler/runtime enforcement and the native binding contract. An embedding project may add read-only source analyzers for authoring rules spanning otherwise unrelated script declarations. Useful checks include:

- `?` is used only on handle-capable reference types;
- event and remote-call handlers match declaration nullability argument by argument;
- a nullable local is narrowed before dereference;
- a guarded native `nptr<T>` is dereferenced directly, not copied into a redundant narrowing alias;
- project style never mixes implicit-handle syntax with explicit `@` syntax;
- forbidden identity operators or redundant defensive null guards do not return.

Keep marker placement explicit. Body-shape inference is unreliable: a forwarding function may never dereference a non-null parameter, while a helper may be the only source of a nullable return. Authors declare `ptr<T>` / `nptr<T>` on native exports and `T` / `T?` in scripts; analyzers verify declarations rather than heuristically rewriting contracts.

Project-side generators and analyzer task names belong in that project's documentation. If a checker becomes reusable across FOnline games, move it to `BuildTools/` with engine-owned tests before citing it here as a standard command.

## Adding / editing markers

When writing a new script function or native export:

1. Choose the nullable contract explicitly in the declaration (`T?` for script handles; `nptr<T>` for nullable native exports, `ptr<T>` for non-null) when the function meaningfully accepts or returns null.
2. Compile the affected script modules to run the engine's nullable diagnostics.
3. Run the project-side declaration-parity and style analyzers required by the embedding project.

Project-side rewriters must not infer contracts: preserve author-chosen markers and remove only guards made redundant by generated runtime checks. If a nullable case is real (for example a `dynamic_cast<X*>(param)` path where null is meaningful), keep the marker and update the analyzer only when its guard-removal pattern is wrong.

## See also

- [Scripting](../../explanation/scripting-runtime/) — overall engine scripting runtime, AngelScript and Managed backends, and method-export organization.
- [Managed C# Scripting](../../how-to/scripting/managed-csharp.md) — generated nullable surface, async lifetime, analyzers, and build/bake validation.
- [Remote Calls](../../reference/scripting/remote-calls.md) - remote-call signatures, handlers, serialization, and project catalog generation.
- [Script Methods Map](../../reference/script-api/method-ownership.md) — current `///@ ExportMethod` file map and ownership boundaries.
- [GeneratedApiAndMetadata.md](../../reference/metadata/index.md) — generated metadata/API flow that must stay aligned with script-visible contracts.
- [SmartPointers.md](smart-pointers.md) — native C++ pointer ownership/nullability vocabulary.
- [Testing](../../../Testing.md) — reusable unit, script, integration, package, and smoke-test boundaries.
