# Nullability

> Engine-owned documentation. Paths under `../` are relative to the FOnline engine root. Paths under `../../` point to an embedding game project such as Last Frontier when this engine is used as a submodule.

Convention and runtime enforcement for nullable values across AngelScript and the native engine boundary.

## Core principle

> Better to not pass `null` at all than to defensively check inside and bail out.
>
> A parameter or return may be marked nullable **only when the function meaningfully handles both null and non-null cases**. Early-exit-on-null guards are a code smell â€” the contract should be non-null and the caller fixed instead.

This applies symmetrically on both sides of the script-engine boundary.

## Script side: `T?` suffix

AngelScript modules in [Scripts/](../../Scripts/) use a Kotlin/C#-style `?` suffix on the type to mark nullability. Default is **non-nullable**.

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

// Parameter may be null â€” body handles both cases
void OnCritterUseWeapon(Critter cr, WeaponUseMode useMode, HitLocation aim, Critter? target, mpos targetHex)
{
    mpos resolvedTargetHex = target != null ? target.Hex : targetHex;
    // ...
}
```

`?` is parsed by the **AngelScript front-end itself** (see `ParseType` in [../ThirdParty/AngelScript/sdk/angelscript/source/as_parser.cpp](../ThirdParty/AngelScript/sdk/angelscript/source/as_parser.cpp), `MakeNullable`/`isNullable` in [../ThirdParty/AngelScript/sdk/angelscript/source/as_datatype.h](../ThirdParty/AngelScript/sdk/angelscript/source/as_datatype.h), and the `CreateDataTypeFromNode` consumer in [../ThirdParty/AngelScript/sdk/angelscript/source/as_builder.cpp](../ThirdParty/AngelScript/sdk/angelscript/source/as_builder.cpp)). The marker is no longer rewritten by the preprocessor â€” the `StripNullableTypeSuffix` pass was removed once the engine learned the suffix directly. Misplaced markers (e.g. `int?`) produce a compile-time error: *"Nullable marker '?' is only allowed on handle types"*.

The AS parser disambiguates the type-suffix `?` from the ternary `?` by only consuming it inside `ParseType`. Inside expressions, `cond ? a : b` continues to parse as the conditional operator.

### `///@ Event` and `///@ RemoteCall` declarations

The same `?` suffix is supported in `///@ Event` and `///@ RemoteCall` tag declarations, and the [`MetadataBaker`](../Source/Tools/MetadataBaker.cpp) propagates the per-arg nullable bit into the baked engine metadata (`ArgDesc::Nullable` on `EntityEventDesc::Args` / `RemoteCallDesc::Args`).

```angelscript
///@ Event Server Game OnCritterDamaged(Critter cr, Critter? attacker, int32 damage)
///@ Event Server Game OnCritterDead(Critter critter, Critter? killer)
///@ RemoteCall Server SwitchCharacter(Critter? newCritter)
```

The declaration is the contract. Every `[[Event]]` subscriber and every `[[ServerRemoteCall]]` / `[[ClientRemoteCall]]` / `[[AdminRemoteCall]]` implementation that matches the event/call name must use the same `?` marker on each argument. [`validate_nullable.py`](../../Tools/NullableEstimate/validate_nullable.py) walks all `.fos` files, pairs declarations with their handlers by function name, and fails on any per-arg nullable mismatch.

```angelscript
// Matches the OnCritterDamaged declaration above.
[[Event]]
void OnCritterDamaged(Critter cr, Critter? attacker, int32 damage) { ... }

// Would be rejected by validate_nullable.py â€” declaration has `Critter?`,
// handler drops the `?`:
[[Event]]
void OnCritterDamaged(Critter cr, Critter attacker, int32 damage) { ... }
```

Because the AngelScript front-end now tracks the per-type nullable bit, the AS engine itself enforces null contracts on handle writes at runtime (see Â«Runtime enforcementÂ» below). `validate_nullable.py` still enforces the static convention that an `[[Event]]` / `[[*RemoteCall]]` handler matches its declaration argument-by-argument â€” the AS engine has no way to know two unrelated declarations are supposed to share a contract.

## Engine side: `FO_NULLABLE` macro

Native methods declared with `///@ ExportMethod` in [../Source/Scripting/](../Source/Scripting/) use the inverse-of-pointer-default macro `FO_NULLABLE`. Defined as empty in [../Source/Essentials/BasicCore.h](../Source/Essentials/BasicCore.h), it documents the nullability contract that codegen emits into the AS-side metadata.

