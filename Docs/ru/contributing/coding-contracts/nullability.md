---
layout: default
title: Nullable-типы
locale: ru
document_id: nullability
permalink: /Docs/ru/contributing/coding-contracts/nullability.html
---

# Nullable-типы

<!-- docs-translation: {"document_id":"nullability","locale":"ru","source_path":"Docs/en/contributing/coding-contracts/nullability.md","source_sha256":"70499fe4522bb639b64cd3d85c842755156da647b3dbd6e1720cf6f101d375b1"} -->

> Документация принадлежит движку. Эта страница задает переиспользуемый
> контракт компилятора, runtime и native-границы. Анализаторы проекта могут
> вводить более строгую политику авторинга, но не являются частью контракта
> движка.

Здесь описана nullability для AngelScript и native-кода. Общая архитектура
скриптов находится в разделе [Скриптовый runtime](../../explanation/scripting-runtime/),
а владение экспортированными native-методами — в
[карте методов](../../reference/script-api/method-ownership.md).

## Основной принцип

> Лучше не передавать `null`, чем защитно проверять его внутри и молча выходить.
>
> Параметр или результат помечается nullable только тогда, когда функция
> осмысленно обрабатывает оба состояния. Ранний выход при `null` часто означает,
> что контракт должен быть non-null, а исправление принадлежит вызывающему коду.

Правило симметрично по обе стороны границы script-engine.

## Скриптовая сторона: суффикс `T?`

AngelScript использует суффикс `?` в стиле Kotlin/C#. По умолчанию handle
**non-nullable**.

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

`?` разбирает сам front-end AngelScript: `ParseType` в
[as_parser.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_parser.cpp),
`MakeNullable`/`isNullable` в
[as_datatype.h](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_datatype.h)
и `CreateDataTypeFromNode` в
[as_builder.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_builder.cpp).
Preprocessor больше не переписывает этот маркер. `int?` и другое неверное
размещение завершается compile-time ошибкой. В выражении тернарный оператор
`cond ? a : b` остается однозначным, поскольку суффикс читается только внутри
`ParseType`.

### Объявления `///@ Event` и `///@ RemoteCall`

Суффикс поддерживается в тегах `///@ Event` и `///@ RemoteCall`.
[`MetadataBaker`](../../../../Source/Tools/MetadataBaker.cpp) переносит бит
nullable каждого аргумента в `ArgDesc::Nullable` у `EntityEventDesc::Args` и
`RemoteCallDesc::Args`.

```angelscript
///@ Event Server Game OnCritterDamaged(Critter cr, Critter? attacker, int32 damage)
///@ Event Server Game OnCritterDead(Critter critter, Critter? killer)
///@ RemoteCall Server SwitchCharacter(Critter? newCritter)
```

Объявление является контрактом. Каждый `[[Event]]`,
`[[ServerRemoteCall]]` и `[[ClientRemoteCall]]` с тем же именем обязан
повторять `?` по аргументам. Baker и side-specific binding проверяют remote
calls; подробности см. в [Remote Calls](../../reference/scripting/remote-calls.md).
`[[AdminRemoteCall]]` является отдельной командной точкой входа.

```angelscript
// Matches the OnCritterDamaged declaration above.
[[Event]]
void OnCritterDamaged(Critter cr, Critter? attacker, int32 damage) { ... }

// Violates declaration parity: declaration has `Critter?`, handler drops `?`.
[[Event]]
void OnCritterDamaged(Critter cr, Critter attacker, int32 damage) { ... }
```

AS runtime применяет null-контракт к записи handle, но сопоставление двух
независимых declaration-ов события или remote call остается задачей
статического анализа проекта.

## Сторона движка: `ptr<T>` / `nptr<T>` и raw-pointer nullability

Native-методы `///@ ExportMethod` из
[`Source/Scripting`](../../../../Source/Scripting/), экспортированные события,
`FO_ENTITY_EVENT`, `///@ ExportRefType` и `///@ EngineHook` используют
словарь [smart pointer-ов](smart-pointers.md): `ptr<T>` для non-null borrow и
`nptr<T>` для nullable borrow. Raw `T*` больше не является spelling-ом
nullable export: codegen отвергает его, а marshalling templates делают
`static_assert`. Raw ABI-значения из низкоуровневого AS plumbing, argv и
внешних C callbacks связываются через `make_ptr(raw)` или `make_nptr(raw)`.
Пустой маркер `FO_NULLABLE` удален.

