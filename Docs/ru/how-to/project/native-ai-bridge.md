---
layout: default
title: Встраивание AiControl listener в игровой клиент
locale: ru
document_id: project-native-ai-bridge
permalink: /Docs/ru/how-to/project/native-ai-bridge.html
---

# Встраивание AiControl listener в игровой клиент
<!-- docs-translation: {"document_id":"project-native-ai-bridge","locale":"ru","source_path":"Docs/en/how-to/project/native-ai-bridge.md","source_sha256":"PLACEHOLDER"} -->
Engine поставляет необязательный native listener для [протокола AiControl](../ai-control-protocol.md): TCP endpoint на loopback внутри клиента разработки, через который QA-инструмент, локальный скрипт или MCP-адаптер наблюдает за игрой и ставит команды в очередь. Engine отвечает за транспорт, авторизацию, ограниченные очереди и завершение работы; игровой проект отвечает за свои настройки, схему наблюдения и набор команд. На этой странице описано, как компилировать listener только в клиенты разработки, запускать его из проектных настроек, регистрировать проектные команды и перевести проект с собственной копии listener. Как и протокол, listener экспериментальный.

## Проверенные исходные пути

- `Source/Client/AiControlListener.h`
- `Source/Client/AiControlListener.cpp`
- `Source/Client/Client.h`
- `Source/Client/Client.cpp`
- `Source/Scripting/ClientGlobalScriptMethods.cpp`
- `Source/Scripting/ServerGlobalScriptMethods.cpp`
- `Source/Server/ServerImage.h`
- `Source/Server/ServerImage.cpp`
- `Source/Essentials/NetSockets.h`
- `Source/Essentials/NetSockets.cpp`
- `Source/Tests/Test_AiControlListener.cpp`
- `Source/Tests/Test_ServerImage.cpp`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/validation-project/CMakeLists.txt`
- `BuildTools/AiControlProtocol.json`
- `BuildTools/ai_control_client.py`
- `Examples/AiControlSample/`

## Что предоставляет Engine и что остаётся в проекте

| Задача | Владелец | Где |
| --- | --- | --- |
| TCP listener, построчный framing, `auth`/`ping`/`status`/`observe`/`events`/`act`, коды ошибок | Engine | `AiControlListener` |
| Loopback по умолчанию, правило токена, ограниченные очередь и история событий, предел строки | Engine | `AiControlListener::Start` и поток сокета |
| Время жизни listener и покадровый вызов native-обработчиков команд | Engine | `ClientEngine` |
| Включение в сборку | Сборка проекта | CMake-опция `FO_AI_CONTROL` |
| Флаг включения, host, port, token, размеры очередей | Настройки проекта | значения, передаваемые в `Game.AiControlStart` |
| Поля наблюдения, `schemaVersion`, готовность, сокрытие данных | Скрипты проекта | `Game.AiControlSetObservation` |
| Имена команд, параметры, проверка, игровые вызовы | Native-код или скрипты проекта | обработчики команд или `Game.AiControlPullCommand` |
| Имена MCP-инструментов, рецепты запуска, политика агента | Инструменты проекта | вне клиента |

Listener никогда не читает и не изменяет игровое состояние в своём потоке. Он копирует каждый принятый запрос `act` в очередь; набор команд проекта выполняется в потоке клиентского цикла.

## Компилируйте его только в клиенты разработки

`FO_AI_CONTROL` (по умолчанию `OFF`, см. [справочник CMake-опций](../../reference/cmake/options.md)) компилирует listener в клиентские сборки и сборки Mapper для Windows, Linux и macOS. Для Web, Android и iOS опция ни на что не влияет. При выключенной опции `Game.AiControlAvailable` равно `false`, `Game.AiControlStart` выбрасывает `AiControlException`, а бинарный файл не содержит кода listener.

Держите решение в одной принадлежащей проекту переменной, чтобы конфигурация выпуска не унаследовала значение разработки по умолчанию:

```cmake
# Development builds default to the listener; release and package jobs configure with -DPROJECT_AI_CONTROL=OFF
option(PROJECT_AI_CONTROL "Compile the AiControl listener into development clients" ON)
SetOption(FO_AI_CONTROL ${PROJECT_AI_CONTROL})
```

Объявите опцию до `StartProjectGeneration()`, рядом с остальными вызовами `SetOption`. Каждый workflow выпуска, магазина и упаковки обязан передавать `-DPROJECT_AI_CONTROL=OFF`. Одной runtime-настройки недостаточно: код слушающего сокета останется в поставляемом бинарном файле.

Listener использует обёртку сокетов Engine (`tcp_server` и `tcp_socket` в `Source/Essentials/NetSockets.h`) и не требует дополнительного пути включения сторонних библиотек. Семейство адресов — IPv4.

## Запускайте и останавливайте его из проектных настроек

Engine не добавляет настроек. Проект объявляет собственные (например, через `///@ Setting Client` в скриптах), решает, включён ли listener, и передаёт значения в script API:

| Script API | Поведение |
| --- | --- |
| `Game.AiControlAvailable` | `true`, если эта сборка скомпилировала listener. |
| `Game.AiControlStart(host, port, token, maxQueuedCommands, maxEvents)` | Синхронно выполняет bind и запускает поток сокета. Выбрасывает `AiControlException`, если listener не скомпилирован или уже запущен, порт вне диапазона 1..65535, предел не положителен, host не разрешается, для не-loopback host задан пустой токен или bind не удался. Пустой host означает `127.0.0.1`. |
| `Game.AiControlStop()` | Прекращает приём соединений, закрывает соединение, дожидается потока сокета и завершает каждую команду в очереди с `success: false`. Для остановленного listener ничего не делает. |
| `Game.AiControlIsRunning()` | `true`, пока поток сокета обслуживает соединения. |
| `Game.AiControlSetObservation(json)` | Заменяет наблюдение. Текст должен быть одним JSON-объектом размером не более 1 MiB минус резерв конверта 1 KiB; переводы строк заменяются пробелами. |
| `Game.AiControlPushEvent(json)` | Добавляет один JSON-объект размером не более 64 KiB в ограниченную историю событий. |
| `Game.AiControlPullCommand(...)` | Забирает следующую команду, которую не взял native-обработчик; см. ниже. |
| `Game.AiControlCompleteCommand(commandSeq, success, message)` | Добавляет терминальное событие `command_completed`; сообщение обрезается до 4 KiB. |
| `Game.AiControlGetStatus()` | Возвращает объект результата `status` в виде JSON-текста. |

Запускайте listener из события старта клиента, когда модули проекта готовы. Если на одной машине работает несколько клиентов, вычисляйте порт каждого экземпляра в проекте (например, базовый порт плюс индекс клиента). Читайте токен из проектного источника секретов или переменной окружения; никогда не помещайте его в закоммиченный конфиг, командную строку, лог или отчёт.

`ClientEngine` создаёт объект listener вместе с движком, вызывает native-обработчики команд один раз за кадр перед `OnLoop` и уничтожает listener в `ClientEngine::Shutdown` сразу после `OnFinish`. Скрипты, публикующие последнее наблюдение или завершающие команды, должны делать это в `OnFinish` или раньше.

## Модель безопасности

- Выключен по умолчанию дважды: опция сборки равна `OFF`, и ничто не запускает listener, пока проект не вызовет `Game.AiControlStart`.
- Сначала loopback: пустой host привязывается к `127.0.0.1`. Любой адрес из `127.0.0.0/8` после разрешения имени считается loopback.
- Не-loopback адрес (включая `0.0.0.0`) требует непустого токена, иначе вызов старта выбрасывает исключение. Вместо маршрутизируемого bind предпочитайте аутентифицированный шифрованный туннель к loopback listener.
- Авторизация действует в пределах соединения. При заданном токене каждый метод, кроме `auth`, возвращает `-32001`, пока `auth` не пройдёт успешно; новое соединение начинается неавторизованным. Сравнение токена занимает одинаковое время для любого кандидата той же длины.
- Одновременно обслуживается одно соединение. Запрос ограничен 1 MiB на строку; более длинная строка получает `-32600`, и соединение закрывается. Peer, который перестал читать, отключается после 10 секунд заблокированной отправки, а `Stop()` прерывает это ожидание.
- Токен и все payload передаются открытым текстом. Listener не даёт server authority: проектные команды должны использовать обычный клиентский ввод или аутентифицированные игровые вызовы.
- Строки лога уровня error и строки, упоминающие исключения или assertions, пересылаются как события `runtime_exception`, пока listener работает. Не допускайте секретов в логах: эти события получает подключённый инструмент.

## Регистрация набора команд проекта

Проект может обрабатывать команды в native-коде, в скриптах или и там, и там. Native-обработчики регистрируются из CLIENT-кода проекта, обычно в engine hook `ClientInitHook` (см. [hooks native-расширений](../../reference/native-extension/hooks.md)):

```cpp
#include "Common.h"

#include "Client.h"

FO_BEGIN_NAMESPACE

///@ EngineHook
FO_SCRIPT_API void ClientInitHook(ptr<ClientEngine> client);

void ClientInitHook(ptr<ClientEngine> client)
{
#if FO_HAVE_AI_CONTROL
    client->GetAiControl()->RegisterCommandHandler("wait", [](const AiControlCommand& command) {
        return AiControlCommandResult {.Success = true, .Message = strex("waited {}", command.IntArg).str()};
    });
#else
    ignore_unused(client);
#endif
}

FO_END_NAMESPACE
```

