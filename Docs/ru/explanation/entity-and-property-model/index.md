---
layout: default
title: Модель сущностей
locale: ru
document_id: entity-model
permalink: /Docs/ru/explanation/entity-and-property-model/
---

<!-- docs-translation: {"document_id":"entity-model","locale":"ru","source_path":"Docs/en/explanation/entity-and-property-model/index.md","source_sha256":"fbe82911b8dd868355ecfc1e50ffba33ff70b72ad4e17dcc55e1f2aefbe078d7"} -->

# Модель сущностей

Этот документ описывает переиспользуемую runtime-модель сущностей: дескрипторы типов сущностей, сгенерированные средства доступа к свойствам, сущности-прототипы, владение внутренними сущностями, события сущностей и модель хранения свойств, на которой строятся другие runtime-системы.

Используйте его при изменении `Source/Common/Entity.*`, `EntityProperties.*`, `EntityProtos.*`, `Properties.*`, `PropertiesSerializer.*`, `ProtoManager.*`, аннотаций метаданных или кода, который сохраняет либо синхронизирует состояние сущностей.

О том, как операции создания, уничтожения и регистрации сущностей сохраняют согласованность при исключении в середине операции, включая terminate-on-OOM, lifecycle-контракт throw-as-signal и политику `FO_STRONG_ASSERT` после изменения состояния, см. [ExceptionSafety.md](../../../ExceptionSafety.md).

## Модель владения

Движок владеет runtime-моделью сущностей и механикой метаданных и свойств. Встраиваемый игровой проект владеет конкретными файлами прототипов, идентификаторами контента, скриптами и игровыми правилами, которые используют эту механику.

Этот документ должен описывать только переиспользуемое поведение движка. Определения предметов, персонажей, карт и локаций конкретного проекта, а также заметки о балансе размещайте в документации встраиваемого проекта.

## Runtime-типы сущностей

`Source/Common/Entity.h` объявляет основную таксономию сущностей через аннотации `///@ ExportEntity`:

- `Game` - глобальная сущность состояния движка и игры.
- `Player` - контракт сущности и представления игрока.
- `Location` - сущность мировой локации с прототипами и временными событиями.
- `Map` - контракт сущности и представления карты с прототипами и временными событиями.
- `Critter` - сущность персонажа, NPC или тела игрока с прототипами и временными событиями.
- `Item` - контракт сущности и представления предмета с прототипами, статическими и абстрактными вариантами и временными событиями.

`EntityTypeDesc` хранит обнаруженные или сгенерированные метаданные каждого типа сущности:

- является ли тип экспортируемым или глобальным;
- поддерживает ли он прототипы, статические и абстрактные сущности либо holder entries;
- используемый типом `PropertyRegistrar`;
- экспортируемые методы и события;
- политику синхронизации и сохранения holder entry.

Поток генерации метаданных и регистрации описан в [GeneratedApiAndMetadata.md](../../reference/metadata/index.md).

## Базовый класс сущности

`Entity` - общий базовый класс всех runtime-сущностей и сущностей-прототипов. Он владеет:

- экземпляром `Properties`;
- необязательными списками callback событий;
- необязательными данными временных событий;
- необязательными holder entries внутренних сущностей;
- флагами состояний destroying и destroyed;
- intrusive-подобным подсчётом ссылок через `AddRef()` / `Release()`.

Важные средства доступа и пути изменения включают:

- идентичность и тип: `GetName()`, `GetId()`, `IsGlobal()`, `GetTypeName()`, `GetTypeNamePlural()`;
- доступ к свойствам: `GetProperties()`, `GetPropertiesForEdit()`, `GetValueAsInt()`, `GetValueAsAny()`, `SetValueAsInt()`, `SetValueAsAny()`;
- raw snapshots данных: `StoreData()`, `RestoreData()`, `SetValueFromData()`;
- состояние жизненного цикла: `IsDestroying()`, `IsDestroyed()`, `MarkAsDestroying()`, `MarkAsDestroyed()`;
- граф владения: `AddInnerEntity()`, `RemoveInnerEntity()`, `ClearInnerEntities()`;
- отправку событий: `SubscribeEvent()`, `UnsubscribeEvent()`, `FireEvent()`.