В native-коде вне export-сигнатур действуют те же `ptr`/`nptr` и owning-формы
`unique_*`/`refcount_*`. `nptr<T>`, `unique_nptr<T>` и `refcount_nptr<T>` —
отдельные nullable-типы. Для dynamic cast владельца вызывайте
`owner.dyn_cast<T>()` напрямую.

Сохраненная сигнатура `ScriptFunc` обязана совпадать со скриптовым callback:
`Item?` хранится как `nptr<Item>`, а `Item` как `ptr<Item>`. Иначе законный
`null` пройдет export-границу и упадет при позднем неявном narrowing.

Предпочитайте non-null spelling. `nptr<T>` нужен только для реального и
обрабатываемого отсутствия: результата fallible cast/lookup, настоящего
transient-null окна поля или boundary-helper-а, который принимает nullable и
явно проверяет его. После guard-а используйте тот же `nptr` напрямую; его
преобразование в `ptr` проверяет non-null в точке преобразования. Owning
wrappers неявно дают borrow, но получение владения и narrowing nullable-owner
остаются явными (`hold_ref`, `adopt_unique_ptr`, `make_unique_del_ptr`,
`take_not_null`, `SafeAlloc::MakeShared`). `.as_ptr()` и `.as_nptr()` полезны
для ясности или overload resolution, но не обязательны.

Не делайте nullable пару `pointer + size` ради пустого буфера. Принимайте
`const_span<uint8_t>` / `span<uint8_t>` и проверяйте `.empty()`. Nullable-local
обычно связывается через `auto`, затем проверяется
`FO_VERIFY_AND_THROW(local, ...)` либо `FO_STRONG_ASSERT` в `noexcept` перед
deref. Project audit может дополнительно проверять guarded dereference; его
правила описаны в [Smart Pointers](smart-pointers.md).

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

`self` и неявный `engine` глобальных методов никогда не помечаются: AS
проверяет receiver до dispatch. Если pointer-аргумент export-метода имеет
default `nullptr`, он обязан быть `nptr<T>`; `ptr<T>` не может иметь такой
default.

### Accessor компонента non-nullable и бросает; проверяйте `Has<Component>`

Getter компонента сущности (`item.Weapon`, `item.MapExit`,
`cr.DialogContext`, `item.Locker` и другие свойства `Component`) является
non-nullable и бросает, если компонента нет. `Entity_GetComponent` в
[AngelScriptEntity.cpp](../../../../Source/Scripting/AngelScript/AngelScriptEntity.cpp)
вызывает `ScriptException`; рядом регистрируется bool-accessor
`Has<Component>`.

```angelscript
// item.Weapon is ItemWeaponComponent (non-nullable) — access directly
int dist = item.Weapon.MaxDist;        // OK; throws iff the item is not a weapon

// probe presence with Has<Component>, never `== null`
if (item.HasWeapon) {
    int d = item.Weapon.MaxDist;        // guarded
}
verify(item.HasWeapon, "Item must be a weapon");
```

Отсутствующий компонент в коде, который предполагает его наличие, является
нарушением инварианта. Не пишите `item.Weapon == null`: getter бросит раньше.
Используйте `item.HasWeapon`. Правило едино для concrete, `Abstract`, `Proto`,
`Static` и fixed type. Учтите только различие имен: `Ammo` является компонентом
Item, а `item.Weapon.Ammo` — nullable-свойством загруженного боеприпаса.

### Бросающие глобальные getter-ы: `Chosen` / `CurMap` / `CurLocation` / `CurPlayer`

Клиентские getter-ы, зарегистрированные в
[ClientGlobalScriptMethods.cpp](../../../../Source/Scripting/ClientGlobalScriptMethods.cpp),
non-nullable и бросают при отсутствии. Для штатной проверки существуют
`HasChosen`, `HasCurMap`, `HasCurLocation`, `HasCurPlayer`.

```angelscript
if (!HasChosen) {
    return;        // no chosen critter right now - handle it
}
Critter cr = Chosen;   // OK - non-nullable, guaranteed present here
```

