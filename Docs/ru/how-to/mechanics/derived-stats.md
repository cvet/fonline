---
layout: default
title: Производные характеристики с геттерами и сеттерами
locale: ru
document_id: recipe-derived-stats
permalink: /Docs/ru/how-to/mechanics/derived-stats.html
recipe_kind: primitive
---

# Производные характеристики с геттерами и сеттерами
<!-- docs-translation: {"document_id":"recipe-derived-stats","locale":"ru","source_path":"Docs/en/how-to/mechanics/derived-stats.md","source_sha256":"127732f982c675280d0ee18d351dc0b99d3fbdffb8b36ab4b93ee70128d9bb06"} -->
Сила и выносливость хранятся; грузоподъёмность и максимум здоровья следуют из них, а текущее здоровье никогда не должно превышать максимум. Этот рецепт вычисляет производные числа в геттерах свойств, зарегистрированных на обеих сторонах, удерживает хранимое здоровье в допустимых пределах преобразующим сеттером, реагирует на изменения сеттерами после сохранения и показывает, что владелец и наблюдатель могут вычислить из того, что реплицирует сервер.

| Backend | Статус |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

`runtime-verified` здесь означает, что сценарий `derived-stats` внутри процесса, сценарий `derived-stats-client` с настоящим клиентом без графического интерфейса и двухфазные сценарии перезапуска `derived-stats-restart-write` и `derived-stats-restart-verify` прошли на собранной песочнице после сборки ресурсов на хосте Windows. Не проверены: компиляция и запуск Managed C# (написание `AddPropertyDeferredSetter` взято со страницы о Managed C#), сеттеры для свойства `Virtual`, преобразующий сеттер на клиенте и теги `Min` и `Max`, упомянутые в расширениях.

## Проверенные исходные пути

- `Examples/MechanicsSandbox/Scripts/Recipes/DerivedStats.fos`
- `Examples/MechanicsSandbox/Scripts/Sandbox.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_DerivedStats.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/DerivedStats.cs`
- `Examples/MechanicsSandbox/Content/DerivedStats.fopro`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Common/Properties.h`
- `Source/Common/Properties.cpp`
- `Source/Scripting/AngelScript/AngelScriptEntity.cpp`
- `Docs/generated/api.json`

## Задача

У персонажа есть базовые значения, которые хранятся и реплицируются, и числа, вычисляемые из них. Если хранить и вычисляемые числа, получатся две копии, которые расходятся; если пересчитывать их вручную в каждом скрипте, формул окажется столько же, сколько мест вызова. Нужна такая форма: одна формула на каждое производное число, доступная везде, где есть входные данные; хранимое значение, остающееся в пределах своей производной границы; и реакция, выполняемая один раз на каждое настоящее изменение. Что означают числа и какая формула верна — игровая политика.

## Используемые примитивы движка

| Символ | Сторона | Роль в рецепте |
| --- | --- | --- |
| `script.method.server.Entity.IsPersistent` | сервер | Проверка в тесте, что у персонажа есть запись в базе данных до перезапуска |
| `script.method.client.Game.Chosen` | клиент | Персонаж владельца в серии запросов клиента |
| `script.method.client.Game.GetCritter` | клиент | Наблюдаемый персонаж в серии запросов клиента |

Рецепт опирается на механизм свойств, который модель API не перечисляет как символы: флаг свойства `Virtual`, `[[PropertyGetter]]`, `[[PropertySetter]]`, `Game.SetPropertyGetter`, `Game.AddPropertySetter` и `[[ModuleInit]]` — встроенные средства backend, а строки `///@ Property` — метаданные проекта. Правило порядка между преобразующими сеттерами и сеттерами после сохранения описано в разделе [преобразование свойства и реакция после записи](../scripting/lifecycle-and-concurrency.md#преобразование-свойства-и-реакция-после-записи). Свойство `Virtual` не может быть `Persistent` или синхронизируемым, и это обеспечивает регистратор при загрузке объявления (`Source/Common/Properties.cpp`).

## Минимальный пример