Не обходите `Properties` при изменении состояния сущности. Callback свойств, overlay-данные, флаги синхронизации и сохранения и видимые скриптам средства доступа зависят от того, что изменение проходит через слой свойств.

## Сгенерированные обёртки свойств

`FO_ENTITY_PROPERTY(type, Name)` разворачивается в небольшой типизированный интерфейс:

- `GetPropertyName()` возвращает зарегистрированный `Property*` по сгенерированному индексу регистрации;
- `GetName()` читает типизированное значение из `Properties`;
- `SetName()` записывает значение через `Properties::SetValue()`;
- `IsNonEmptyName()` проверяет наличие raw-данных свойства.

Эти generated accessors являются `noexcept`; внутренняя validation доступа служит fatal tripwire, а не recoverable synchronization boundary. Доступ к owning entity должен быть уже доказан до чтения или записи свойства. Если проверка сработала, исправляйте caller, пропустивший синхронизацию, а не ослабляйте accessor.

Доказательство доступа к одной сущности не доказывает автоматически доступ к другой сущности, возвращённой из неё. В частности, `ServerEntity::GetParent()` проверяет child, но возвращает parent, который намеренно может быть uncovered, а lock дочерней сущности не покрывает её ancestor. Такой handle можно вернуть, но перед чтением свойств parent его доступ нужно проверить.

Item ownership resolvers являются эталонным шаблоном: пути, продолжающие обход ownership, доказывают каждого critter/container parent перед чтением `GetMapId()`, `GetHex()` или `Ownership`; пути, которые лишь возвращают `Item.GetMap()`, не добавляют validation, отклоняющую обычные map-owned items. Срабатывание accessor tripwire означает, что caller не предоставил нужное доказательство.

`Source/Common/EntityProperties.h` определяет сгенерированные классы-обёртки свойств:

- `GameProperties`
- `PlayerProperties`
- `ItemProperties`
- `CritterProperties`
- `MapProperties`
- `LocationProperties`

Сам `EntityProperties` добавляет общие persistent-поля:

- `CustomHolderId`
- `CustomHolderEntry`
- `ExplicitlyPersistent`

Сгенерированные классы-обёртки являются тонким слоем над `Properties`; фактическое хранение, сведения о типах, флаги синхронизации и сохранения, callback и решения о сериализации принадлежат `Property`, `Properties` и `PropertyRegistrar`.

Серверные AngelScript getters свойств копируют raw-данные невиртуальных свойств через `Properties::CopyRawData()` перед преобразованием в скриптовые значения. `Properties` блокирует только окно копирования или записи raw-буфера; setter и post-setter callback выполняются вне блокировки хранилища, чтобы отправка событий, смена родителя и уничтожение не наследовали блокировку буфера свойств.

Типизированное и скриптовое присваивание свойств отклоняет не конечные floating-point leaves до записи, в том числе значения внутри массивов, структур и ключей или значений словарей. Та же проверка повторяется после того, как setter callback изменили raw-данные, а сериализация документа и текста отклоняет не конечные значения, если доверенное бинарное восстановление или native-код передали повреждённый payload.

Числовые metadata `Min = value` и `Max = value` применяются при каждой записи.
Регистрация принимает signed integer и конечные decimal literals только в
границах scalar base type свойства, запрещает повторные bounds, enum, compound
records, dictionaries и инвертированный диапазон. Для plain numeric array одна
граница применяется к каждому элементу. Assignment, raw restore,
text/document load и payload после setter clamp-ятся до сравнения с сохранённым
значением. Поэтому запись, полностью поглощённая диапазоном, не вызывает setter
или post-setter notification об изменении. Проверка finite выполняется до clamp:
NaN и infinity являются ошибкой, а не значениями для замены endpoint-ом.

