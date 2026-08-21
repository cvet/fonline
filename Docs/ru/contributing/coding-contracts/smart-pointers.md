---
layout: default
title: Умные указатели
locale: ru
document_id: smart-pointers
permalink: /Docs/ru/contributing/coding-contracts/smart-pointers.html
---

# Умные указатели

<!-- docs-translation: {"document_id":"smart-pointers","locale":"ru","source_path":"Docs/en/contributing/coding-contracts/smart-pointers.md","source_sha256":"4fd60f2e817a1c1b0da43bab015e10323067cac06e084689feb06c34b12894d5"} -->

> Документация движка. Эта страница определяет словарь native C++-указателей
> из `Source/Essentials/SmartPointers.h`: владение, nullability, правила
> миграции и требования к проверке.

## Назначение

Движок использует небольшие обёртки, чтобы контракт указателя был виден на
уровне типа. Долгосрочное соглашение соответствует модели nullability в
скриптах: отсутствие должно быть явным. Для non-null типа `null` не является
нормальным состоянием; nullable-тип использует написание с `n*`.

Переход от старого имени `raw_ptr` выполнялся поэтапно. `raw_ptr<T>` и
`nullable_raw_ptr<T>` удалены из API движка; используйте `ptr<T>` и `nptr<T>`.
`nptr<T>`, `unique_nptr<T>` и `refcount_nptr<T>` являются отдельными nullable
wrapper types. Обычные `ptr<T>`, `unique_ptr<T>` и `refcount_ptr<T>` всегда
подчиняются строгому non-null контракту; nullable-состояние принадлежит
соответствующему типу `n*`.

## Словарь указателей

| Смысл | Non-null написание | Nullable написание |
| --- | --- | --- |
| Невладеющий borrowed pointer | `ptr<T>` | `nptr<T>` |
| Уникально владеющий pointer | `unique_ptr<T>` | `unique_nptr<T>` |
| Владеющий intrusive-refcount pointer | `refcount_ptr<T>` | `refcount_nptr<T>` |
| Уникальный pointer на массив | `unique_arr_ptr<T>` | Будущий `unique_arr_nptr<T>`, если понадобится |
| Уникальный pointer с custom deleter | `unique_del_ptr<T>` | `unique_del_nptr<T>` |

`raw_ptr<T>` / `nullable_raw_ptr<T>` являются устаревшими написаниями. Новый
код движка должен применять `ptr<T>` или `nptr<T>`.

## Контракты

`ptr<T>` является невладеющим borrowed pointer. В пригодном для использования
состоянии он non-null, не имеет обычного пути присваивания/проверки `nullptr` и
не должен моделировать optional-состояние.

`nptr<T>` является nullable borrowed pointer. Используйте его для lookup,
текущего/активного/выбранного состояния, отсутствующих backend handles и
результатов API, где отсутствие нормально.

**Оставляйте проверенное nullable-значение в `nptr<T>` и разыменовывайте его
напрямую.** Не создавайте копию `nullable_x` и не сужайте её до `ptr<T>` только
ради вызова. У `nptr<T>` есть `operator->` / `operator*` с тем же unchecked
deref, что у `ptr<T>`, поэтому после guard прямое разыменование бесплатно, а
чистое доменное имя остаётся у самого `nptr<T>`:

```cpp
nptr<Critter> target = engine->GetCritter(id);   // clean domain name stays on the nptr
if (!target) {
    return;                                       // guard proves non-null for the rest of the scope
}
target->Foo();                                    // deref the nptr directly — no as_ptr(), no copy
```

Guard может быть ранним `if (!target) { <recover>; return; }`, положительным
`if (target) { ... }`, short-circuit `target && target->Foo()`, if-init,
условием цикла или завершающим при ошибке `FO_VERIFY_AND_THROW(target, "...")`
/ `FO_STRONG_ASSERT(target, "...")`. Каждый вариант доказывает non-null, и
`as_ptr()` лишь добавил бы повторный assert. В условиях verify/assert пишите
truthiness указателя напрямую: `target`, `size == 0 || data`; если присутствие
сравнивается с другим bool, вычислите его явно через
`static_cast<bool>(target)` для wrapper или `raw != nullptr` для raw pointer.
Подключающий проект может проверять правило guard-aware анализатором над Engine
и native extensions. Репозиторий Engine владеет правилом и wrapper/runtime
тестами, но пока не поставляет такой анализатор. Проектная проверка
`NullableLocalDereference` должна исправлять каждый hit в исходнике, а не
скрывать его count baseline.

