---
layout: default
title: Удалённые вызовы
locale: ru
document_id: remote-calls
permalink: /Docs/ru/reference/scripting/remote-calls.html
---

# Удалённые вызовы
<!-- docs-translation: {"document_id":"remote-calls","locale":"ru","source_path":"Docs/en/reference/scripting/remote-calls.md","source_sha256":"0296d8b306436a22e69b8791166421dd04b7ff33a69a3f2fd0d351b31c7659ab"} -->
> Документация движка. Эта страница определяет переиспользуемый контракт удалённых вызовов FOnline. Каждый подключающий проект владеет конкретными вызовами, правилами авторизации, сгенерированным каталогом вызовов и политикой совместимости.

## Назначение

Используйте удалённые вызовы для односторонних сообщений между аутентифицированным клиентским runtime и сервером либо от сервера клиенту подключённого игрока. Удалённый вызов состоит из четырёх частей:

1. одного общего объявления `///@ RemoteCall`;
2. исходящего метода, сгенерированного на отправляющей стороне;
3. обработчика принимающей стороны с AngelScript `[[ServerRemoteCall]]` / `[[ClientRemoteCall]]` либо Managed C# `[ServerRemoteCall]` / `[ClientRemoteCall]`;
4. запечённых метаданных, которые задают обоим runtime одинаковые имя, порядок аргументов, типы, nullable-признаки и подсказку имени исходного файла.

Удалённые вызовы не являются событиями движка, обычными вызовами функций или функциями «запрос-ответ». Они возвращают `void`; ответ оформляется как другой явно объявленный удалённый вызов.

## Проверенные пути исходного кода

- `Source/Tools/MetadataBaker.cpp`
- `Source/Common/EngineBase.cpp`
- `Source/Common/MetadataRegistration.cpp`
- `Source/Common/Settings.inc`
- `Source/Common/NetBuffer.cpp`
- `Source/Server/ClientDataValidation.h`
- `Source/Server/ClientDataValidation.cpp`
- `Source/Server/Server.cpp`
- `Source/Scripting/AngelScript/AngelScriptRemoteCalls.cpp`
- `Source/Scripting/AngelScript/AngelScriptAttributes.cpp`
- `Source/Scripting/Managed/ManagedScriptBackend.cpp`
- `Source/Scripting/Managed/CoreScripts/RemoteCall.cs`
- `Source/Scripting/Managed/CoreScripts/RemoteCallScriptFuncs.cs`
- `Source/Tools/ManagedScriptBaker.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `Source/Tests/Test_AngelScriptAttributes.cpp`
- `Source/Tests/Test_ManagedScriptBaker.cpp`
- `Source/Scripting/Managed/Tests/BootstrapScenarios.cs`
- `Source/Tests/Test_ClientDataValidation.cpp`
- `Source/Tests/Test_NetBuffer.cpp`
- `BuildTools/docs_metadata.py`
- `BuildTools/tests/test_docs_metadata.py`
- `Examples/MinimalProject/Scripts/Starter.fos`
- `Examples/MinimalProject/run_starter_smoke.py`

## Направление и runtime-интерфейс

Цель в объявлении называет принимающую сторону.

| Цель | Отправляющая сторона | Исходящий интерфейс | Принимающий обработчик |
| --- | --- | --- | --- |
| `Server` | клиент | `player.ServerCall.Name(args...)` | `[[ServerRemoteCall]] void Namespace::Name(Player player, args...)` |
| `Client` | сервер | `player.ClientCall.Name(args...)` или `cr.PlayerClientCall.Name(args...)` | `[[ClientRemoteCall]] void Namespace::Name(args...)` |

Для цели `Server` принимающий обработчик получает вызывающего `Player` первым аргументом. Этот аргумент предоставляет движок; в объявлении `///@ RemoteCall` его не пишут. Обработчик цели `Client` получает только объявленные аргументы.

`Player.ClientCall` адресует вызов клиенту этого игрока. `Critter.PlayerClientCall` маршрутизирует его через игрока, связанного с криттером. Объект вызывающей стороны участвует в маршрутизации и не сериализуется как объявленный аргумент.