Хранилище raw-данных свойств имеет естественное выравнивание: storage blob и буферы `PropertyRawData` начинаются с максимального выравнивания, регистрация layout структуры проверяет выравнивание смещений полей, а overlay/POD offsets следуют выравниванию данных каждого свойства. Поэтому readers свойств используют обычные типизированные загрузки без обходов unaligned access и runtime-проверок выравнивания; нарушение контракта обнаруживают sanitizer-сборки. Равенство raw payload проверяется побайтно (`MemCompare`): общая длина payload не повышает требования к его выравниванию.

## Runtime свойств

`Source/Common/Properties.h` определяет четыре центральных компонента:

- `PropertyRawData` - временный типизированный или raw-буфер для getters, setters и путей raw restore.
- `Property` - метаданные одного свойства: имя и компонент, базовый тип, форма коллекции, флаги синхронизации, mutability, persistence, nullability, callback и индекс регистрации.
- `Properties` - хранилище значений одной сущности, связь base/overlay, raw snapshot/restore, импорт и экспорт текста, типизированные helpers get/set и разрешение hash.
- `PropertyRegistrar` - реестр одного типа сущности, который создаёт свойства из токенов метаданных и отслеживает lookup, группы, компоненты, layout данных и public/protected/private пространства данных.

Флаги свойств определяют поведение системы:

- `Common`, `ServerOnly`, `ClientOnly` задают видимость по сторонам.
- `Synced`, `OwnerSync`, `PublicSync`, `NoSync` задают сетевую репликацию.
- `ModifiableByClient` и `ModifiableByAnyClient` разрешают изменения, пришедшие от клиента.
- `Virtual`, `Mutable`, `Persistent`, `Historical`, `Nullable` и `Temporary` влияют на хранение, callback, persistence и скриптовые контракты.
- `Min` и `Max` задают включительный clamp range числового scalar или plain numeric array.

При изменении метаданных свойств одновременно обновляйте runtime- и script/nullability-документацию, если изменение затрагивает видимые скриптам сигнатуры. См. [Nullability.md](../../../Nullability.md).

## Базовые свойства и overlays

Экземпляр `Properties` может иметь базовые свойства. Это активно используется runtime-сущностями, производными от прототипов:

- base data задают унаследованные или стандартные значения;
- overlay entries хранят значения, отличающиеся от base, либо требующие явных локальных данных;
- `CompareData()` может игнорировать временные свойства при сравнении snapshots;
- `StoreData()` может включать или исключать protected data в зависимости от требований синхронизации и сохранения;
- `RemoveSyncedOverlayEntries()` и связанные helpers overlay сохраняют компактность реплицируемого состояния.

Таким образом, runtime-сущность не является простым плоским отображением имени свойства в значение. При отладке определите, поступает ли значение из base properties, собственного POD/complex storage или overlay data.

Overlays, производные от прототипов, лениво создают плотный индекс свойств: небольшие overlays используют линейный поиск по отсортированным entries, а `_overlayEntryIndex` строится только после достижения порогового количества overlay entries. Это не позволяет обычному overlay с малым числом entries выделять индекс размером со все зарегистрированные свойства, но сохраняет плотный lookup для крупных overlays.