**Проверенный `nptr<T>` неявно сужается до `ptr<T>`: не пишите `.as_ptr()` там,
где ожидается `ptr<T>`.** Преобразующий конструктор проверяет non-null в точке
преобразования тем же always-on assert, что и `.as_ptr()`, поэтому проверенное
значение напрямую передаётся в параметр, поле или return типа `ptr<T>`:

```cpp
nptr<Map> map = EntityMngr.GetMap(map_id);       // clean domain name on the nptr
if (!map) {
    return;                                       // guard proves non-null
}
FindPath(map, cr, from_hex, to_hex);              // FindPath takes ptr<Map> — implicit narrow, no .as_ptr()
```

Преобразование распространяет `const` и сохраняет mutability: из
`const nptr<T>` получается только `ptr<const T>`. Если nullable source хранится
в deduced local, а дальше требуется присутствие, сохраняйте сам wrapper и
проверяйте его до deref или неявной передачи в `ptr<T>`. Не полагайтесь на
скрытый assert внутри `.as_ptr()`:

```cpp
auto target = engine->GetCritter(id);
FO_VERIFY_AND_THROW(target, "Target critter not found");
target->Foo();                                    // checked nptr, direct deref
UseTarget(target);                                // UseTarget takes ptr<Critter>
```

Явные `.as_ptr()` / `.as_nptr()` уместны, когда показывают borrow, разрешают
overload/template deduction, материализуют значение после move владельца или
создают копируемый lambda capture. Если destination type уже однозначен,
оставляйте неявное преобразование.

Для raw `T*` используйте глобальный helper и deduced local:

```cpp
auto target = make_ptr(raw_target);               // raw T* -> ptr<T>, asserts non-null
auto maybe_target = make_nptr(raw_target);        // raw T* -> nptr<T>, preserves null
```

Владеющие wrapper заимствуются так же: `refcount_ptr<T>` /
`refcount_nptr<T>`, `unique_ptr<T>` / `unique_nptr<T>`, `unique_del_ptr<T>` /
`unique_del_nptr<T>`, `unique_arr_ptr<T>` и `shared_ptr<T>` неявно переходят в
borrow-site `ptr<T>` / `nptr<T>` через `get()`. Преобразование в `ptr<T>`
проверяет non-null, а в `nptr<T>` сохраняет отсутствие; владение не передаётся.
В call/typed-return/member site с известным destination type передавайте owner
напрямую. Для локального доступа используйте исходный owner или `auto&`. Только
если deduction, move owner или lambda capture действительно требуют отдельный
borrow, создавайте `auto borrow = owner.as_ptr();` / `auto maybe_borrow =
owner.as_nptr();` и проверяйте nullable до dereference. Если nullable owner
только что получен из фабрики с проверенным non-null результатом и нужен для
нескольких обращений, работайте с ним напрямую.

Если временный borrow нужен только для dynamic cast, вызывайте `dyn_cast<T>()`
у owner: `_views[i].dyn_cast<EditorAssetView>()`, а не
`_views[i].as_ptr().dyn_cast<EditorAssetView>()`. Cast у `unique_ptr<T>` /
`unique_nptr<T>` возвращает borrowed `nptr<U>`. `refcount_ptr<T>` /
`refcount_nptr<T>` возвращают владеющий `refcount_nptr<U>`, если `U` сам
intrusive-refcountable, и borrowed `nptr<U>` для mixin/interface без refcount.
Намеренное получение владения из borrow пишется явно: `hold_ref()` /
`try_hold_ref()` или `require_refcount_ptr(...)`.

Обратные переходы от borrowed wrapper к owner остаются явными и проверяемыми:
`hold_ref()` / `try_hold_ref()` для intrusive refs,
`adopt_unique_ptr(ptr<T>)` для scalar unique adoption,
`make_unique_del_ptr(...)` для custom deleter, `take_not_null()` для narrowing
nullable owner и `SafeAlloc::MakeShared(...)` / domain factory для shared
ownership. Неявного перехода `ptr<T>` / `nptr<T>` в owner нет.

