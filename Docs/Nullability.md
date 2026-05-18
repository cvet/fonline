# Nullability

> Engine-owned documentation. Paths under `../` are relative to the FOnline engine root. Paths under `../../` point to an embedding game project such as Last Frontier when this engine is used as a submodule.

Convention and runtime enforcement for nullable values across AngelScript and the native engine boundary. For the broader scripting runtime, see [Scripting.md](Scripting.md); for exported native method ownership, see [ScriptMethodsMap.md](ScriptMethodsMap.md).

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

The `?` suffix is **stripped by the engine preprocessor** before AngelScript parses the source â€” see `StripNullableTypeSuffix` in [../Source/Scripting/AngelScript/AngelScriptAttributes.cpp](../Source/Scripting/AngelScript/AngelScriptAttributes.cpp). The marker is purely script-source-level documentation; AS still sees plain handle types (`Critter@`, etc.).

The preprocessor distinguishes type-suffix `?` from ternary operator `?` by scanning forward at the same nesting level: a type-suffix is followed by an identifier/`[`/`,`/`)` boundary; a ternary is followed by `:` after the truthy expression.

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

Because the AngelScript preprocessor strips `?` before AS sees the source, the AS engine itself only enforces the bare types. The nullable contract is enforced statically by `validate_nullable.py` (and the engine's runtime null guards on entity meta-types â€” see Â«Runtime enforcementÂ» below).

## Engine side: `FO_NULLABLE` macro

Native methods declared with `///@ ExportMethod` in [../Source/Scripting/](../Source/Scripting/) use the inverse-of-pointer-default macro `FO_NULLABLE`. The owning method files are mapped in [ScriptMethodsMap.md](ScriptMethodsMap.md). Defined as empty in [../Source/Essentials/BasicCore.h](../Source/Essentials/BasicCore.h), it documents the nullability contract that codegen emits into the AS-side metadata.

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

## Runtime enforcement

Runtime validation is plumbed through codegen-generated `MethodDesc::Call` lambdas, **not** the AS-to-native bridge. [../BuildTools/codegen.py](../BuildTools/codegen.py) emits per-method calls to `NativeDataProvider::CheckArgNotNull` / `CheckReturnNotNull` (defined in [../Source/Common/ScriptSystem.h](../Source/Common/ScriptSystem.h)) right before/after the native invocation:

```
MethodDesc::Call(call)
  â†’ NativeDataProvider::CheckArgNotNull(call, i, "Server_Player_SetCritter", "cr", "Critter")   // for each non-nullable entity arg
  â†’ native invocation
  â†’ NativeDataProvider::CheckReturnNotNull(call, "...", "...")                                  // for non-nullable entity return
```

Doing it at the `MethodDesc::Call` boundary means **every** caller of an `///@ ExportMethod` is covered â€” the AS-to-native bridge, native test harnesses, future Mono-backend dispatch, anyone. The check has no per-call lookup cost beyond a single pointer compare.

Violation surface: `ScriptException` with the method name, parameter name and type, propagated to the calling AngelScript context.

**Scope of enforcement:** every **script handle** crossing the script â†” native boundary is validated. Concretely codegen emits the check when the meta-type is one of:
- a `///@ ExportEntity` name (`Critter`, `Item`, `Map`, `Location`, `Player`, `Game`, `ImGui`) or the generic `Entity`,
- an entity relative (`Abstract<Entity>`, `Proto<Entity>`, `Static<Entity>` â€” currently `AbstractItem`, `ProtoCritter`, `ProtoItem`, `ProtoLocation`, `ProtoMap`, `StaticItem`),
- a `///@ ExportRefType` class (`MovingContext`, `MapSpriteHolder`, `SpritePattern`, `VideoPlayback`, `ScriptImGui`).

On the C++ engine side the matching pointer spellings (`Critter*`/`CritterView*`, `Map*`/`MapView*`, `ProtoItem*`, `StaticItem*`, `MovingContext*`, â€¦) are all in scope. The membership test lives in `is_validated_pointer_meta_type(...)` in [../BuildTools/codegen.py](../BuildTools/codegen.py); the [validate_nullable.py](#tooling) walker mirrors it by parsing `///@ ExportEntity` / `///@ ExportRefType` headers at runtime.

Marking `?` / `FO_NULLABLE` on a primitive value type (`int`, `bool`, `mpos`, `hstring`, â€¦) is the only misuse [validate_nullable.py](#tooling) flags â€” those types have no `null` representation, so the marker is meaningless.

**Out of scope (not implemented yet):** script-to-script call validation. AS does not natively call our bridge for direct scriptâ†’script invocation; runtime enforcement there would require patching the AS interpreter (`asCContext::ExecuteNext`). In practice script-to-script null contracts are kept by:
- the static analyzer (see [Tooling](#tooling))
- the convention itself â€” every chain eventually reaches an engine call, which IS validated

## Tooling

Four Python tools in [Tools/NullableEstimate/](../../Tools/NullableEstimate/):

| Tool | Purpose |
|------|---------|
| `apply_nullables.py` | Scans `.fos` and strips dead defensive null guards on entity-pointer params that are NOT marked `?` â€” codegen / convention guarantees them non-null. Does **not** add or remove `?` markers; the author owns placement. Idempotent. |
| `apply_native_nullable.py` | Walks `///@ ExportMethod` declarations in `../Source/Scripting/*ScriptMethods.cpp` and strips dead defensive `if (param == nullptr) throw ...;` guards on entity-pointer params that are NOT marked `FO_NULLABLE` â€” codegen emits `NativeDataProvider::CheckArgNotNull` for those. Does **not** add or remove `FO_NULLABLE` markers; the author owns placement. Idempotent. |
| `validate_nullable.py` | Read-only placement check. Fails when `FO_NULLABLE` appears outside an `///@ ExportMethod` signature, on a non-pointer / primitive type, or when a script `?` is applied to a primitive. The marker on a non-entity handle (RefType / script class) is **allowed** â€” codegen does not emit a runtime check for it, but the marker is valid declarative documentation. |
| `estimate_nullables.py` | Read-only coverage report â€” counts function/parameter/return shapes across `.fos`. |

The applier tools accept `--check` (exit non-zero if dead defensive guards still exist) or `--dry-run` (preview without writing). `validate_nullable.py` is always read-only. CI uses these check modes to fail PRs that drift.

### Why marker placement stays explicit

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

When you write a new script function or native export:

1. Choose the nullable contract explicitly in the declaration (`T?` for script, `FO_NULLABLE` for native exported pointers) when the function meaningfully accepts or returns null.
2. Run the nullable applier/check tasks to remove dead defensive guards for non-null contracts and to verify idempotency.
3. Run `validate_nullable.py` to catch misplaced markers or primitive/value-type misuse.

The current appliers do not own contract inference: they preserve author-chosen markers and remove guard code made redundant by generated runtime checks. If a nullable case is real (for example a `dynamic_cast<X*>(param)` path where null is meaningful), keep the marker in the signature and update the analyzer only when its guard-removal pattern is wrong.

## See also

- [Scripting.md](Scripting.md) — overall engine scripting runtime, AngelScript backend, and method-export organization.
- [ScriptMethodsMap.md](ScriptMethodsMap.md) — current `///@ ExportMethod` file map and ownership boundaries.
- [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md) — generated metadata/API flow that must stay aligned with script-visible contracts.
- [../Source/Tests/README.md](../Source/Tests/README.md) — current test-suite source of truth until a dedicated engine `Docs/Testing.md` page exists.