```cpp
///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Map* Server_Critter_GetMap(Critter* self)
{
    return self->GetEngine()->EntityMngr.GetMap(self->GetMapId());
}

///@ ExportMethod
FO_SCRIPT_API void Server_Player_SwitchCritter(Player* self, FO_NULLABLE Critter* cr)
{
    self->GetEngine()->SwitchPlayerCritter(self, cr);
}
```

The `self` (first parameter â€” `this` receiver) and the implicit `engine` parameter for global methods are **never** marked: AS validates `this` before dispatch.

### Component accessors are non-nullable and throw; probe with `Has<Component>`

An entity component getter (`item.Weapon`, `item.MapExit`, `cr.DialogContext`, `item.Locker`, â€¦ â€” every property declared `///@ Property <Entity> ... <Name> Component`) is **non-nullable** and **throws** when the component is absent. `Entity_GetComponent` in [../Source/Scripting/AngelScript/AngelScriptEntity.cpp](../Source/Scripting/AngelScript/AngelScriptEntity.cpp) raises a `ScriptException` unless the presence bool is set, so the getter is registered as `{}@ get_{}() const` (no `?`). Alongside each component getter a `bool get_Has<Component>() const` accessor is registered for presence probes.

```angelscript
// item.Weapon is ItemWeaponComponent (non-nullable) â€” access directly
int dist = item.Weapon.MaxDist;        // OK; throws iff the item is not a weapon

// probe presence with Has<Component>, never `== null`
if (item.HasWeapon) {
    int d = item.Weapon.MaxDist;        // guarded
}
verify(item.HasWeapon, "Item must be a weapon");
```

This mirrors the throwing global getters below (`Chosen` / `CurMap` / â€¦): a missing component in code that already assumes it is present is an **invariant violation**, not a recoverable null. Do **not** write `item.Weapon == null` / `!= null` â€” the getter would throw at the access; write `!item.HasWeapon` / `item.HasWeapon`. The four entity flavors (concrete / `Abstract` / `Proto` / `Static`) and fixed types all share this registration, so `proto.HasWeapon`, `abstractItem.HasWeapon`, etc. are all available. (One name clash to keep in mind: `Ammo` is both an Item component and a nullable *property* of the weapon component â€” `item.Weapon.Ammo` is the loaded ammo item and remains a normal nullable `== null` check.)

### Throwing global getters: `Chosen` / `CurMap` / `CurLocation` / `CurPlayer`

The client global getters `Chosen`, `CurMap`, `CurLocation`, `CurPlayer` (registered in [../Source/Scripting/ClientGlobalScriptMethods.cpp](../Source/Scripting/ClientGlobalScriptMethods.cpp)) are **non-nullable** and **throw** when accessed while absent (e.g. "No chosen critter"). This is deliberate: most client code runs only in contexts where they exist (the in-game UI is disabled without a `Chosen`), so it should read `Chosen.X` directly without a null dance. To check presence where absence is a valid state, use the matching bool predicate:

```angelscript
if (!HasChosen) {
    return;        // no chosen critter right now - handle it
}
Critter cr = Chosen;   // OK - non-nullable, guaranteed present here
```