Смещения overlay data имеют естественное выравнивание. Каждое свойство при регистрации получает внутреннее выравнивание данных (`Property::GetDataAlignment()`): простые значения и POD arrays используют наибольшую степень двойки, на которую делится размер base type, с ограничением `MAX_SERIALIZED_ALIGNMENT`; string arrays выравниваются по своим u32 prefixes, ref-type payloads по `MAX_SERIALIZED_ALIGNMENT`, dicts по наиболее строгому выравниванию ключа, значения и prefix, а одиночные strings остаются byte-aligned. `AllocOverlayData` сначала выполняет best-fit поиск по освобождённым holes и alignment paddings между существующими entries и только при отсутствии подходящего hole расширяет выровненный хвост overlay. `_overlayGarbageSize` точно учитывает байты внутри используемого диапазона, не принадлежащие ни одному entry, благодаря чему allocator пропускает поиск, если ни один hole заведомо не подходит. Рост capacity повторно оценивается после repack, поскольку перемещение entries переменного размера может сдвинуть конечный выровненный хвост за первоначально выбранную границу capacity. Repack и rebuild-from-full раскладывают данные entries в стабильном порядке убывания выравнивания, минимизируя padding: простые entries располагаются подряд, а complex payloads переменного размера могут оставлять небольшие выровненные gaps, учтённые как garbage. Основной POD block registrator выровнен конструктивно: offsets кратны размеру base type, а section bases кратны 8, поэтому `GetRawData()` всегда возвращает данные, выровненные для внутреннего layout независимо от backing storage.

Внутренняя структура complex raw data свойств также выровнена. Контракт layout определён в `Properties.h` и использует `alignment_for_size()`: внутри blob каждый fixed-size item размещается с естественным выравниванием - u32 length/count prefixes по 4, POD keys/values/element runs по наибольшей степени двойки, на которую делится их размер, с ограничением `MAX_SERIALIZED_ALIGNMENT`, вложенные ref-type payloads по `MAX_SERIALIZED_ALIGNMENT`, payload полей ref-blob по собственному data alignment поля. Байты strings не выравниваются, padding bytes равны нулю, после последнего item padding отсутствует, поэтому dict parsers по-прежнему завершаются при точном исчерпании буфера. Все blob codecs повторяют одни и те же шаги `align_up`: `PropertiesSerializer` для values/text, AngelScript marshaling в `AngelScriptHelpers.cpp`, `DynamicRefTypeInstance` в `ScriptSystem.cpp`, codec string array в `Properties.h` и inbound validator в `ClientDataValidation.cpp`, который дополнительно отклоняет ненулевой padding в недоверенных клиентских payloads. Поскольку начало blob выровнено storage-слоем, а внутренние элементы следуют контракту, fixed-size items можно читать и записывать прямым типизированным доступом. Изменение этого layout нарушает совместимость клиента и сервера: обновите compatibility version marker в `Common.h` и повторно запеките ресурсы.

`MAX_SERIALIZED_ALIGNMENT`, определённый в `BasicCore.h` и сейчас равный 8, ограничивает весь контракт. Это единая compile-time константа, а не platform-dependent `alignof(std::max_align_t)`, равный 16 на x64 из-за `long double` и 8 на wasm, чтобы serialized byte layout был одинаков на всех targets независимо от platform `max_align_t` или стандартного выравнивания `new`. Контракт корректен, только пока каждый serialized scalar leaf помещается в `MAX_SERIALIZED_ALIGNMENT`: layout позднее читает fixed-size items прямым типизированным доступом (`reinterpret_as<T>()`), поэтому over-aligned leaf привёл бы к misaligned load, то есть UB, sanitizer trap или hard fault на targets со строгим выравниванием. Это закреплено на этапе компиляции: fundamental integer/float leaves проверяются в `BasicCore.h`, hash leaf hashed string в `Properties.h`, а каждый типизированный accessor свойства (`GetValue`/`GetValueFast`/`SetValue`) при инстанцировании статически проверяет `alignof(T) <= MAX_SERIALIZED_ALIGNMENT`. Добавление over-aligned типа (`SIMD`, `__int128`, `long double`, `alignas(16)`) в любой serialized path приводит к ошибке сборки, а не к незаметному недовыравниванию. Текущая type grammar не может выразить такой leaf: primitives имеют размер не более 8 байт, `hstring` сериализуется как 64-bit hash, а structs состоят из этих элементов, поэтому `alignof(struct)` не превышает 8.

## Прототипы

`Source/Common/EntityProtos.h` определяет сущности-прототипы:

- `ProtoEntity` - базовая сущность с `GetProtoId()` и `CollectionName`.
- `EntityWithProto` - mix-in для runtime-сущностей, хранящих ссылку на `ProtoEntity`.
- `ProtoItem`, `ProtoCritter`, `ProtoMap`, `ProtoLocation` - типизированные сущности-прототипы со сгенерированными обёртками свойств.
- `ProtoCustomEntity` - путь custom prototype entity.

`Source/Common/ProtoManager.*` владеет lookup и загрузкой прототипов:

- `GetProtoItem()`, `GetProtoCritter()`, `GetProtoMap()`, `GetProtoLocation()`;
- общими `GetProtoEntity(type, pid)` и `GetProtoEntities(type)`;
- `AddProto()` для добавления созданных прототипов;
- `LoadFromResources()` для загрузки baked или resource-backed данных прототипов.

Загрузка прототипов граничит с запеканием ресурсов. Синтаксис авторинга, идентичность, наследование, применимость встроенных свойств, ссылки и миграции описаны в [Формате прототипов](../../how-to/content/prototype-format.md) и сгенерированном [каталоге свойств](../../reference/prototype-format/properties.md). Оркестрация baker описана в [Baking Pipeline](../content-pipeline/baking.md).

## Внутренние сущности и holders

Сущности могут хранить другие сущности в именованных entries. Метаданные holder находятся в `EntityTypeDesc::HolderEntryDesc`:

- `TargetType` - тип сущности, который может храниться в entry;
- `Sync` - `NoSync`, `OwnerSync` или `PublicSync`;
- `Persistent` - участвует ли членство в holder в persistence.

Общие persistent-поля `CustomHolderId` и `CustomHolderEntry` позволяют custom entities записывать отношения holder. `EntityManagerApi` предоставляет hooks создания, lookup и уничтожения custom entity:

- `CreateCustomInnerEntity()`
- `CreateCustomEntity()`
- `GetCustomEntity()`
- `DestroyEntity()`

Custom entity публикуется в глобальном registry только после полного связывания с holder. Оба пути публикации - `EntityManager::CreateCustomInnerEntity()` и путь загрузки inner entity - устанавливают parent link, `EntityLock` ближайшего holder либо lock движка для engine-held entries, а также `CustomHolderEntry` / `CustomHolderId` до вызова `RegisterCustomEntity()`. Регистрация проверяет именно эту связь и требует, чтобы текущий поток удерживал holder lock. Регистрация до связывания сделала бы сущность глобально доступной, пока у неё ещё нет lock.

Holder lock предоставляет инициатор публикации. Runtime-вызов `CreateCustomInnerEntity()` выполняется под подготовленным cover вызывающей стороны; для engine-held entry это скриптовый `Game.Lock()`. World loader захватывает каждую новую location, map, critter и item перед спуском в их inner entities. Singleton движка - единственный holder, существующий до загрузки, поэтому `EntityManager::LoadEntities()` самостоятельно захватывает singleton lock движка на время прохода engine-held inner entities и освобождает его перед восстановлением locations.

При изменении holder behavior проверяйте server/client entity managers и persistence paths вместе с `Entity.*`.

## События и временные события

`FO_ENTITY_EVENT(Name, Args...)` создаёт member `EntityEventWrapper`. Callback событий упорядочены по priority и возвращают `Entity::EventResult`:

- `ContinueChain`
- `StopChain`

`EntityEventWrapper::Fire()` по-разному формирует native call data для global и non-global entities: для non-global event сущность добавляется первым аргументом.

`Entity::TimeEventData` хранит запланированные script callback, fire time, repeat duration и script data. Сущности с поддержкой временных событий объявляются флагом метаданных `HasTimeEvents` в аннотациях `ExportEntity`.