Mapper не поддерживает удалённые вызовы. `[[AdminRemoteCall]]` относится к отдельному пути административных команд: его не объявляют как `///@ RemoteCall Admin` и не включают в проектные каталоги удалённых вызовов.

Transport contract общий, а authoring syntax и текущее покрытие типов различаются:

| Аспект | AngelScript | Managed C# |
| --- | --- | --- |
| Inbound marker | `[[ServerRemoteCall]]` / `[[ClientRemoteCall]]` | `[ServerRemoteCall]` / `[ClientRemoteCall]` |
| Outbound call | generated `player.ServerCall.Name(...)`, `player.ClientCall.Name(...)` или `cr.PlayerClientCall.Name(...)` | те же generated property и typed method names |
| Поиск handler | stem файла объявления выбирает совпадающий namespace | assembly discovery выбирает attributed static method по имени call; file-stem namespace rule отсутствует |
| Возврат handler | `void`; `[[Async]]` использует AngelScript suspension | `void`, `Task` или `Task<T>`; completion/fault наблюдается, значение `Task<T>` игнорируется, потому что wire result отсутствует |
| Текущее покрытие collections | arrays, dictionaries и dictionaries of arrays поддерживаются AngelScript serializer | только scalars и arrays; dictionaries отклоняются до расширения Managed bridge |

Оба backend используют один baked `RemoteCallDesc`, native hostile-input validation, byte format, per-call limits, routing и authority boundary. Parity здесь означает единый network contract с явно описанными различиями возможностей, а не два несовместимых протокола.

## Грамматика объявления

Объявите каждый контракт один раз в исходном скрипте, видимом при запекании метаданных:

```angelscript
///@ RemoteCall Server RequestRename(string name) MaxBytes 256
///@ RemoteCall Client RenameResult(bool accepted, string reason) MaxBytes 512
```

Грамматика:

```text
///@ RemoteCall (Server|Client) Name([Type[?] argument [, ...]]) [MaxBytes N] [MaxCollectionSize N]
```

Правила, которые проверяет `MetadataBaker`:

- цель должна быть ровно `Server` или `Client`;
- после имени должны идти круглые скобки;
- тип каждого аргумента должен разрешаться через метаданные движка;
- каждый аргумент должен иметь имя;
- аргументы разделяются запятыми;
- `?` записывается отдельно от разрешённого типа.
- после закрывающей скобки допускаются только `MaxBytes` и `MaxCollectionSize`: каждый option можно указать не более одного раза, за ним должно следовать неотрицательное целое число; `0` или отсутствие option означает отсутствие структурного per-call предела.

Имена регистрируются в картах входящих и исходящих вызовов и должны быть уникальны внутри каждого направления на одной стороне runtime. Считайте имя вызова, цель, порядок аргументов, типы и nullable-признаки единым сетевым контрактом.

## Контракт файла и пространства имён AngelScript

Baker сохраняет в качестве подсказки подсистемы только имя исходного файла объявления, например `AccountUi.fos`. AngelScript binder удаляет расширение и ищет принимающую функцию в одноимённом пространстве имён:

```angelscript
// AccountUi.fos
namespace AccountUi
{

///@ RemoteCall Server RequestRename(string name)
///@ RemoteCall Client RenameResult(bool accepted, string reason)

#if SERVER

[[ServerRemoteCall]]
void RequestRename(Player player, string name)
{
    // Authorize and validate before changing server state.
}

#endif

#if CLIENT

[[ClientRemoteCall]]
void RenameResult(bool accepted, string reason)
{
    // Update client presentation only.
}

#endif

}
```

Для этого файла требуется пространство имён `AccountUi`. Переименование или перемещение скрипта без согласованного изменения основы имени файла, пространства имён, объявлений и обработчиков приводит к ошибке привязки. Если входящий обработчик отсутствует, runtime сообщает ожидаемую декларацию.

Обработчики должны использовать атрибут своей принимающей стороны. Валидатор отклоняет обработчик с атрибутом противоположной стороны, атрибутированную функцию без соответствующего входящего объявления и обычный прямой вызов обработчика remote call. Отправляйте вызов через сгенерированный интерфейс:

```angelscript
// Client to server.
player.ServerCall.RequestRename("Ranger");

// Server to one player's client.
player.ClientCall.RenameResult(true, "");
```