`HasChosen` / `HasCurMap` / `HasCurLocation` / `HasCurPlayer` are the presence checks. Do **not** write `Chosen == null` / `Chosen != null`: comparing a non-nullable handle to null both trips the redundant-comparison warning (#5) and *throws* (evaluating `Chosen` when absent), so it is doubly wrong - use `!HasChosen` / `HasChosen` instead.

### Throwing proto getters: `Game.GetProtoItem/Critter/Map/Location` + `CheckProtoX`

`Game.GetProtoItem`, `GetProtoCritter`, `GetProtoMap`, `GetProtoLocation` (in [../Source/Scripting/CommonGlobalScriptMethods.cpp](../Source/Scripting/CommonGlobalScriptMethods.cpp)) are **non-nullable** and **throw** when no proto with that id exists (e.g. "Item proto not found"). Each has a matching `Game.CheckProtoItem/Critter/Map/Location(pid)` bool predicate for the case where the id may legitimately be missing (stale checkpoint/map data, user-supplied ids, …).

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

Do **not** write `Game.GetProtoX(pid) == null` / `!= null` (it throws on a missing proto before the comparison) - use `!Game.CheckProtoX(pid)` / `Game.CheckProtoX(pid)`.

The same applies to the **codegen-generated proto/fixed-type getters** for custom entities (`Game.GetProtoModifier`, `Game.GetProtoFaction`, `Game.GetEncounterProfileData`, `Game.GetWeatherType`, `Game.GetItemBag`, …): they are non-nullable and **throw** when the id is unknown, with a matching `Game.Check<Name>(pid)` predicate. Registered in `register_entity_protos` / `register_fixed_type` (`Game_GetProtoCustomEntity` / `Game_CheckProtoCustomEntity`) in [../Source/Scripting/AngelScript/AngelScriptEntity.cpp](../Source/Scripting/AngelScript/AngelScriptEntity.cpp). Read directly when the id is known (`instance.ProtoId`, an authored content id); probe with `Check<Name>` only where the id may legitimately be absent.

## Runtime enforcement

There are now two complementary runtime gates:

### Script-side: `asBC_RefCpyChk` on handle assignments

The AngelScript compiler emits a new `asBC_RefCpyChk` bytecode (defined in [../ThirdParty/AngelScript/sdk/angelscript/include/angelscript.h](../ThirdParty/AngelScript/sdk/angelscript/include/angelscript.h), handler in [../ThirdParty/AngelScript/sdk/angelscript/source/as_context.cpp](../ThirdParty/AngelScript/sdk/angelscript/source/as_context.cpp)) whenever the destination of a handle write is **non-nullable** (`T` without `?`) and the destination is **user-declared** (not a compiler-generated temporary). It is a drop-in REFCPY variant that raises a *Null pointer access* exception when the source handle is null. Emission sites are `PerformAssignment` and `CompileInitializationWithAssignment` in [../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp](../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp); `T?` declarations fall back to the original `asBC_REFCPY` and accept null silently.

The temp guard matters because `PerformAssignment` is reused for argument-setup slots whose type is inherited from a native parameter (no `?` syntax on the AS side). Nullability of those slots is owned by the native-boundary check below â€” letting the AS-level check skip temporaries keeps `func(null)` working for `FO_NULLABLE` natives.

This means **script-to-script assignments** of null to a non-nullable handle still throw, even when the value originates from another script function rather than a native call.

### Native-boundary: codegen-emitted arg/return checks

Native validation continues to be plumbed through codegen-generated `MethodDesc::Call` lambdas, **not** the AS-to-native bridge. [../BuildTools/codegen.py](../BuildTools/codegen.py) emits per-method calls to `NativeDataProvider::CheckArgNotNull` / `CheckReturnNotNull` (defined in [../Source/Common/ScriptSystem.h](../Source/Common/ScriptSystem.h)) right before/after the native invocation:

```
MethodDesc::Call(call)
  â†’ NativeDataProvider::CheckArgNotNull(call, i, "Server_Player_SetCritter", "cr", "Critter")   // for each non-nullable entity arg
  â†’ native invocation
  â†’ NativeDataProvider::CheckReturnNotNull(call, "...", "...")                                  // for non-nullable entity return
```

Doing it at the `MethodDesc::Call` boundary means **every** caller of an `///@ ExportMethod` is covered â€” the AS-to-native bridge, native test harnesses, future Mono-backend dispatch, anyone. The check has no per-call lookup cost beyond a single pointer compare.

Violation surface: `ScriptException`, propagated to the calling AngelScript context. Three distinct messages:

- *"Null assignment to non-nullable handle"* â€” raised by `asBC_RefCpyChk` (the new AS-side check on bare-handle writes). Distinct from the generic null-deref so stack traces clearly point at the bad assignment rather than a downstream method call.
- *"Null pointer access"* â€” the original AS message, raised by `asBC_CHKREF` / `asBC_ChkRefS` / `asBC_ChkNullV` / `asBC_ChkNullS` for dereferences, indexing, and method calls on a null handle.
- Native-boundary checks emit the method name, parameter name, and type via the codegen-generated `NativeDataProvider::Check{Arg,Return}NotNull`.

### Compile-time guarantees

In addition to the runtime `asBC_RefCpyChk`, the AS front-end raises two compile-time errors on null-unsafe assignments before any bytecode is generated:

1. **Bare `null` to a non-nullable handle is always rejected.** Always-on, no engine property required. Emits:
   *"Cannot assign 'null' to a non-nullable handle of type 'T' (use 'T?' to allow null)"*.
   This catches `T x = null;`, `someField = null;`, and the implicit `T x;` form that AS lowers into a null initializer.

2. **Nullable handle source to a non-nullable destination is rejected when `asEP_DISALLOW_NULLABLE_TO_NON_NULLABLE` is enabled.** Last Frontier turns this property on at engine setup in [../Source/Scripting/AngelScript/AngelScriptBackend.cpp](../Source/Scripting/AngelScript/AngelScriptBackend.cpp). Emits:
   *"Cannot assign nullable 'T?' to non-nullable 'T' without a null-check (add `if (src != null)` or change the destination to 'T?')"*.

3. **Redundant `?` on local initializer is warned about.** When the destination is declared `T?` but the initializer is statically a non-nullable handle (the source type cannot produce `null`), the front-end emits:
   *"Redundant '?': initializer of type 'T' cannot be null; declare destination as 'T' instead"*.
   This catches stale `T?` annotations left behind after an API tightened its return type (e.g. `FO_NULLABLE Critter*` â†’ `Critter*`), and the `if (cr == null)` dead branch that usually follows. The diagnostic is a *warning* (compilation succeeds) so existing scripts keep building while authors clean up; the message names both the current source type and the bare destination spelling so the fix is a one-character drop.

4. **Dereferencing an un-narrowed nullable handle is warned about.** When a `T?` value is dereferenced (`x.Member`, `x[i]`, `x.Method()`) without first narrowing it to non-null, the front-end emits:
   *"Dereference of nullable handle 'T?' without a null-check; narrow it first (e.g. `if (x == null) return;`) so it becomes 'T'"*.
   Emitted from the `ttDot` branch of `CompileExpressionPostOp` in [../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp](../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp). It is a *warning*, not an error â€” the runtime null-deref check (`asBC_CHKREF` etc.) is still the safety net â€” so authors clean up at their own pace. Narrowing only tracks **named locals/params**: `x.Method() == null || x.Method().Foo` still warns on the second call because each getter call is a fresh expression; bind to a local (`auto v = x.Method(); if (v == null) return; v.Foo`) to narrow.

5. **Redundant null comparison against a non-nullable handle is warned about.** When one operand of `==` / `!=` is `null` and the other is a statically non-nullable **named** handle (a non-nullable local/param, or a local already narrowed to non-null), the comparison has a constant result and the guarded branch is dead. The front-end emits:
   *"Redundant null comparison: 'T' is a non-nullable handle and can never be null; remove the check (or make the source nullable if it actually can be null)"*.
   Emitted from `CompileOperatorOnHandles` in [../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp](../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp). This is the inverse of the deref warning (#4): together they push every nullable value toward exactly one null-check at the point it is introduced. Like #3/#4 it is a *warning*. The "or make the source nullable" hint matters â€” a hit often means the **source** is mis-modeled as non-nullable (e.g. a native return missing `FO_NULLABLE`), not that the check is truly dead. **Only named operands warn:** a *temporary* with a non-nullable static type is excluded, because it can still be null at runtime â€” most importantly `cast<T>(x)`, which yields null on a failed cast, so `cast<T>(x) != null` is a genuine, non-redundant check (the same applies to a call result like `GetThing() != null`). Bind such a value to a local and check the local if you want the redundancy tracked.

The runtime `asBC_RefCpyChk` is still the safety net underneath both errors: even when the compile-time pass accepts an assignment (e.g. through a `dict<K, V>.get(key, null)` that returns a statically-non-nullable reference bound to null at runtime), the bytecode still throws on null write.

### Identity comparison: `==` only, never `is` / `!is`

`is` and `!is` are **banned in `.fos`** by project convention. Use `==` and `!=` for both null checks and handle-identity comparison. A FOnline patch on `CompileOperator` ([../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp](../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp)) falls back to `asBC_CmpPtr` (handle identity) when no `opEquals` is registered for the ref type, so the two forms produce the same bytecode:

| Operands | `==` / `!=` behavior |
|----------|-----|
| Both sides are entity types (`Critter`, `Item`, `Map`, ...) with codegen-emitted `opEquals` | id-based equality (compares entity id) |
| Either side is `null` | null-handle compare |
| Both sides are ref types **without** `opEquals` (`Gui::Screen`, `Object`, custom classes, funcdef handles) | handle-identity (`asBC_CmpPtr`) - same as `is` |
| One side is a reference and the other is a handle | implicit conversion to handle, then handle-identity |

This means the choice between "id-based" and "pointer-based" is determined by the *type*, not the operator - so the operator carries no extra information and `is` is pure cognitive noise. [`validate_nullable.py`](../../Tools/NullableEstimate/validate_nullable.py) (`_validate_no_is_operator`) flags any `is` / `!is` usage in `.fos` and CI rejects PRs that re-introduce it.

### Smart-cast (flow-sensitive narrowing)

Kotlin-style smart-cast narrows a `T?` local back to `T` inside a region that is provably non-null, so the body can use it without an explicit cast. Implemented as a per-scope `smartCasts` stack in [../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp](../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp); see `DetectNullCheckPattern`, `GetNarrowedTypeForLocal`, and the integrations in `CompileIfStatement` (statement guards), `CompileCondition` (ternary branches), and `CompilePostFixExpression` (`&&` / `||` short-circuit operands). For the short-circuit case the `!=` / `==` check tags its result context in `CompileOperatorOnHandles`; that tag carries a **list** of narrowed locals (`nullCheckNarrowList` on `asCExprContext`), and each chained `&&` (keeps the `!=`-tags) / `||` (keeps the `==`-tags) **accumulates all** matching-polarity narrowings from both operands — so `a != null && b != null && a.X && b.Y` narrows both `a` and `b` in the trailing operands, not just the nearest check. When a tagged operand lands on the postfix evaluation stack, `ShortCircuitNarrowsRightOperand` scans ahead to confirm it is consumed as the *left* operand of a matching short-circuit, and if so every narrowing in the list is pushed for that operator's **entire right operand** (tracked by expr-stack index, popped when the operator is reached) — so `x != null && x.Prop == y` narrows `x.Prop`, not just a term sitting immediately before the `&&`. The tag stores each local's *stack offset*, which is **negative for parameters** — so a dedicated `nullCheckNarrowValid` flag (not the offset sign) marks "tag present", otherwise narrowing would silently skip every parameter. For statement guards, a then/else branch counts as "exits without falling through" (so the complementary narrowing applies afterwards) not only for `return`/`throw` but also for `break` / `continue` (`StatementAbortsFallThrough`) — so `if (x == null) continue;` inside a loop narrows `x` for the rest of the body exactly like an early `return`. This narrowing-only signal never feeds the function-must-return-value analysis (which still ignores `break`/`continue`).

Supported patterns:

```angelscript
// 1) `if (x != null) { ... }` narrows in the then-branch
Item? maybeItem = GetMaybeItem();
if (maybeItem != null) {
    Item item = maybeItem;        // OK â€” compiler treats maybeItem as Item here
    item.Use();
}

// 2) `if (x == null) { <recover>; return; } <code>` â€” early-exit narrows after the if
Critter? maybeCr = GetMaybeCr();
if (maybeCr == null) {
    Logging::Warning("CharacterRoster", "main_critter_missing player=" + player.Name);
    return null;
}
Critter cr = maybeCr;             // OK â€” the early return rules out null

// 2b) break / continue guards narrow the same way (loop bodies)
for (int i = 0; i < ids.length(); i++) {
    Critter? probe = Game.GetCritter(ids[i]);
    if (probe == null) {
        continue;                 // bails this iteration
    }
    probe.Use();                  // OK â€” narrowed for the rest of the loop body
}

// 3) Compound `&&` / `||` shapes narrow every recognised atom
if (a != null && b != null && c != null) {
    Item ai = a; Item bi = b; Item ci = c;   // OK â€” all three narrowed
}
if (a == null || b == null) {
    return;
}
Item ai = a; Item bi = b;                    // OK â€” both narrowed after early return

// 4) Assignment invalidates the narrowing on that local
if (x != null) {
    x = GetMaybeNull();           // x becomes nullable again
    Item y = x;                   // compile-time error here
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
bool cmp   = maybeItem != null && maybeItem.Count == wanted;        // maybeItem.Count narrowed
if (maybeItem != null && maybeItem.Count > 0 && Other()) { ... }    // narrowed across the compound
// every checked local in the chain narrows in the later operands, not just the nearest:
if (a != null && b != null && a.Count == b.Count) { ... }          // both a and b narrowed
if (a == null || b == null || a.Count != b.Count) { return; }      // both narrowed past the ||s

// 6) Ternary branches narrow when the condition is a null-check
int n = maybeItem != null ? maybeItem.Count : 0;         // then-branch narrowed
int m = maybeItem == null ? 0 : maybeItem.Count;         // else-branch narrowed
```

Smart-cast deliberately does **not** narrow:
- Class fields or globals (`obj.Field`, `g_Var`). Snapshot to a local first.
- Method return values (`GetMaybe()`). Bind to a local.
- The result of `cast<T>(...)`. Bind to a local.
- Conditions with mixed `&&` / `||` at the same precedence level inside an `if`. Split the `if`.
- A local **reassigned** inside one of the chain's operands (rare): the chain narrowing follows the immediate-narrowing contract and does not re-track the new value.

When smart-cast can't see through the shape, the established fallbacks are: bind the expression to a local, change the destination to `T?`, or wrap the use in `if (x == null) { <recover>; return ...; }`.

### The `verify` macro

`verify(cond, message, ...)` is a variadic preprocessor macro defined in [Core.fos](../Source/Scripting/AngelScript/CoreScripts/Core.fos) (visible in every `.fos` module, like all `Core.fos` `#define`s):

```
#define verify(cond, ...) if (!(cond)) throw(__VA_ARGS__)
```

It states an **invariant**: a condition that holds whenever our own code - server logic *and* our client - behaves correctly. A failure means a bug, so it throws. Crucially these checks run in **every** configuration; there is no `NDEBUG`-style strip, so they are always the runtime guard, never a debug-only check. (The name is `verify`, not `assert`, precisely to signal that — C's `assert` connotes a debug-only check that is compiled out in release, which would be dangerous here.)

#### Verify vs. graceful recovery

Choose by *what a failure represents*. **Scripts never call `throw` directly** - every failure path goes through `verify`:

| Shape | Use when |
|-------|----------|
| `verify(cond, "...")` | `cond` is an **invariant** - it must hold whenever our system behaves correctly. A violation is a bug (or a tampered client; see below). |
| `verify(false, "...")` | an **unconditional** failure with no single guard condition: an unreachable branch (`switch` default, post-loop "not found"), or a guard whose body does more than fail (e.g. log-then-fail). Always throws - the replacement for what used to be a bare `throw(...)`. It is **no-return**, so no `return` / `break` is needed after it (the compiler treats a constant-true `if` whose body terminates as terminating). |
| `if (x == null) { <recover>; return ...; }` | the state is an **expected, recoverable** runtime outcome - log and fall back, do not throw. |

#### Client input is an invariant, not an "untrusted input" path

Malformed data from the client is checked with `verify`, **the same as data we produced ourselves** - not a separate defensive-validation layer. We model our client as cooperative: a bad `///@ RemoteCall` argument, a client-writable property out of range, or a malformed request payload is, in the normal case, a bug in *our own* client, i.e. an invariant violation.

A hacked client can of course send anything - but because these checks always execute, the very same `verify` still fires at runtime, throws, and fails the request, so the server stays protected. One `verify` therefore covers both "our bug" and "tampered client"; there is no need to special-case the server boundary with extra validation.

#### Narrowing and arguments

`throw(message, ...)` is the exception-raising **global function** (registered in [AngelScriptGlobals.cpp](../Source/Scripting/AngelScript/AngelScriptGlobals.cpp); AngelScript has no `throw` keyword, so the name is free) - but it is only the **primitive the `verify` macro expands to**. Scripts do **not** call `throw` directly; an unconditional failure is written `verify(false, message, ...)`. Because `throw` is marked noreturn, a passing `verify(x != null, "...")` **narrows** `x` to non-null for the rest of the scope, exactly like the `if (x == null) return;` early-exit guard - the macro expands to `if (!(x != null)) throw(...)`, whose then-branch is noreturn. So a verify both documents the invariant and removes the dereference warning that follows.

The macro is **variadic**: `verify(cond, message, ctx...)` forwards `message` and any trailing context values straight to `throw`, so a verify carries the same diagnostic context a bare `throw` would (e.g. `verify(trader != null, "Barter target not exists", player, traderId)`). Author the `message` as a readable sentence (`"No current player"`, not `"Expected: CurPlayer != null"`), and pass the entities / ids / values you'd want in the exception as extra arguments.

**Scope of enforcement:** every **script handle** crossing the script â†” native boundary is validated. Concretely codegen emits the check when the meta-type is one of:
- a `///@ ExportEntity` name (`Critter`, `Item`, `Map`, `Location`, `Player`, `Game`, `ImGui`) or the generic `Entity`,
- an entity relative (`Abstract<Entity>`, `Proto<Entity>`, `Static<Entity>` â€” currently `AbstractItem`, `ProtoCritter`, `ProtoItem`, `ProtoLocation`, `ProtoMap`, `StaticItem`),
- a `///@ ExportRefType` class (`MovingContext`, `MapSpriteHolder`, `SpritePattern`, `VideoPlayback`, `ScriptImGui`).

On the C++ engine side the matching pointer spellings (`Critter*`/`CritterView*`, `Map*`/`MapView*`, `ProtoItem*`, `StaticItem*`, `MovingContext*`, â€¦) are all in scope. The membership test lives in `is_validated_pointer_meta_type(...)` in [../BuildTools/codegen.py](../BuildTools/codegen.py); the [validate_nullable.py](#tooling) walker mirrors it by parsing `///@ ExportEntity` / `///@ ExportRefType` headers at runtime.

Marking `?` / `FO_NULLABLE` on a primitive value type (`int`, `bool`, `mpos`, `hstring`, â€¦) is the only misuse [validate_nullable.py](#tooling) flags â€” those types have no `null` representation, so the marker is meaningless.

**Out of scope:** script-to-script parameter passing. The `asBC_RefCpyChk` instruction covers handle **assignments** and **initializations**; AS argument passing typically uses copy-on-call patterns that bypass REFCPY. Static analysis (`validate_nullable.py`) plus the native-boundary check still cover the common case where a script value flows through an engine call.

### Migration note

The change is **source-incompatible** for any AS code that assigned `null` â€” or a value that might be null at runtime â€” to a bare handle. After this change, those scripts must mark the destination `T?`. The convention in `Scripts/` already follows this rule (`validate_nullable.py` would have flagged drift), and inline test scripts embedded in `Engine/Source/Tests/Test_*.cpp` were updated in lock-step so the whole `LF_UnitTests` suite passes again.

The strict compile-time variant (`asEP_DISALLOW_NULLABLE_TO_NON_NULLABLE`) catches more shapes at build time. Last Frontier enables it by default and the `Scripts/` migration to use `T?` on `T x = expr.get(key, null);` / `T field = null;` / `T x = maybeNull();` style patterns is tracked through the gameplay-test runs â€” any remaining runtime *"Null assignment to non-nullable handle"* is a still-needed marker fix-up. Common shapes that the smart-cast or compile-time checker cannot see through (and therefore require an explicit `T?` on the destination or an `if (x == null) { <recover>; return null; }` guard before the use): `dict<K,V>.get(key, null)`, output-reference parameters (`Map& out`), and class fields assigned across method boundaries.

Style note for this codebase: the engine runs with `asEP_ALLOW_IMPLICIT_HANDLE_TYPES`, so script code must use the implicit-handle form (`Critter`, `Item?`, `array<Critter>`) rather than the explicit `@` form (`Critter@`, `Item@?`, `array<Critter@>`) â€” the implicit form is the project's convention everywhere the type is itself a ref class. The builder rejects an explicit `@` on `asOBJ_IMPLICIT_HANDLE` types when compiling a user-authored script section with the error *"Explicit handle '@' is not allowed on implicit-handle type 'X'"*. Funcdef parameters keep the `Type@+` form (handle with auto-add-ref) because the trailing `+` modifier is semantically required and the bare `Type+` form is not a valid AS signature.

Native API registrations (`RegisterObjectType`, `RegisterFuncdef`, `RegisterObjectMethod`, `GetFunctionByDecl`) and engine-generated declaration strings continue to accept the explicit `@`. The rejection only fires when there is a script module being built (`module != 0`) and the builder is not in silent lookup mode â€” see `CreateDataTypeFromNode` in [../ThirdParty/AngelScript/sdk/angelscript/source/as_builder.cpp](../ThirdParty/AngelScript/sdk/angelscript/source/as_builder.cpp).

## Tooling

Five Python tools in [Tools/NullableEstimate/](../../Tools/NullableEstimate/):

| Tool | Purpose |
|------|---------|
| `apply_nullables.py` | Scans `.fos` and strips dead defensive null guards on entity-pointer params that are NOT marked `?` â€” codegen / convention guarantees them non-null. Does **not** add or remove `?` markers; the author owns placement. Idempotent. |
| `apply_native_nullable.py` | Walks `///@ ExportMethod` declarations in `../Source/Scripting/*ScriptMethods.cpp` and strips dead defensive `if (param == nullptr) throw ...;` guards on entity-pointer params that are NOT marked `FO_NULLABLE` â€” codegen emits `NativeDataProvider::CheckArgNotNull` for those. Does **not** add or remove `FO_NULLABLE` markers; the author owns placement. Idempotent. |
| `apply_local_nullables.py` | Promotes local variable declarations `T name = expr;` â€” where `name` is later null-checked in the same function body â€” to `T? name = expr;`. The author already proves intent with the null-check; the type just needs to agree. Idempotent. Skips primitives and enums. |
| `validate_nullable.py` | Read-only placement check. Five layers: (1) `FO_NULLABLE` must sit inside an `///@ ExportMethod` signature and target a pointer, (2) script `?` must target a handle-able ref type, (3) explicit `@` on `asOBJ_IMPLICIT_HANDLE` types is rejected because the engine will reject the build anyway, (4) a local declaration whose name is null-checked below must already be `?`, (5) a `T?` local must not be dereferenced (`name.X` / `name[i]`) before *some* null-check â€” crashes at runtime with *"Null pointer access"* otherwise. |
| `estimate_nullables.py` | Read-only coverage report â€” counts function/parameter/return shapes across `.fos`. |

The applier tools accept `--check` (exit non-zero if dead defensive guards still exist) or `--dry-run` (preview without writing). `validate_nullable.py` is always read-only. CI uses these check modes to fail PRs that drift.

### Why the appliers don't auto-add markers

Earlier revisions of the analyzers tried to infer marker placement from body shape (`return nullptr;` somewhere â†’ mark return; no defensive throw + no dereference â†’ mark param). The heuristics produced churn against a curated codebase: a one-liner `void TransferToMap(Critter* self, Map* map, mpos hex) { ... transfer(self, map, hex, ...); }` would round-trip to `FO_NULLABLE Map* map` because the body only forwards the pointer â€” but the contract is non-null. Author intent is the source of truth; the analyzer's job is now only to delete dead guards that codegen has made redundant.

## Workflows

### VS Code tasks ([.vscode/tasks.json](../../.vscode/tasks.json))

Generators (modify code) â€” run after editing scripts or native exports:
- `Generate :: Nullable Markers (Scripts)` â†’ `apply_nullables.py`
- `Generate :: Nullable Markers (Engine)` â†’ `apply_native_nullable.py`
- `Generate and Format All` â†’ bundles both, then formatter pass

Analyzers (check-only, exit non-zero on drift) â€” run before committing or in CI:
- `Analyze :: Nullable Markers (Scripts)` â†’ `apply_nullables.py --check`
- `Analyze :: Nullable Markers (Engine)` â†’ `apply_native_nullable.py --check`
- `Analyze :: Nullable Placement` â†’ `validate_nullable.py`
- `Analyze :: Nullable Coverage` â†’ `estimate_nullables.py`
- `Analyze All` â†’ bundles all four

### CI

`.github/workflows/ci.yml` (`analyze` job) runs the script and engine appliers in `--check` mode plus `validate_nullable.py` on every push and PR. Drift in either the script `?` markers, the native `FO_NULLABLE` annotations, or a misplaced marker (outside an `///@ ExportMethod` or on a non-entity type) fails the run with a hint pointing to the generator task to fix it.

### Manual

```bash
python Tools/NullableEstimate/apply_nullables.py          # apply script-side markers
python Tools/NullableEstimate/apply_native_nullable.py    # apply engine-side markers
python Tools/NullableEstimate/validate_nullable.py        # check placement
python Tools/NullableEstimate/estimate_nullables.py       # report only
```

Append `--check` to either applier to verify idempotency without writing files.

## Adding / editing markers

When you write a new script function or native export, you can either:
1. Write it however you want and run `Generate :: Nullable Markers` â€” the analyzer fills in the markers per the rule above and strips any dead defensive code.
2. Write the marker yourself if you know better than the heuristic (e.g. user-facing API where you want to lock the contract). The analyzer is idempotent: it won't re-introduce dead checks once stripped, and respects existing markers when their pattern matches the rule.

When the analyzer's heuristic gets it wrong (saw it with `dynamic_cast<X*>(param)` paths where param is genuinely nullable), prefer extending the heuristic in [apply_native_nullable.py](../../Tools/NullableEstimate/apply_native_nullable.py) over a one-off manual edit â€” the next CI check will revert manual edits otherwise.

## See also

- [Scripts.md](../../Docs/Scripts.md) â€” overall AngelScript module organization and conventions.
- [NativeExtensions.md](../../Docs/NativeExtensions.md) â€” `///@ ExportMethod` codegen pipeline and engine source layout.
- [Testing.md](../../Docs/Testing.md) â€” running unit and gameplay tests that exercise the runtime check.