Если суженный `ptr<T>` действительно сосуществует с nullable в одной области,
называйте их по роли: чистое доменное имя получает рабочее значение, а boundary
value существует только для проверки или narrowing:

| Тип | local / parameter (`snake_case`) | data member (`_camelCase`) |
|---|---|---|
| raw `T*` | `raw_target` | `_rawTarget` |
| `nptr<T>`, сужаемый в сосуществующий `ptr<T>` | `maybe_target` / доменное имя | `_maybeTarget` / доменное имя |
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

Обычный случай оставляет чистое имя проверенному `nptr<T>`, поэтому не вводите
`nullable_` как ритуал narrowing, не придумывайте `_lookup`, `_ref`, `_ptr` или
`_begin` для каждого вызова и не копируйте именованный nullable/raw pointer
только ради префикса. После `!= nullptr` связывайте raw pointer сразу с
`ptr<T>`, без промежуточного `nptr<T>`.

Заведомо non-null значение, например `std::string::data()` /
`string_view::data()`, `&object` или уже non-null результат, связывайте прямо с
`ptr<T>` без проверки и `nptr<T>`. Не создавайте wrapper для локального pointer,
который не покидает функцию и нужен только для address arithmetic; достаточно
`ptr<T>` или raw pointer для чистой математики:

```cpp
ptr<const char> view_begin = _sv.data();        // data() is never null — no nptr, no check
ptr<const char> storage_begin = _s.data();
ptr<const char> storage_end = storage_begin.get() + _s.size();
if (view_begin < storage_begin || !(view_begin < storage_end)) { ... }
```

Исключение: exported/ABI parameter с именем, закреплённым внешним контрактом или
проектным allowlist, сохраняет имя; сужайте его в local `<name>_ptr`.
То же именование относится к `unique_nptr<T>::take_not_null()`,
`refcount_nptr<T>::take_not_null()` и custom-deleter narrowing.

`unique_ptr<T>` владеет одним объектом в usable state. Moved-from объект может
быть пуст только до destruction или reassignment. Если пустота нормальна для
модели, используйте `unique_nptr<T>`.

`unique_nptr<T>` владеет нулём или одним объектом и подходит для lazy resources,
optional owned state и объектов, создаваемых после owner или очищаемых до его
уничтожения. Проверенный nullable owner переносится в `unique_ptr<T>` через
`unique_nptr<T>::take_not_null()`, явно проверяющий invariant. Для custom
deleter используйте `take_not_null(unique_del_nptr<T>&)`; helper переносит и
сохранённый deleter.

В strict contract `unique_ptr<T>::release()` и
`unique_del_ptr<T>::release()` возвращают `ptr<T>`, а nullable-owner release
сохраняет `nptr<T>` или raw ABI storage. Разворачивайте `.get()` только на
явной ABI-, allocator- или adoption-границе. Helper, у которого type cast или
lookup может не сработать при передаче unique ownership, возвращает
`unique_nptr<T>`: неудачный cast является нормальным отсутствием.

`unique_del_ptr<T>` / `unique_del_nptr<T>` используются на внешних cleanup
boundaries. Non-null вариант допустим после доказательства присутствия и не
принимает default/null construction; nullable-вариант нужен для пустого, lazy,
moved-from или resettable состояния. Typed C array/buffer индексируйте через
сам owner (`owner[index]`), без временного borrow; `void` owner намеренно не
поддерживает indexing. Непрозрачный C resource можно хранить прямо как
`unique_del_ptr<void>` / `unique_del_nptr<void>`, если cleanup принимает
соответствующий `void*`.

`refcount_ptr<T>` владеет non-null intrusive reference: копирование увеличивает
счётчик, destruction уменьшает. `refcount_nptr<T>` хранит reference при наличии
и может быть пуст; он подходит для optional entity/view/current state и lookup,
которому нужно удержать найденный объект. Borrow выполняется неявно, а
`refcount_nptr<T>::take_not_null()` переносит проверенное владение в
`refcount_ptr<T>` без дополнительного add-ref.