## Контракт binding Managed C#

Разместите тот же declaration tag в `.cs` source, входящем в Managed resource pack. Managed baker создаёт typed method `RemoteCaller` для каждой outbound metadata record и добавляет соответствующее property `ServerCall`, `ClientCall` или `PlayerClientCall` generated entity types. Inbound methods обнаруживаются по всей project assembly; они должны быть static, иметь attribute принимающей стороны, точное имя и arguments call, а на server первым принимать предоставленный движком `Player`:

```csharp
namespace Game.Scripts;

using System.Threading.Tasks;
using FOnline;

///@ RemoteCall Server RequestRename(string name) MaxBytes 256
///@ RemoteCall Client RenameResult(bool accepted, string reason) MaxBytes 512

public static class AccountUi
{
    [ServerRemoteCall]
    public static async Task RequestRename(Player player, string name)
    {
        // Authorize, validate, and reacquire cover after every await.
        await Game.YieldAsync(1);
    }

    [ClientRemoteCall]
    public static void RenameResult(bool accepted, string reason)
    {
        // Update client presentation only.
    }
}
```

Отправляйте через generated typed surface:

```csharp
player.ServerCall.RequestRename("Ranger");
player.ClientCall.RenameResult(true, "");
```

`FOnline.RemoteCall.Send(...)` остаётся низкоуровневым named entry point для generated surface и диагностики; gameplay code следует предпочитать generated method, чтобы C# compiler проверял список arguments. `RemoteCall.Loopback(...)` предназначен только для diagnostic/tests и не доказывает real peer, authentication или transport path.

## Аргументы и сериализация

Общий wire format поддерживает разрешённые metadata primitives, integer-backed enums, `string`, `hstring`, зарегистрированные reference types, fixed-layout structs, arrays, dictionaries и dictionaries of arrays. AngelScript bridge покрывает весь этот список. Текущий Managed bridge принимает scalar values и arrays, а из ref types — только dynamic managed ref types; dictionaries, dictionary-of-array shapes и native-only ref wrappers отклоняются. Поэтому известность типа metadata ещё не является полноценным backend/transport test: скомпилируйте и запеките обе стороны, затем выполните call через каждый поставляемый scripting backend и целевой network path.

Правила `T?` описаны в [Nullability.md](../../../Nullability.md). Объявление является авторитетным: принимающий обработчик должен точно повторять тип и nullable-признак каждого аргумента. Не используйте nullable-синтаксис вместо необязательного поля с предметным смыслом; определите этот смысл в протоколе явно.

Делайте payload ограниченным и предназначенным для одной задачи. В частности:

- предпочитайте компактные идентификаторы и заново разрешайте серверные сущности на принимающей стороне;
- задавайте проектные пределы длины строк и количества элементов коллекций;
- не передавайте большие снимки состояния, когда достаточно идентификатора и ревизии;
- добавляйте явный идентификатор запроса или корреляции, если одновременно могут ожидаться несколько ответов;
- вводите новый вызов или версионированный payload, если старые и новые клиенты не могут одинаково интерпретировать сигнатуру.

Для каждого недоверенного вызова клиент-сервер задавайте `MaxBytes` как максимальный корректный размер сериализованного тела, а `MaxCollectionSize` — как максимальный корректный размер любой объявленной коллекции. Предел коллекции отдельно применяется к массивам, словарям, внешнему словарю и вложенным массивам словаря массивов. Если предел не объявлен, в метаданных остаётся `0`: это явное неограниченное структурное значение, а не рекомендуемый production default.

## Авторитет и обработка ошибок

Удалённый вызов с клиента на сервер является запросом, а не доказательством разрешённости действия. Серверный обработчик должен получить личность вызывающего из предоставленного движком `Player`, а затем до изменения состояния проверить аутентификацию, владение, валидность сущностей, текущее игровое состояние, стоимость ресурсов, ограничения частоты и все проектные правила авторизации.

Не принимайте идентификатор игрока или владельца из payload как замену вызывающему `Player`. Разрешайте идентификаторы payload по обычному контракту синхронизации и доступа к сущностям, а после любого yield повторно проверяйте состояние перед использованием захваченных сущностей. Клиентские обработчики должны отвечать за представление; авторитетное состояние остаётся на сервере.