`Chosen == null` одновременно вызывает предупреждение о лишнем сравнении и
может бросить при вычислении getter-а; используйте `!HasChosen`.

### `Game` при завершении: `IsGameDestroying`

`Game` — non-nullable global handle. Реально он отсутствует только в
деструкторах script-object-ов во время разрушения backend-а: GC вызывает их
после сброса engine pointer в
[AngelScriptBackend.cpp](../../../../Source/Scripting/AngelScript/AngelScriptBackend.cpp),
а `get_Game` бросает. Проверка `Game != null` не помогает, потому что сама
вычисляет бросающий getter.

`IsGameDestroying`, зарегистрированный рядом с `get_Game` в
[AngelScriptGlobals.cpp](../../../../Source/Scripting/AngelScript/AngelScriptGlobals.cpp),
безопасно сообщает тот же факт через `HasGameEngine()`.

```angelscript
~Sprite()
{
    // Game engine may already be gone during shutdown; freeing the sprite then is both impossible and unnecessary
    if (!IsGameDestroying) {
        Unload();   // calls Game.FreeSprite(...)
    }
}
```

Используйте probe только в деструкторах и teardown-путях. В остальных местах
`Game` гарантирован и читается напрямую.

### Бросающие proto-getter-ы: `Game.GetProtoItem/Critter/Map/Location` + `CheckProtoX`

`Game.GetProtoItem`, `GetProtoCritter`, `GetProtoMap`, `GetProtoLocation` из
[CommonGlobalScriptMethods.cpp](../../../../Source/Scripting/CommonGlobalScriptMethods.cpp)
non-nullable и бросают для неизвестного id. Когда отсутствие допустимо,
используйте соответствующий `Game.CheckProtoX(pid)`.

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

Не сравнивайте результат `Game.GetProtoX(pid)` с `null`: getter бросает раньше.
То же относится к codegen-getter-ам custom proto/fixed types, например
`Game.GetProtoModifier`, `Game.GetProtoFaction`,
`Game.GetEncounterProfileData`, `Game.GetWeatherType`, `Game.GetItemBag`.
Для них генерируется `Game.Check<Name>(pid)` в `register_entity_protos` и
`register_fixed_type` из
[AngelScriptEntity.cpp](../../../../Source/Scripting/AngelScript/AngelScriptEntity.cpp).
Известный authored id читайте напрямую, потенциально отсутствующий сначала
проверяйте.

## Runtime enforcement

Контракт обеспечивают два дополняющих runtime-рубежа.

### Скриптовая сторона: `asBC_RefCpyChk` при записи handle

AngelScript compiler создает `asBC_RefCpyChk` для записи в пользовательский
non-nullable handle. Инструкция определена в
[angelscript.h](../../../../ThirdParty/AngelScript/sdk/angelscript/include/angelscript.h),
реализована в
[as_context.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_context.cpp),
а места emission находятся в `PerformAssignment` и
`CompileInitializationWithAssignment` из
[as_compiler.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp).
Null-source вызывает `Null assignment to non-nullable handle`; `T?` использует
обычный `asBC_REFCPY`.

Выбор инструкции основан на **объявленном** типе назначения, а не на текущем
smart-cast. Поэтому `x = null` внутри guard-а для объявленного `T?` законно
сбрасывает narrowing; после записи следующая операция снова видит `T?`.
Compiler-generated temporaries пропускают эту проверку, потому что nullability
native-параметра проверяет следующий рубеж. Script-to-script initialization и
assignment по-прежнему защищены.

### Native-граница: сгенерированные проверки аргументов и результата

[`BuildTools/codegen.py`](../../../../BuildTools/codegen.py) вставляет
`NativeDataProvider::CheckArgNotNull` и `CheckReturnNotNull` из
[`ScriptSystem.h`](../../../../Source/Common/ScriptSystem.h) непосредственно в
`MethodDesc::Call`, до и после native invocation:

```text
MethodDesc::Call(call)
  → NativeDataProvider::CheckArgNotNull(call, i, "Server_Player_SetCritter", "cr", "Critter")   // for each non-nullable entity arg
  → native invocation
  → NativeDataProvider::CheckReturnNotNull(call, "...", "...")                                  // for non-nullable entity return
```