Не оборачивайте nullable intrusive ownership в
`optional<refcount_ptr<T>>`; используйте `refcount_nptr<T>`. Результат загрузки,
различающий «отсутствует» и «ошибка», возвращает `refcount_nptr<T>` вместе с
отдельным error flag, например `bool& is_error`. Аналогично избегайте
`optional<ptr<T>>`, `optional<nptr<T>>`, `optional<unique_ptr<T>>`,
`optional<unique_nptr<T>>` и `optional<refcount_nptr<T>>`; используйте прямой
словарь wrapper или именованный domain result type.

`shared_ptr<T>` и `weak_ptr<T>` являются собственными shared-ownership типами
движка, без `std::shared_ptr` внутри. Atomic control block владеет объектом через
strong count и собой через weak count; объект размещён в той же allocation
через `SafeAlloc::MakeShared()`, а virtual destruction hook не требует полного
pointee type у holder. Типы с `shared_from_this()` / `weak_from_this()`
наследуются от `enable_shared_from_this<T>`; factory подключает embedded weak
reference после construction, поэтому использовать его в constructor нельзя.
Member casts: `shared_ptr<U>::dyn_cast<T>()` разделяет control block и пуст при
неудаче; `shared_ptr<const T>::cast_no_const()` является явным escape hatch.
Присутствующий shared owner неявно заимствуется в `ptr<T>`, потенциально пустой
в `nptr<T>`; owner должен жить весь период borrow. `unique_arr_ptr<T>` и
`unique_del_nptr<T>` тоже принадлежат движку, а
`unique_del_nptr<T>::get_deleter()` открывает deleter для проверяемой передачи.

Состояние class/struct не должно хранить C++ reference members (`T& _member`
или `const T& _member`). Параметры constructor и короткие local alias могут
оставаться reference. Обязательная stored borrowed dependency хранится в
проверенном `ptr<T>`, optional dependency в `nptr<T>`, `unique_nptr<T>` или
`refcount_nptr<T>`. Value member применяется только для действительно
принадлежащих объекту данных.

Аргументы процесса/runtime входят через ABI `argc` / `argv` и сразу переходят
в wrapper vocabulary. Для внутренних view используйте `CommandLineArg` /
`CommandLineArgs`; временный `char**` создавайте только перед последним ABI
handoff.

На проверенной raw cleanup boundary применяйте именованный helper вместо owner
construction из `.get()`: `adopt_unique_ptr(ptr<T>)` для scalar object и
`make_unique_del_ptr(ptr<T>, deleter)` /
`make_unique_del_ptr(nptr<T>, deleter)` для custom deleter. Domain helper может
оборачивать эти primitives, но call site не должен писать
`unique_ptr<T> {value.get()}` или `unique_del_ptr<T> {value.get(), deleter}`.

Для low-level byte/ABI reinterpret pointee type используйте
`ptr<T>::reinterpret_as<U>()` / `nptr<T>::reinterpret_as<U>()`, а не roundtrip
через raw `void*`. Результат сохраняет nullability и `const`, поддерживая
`ptr<void>` source.

`ptr<T>::void_cast()` / `nptr<T>::void_cast()` являются односторонним C/ABI
handoff и возвращают raw `void*`. Concrete typed nullable borrow восстанавливает
`cast_from_void<T*>(void_ptr)`. Для indirection вида `void**` в AngelScript
handle-slot plumbing переинтерпретируйте wrapper через
`ptr<void>::reinterpret_as<void*>()` или
`nptr<const void>::reinterpret_as<const void*>()`. Уже обёрнутый source не
оборачивайте повторно. Concrete-to-concrete storage reinterpret выполняйте
через явный deduced borrow; raw source сначала связывайте `make_ptr` /
`make_nptr`.

Mutable **byte span** превращается в typed span через
`bytes_to_objects<T>(span<uint8_t>)`, который проверяет кратность размера и
использует `reinterpret_as`; обратный `object_to_bytes<T>(T&)` предоставляет
`span<uint8_t>` одного объекта.

Typed **element span** из borrow pointer и length создавайте через
`make_span(ptr<T>, length)` / `make_const_span(ptr<T>, length)`, чтобы `.get()`
оставался внутри helper. Raw overload `make_const_span(const T*, n)` подходит
для container data. Другие overload `make_span` считают байты. Для текстового
`ptr<char>` / `ptr<const char>` используйте `as_str(size_t len)`.