На сервере `ServerEngine::Process_RemoteCall()` отклоняет отрицательный размер payload или размер, превышающий остаток текущего frame, и разрешает объявленный вызов до allocation или копирования его тела. Поэтому неизвестный вызов не может принудить сервер выделить память под тело. Эффективный предел байтов — меньшее ненулевое значение `MaxBytes` этого вызова и общей настройки `ServerNetwork.MaxRemoteCallPayloadSize` (по умолчанию 1 МиБ). Per-call значение определяет допустимую форму протокола, а global value остаётся защитным пределом для враждебного input.

Затем сервер выполняет `ValidateInboundRemoteCallData()` до получения cover вызывающего `Player` и запуска handler. Validator проходит metadata shape, проверяет enum/hash/reference data, отклоняет отрицательное или превышающее предел число элементов collections, доказывает наличие минимально требуемых bytes до обхода collection и требует полного потребления payload. AngelScript decoder независимо применяет тот же предел collection до `Reserve()` или создания container, включая nested dictionary arrays. Managed decoder использует общий reader `RemoteCallWire`, повторяет проверки payload и array ceiling до создания managed `List<T>`, требует полного потребления и вызывает attributed handler внутри backend context. Эти transport checks не заменяют domain validation. Записывайте в log достаточно call/caller context для диагностики, не журналируя secrets или полный untrusted payload.

## Запечённые метаданные

`MetadataBaker` создаёт по одному бинарному файлу метаданных для каждой цели. Запись remote call содержит:

```text
name, source-file hint, In|Out, type, nullable marker, argument name, ..., Limits, max-bytes, max-collection-size
```

Для цели `Server` серверные метаданные записывают `In`, а клиентские — `Out`. Для цели `Client` направления меняются местами. Каждая запись заканчивается обязательным трёхтокенным trailer `Limits`, включая `Limits 0 0`, если в declaration нет обоих options; dynamic registration отклоняет прежнюю форму без trailer. Допустимые записи превращаются в `RemoteCallDesc`. AngelScript регистрирует caller methods и связывает file/namespace handlers; Managed baker генерирует typed caller methods, а Managed runtime связывает attributed assembly methods по имени.

Запечённый формат сохраняет имя исходного файла, но не путь относительно репозитория и не строку объявления. Поэтому документация, созданная из `.fometa`, должна показывать это поле как подсказку исходного файла, а не выдумывать полное происхождение.

## Создание проектного каталога

Генератор движка читает файлы, которыми владеет parser. Он не разбирает `.fos` или `.cs` второй грамматикой:

```bash
python Engine/BuildTools/docs_metadata.py \
  --root . \
  --metadata Baking/Metadata/Metadata.fometa-server \
  --metadata Baking/Metadata/Metadata.fometa-client \
  --write
```

По умолчанию он создаёт принадлежащие проекту артефакты:

- `Docs/generated/project-remote-calls.json` для инструментов и AI-поиска;
- `Docs/generated/project-remote-calls.md` для GitHub Pages и чтения человеком.

Каждый вызов получает стабильный ID вида `script.remote-call.<target>.<name>` и статус `project-owned`. Этот статус определяет границу владения, а не обещает, что игра сохраняет обратную совместимость вызова.

Генератор строго декодирует бинарный контейнер, проверяет UTF-8 и обязательный limits trailer, отклоняет дублирующиеся входы и вызовы и требует совпадения сигнатур и структурных пределов на серверной и клиентской сторонах. JSON model и Markdown table показывают выбранный scripting backend, корректный для него синтаксис handler attribute, `MaxBytes` и `MaxCollectionSize` рядом с каждым вызовом. Managed handler показан неквалифицированной lookup signature, потому что baked metadata не сохраняет его declaring C# type и не сообщает, возвращает ли реализация `void` или `Task`; source hint остаётся указателем для проверки объявления. Генератор также записывает SHA-256 входных файлов. После запекания используйте те же входные пути с `--check` в CI проекта:

```bash
python Engine/BuildTools/docs_metadata.py \
  --root . \
  --metadata Baking/Metadata/Metadata.fometa-server \
  --metadata Baking/Metadata/Metadata.fometa-client \
  --check
```