Массивы здесь целиком не сканируются: metadata пока не различает
`array<T>` и `array<T?>`, а sparse/null-element массивы допустимы. Инвариант
элементов проверяет владелец конкретного API. Scalar-проверка покрывает любого
caller-а `///@ ExportMethod` и стоит одно сравнение pointer-а.

Нарушения становятся `ScriptException` с разными сообщениями:

- `Null assignment to non-nullable handle` от `asBC_RefCpyChk`;
- `Null pointer access` от исходных dereference-checks (`asBC_CHKREF` и
  родственных инструкций);
- native boundary называет метод, параметр и тип через сгенерированные checks.

### Compile-time гарантии

Front-end дополнительно применяет пять правил:

1. Bare `null` в non-nullable handle всегда является ошибкой, включая
   неявный `T x;` с null initializer.
2. Nullable source в non-nullable destination является ошибкой при включенном
   `asEP_DISALLOW_NULLABLE_TO_NON_NULLABLE`; FOnline включает его в
   [AngelScriptBackend.cpp](../../../../Source/Scripting/AngelScript/AngelScriptBackend.cpp).
3. `T?` с заведомо non-null initializer вызывает warning `Redundant '?'`.
   Исключения: `cast<T>(...)`, ternary и `T@const&`, где runtime-значение может
   быть null (`dict.get(key, default)`).
4. Dereference ненаруженного `T?` вызывает warning и требует local/param guard.
   Повторный вызов getter-а является новым выражением, поэтому его сначала
   связывают с local.
5. `== null` и `!= null` со статически non-null handle вызывают warning о
   константном результате. Это относится и к временным значениям. Бросающие
   getter-ы проверяются через `Has*`/`Check*`, fallible cast записывается как
   `cast<T?>`, nullable proto/fixed property получает flag `Nullable`, а native
   nullable return — `nptr<T>`.

Conditional expression становится `T?`, если хотя бы одна ветвь nullable или
равна `null`. Патч `CompileCondition` сохраняет этот бит после унификации типов.
Runtime `asBC_RefCpyChk` остается последней защитой для форм, которые
compile-time анализ не видит, например null из `dict.get`.

### Сравнение идентичности: только `==`, без `is` / `!is`

Проектная конвенция запрещает `is` и `!is` в `.fos`. Патч `CompileOperator` в
[as_compiler.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp)
использует `asBC_CmpPtr`, если ref type не имеет `opEquals`.

| Операнды | Поведение `==` / `!=` |
|---|---|
| Две entity-типа с codegen `opEquals` | Сравнение id. |
| Одна сторона `null` | Сравнение handle с null. |
| Ref types без `opEquals` | Идентичность handle через `asBC_CmpPtr`. |
| Reference и handle | Неявное преобразование к handle, затем идентичность. |

Таким образом, id-based или pointer-based семантику определяет тип, а не
оператор. Проект может закреплять это read-only CI-проверкой.

### Smart-cast: flow-sensitive narrowing

Smart-cast сужает local `T?` до `T` в доказуемо non-null области. Реализация
использует per-scope `smartCasts` в
[as_compiler.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp):
`DetectNullCheckPattern`, `GetNarrowedTypeForLocal`, `CompileIfStatement`,
`CompileCondition`, `CompilePostFixExpression`. Цепочки `&&`/`||` накапливают
все подходящие locals через `nullCheckNarrowList`, включая параметры с
отрицательным stack offset. `break` и `continue` считаются выходом из текущего
пути для narrowing, но не для анализа обязательного return.

Поддерживаются следующие формы:

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
bool cmp   = maybeItem != null && maybeItem.Count == wanted;        // maybeItem.Count narrowed
if (maybeItem != null && maybeItem.Count > 0 && Other()) { ... }    // narrowed across the compound
// every checked local in the chain narrows in the later operands, not just the nearest:
if (a != null && b != null && a.Count == b.Count) { ... }          // both a and b narrowed
if (a == null || b == null || a.Count != b.Count) { return; }      // both narrowed past the ||s