Правила маршрутизации команд:

1. Команды сохраняют порядок `act`. Native-обработчики выполняются в потоке клиентского цикла: либо из покадровой диспетчеризации, либо внутри `Game.AiControlPullCommand`.
2. Команда, для `type` которой зарегистрирован обработчик, завершается результатом `AiControlCommandResult` этого обработчика.
3. Регистрация пустого типа устанавливает резервный обработчик для всех типов без собственного обработчика. Без резервного обработчика первая невостребованная команда останавливает native-диспетчеризацию, пока скрипт не заберёт её через `Game.AiControlPullCommand` и не завершит через `Game.AiControlCompleteCommand`.
4. Обработчик, выбросивший исключение, завершает свою команду с `success: false` и текстом исключения, после чего исключение передаётся в клиентский цикл.
5. Повторная регистрация того же типа выбрасывает исключение. Обработчики остаются зарегистрированными до завершения клиента, поэтому захватывайте только состояние, которое живёт столько же, сколько client engine.

Проект, обрабатывающий команды только в native-коде, обязан зарегистрировать резервный обработчик, отклоняющий неизвестные типы; иначе неизвестная команда будет ждать скриптового потребителя, который никогда не появится. Проект, обрабатывающий команды только в скриптах, ничего не регистрирует и извлекает команды через `Game.AiControlPullCommand` в своём событии цикла.

`AiControlCommand` декодирует вспомогательные поля протокола (`targetId`, `itemId`, `auxId`, `x`, `y`, `screenX`, `screenY`, `intArg`, `stringArg`, `append`) и сохраняет полный объект `params` в `ParamsJson` для проектных полей. Типы полей строгие: идентификаторы — целые JSON-числа или десятичные строки, координаты и `intArg` — 32-битные целые, `stringArg` — строка, `append` — boolean. Неверный тип отклоняется с `-32602` до постановки команды в очередь. Каждая команда из очереди должна завершиться ровно одним событием `command_completed`.

## Проверка listener

Модульные тесты Engine запускают настоящие listener на loopback-портах начиная с 46100 и покрывают отказ от небезопасной конфигурации, токен и авторизацию на соединение, ошибки конверта и framing, CRLF и несколько запросов за одну запись, выравнивание наблюдения в одну строку, декодирование полей, переполнение очереди, вытеснение истории, порядок и сбой обработчиков, слишком большие и брошенные peer, ограниченное по времени `Stop()`, повторное использование порта после остановки и уничтожение работающего listener. `BuildTools/validation-project`, который использует `validate unit-tests`, включает `FO_AI_CONTROL`.

Запускайте их из сконфигурированного каталога сборки проекта, в котором включена опция (замените `<ProjectDevName>`):

```powershell
cmake --build . --config Release --target <ProjectDevName>_UnitTests
.\Binaries\Tests-Windows-win64\<ProjectDevName>_UnitTests.exe "AiControlListener*"
```

```bash
cmake --build . --config Release --target <ProjectDevName>_UnitTests
./Binaries/Tests-Linux-x64/<ProjectDevName>_UnitTests "AiControlListener*"
```

Тестовый бинарный файл завершается с кодом `0` и печатает `All tests passed`, когда все случаи проходят; любая непрошедшая проверка даёт ненулевой код. Эти тесты доказывают работу транспорта внутри одного процесса. Интеграцию проекта доказывайте отдельно на запущенном клиенте разработки с эталонным клиентом:

```powershell
$env:PROJECT_AI_TOKEN = "<token>"
python BuildTools\ai_control_client.py --port <port> --token-env PROJECT_AI_TOKEN status
python BuildTools\ai_control_client.py --port <port> --token-env PROJECT_AI_TOKEN act --type wait --int-arg 1
python BuildTools\ai_control_client.py --port <port> --token-env PROJECT_AI_TOKEN events
```

```bash
export PROJECT_AI_TOKEN='<token>'
python3 BuildTools/ai_control_client.py --port <port> --token-env PROJECT_AI_TOKEN status
python3 BuildTools/ai_control_client.py --port <port> --token-env PROJECT_AI_TOKEN act --type wait --int-arg 1
python3 BuildTools/ai_control_client.py --port <port> --token-env PROJECT_AI_TOKEN events
```

Подключите модульные тесты к CI через обычную задачу модульных тестов проекта; отдельная задача для listener не нужна. Добавьте проверку артефакта выпуска: клиент, собранный в конфигурации выпуска, сообщает `Game.AiControlAvailable == false` или не содержит символов listener.

## Чтение собранных изображений на сервере

Проекты, державшие собственную копию listener, часто держали рядом серверное чтение изображений по слотам, например для выборки маски карты мира. Engine предоставляет такое чтение как ссылочный тип, которым владеет скрипт:

| Script API | Поведение |
| --- | --- |
| `Game.LoadImage(imageName)` | Читает собранный ресурс изображения ровно с одним направлением и одним кадром и возвращает новый `ServerImage`. Выбрасывает `ScriptException` для пустого имени, отсутствующего файла, нескольких направлений или кадров, кадра со ссылкой на другой кадр или пустого кадра. |
| `ServerImage.GetSize()` | Ширина и высота в пикселях. |
| `ServerImage.GetColor(pos)` | `ucolor` одного пикселя; выбрасывает `ScriptException` для позиции вне изображения. |

Изображение живёт, пока скрипт хранит ссылку на него; нумерованных слотов, которые нужно освобождать, нет. Проект с экспортами `Game.LoadImage(slot, name)` и `Game.GetImageColor(slot, pos)` удаляет их (codegen сообщает о дублирующемся `Game.LoadImage`, пока они есть), хранит возвращённый `ServerImage` в переменной скрипта и заменяет `GetImageColor(slot, pos)` на `image.GetColor(pos)`. Модульные тесты теперь получают настоящее изображение или исключение вместо пустого размера.

## Расширение в проекте

- Храните проектные настройки (`AiControl.Enabled`, host, port, шаг портов, токен, размеры очередей, интервал наблюдения) в проекте и превращайте их в один вызов `Game.AiControlStart`.
- Размещайте игровые команды в проектных обработчиках или скриптах. Называйте их по действиям игрока, проверяйте теми же правилами, что и ввод игрока, и возвращайте стабильные сообщения завершения.
- Версионируйте объект наблюдения проектным `schemaVersion` и публикуйте только полные снимки.
- Добавляйте проектные события (например, `bridge_started`, уведомления о диалогах или бое) через `Game.AiControlPushEvent`. Engine резервирует только `command_completed` и `runtime_exception`.
- Держите MCP-адаптеры, имена инструментов и рецепты запуска в инструментах проекта; стройте их на `BuildTools/ai_control_client.py` или тех же правилах wire-протокола.

### Переход с проектной копии listener

Проект, компилировавший собственный listener в CLIENT-расширение (экспортируемые методы `Client_Game_AiControl*`), переходит так:

1. Удалите проектный исходник listener и его регистрацию в CMake, проектный путь включения asio, который существовал только ради него, и проектный compile definition, который его включал. Вместо этого задавайте `FO_AI_CONTROL` из проектной опции разработки. Codegen сообщает о дублирующихся экспортах, пока старые методы не удалены.
2. Замените `Game.AiControlStart(enabled, host, port, token, maxQueuedCommands, maxEvents)` проверкой проектной настройки включения и вызовом `Game.AiControlStart(host, port, token, maxQueuedCommands, maxEvents)`. Вызов выбрасывает исключение вместо возврата `false`; нулевые и отрицательные пределы больше не поднимаются до 1, а повторный старт без `Game.AiControlStop()` выбрасывает исключение.
3. Добавьте выходной параметр `paramsJson` в каждый вызов `Game.AiControlPullCommand`. Проектные поля, которые старая структура команды хранила в дополнительном слоте, читаются из `paramsJson` или переносятся в `stringArg` в адаптере проекта.
4. Публикуйте наблюдения и события как JSON-объекты. Пустая строка больше не превращается в `{}`, а неверный JSON выбрасывает исключение в публикующем вызове вместо поля ошибки разбора в ответе.
5. Передавайте значения полей с их JSON-типами. Строки вроде `"5"` для `x` или `intArg` отклоняются с `-32602`; идентификаторы могут оставаться десятичными строками.
6. Оставьте в проектном расширении под собственными именами вспомогательные функции, не входящие в протокол (поиск идентификатора сущности, имитация касаний, индекс встроенного клиента).
7. Пересоберите все клиентские роли, запустите тесты Engine `AiControlListener*` и проверки проекта на живом клиенте и убедитесь, что конфигурация выпуска исключает listener.

## Контрольный список проверки

- Клиенты разработки конфигурируются с `FO_AI_CONTROL=ON`; задачи выпуска и упаковки передают проектный переключатель, который его выключает.
- `Game.AiControlStart` вызывается один раз, после инициализации проекта, с loopback host или с непустым токеном из источника секретов.
- Для каждого принимаемого проектом типа команды есть native-обработчик или скриптовый потребитель, и каждая извлечённая команда завершается.
- Наблюдения — полные JSON-объекты с проектным `schemaVersion` без скрытого состояния сервера и секретов.
- Модульные тесты `AiControlListener*` проходят, эталонный клиент достигает запущенного клиента разработки, а сборка выпуска сообщает, что listener недоступен.