Флаг `--allow-unpaired` предназначен для диагностики, когда доступна только одна сторона. Не используйте его для публикуемого production-каталога: одиночная запись не доказывает согласованность отправителя и получателя.

## Совместимость и выпуск

Проектные remote calls являются запечёнными проектными метаданными и намеренно не входят в [сгенерированную модель API движка](../../../generated/api.json). Игра должна хранить сгенерированный каталог в репозитории, сравнивать его при review и развёртывать согласованные клиентскую и серверную ревизии проекта.

Переименование вызова, изменение его цели, перестановка аргументов, изменение типа или nullable-признака аргумента либо изменение любого структурного предела является изменением сетевого контракта. Зафиксируйте его в release notes и политике совместимости подключающего проекта. Изменения транспорта, формы метаданных или регистрации remote calls на уровне движка также должны соблюдать правило версии совместимости из [AGENTS.md](../../../../AGENTS.md#validation-routing).

## Диагностика

- `Remote call function not found`: в AngelScript проверьте stem файла и namespace, side guards, имя handler и точную signature; в Managed C# проверьте, что source попал в target assembly и найден matching attributed static method.
- Ошибка проверки атрибута: используйте только marker принимающей стороны — `[[ServerRemoteCall]]` / `[[ClientRemoteCall]]` либо `[ServerRemoteCall]` / `[ClientRemoteCall]`.
- Managed rejection collection/ref type: сократите declaration до scalars, arrays и dynamic managed ref types либо оставьте этот call на AngelScript до расширения Managed bridge.
- Дублирующаяся регистрация: имена вызовов пересекаются внутри одного входящего или исходящего направления; переименуйте один контракт.
- Одиночная запись документации: серверные и клиентские метаданные получены из разных или неполных bake либо один файл устарел.
- Отсутствующий или несовпадающий limits record: заново запеките обе цели на одной revision Engine/project; каждая запись должна заканчиваться `Limits <max-bytes> <max-collection-size>`, и обе стороны должны совпадать.
- Нет ссылки на исходный код в сгенерированном результате: `.fometa` хранит только подсказку имени файла; исследуйте инвентарь исходного кода проекта вместо угадывания пути.
- Ошибка сериализации runtime: сократите контракт до заведомо поддерживаемых типов и добавьте узкий сквозной транспортный тест.

## Контрольный список проверки

1. Держите каждое объявление общим для входных метаданных обеих сторон.
2. Реализуйте точную входящую сигнатуру на правильной стороне и с правильным атрибутом.
3. Выполните обычное запекание ресурсов проекта; для каждого включённого backend требуются AngelScript compile без warnings и/или чистые Managed C# compile плюс analyzer pass.
4. Создайте и проверьте парный JSON/Markdown-каталог из этого bake.
5. Выполняйте `BuildTools/docs_metadata.py --check` с теми же входами в CI.
6. Проверьте авторизацию client-to-server и представление server-to-client через реальную сеть или интеграционный тест проекта.
7. Проверьте отклонение прав, устаревших идентификаторов, некорректных предметных значений, global/per-call пределов payload, а также пределов внешних и вложенных коллекций.
8. Для каждого несовместимого изменения вызова задайте явное решение по выпуску и совместимости.

Принадлежащий движку AngelScript [минимальный проект](../../../../Examples/MinimalProject/README.ru.md) доказывает разбор declarations, обе inbound binding, paired baked metadata, stable catalog IDs и server lifecycle startup. `Test_ManagedScriptBaker` вместе с Managed CoreScripts tests доказывают generated caller shape и managed handler registration/loopback. Ни один из этих уровней не заменяет реальные multiplayer transport/authorization tests игры.

## См. также

- [Скриптовый runtime](../../explanation/scripting-runtime/) — устройство скриптовой подсистемы и владение callback.
- [Nullability.md](../../../Nullability.md) — nullable-контракт скриптов и совпадение сигнатур remote call.
- [GeneratedApiAndMetadata.md](../metadata/index.md) — граница владения codegen и запечённых метаданных.
- [Сеть и авторитетность](../../explanation/authority-and-networking/) — модель транспорта и соединений.
- [Baking Pipeline](../../explanation/content-pipeline/baking.md) — пакеты ресурсов и выполнение baker.