// 6) Ternary branches narrow when the condition is a null-check
int n = maybeItem != null ? maybeItem.Count : 0;         // then-branch narrowed
int m = maybeItem == null ? 0 : maybeItem.Count;         // else-branch narrowed
```

Smart-cast намеренно не сужает class fields и globals, результаты методов,
bare `cast<T>`, смешанные `&&`/`||` одного уровня и local, переназначенный
внутри operand-а цепочки. Свяжите выражение с local, разделите условие,
оставьте destination `T?` или используйте явную recovery-ветку.

<a id="reference-casts"></a>

### Reference cast: `cast<T?>(x)`

Reference cast может вернуть `null`. Fallible-форму пишут как `cast<T?>(x)`:
сравнение с null становится законным, а dereference требует предварительного
narrowing. Bare `cast<T>(x)` означает обещание успеха и может использоваться
цепочкой, но его сравнение с null считается лишним. Это реализует
`CompileConversion` в
[as_compiler.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp).

Эта форма также честно читает sparse legacy `array<T>`, где пустая handle-cell
может быть runtime-null при статическом типе `T`. Предпочтительно объявлять
такое хранилище `array<T?>`; для существующего sparse массива используйте
`cast<T?>(field[i])`. Политику null-элементов параметров, результатов и
sync/lock scopes задает владелец конкретного API.

### Nullable property handles

`///@ Property` с handle-типом proto или fixed type по умолчанию non-nullable,
хотя пустое поле физически возвращает `null`. Если unset является допустимым
состоянием, добавьте flag `Nullable`:

```angelscript
///@ Property Item Server ProtoItem UsableOn.TargetItem Nullable
///@ Property Item Common ItemBag Harvested.SmallBag Nullable
///@ Property Critter Server ProtoItem StartWeapon Nullable
```

Flag разбирает `Properties.cpp`, а
[AngelScriptEntity.cpp](../../../../Source/Scripting/AngelScript/AngelScriptEntity.cpp)
регистрирует getter как `@?`; `MetadataBaker` разрешает его только для
FixedType/Proto entity property. `ItemBag?` в теге не заменяет flag.

У `Mutable` nullable property setter также регистрируется как `@?+`. Это
обязательно: AngelScript выводит статический тип virtual property из параметра
setter-а, если setter существует. Non-null setter рядом с nullable getter
делал бы оба spelling-а чтения ошибочными.

### Макрос `verify`

`verify(cond, message, ...)` определен в
[Core.fos](../../../../Source/Scripting/AngelScript/CoreScripts/Core.fos) и
видим во всех `.fos`:

```text
#define verify(cond, ...) if (!(cond)) throw(__VA_ARGS__)
```

Он выражает всегда выполняемый инвариант нашей серверной и клиентской логики.
В release он не удаляется. Название отличает его от debug-only `assert`.

#### `verify` и штатное восстановление

Скрипты не вызывают `throw` напрямую, все failure-paths проходят через
`verify`:

| Форма | Когда использовать |
|---|---|
| `verify(cond, "...")` | Нарушение `cond` означает баг или запрещенный запрос. |
| `verify(false, "...")` | Безусловно недостижимая ветка или log-then-fail; это no-return, дополнительный `return` не нужен. |
| `if (x == null) { <recover>; return ...; }` | Ожидаемый runtime-результат, для которого есть fallback. |

#### Клиентский ввод: transport validation и скриптовые инварианты

Любой client-originated payload недоверен на серверной границе. Движок до
синхронизации и dispatch проверяет framing remote call, размер payload,
границы коллекций, формы encoded values и полное потребление буфера; см.
[Remote Calls](../../reference/scripting/remote-calls.md). Handler проекта до
первого изменения проверяет авторизацию, владение объектом, диапазоны,
переходы состояния и другие доменные правила.

Внутри скрипта `verify` является всегда активным механизмом отклонения данных,
нарушающих контракт handler-а. Он дополняет, а не заменяет native transport
checks и семантическую проверку. Для ожидаемого gameplay-отказа используйте
обычную recovery-ветку.

#### Narrowing и аргументы