В `Examples/MechanicsSandbox/`: `Scripts/Recipes/DerivedStats.fos` содержит объявления, геттеры и сеттеры; `Content/DerivedStats.fopro` — прототип `DerivedStats_Npc`; `Scripts/Tests/Test_DerivedStats.fos` — сценарии, включая серию запросов клиента без графического интерфейса, которая читает оба вида персонажа; `Managed/Recipes/DerivedStats.cs` — форму Managed C#. Команды приведены в разделе [Маршрут проверки](#маршрут-проверки).

## Пошагово

### 1. Объявите входные данные, производные значения и константы

Хранимые входные данные — `Mutable Persistent` с флагом синхронизации. Производные значения — `Virtual`: у них нет хранилища, флага синхронизации и записи. Исключение — `MaxLifeShown`, которое объясняет шаг 5:

```angelscript
///@ Property Critter Common int32 Strength Mutable Persistent PublicSync
///@ Property Critter Common int32 Endurance Mutable Persistent OwnerSync
///@ Property Critter Common int32 CurLife Mutable Persistent PublicSync
///@ Property Critter Common bool Collapsed Mutable Persistent PublicSync
///@ Property Critter Common int32 CarryWeight Virtual
///@ Property Critter Common int32 MaxLife Virtual
///@ Property Critter Common int32 MaxLifeShown Mutable Persistent PublicSync

const int32 BaseCarryWeight = 20;
const int32 CarryPerStrength = 10;
const int32 BaseLife = 20;
const int32 LifePerEndurance = 5;
```

### 2. Зарегистрируйте геттеры на каждой стороне, которая читает значение

Геттер — функция сущности, регистрируемая один раз на свойство и сторону. Она выполняется при каждом чтении и возвращает значение на этот момент. Делайте её чистой функцией свойств, которые есть у читающей стороны:

```angelscript
#if SERVER || CLIENT

// Getters are registered on every side that reads the property: a side without one cannot read it
[[ModuleInit]]
void InitDerivedStats()
{
    Game.SetPropertyGetter(CritterProperty::CarryWeight, GetCarryWeight);
    Game.SetPropertyGetter(CritterProperty::MaxLife, GetMaxLife);
}

// Pure functions of replicated inputs: no random, no clock, no other entity
[[PropertyGetter]]
int32 GetCarryWeight(Critter cr)
{
    return BaseCarryWeight + cr.Strength * CarryPerStrength;
}

[[PropertyGetter]]
int32 GetMaxLife(Critter cr)
{
    return BaseLife + cr.Endurance * LifePerEndurance;
}

#endif
```

### 3. Удерживайте хранимое значение в пределах преобразующим сеттером

Сеттер, принимающий значение по ссылке, выполняется до сохранения и может переписать его; чтение сущности внутри него по-прежнему возвращает предыдущее значение. Здесь сеттеры работают только на сервере, потому что каждую запись выполняет сервер:

```angelscript
#if SERVER

[[ModuleInit]]
void InitDerivedStatsServer()
{
    Game.AddPropertySetter(CritterProperty::CurLife, ClampCurLife);
    Game.AddPropertySetter(CritterProperty::CurLife, OnCurLifeChanged);
    Game.AddPropertySetter(CritterProperty::Endurance, OnEnduranceChanged);
}

// The transforming setter runs before storage and rewrites the proposed value; the entity still holds the old one
[[PropertySetter]]
void ClampCurLife(Critter cr, int32& value)
{
    int32 maxLife = cr.MaxLife;
    if (value > maxLife) {
        value = maxLife;
    }

    if (value < 0) {
        value = 0;
    }
}
```

### 4. Реагируйте после сохранения и следите за каскадами

Сеттер, принимающий только сущность, выполняется после сохранения и читает зафиксированное значение. Оба обработчика ниже присваивают другое свойство; это присваивание проходит через собственные сеттеры того свойства, поэтому уменьшение выносливости понижает `CurLife` через ограничение выше:

```angelscript
// The reaction setter runs after storage and sees the committed value; a request clamped back to the stored value still runs it
[[PropertySetter]]
void OnCurLifeChanged(Critter cr)
{
    bool collapsed = cr.CurLife == 0;
    if (cr.Collapsed != collapsed) {
        cr.Collapsed = collapsed;
        Game.Log("derived_stats_collapsed_changed critter=" + cr.Id.value + " collapsed=" + (collapsed ? "true" : "false"));
    }
}

// Endurance is owner-only, so observers cannot compute MaxLife; a public mirror carries the result to them
[[PropertySetter]]
void OnEnduranceChanged(Critter cr)
{
    cr.MaxLifeShown = cr.MaxLife;

    // A smaller cap also lowers the stored life; the assignment goes through the clamping setter
    if (cr.CurLife > cr.MaxLife) {
        cr.CurLife = cr.MaxLife;
    }
}
```

### 5. Дайте наблюдателям то, чего они не могут вычислить

Флаги синхронизации определяют, какая сторона может вычислить геттер. `Strength` — `PublicSync`, поэтому наблюдатель сам вычисляет `CarryWeight`. `Endurance` — `OwnerSync`, поэтому наблюдатель не получает выносливость, и его геттер `MaxLife` видит `0`; потому сервер в сеттере выносливости записывает публичное `MaxLifeShown`, а это свойство несёт значение сервера всем.

## Полномочия и синхронизация

- Сервер записывает хранимые значения и регистрирует сеттеры. Ни у одного из них нет флага `ModifiableByClient`, поэтому клиент не может их присваивать.
- Геттер должен быть зарегистрирован на каждой стороне, которая читает свойство. Песочница регистрирует оба под `#if SERVER || CLIENT`; у контрольного свойства `TestServerOnlyBonus` геттер есть только на сервере, и его чтение на клиенте выбрасывает исключение в AngelScript (`Getter not set`, `Entity_GetPropertyValue` в `Source/Scripting/AngelScript/AngelScriptEntity.cpp`). Нативное типизированное чтение свойства `Virtual` без геттера вместо этого возвращает значение по умолчанию (`Properties::GetValue`); результат в Managed C# не запускался.
- Результат репликации по серии запросов клиента: владелец с силой `2` и выносливостью `3` вычисляет `CarryWeight` и `MaxLife` как `40` и `35`, то есть так же, как сервер. Наблюдатель за другим персонажем с силой `5` и выносливостью `6` вычисляет грузоподъёмность `70`, читает выносливость `0`, вычисляет базовый максимум здоровья `20` и читает `50` из зеркала.
- Геттеры и сеттеры выполняются с покрытой сущностью. Сеттеру, который затрагивает другую сущность, нужно, чтобы и она была под cover ([захват нескольких сущностей для одного действия](cover-transactions.md)).

## Сохранение и миграция