### Always-on проверка non-null

Non-null invariant проверяется **всегда**, во всех build configuration, а не
debug-only `assert`. Construction `ptr<T>` из null raw pointer, каждый borrow
nullable/owner в `ptr<T>` и каждый ownership narrowing проверяют invariant; при
нарушении синхронно записываются expression, source location и native stack,
после чего процесс завершается.

Проверка называется `FO_BASIC_STRONG_ASSERT(expr)` и полностью принадлежит
`Essentials/FatalError.h/.cpp`, расположенному после `StackTrace` /
`BaseLogging` и до `SmartPointers` в строгом cascade Essentials. Поэтому
`SmartPointers` зависит только вверх и не обращается к более позднему
`ExceptionHandling`. `noexcept` reporter использует ранний native fatal path,
а не создаёт `StrongAssertationException`, поэтому безопасен из `noexcept`
members. Любой module Essentials после `FatalError` может использовать тот же
primitive; код на уровне `BasicCore`, `GlobalData`, `StackTrace` или
`BaseLogging` обязан применять механизм своего слоя.

Следствие: null внутри `ptr<T>` завершает процесс в точке construction даже в
release. Частые источники: `.data()` пустого контейнера, способный вернуть null,
и raw API с null как «не найдено». Реально nullable значение храните в
`nptr<T>`; transient buffer, используемый один раз, можно передать напрямую в
consumer с `nptr`/raw.

## Мосты refcount

Переход raw или borrowed pointer во владение intrusive refcount всегда явный:

```cpp
refcount_ptr<Entity> held = entity_ptr.hold_ref();
refcount_nptr<Entity> maybe_held = maybe_entity.try_hold_ref();

ptr<Entity> borrowed = held;
nptr<Entity> maybe_borrowed = maybe_held;
```

Если raw pointer неизбежен на ABI, atomic или allocator boundary, используйте
именованные factories:

```cpp
refcount_ptr<Entity> held_from_raw = refcount_ptr<Entity>::from_add_ref(raw_entity);
refcount_nptr<Entity> maybe_held_from_raw = refcount_ptr<Entity>::try_from_add_ref(raw_entity);
refcount_ptr<Entity> adopted = refcount_ptr<Entity>::from_adopted_ref(raw_entity_with_existing_ref);
```

Прямые `refcount_ptr<T>(T*)`, `operator=(T*)` и public `adopt_tag` недоступны,
чтобы raw/refcount переход был заметен при review.

## Контракт non-null и явных мостов

Эти правила безусловны. Старые `FO_STRICT_*` migration flags удалены; существует
только strict behavior.

- Прямые raw construction/assignment `refcount_ptr<T>` и public `adopt_tag`
  недоступны; используйте именованные factories.
- У `ptr<T>` нет default/null construction, assignment/comparison с `nullptr`,
  `operator bool`, `get_pp()` или `reset()` без replacement.
- У `unique_ptr<T>` / `unique_del_ptr<T>` / `refcount_ptr<T>` нет normal empty
  state. Их `release()` сохраняет non-null wrapper shape; nullable owners
  сохраняют nullable/raw ABI форму.

Любые два wrapper сравниваются напрямую через heterogeneous `operator==`:
пишите `item == other_item`, а не `.get()` с обеих сторон. Проектный анализатор
может называть вторую форму `DirectWrapperGetComparison`, но Engine-owned
контракт задают операторы wrapper и `Test_SmartPointers.cpp`.

## Allowlist raw pointer

Не навязывайте wrapper там, где raw pointer яснее отражает ABI или low-level
представление:

- `void*` и byte buffer с соседним size;
- internals allocator, placement new/delete и pointer arithmetic;
- C, OS, graphics, COM-style и third-party ABI;
- process/runtime `argc` / `argv` и последний `char**` handoff;
- generic AngelScript plumbing, registration strings, handle slots, low-level
  `char*` / `char**`; script-visible handle signatures используют wrapper;
- `std::atomic<T*>` до появления специального atomic nullable wrapper.

Во всех остальных Engine-owned API используйте `ptr<T>` / `nptr<T>` для borrow
и `unique_*` / `refcount_*` для владения.

## Граница script binding (`FO_SCRIPT_API`)