`TimeEventManager::CancelAllForEntity()` очищает runtime-состояние временных событий сущности до уведомления внешнего dispatcher. Стандартное исключение из одного cancellation hook регистрируется отдельно и не мешает оставшимся cancellation notifications; сама операция имеет `noexcept`, поэтому teardown не может завершиться с частично отправленными уведомлениями.

`StartTimeEvent()` отклоняет и destroyed, и destroying entities, поэтому finish или cancellation callback не могут повторно запланировать работу после начала уничтожения сущности.

Внешний dispatcher может измерять запланированную задержку по часам, которые идут не так, как engine frame clock, например когда `DeltaTimeCap` ограничивает время кадра при работе под debugger или без сети. `TimeEventManager::FireAndAdvance()` повторно сверяет сохранённый engine-clock `FireTime` перед вызовом script; при раннем пробуждении dispatcher получает оставшуюся задержку, а callback остаётся запланированным.

## Связи сериализации

Состояние сущности сериализуется через данные свойств, а не ручным копированием полей сущности:

- raw binary snapshots свойств: `Entity::StoreData()` / `RestoreData()` и `Properties::StoreData()` / `RestoreData()`;
- полные данные свойств: `Properties::StoreAllData()` / `RestoreAllData()`;
- преобразование текста и документов: `Properties::SaveToText()`, `ApplyFromText()` и `PropertiesSerializer.*`.

При преобразовании числовых значений свойств из текста или документа serializer отклоняет значения, которые не помещаются в целевой primitive width, вместо переполнения или создания infinity.

Binary restore paths (`RestoreData`, `RestoreAllData`) также проверяют snapshot по layout registrator до копирования. Индексы свойств за пределами таблицы registrator, слишком крупные blocks и POD sections `(start_pos, len)` за пределами буфера отклоняются исключением, а не записываются. Повреждённый или враждебный snapshot закрывается с ошибкой вместо переполнения storage свойств.

Persistence backends хранят records `AnyData::Document`. Подробности database commit и recovery приведены в [сохранении данных](../persistence/).

## Проверяемые тесты

К соответствующим тестам относятся:

- `Source/Tests/Test_EntityLifecycle.cpp`
- `Source/Tests/Test_EntityProtos.cpp`
- `Source/Tests/Test_LocationAndEntityMgmt.cpp`
- `Source/Tests/Test_ScriptEntityOps.cpp`
- тесты свойств и метаданных, например `Test_Properties.cpp` и `Test_EngineMetadata.cpp`, если они присутствуют в checkout.

## Маршрутизация изменений

- Базовая сущность, события, holders и хранение временных событий: `Source/Common/Entity.*`.
- Сгенерированные классы-обёртки свойств: `Source/Common/EntityProperties.*`.
- Классы сущностей-прототипов: `Source/Common/EntityProtos.*`.
- Lookup и загрузка прототипов: `Source/Common/ProtoManager.*`.
- Хранение и флаги свойств: `Source/Common/Properties.*`.
- Преобразование свойств в документ и текст: `Source/Common/PropertiesSerializer.*`.
- Сгенерированные метаданные и регистрация: [GeneratedApiAndMetadata.md](../../reference/metadata/index.md).
- Persistence: [сохранение данных](../persistence/).
- Сетевая репликация и command buffers: [сеть и авторитетность](../authority-and-networking/).

## Checklist проверки

1. Соберите минимальную цель, компилирующую сгенерированный код сущностей и свойств.
2. Запустите тесты жизненного цикла сущностей и прототипов, относящиеся к изменённому типу.
3. Запустите тесты свойств и метаданных при изменении флагов свойств, регистрации или сериализации.
4. Для синхронизируемого свойства проверьте пути репликации клиента и сервера и обновите [сеть и авторитетность](../authority-and-networking/), если поведение изменилось.
5. Для persistent-свойства проверьте database save/load paths и обновите [сохранение данных](../persistence/), если поведение изменилось.
6. Для видимого скриптам свойства или метода проверьте сгенерированный script API и при необходимости обновите [Nullability.md](../../../Nullability.md).