Входные данные, `CurLife`, `Collapsed` и зеркало сохраняются; `CarryWeight` и `MaxLife` не хранятся и пересчитываются из восстановленных входных данных. Восстановление сущности не запускает ни одного сеттера свойств: после настоящего перезапуска счётчик post-сеттера в песочнице равен `0`, а `CarryWeight` равен `50` и `MaxLife` равен `40`. Поэтому значение, которое поддерживает сеттер, нужно сохранять, как `MaxLifeShown`, или перестраивать при загрузке в `Game.OnCritterInit` при `firstTime == false` ([Собственное сохраняемое состояние персонажа](persistent-state.md#сохранение-и-миграция)).

Изменение формулы меняет каждое производное значение при следующем чтении, но не хранимое значение, которое было ограничено по старой формуле: сохранённое `CurLife` может превышать новый, меньший `MaxLife` до следующего присваивания. Пересчитывайте при загрузке или мигрируйте. Переименованным входным данным нужно правило `Property Rename` ([миграции данных](../project/data-migrations.md#объявляйте-правило-для-каждого-переименования-удаления-и-переноса)).

## Ловушки и антипаттерны

- Считать, что сеттер-реакция выполняется только при изменении хранимого значения. Движок сравнивает запрос с хранимым значением до запуска любого преобразующего сеттера: присваивание уже хранимого значения не запускает ничего, но другой запрос, который ограничение превращает обратно в хранимое значение, всё равно запускает сеттеры после сохранения (`Properties::SetValue` в `Source/Common/Properties.h`). Песочница присваивает `900` персонажу, стоящему на пределе `40`: значение остаётся `40`, а реакция выполняется ещё раз. Пишите реакции, которые сравнивают с состоянием, как `OnCurLifeChanged` с `Collapsed`.
- Читать новое значение в преобразующем сеттере. Он видит предыдущее хранимое значение и предложенное в своём параметре; песочница записывает там `10` перед присваиванием `25`, а сеттер после сохранения записывает `25`.
- Регистрировать геттер только на одной стороне. В AngelScript другая сторона выбрасывает исключение при чтении, поэтому регистрируйте под `#if SERVER || CLIENT` и не используйте в функции данные только сервера.
- Вычислять по данным, которых у читающей стороны нет. `MaxLife` наблюдателя был `20`, а не `50`, потому что вход `OwnerSync` читается там как `0`. Реплицируйте результат через публичное зеркало или вычисляйте его только на стороне владельца.
- Допускать цикл сеттеров. Сеттер, присваивающий другое свойство, запускает сеттеры того свойства, поэтому два сеттера, записывающие свойства друг друга, никогда не успокоятся.
- Класть в геттер случайность, часы или другую сущность. Он выполняется при каждом чтении на обеих сторонах, и результаты обязаны совпадать.
- Ожидать, что сеттер перестроит состояние при загрузке. Восстановление не запускает ни одного, как показывает тест перезапуска.

## Маршрут проверки

Сценарий `derived-stats` запускает `-Testing.Filter derived_stats.` на `RecipeTests`. Его самая острая проверка доказывает правило равного запроса выше:

```angelscript
    cr.CurLife = 12;
    int32 calls = cr.TestPostCount;
    cr.CurLife = 12;
    Testing::ExpectEq(ctx, cr.TestPostCount, calls, "Assigning the stored value again runs no setter");

    // A different request that the clamp turns into the stored value is not equal on entry, so reactions still run
    cr.CurLife = 40;
    calls = cr.TestPostCount;
    bool collapsed = cr.Collapsed;
    cr.CurLife = 900;
    Testing::ExpectEq(ctx, cr.CurLife, 40, "The clamp kept the stored value");
    Testing::ExpectEq(ctx, cr.TestPostCount, calls + 1, "The reaction setter ran although the stored value did not change");
    Testing::Expect(ctx, cr.Collapsed == collapsed, "The idempotent reaction changed nothing");
```

Сценарий `derived-stats-client` запускает сервер с `RecipeNetworkTests` и `-Testing.Filter derived_stats_client.`, ждёт `derived_stats_waiting_for_client` и запускает клиент без графического интерфейса с `RecipeClient`, как в [маршруте проверки проверяемого запроса действия](validated-action.md#маршрут-проверки). Клиент читает своего персонажа и наблюдаемого и сообщает числа; чтение контрольного свойства только для сервера служит проверкой исключения:

```angelscript
    bool bonusThrew = false;

    try {
        int32 bonus = own.TestServerOnlyBonus;
        Game.Log("derived_stats_client_unexpected_bonus value=" + bonus);
    }
    catch {
        bonusThrew = true;
    }

    CurPlayer.ServerCall.StatsDrillReport(testId, own.CarryWeight, own.MaxLife, npc.CarryWeight, npc.MaxLife, npc.MaxLifeShown, npc.Endurance, bonusThrew);
```

Пара перезапуска использует `RecipeRestart` с каталогом JSON, как в [маршруте проверки сохраняемого состояния](persistent-state.md#маршрут-проверки). Фаза проверки читает восстановленного персонажа:

```angelscript
    Critter cr = critters[0];
    Testing::ExpectEq(ctx, cr.Strength, 3, "The stored strength survived");
    Testing::ExpectEq(ctx, cr.CurLife, 33, "The stored life survived");
    Testing::ExpectEq(ctx, cr.MaxLifeShown, 40, "The persisted mirror survived");
    Testing::ExpectEq(ctx, cr.CarryWeight, 50, "The virtual carry weight is recomputed from stored values");
    Testing::ExpectEq(ctx, cr.MaxLife, 40, "The virtual maximum life is recomputed from stored values");
    Testing::ExpectEq(ctx, cr.TestPostCount, 0, "Restoring a critter runs no property setter");
    Testing::Pass(ctx);
```

Запустите все сценарии песочницы из `Examples/MechanicsSandbox` командой `python validate.py` на Windows (PowerShell) или `python3 validate.py` на Linux (bash); запуск успешен, когда `BuildTools/gameplay_test_runner.py` сообщает `status=passed` для каждого сценария и завершается с кодом `0`. Сценарий клиента требует такой серверный маркер:

```text
derived_stats_report own=40/35 npc=70/20/50 npc_endurance=0
```

## Managed C#

Статус `shape`: тест движка разбирает `Managed/Recipes/DerivedStats.cs` и сравнивает его строки `///@` с модулем AngelScript, они совпадают; ни одна линия движка его не компилирует и не запускает. Сгенерированный `Game.AddPropertyDeferredSetter` явно выбирает путь после сохранения, `ref int` отмечает преобразующий сеттер, а обработчики объявлены `internal` с `[RequiresCover]` ([форма авторского кода Managed C#](../scripting/managed-csharp.md#форма-авторского-кода)):

```csharp
    [ModuleInit]
    internal static void InitDerivedStatsServer()
    {
        Game.AddPropertySetter(CritterProperty.CurLife, ClampCurLife);
        Game.AddPropertyDeferredSetter(CritterProperty.CurLife, OnCurLifeChanged);
        Game.AddPropertyDeferredSetter(CritterProperty.Endurance, OnEnduranceChanged);
    }

    // The transforming setter runs before storage and rewrites the proposed value; the entity still holds the old one
    [PropertySetter]
    internal static void ClampCurLife([RequiresCover] Critter cr, ref int value)
    {
        int maxLife = cr.MaxLife;
        if (value > maxLife) {
            value = maxLife;
        }

        if (value < 0) {
            value = 0;
        }
    }
```

Проект на managed backend должен выполнить ту же серию запросов клиента, прежде чем полагаться на эту форму.

## Как это расширяет крупный проект

- **Формулы как данные.** Четыре константы становятся значениями `///@ Setting` или строкой [каталога FixedType](../../explanation/entity-and-property-model/choosing-a-representation.md#строка-каталога-fixedtype) на каждый архетип, читаемой внутри геттера. Настройка — конфигурация только для чтения, поэтому геттер остаётся чистой функцией от неё.
- **Постоянные пределы без сеттера.** Объявление свойства принимает теги `Min = <number>` и `Max = <number>`, и движок ограничивает присваиваемое значение ими (`ValidateAndClampRawData` в `Source/Common/Properties.h`); в песочнице это не запускалось. Граница, зависящая от другого свойства, как `MaxLife`, по-прежнему требует преобразующего сеттера.
- **Много входных данных.** Экипировка, эффекты и бонусы статусов становятся дополнительными входными данными того же геттера, у каждого своё решение о синхронизации; структурированное значение в одном [свойстве RefType](../../explanation/entity-and-property-model/choosing-a-representation.md#reftype) читается, изменяется и присваивается обратно целиком.
- **События проекта.** Вызывайте событие проекта из сеттера-реакции, когда другим модулям нужно изменение, чтобы у него был один производитель.
- **Что остаётся в игровом проекте.** Набор характеристик, формулы, границы и то, что означает обессиленный персонаж.

## См. также

- [Создание игровых механик](index.md)
- [Собственное сохраняемое состояние персонажа](persistent-state.md)
- [Экипировка в слот по одному общему правилу](equip-slot.md)
- [Жизненный цикл и конкурентность скриптов](../scripting/lifecycle-and-concurrency.md#преобразование-свойства-и-реакция-после-записи)
- [Выбор представления игровых данных](../../explanation/entity-and-property-model/choosing-a-representation.md)
- [Скрипты Managed C#](../scripting/managed-csharp.md)
- [Миграция сохранённых данных и контента в игровом проекте](../project/data-migrations.md)