`throw(message, ...)` — зарегистрированная в
[AngelScriptGlobals.cpp](../../../../Source/Scripting/AngelScript/AngelScriptGlobals.cpp)
no-return global function, а не keyword. Она является примитивом, в который
раскрывается `verify`; напрямую скрипты ее не вызывают. Успешный
`verify(x != null, ...)` сужает `x` до конца scope так же, как ранний return.
Variadic-аргументы передают в исключение полезный контекст; сообщение должно
быть читаемым предложением.

Проверяется каждый script handle на native-границе:

- `///@ ExportEntity` и generic `Entity`;
- `Abstract<Entity>`, `Proto<Entity>`, `Static<Entity>`;
- `///@ ExportRefType`, включая `MovingContext`, `MapSpriteHolder`,
  `SpritePattern`, `VideoPlayback`, `ScriptImGui`.

Единственным арбитром является `is_validated_pointer_meta_type(...)` в
[`codegen.py`](../../../../BuildTools/codegen.py). `ptr<T>` считается non-null,
`nptr<T>` nullable, raw handle pointer отвергается. `?` на primitive type
запрещен.

Script-to-script передача аргументов не полностью покрыта `asBC_RefCpyChk`,
поскольку call setup может обходить REFCPY. Ее дополняют анализатор declaration
проекта и native-boundary checks.

### Примечание по миграции

Изменение source-incompatible для скриптов, записывавших потенциальный `null` в
bare handle: destination теперь обязан быть `T?`. Inline test scripts в
`Source/Tests/Test_*.cpp` обновлены вместе с runtime.

Проект после миграции должен включать
`asEP_DISALLOW_NULLABLE_TO_NON_NULLABLE`. Оставшийся runtime exception обычно
означает пропущенный marker или неверный non-null контракт. Формы, невидимые
smart-cast: `dict<K,V>.get(key, null)`, output references и class fields,
заполняемые между методами.

FOnline работает с `asEP_ALLOW_IMPLICIT_HANDLE_TYPES`: пользовательский код
пишет `Critter`, `Item?`, `array<Critter>`, а не `Critter@`, `Item@?`.
Builder отвергает explicit `@` для implicit-handle типов. Funcdef сохраняет
`Type@+`, поскольку `+` семантически нужен. Native registration strings и
lookup declarations могут использовать explicit `@`; запрет действует только
при сборке пользовательского модуля, не в silent lookup mode. Реализация —
`CreateDataTypeFromNode` в
[as_builder.cpp](../../../../ThirdParty/AngelScript/sdk/angelscript/source/as_builder.cpp).

## Инструменты проекта

Движок владеет compiler/runtime enforcement и native binding contract. Проект
может добавить read-only анализаторы для правил, связывающих независимые
declaration-ы:

- `?` только на handle-capable reference types;
- parity nullability у event и remote-call handlers;
- narrowing nullable local перед dereference;
- прямой dereference проверенного `nptr<T>` без лишнего alias;
- единый implicit-handle style без `@`;
- запрет `is`/`!is` и возврата лишних defensive guards.

Marker задается автором явно. Выводить nullability из тела ненадежно.
Названия project generators и analyzer tasks остаются в документации проекта.
Переиспользуемый checker сначала должен перейти в `BuildTools/` вместе с
engine-owned tests.

## Добавление и изменение маркеров

1. Явно выберите контракт в declaration: `T?` в script, `nptr<T>` для
   nullable native export и `ptr<T>` для non-null.
2. Скомпилируйте затронутые script modules, чтобы запустить диагностики движка.
3. Запустите project-side проверки declaration parity и style.

Rewriter проекта не должен выводить контракт сам. Он сохраняет выбранные
автором markers и может удалить только guard, ставший лишним из-за generated
runtime checks. Реальный nullable path, например осмысленный результат
`dynamic_cast`, остается в сигнатуре.

## См. также

- [Скриптовый runtime](../../explanation/scripting-runtime/) — архитектура backend-а и exports.
- [Remote Calls](../../reference/scripting/remote-calls.md) — сигнатуры, serialization и handlers.
- [Карта методов](../../reference/script-api/method-ownership.md) — владение `///@ ExportMethod`.
- [Generated API и metadata](../../reference/metadata/index.md) — поток generated contracts.
- [Smart Pointers](smart-pointers.md) — native ownership/nullability vocabulary.
- [Testing](../../../Testing.md) — границы unit, script, integration, package и smoke tests.