Binding `///@ ExportMethod` создаёт `BuildTools/codegen.py`, и каждый
script-visible handle в generated ABI использует `ptr<T>` / `nptr<T>`, включая
`///@ ExportEvent`, `FO_ENTITY_EVENT`, `///@ ExportRefType` и
`///@ EngineHook`. Codegen запрещает bare raw handle pointers, а
`NativeDataProvider` / `NativeDataCaller` содержат static assert.

- Receiver engine/entity всегда non-null и записывается `ptr<EngineType>` /
  `ptr<EntityType>`.
- Non-null argument/return использует `ptr<T>`, nullable использует `nptr<T>`.
- Container elements также используют wrapper: `vector<ptr<T>>`,
  `readonly_vector<ptr<T>>` / `readonly_vector<nptr<T>>`. Только metadata
  AngelScript сводит element к raw handle, сохраняя registered signature и
  compatibility hash; generated extern и `NativeCall` сохраняют C++ wrapper.
- Методы `///@ ExportRefType` используют wrapper для return и generated
  `ptr<RefType>` receiver; script metadata и compatibility hash не меняются.
- Регистрация AngelScript не меняется, поэтому миграция `.fos`, bytecode или
  save не нужна; изменяется только C++ glue type.

Raw на ABI edge намеренно остаются generic AngelScript plumbing, handle slots,
`char*` / `char**`, process argv и сторонние C callback. В export body связывайте
raw input с wrapper до обычной работы и вызывайте `.get()` только в последней
handoff line. Если declared return type сам raw pointer, `return wrapper.get();`
допустим; если функция возвращает wrapper, возвращайте wrapper. То же относится
к `get_no_const()` на последней mutable script ABI edge и placement-new в
`GetAddressOfReturnLocation()`.

## Правила миграции

1. Замените legacy `raw_ptr<T>` на `ptr<T>` без изменения поведения.
2. Замените legacy `nullable_raw_ptr<T>` на `nptr<T>`.
3. Пустые `ptr<T> ... {}` переведите в `nptr<T>`.
4. Пустые `unique_ptr<T> ... {}` переведите в `unique_nptr<T>`.
5. Пустые `refcount_ptr<T> ... {}` переведите в `refcount_nptr<T>`.
6. Проверьте lookup API, current-state fields и boundary adapters до tightening.
7. Передавайте проверенный `nptr<T>` прямо в `ptr<T>` site; отдельный
   `.as_ptr()` нужен только для самостоятельного borrow.
8. Для ownership narrowing применяйте `take_not_null()` соответствующего owner.
9. Заимствуйте refcount owner неявно; сужайте ownership только явно.
10. Вызывайте owner `dyn_cast<T>()` напрямую.
11. Return, способный вернуть `nullptr`/`{}`, должен иметь nullable type.
12. Не возвращайте удалённые permissive `FO_STRICT_*` modes; при интеграции
    старой ветки сначала перенесите optional state в nullable wrappers.

## Проверка

Во время staged migration используйте grep gates:

```powershell
rg "\braw_ptr<" Source SourceExt
rg "^\s*(mutable\s+)?ptr<[^\n;]+>\s+\w+[^;{}]*\{\}\s*;" Source SourceExt
rg "^\s*(mutable\s+)?unique_ptr<[^\n;]+>\s+\w+[^;{}]*\{\}\s*;" Source SourceExt
rg "^\s*(mutable\s+)?refcount_ptr<[^\n;]+>\s+\w+[^;{}]*\{\}\s*;" Source SourceExt
```

После pointer-layer изменений запускайте generated engine unit-test target
подключающего проекта. При изменении native bindings или exported signatures
дополнительно выполняйте script compilation/baking validation проекта.

Engine пока не поставляет whole-tree textual/AST smart-pointer checker.
Подключающий проект может хранить migration guards, но его commands, task names,
baselines и allowlists не являются нормативным доказательством движка.

Проектный checker должен применять точные line-level allowlists только для
проверенных ABI/low-level boundaries, а не count budgets; напрямую запрещать
class reference members и unguarded nullable dereference; отличать inventory
nullable owner от violation; использовать настоящий `compile_commands.json`
для AST. Перенесите checker в `BuildTools/` и добавьте Engine-owned tests прежде,
чем объявлять его стандартной командой этой документации.
