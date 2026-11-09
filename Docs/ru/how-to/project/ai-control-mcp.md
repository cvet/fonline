---
layout: default
title: Управление игрой через MCP-адаптер AiControl
locale: ru
document_id: project-ai-control-mcp
permalink: /Docs/ru/how-to/project/ai-control-mcp.html
---

# Управление игрой через MCP-адаптер AiControl
<!-- docs-translation: {"document_id":"project-ai-control-mcp","locale":"ru","source_path":"Docs/en/how-to/project/ai-control-mcp.md","source_sha256":"bfe502eccdbae819a627889071a81cecfd0686874c011fb00f36a1f8237b7617"} -->
Руководство показывает, как игровой проект даёт ИИ-агенту наблюдать за
отладочной сборкой и играть в неё через MCP-адаптер движка из
`BuildTools/ai_control_mcp`. Адаптер говорит с MCP-клиентом агента по Model
Context Protocol через stdio, а с bridge проекта — по
[протоколу AiControl](../ai-control-protocol.md) через TCP. Описаны команды, коды
выхода, общие инструменты, правила безопасности, профиль запуска, который
стартует собственные процессы адаптера, подключение к локальной задаче и CI, а
также точка расширения, через которую проект регистрирует свои игровые
инструменты.

Адаптер — инструмент разработки и QA. Он никогда не входит в выпускаемую игру:
не упаковывайте его, не запускайте его из рабочего клиента или сервера и
держите bridge, с которым он работает, вырезанным из release-сборок, как требуют
правила безопасности протокола.

## Проверенные исходные пути

- `BuildTools/ai_control_mcp/__main__.py`
- `BuildTools/ai_control_mcp/cli.py`
- `BuildTools/ai_control_mcp/stdio.py`
- `BuildTools/ai_control_mcp/server.py`
- `BuildTools/ai_control_mcp/registry.py`
- `BuildTools/ai_control_mcp/context.py`
- `BuildTools/ai_control_mcp/schemas.py`
- `BuildTools/ai_control_mcp/core_tools.py`
- `BuildTools/ai_control_mcp/session.py`
- `BuildTools/ai_control_mcp/launch.py`
- `BuildTools/ai_control_mcp/process_owner.py`
- `BuildTools/ai_control_mcp/runner.py`
- `BuildTools/ai_control_mcp/advisory.py`
- `BuildTools/ai_control_client.py`
- `BuildTools/AiControlProtocol.json`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/foconfig.py`
- `BuildTools/tests/test_ai_control_mcp.py`
- `BuildTools/tests/test_ai_control_protocol.py`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `Source/Common/Settings.cpp`
- `Source/Common/Settings.inc`
- `Source/Frontend/ApplicationInit.cpp`
- `Examples/AiControlSample/ai_control_sample.py`
- `Examples/AiControlSample/README.md`
- `Docs/en/how-to/ai-control-protocol.md`
- `Docs/en/how-to/build/project-configuration.md`
- `Docs/en/how-to/release/security-and-secrets.md`

## Что делает адаптер

| Слой | Владелец | Содержание |
|---|---|---|
| MCP stdio | Движок | Одно сообщение JSON-RPC 2.0 в UTF-8 на строку; `initialize`, `ping`, `tools/*`, `resources/*`, `prompts/*`; пакеты, повреждённые кадры и слишком длинные строки отклоняются без завершения сеанса |
| Транспорт AiControl | Движок | `BuildTools/ai_control_client.py`; одно авторизованное соединение на запрос, строгая проверка конверта и id, предел строки 1 МиБ |
| Сеанс | Движок | Реестр endpoint, выбранный endpoint, отдельный курсор событий для каждого endpoint, свежее завершение команд, свежие наблюдения, ожидание готовности |
| Процессы | Движок | Профиль запуска, поиск бинарников, командные строки `-ApplyConfig`/`-ApplySubConfig`/`--Group.Name value`, независимые владельцы процессов, хвосты журналов |
| Игровые инструменты | Проект | Типизированные действия, проекции наблюдения, условие готовности, prompts, руководства, рецепты, память и политика агента |

Запускайте его, когда агент или сценарный плейтест должен управлять настоящим
клиентом: при воспроизведении отчёта об ошибке обычным путём игрока, при
smoke-прогоне после изменения геймплея или в исследовательском сеансе. Для
детерминированных тестов без агента лучше подходят
[маршруты тестов проекта](project-tests.md).

## Запуск

Из корня игрового проекта, который встраивает движок в `Engine/`, сначала
проверьте конфигурацию: `--list-tools` загружает проектный модуль и профиль
запуска, печатает все инструменты и завершается.

```powershell
$env:FONLINE_AI_TOKEN = "<development token>"
py -3 Engine\BuildTools\ai_control_mcp --project-root . --project-module Tools/AiControl/mygame_mcp.py --launch-profile Tools/AiControl/launch-profile.json --tool-prefix mygame_ --list-tools
```

```bash
export FONLINE_AI_TOKEN='<development token>'
python3 Engine/BuildTools/ai_control_mcp --project-root . --project-module Tools/AiControl/mygame_mcp.py --launch-profile Tools/AiControl/launch-profile.json --tool-prefix mygame_ --list-tools
```

Без `--list-tools` та же команда обслуживает MCP через stdin/stdout, пока клиент
не закроет stdin; диагностика идёт в stderr. MCP-клиент запускает адаптер как
stdio-сервер. Точный файл конфигурации принадлежит клиенту; типичная запись
выглядит так:

```json
{
  "mcpServers": {
    "mygame": {
      "command": "python3",
      "args": [
        "Engine/BuildTools/ai_control_mcp",
        "--project-root", ".",
        "--project-module", "Tools/AiControl/mygame_mcp.py",
        "--launch-profile", "Tools/AiControl/launch-profile.json",
        "--tool-prefix", "mygame_"
      ]
    }
  }
}
```

Переменную с токеном задавайте в окружении, которое наследует клиент, и никогда
не в этом файле. Чтобы попробовать адаптер без игры, запустите в другом
терминале пример протокола из
[Examples/AiControlSample](../../../../Examples/AiControlSample/README.ru.md)
командой `py -3 Engine\Examples\AiControlSample\ai_control_sample.py` (он слушает
порт по умолчанию `43011`) и вызовите на нём `mygame_ping`, `mygame_observe` и
`mygame_act`.

### Коды выхода

| Код | Значение |
|---|---|
| `0` | Стандартный ввод закончился или пришёл сигнал завершения; собственные процессы остановлены, если не задан `--keep-processes-on-exit`. `--list-tools` выполнен |
| `2` | Неверный вызов или конфигурация: неизвестный параметр, нет корня проекта, проектный модуль не найден или не содержит `register(registry)`, повторное имя инструмента, неверный профиль запуска, не-loopback хост без `--allow-remote` или без токена |

Неудачный вызов инструмента никогда не завершает процесс. Неверные аргументы
инструмента возвращают ошибку JSON-RPC `-32602`; сбой bridge (отказ в
соединении, тайм-аут, повреждённый ответ, `queue_full`, отказ в авторизации)
возвращает результат инструмента с `isError: true` и объектом `error`;
непредвиденное исключение обработчика возвращает `-32603`.

## Параметры

| Параметр | Назначение |
|---|---|
| `--host`, `--port` | Endpoint по умолчанию и базовый порт для запускаемых bridge; по умолчанию `127.0.0.1:43011` |
| `--timeout` | Тайм-аут сокета на один запрос к bridge в секундах; по умолчанию `5.0` |
| `--token-env NAME` | Переменная окружения с общим токеном; по умолчанию `FONLINE_AI_TOKEN`. Параметра, принимающего сам токен, нет |
| `--allow-remote` | Разрешить не-loopback endpoint; им дополнительно нужен непустой токен |
| `--project-root DIR` | Корень для путей профиля запуска, бинарников и журналов; по умолчанию текущий каталог |
| `--project-module SPEC` | Имя модуля через точку или файл `.py`, чей `register(registry)` добавляет проектные инструменты; можно повторять |
| `--module-path DIR` | Каталог, добавляемый в путь импорта для модулей, заданных через точку; можно повторять |
| `--launch-profile FILE` | JSON-профиль запуска; заменяет профиль, заданный проектным модулем |
| `--tool-prefix TEXT` | Префикс имён базовых инструментов; по умолчанию `fo_` |
| `--server-name TEXT` | `serverInfo.name`, сообщаемое клиенту; по умолчанию `fonline-ai-control` |
| `--keep-processes-on-exit` | Оставить запущенные процессы работать после выхода адаптера |
| `--max-message-bytes N` | Самая длинная допустимая строка MCP; по умолчанию 4 МиБ, минимум 1024 |
| `--list-tools` | Напечатать имена и описания инструментов в JSON и выйти |

Сгенерированный [справочник helper CLI](../../reference/helper-cli/commands.md)
фиксирует точный текст справки текущей ревизии.

## Базовые инструменты

Ядро регистрирует только инструменты, которые следуют из
[методов протокола](../../reference/ai-control-protocol/methods.md), а также
управление endpoint, готовностью и процессами. В протоколе нет метода снимка
экрана, поэтому снимки экрана — проектные инструменты. Имена ниже даны без
префикса.

| Инструмент | Назначение |
|---|---|
| `status`, `ping`, `observe` | Методы протокола; `observe` с `fresh: true` ждёт до `freshTimeoutMs` строго более нового `observationSeq` и сообщает `observationFresh` |
| `events` | События после явно заданного `afterSeq`; курсор не сдвигается |
| `next_events` | События после курсора endpoint со сдвигом курсора; `reset` начинает с `0`, `seekLatest` переходит к `latestSeq` |
| `act` | Ставит в очередь `command` (объект с непустым `type`); с `waitForCompletion` или `syncAfterCompletion` ждёт его завершения |
| `wait_command` | Ждёт `command_completed` для `commandSeq`, записанного после `afterSeq` |
| `connect` | Выбирает зарегистрированный endpoint, endpoint управляемого процесса или прямые `host`/`port` и проверяет его через `ping` |
| `endpoints` | Зарегистрированные endpoint, выбор и курсоры; `probe: true` проверяет каждый через `ping` |
| `wait_ready`, `wait_all_ready` | Опрашивают `observe`, пока не выполнится условие готовности, не завершится процесс-владелец или не истечёт тайм-аут |
| `launch_options` | Роли, бинарники, подключение bridge и секции `[SubConfig]` настроенного `.fomain` |
| `launch`, `processes`, `stop_process` | Запуск роли профиля под независимым владельцем, список собственных процессов, остановка одного дерева или всех деревьев |
| `logs` | Хвост журнала собственного процесса (по умолчанию `<binary stem>.log` в его рабочем каталоге) или названного журнала в корне проекта |

Инструменты, работающие с endpoint, принимают `endpointId` или `processId` с
необязательным `endpointIndex`; без них используется выбранный endpoint.

### Жизненный цикл команды и свежесть

- Принятие — ещё не завершение. `act` возвращает `accepted` и `commandSeq`; при
  ожидании добавляется `completion` с `completed`, `timedOut`, событием `event`,
  прочитанными `events` и курсором.
- Перед отправкой команды с ожиданием адаптер читает `latestSeq` bridge.
  Засчитывается только событие `command_completed` с бо́льшим номером, поэтому
  старое завершение никогда не удовлетворит новое ожидание.
- `syncAfterCompletion` возвращает `sync.observation` из снимка, опубликованного
  после того, как завершение было замечено, и `observationFresh: false`, если
  такой снимок не пришёл за `freshTimeoutMs`.
- Если `latestSeq` стал меньше курсора, bridge перезапустился: курсор
  сбрасывается в `0`, а результат содержит `historyReset: true`. Команда с
  ожиданием тогда сообщает `completed: false`, а не ждёт события, которое уже не
  придёт.
- История событий в bridge ограничена. Медленный читатель может пропустить
  события; пересинхронизируйтесь через `observe`.

## Правила безопасности

Адаптер соблюдает
[границу безопасности протокола](../ai-control-protocol.md#граница-безопасности):

- По умолчанию только loopback. Не-loopback endpoint, будь он из `--host`,
  `connect` или `bridgeHost` при запуске, требует `--allow-remote` и непустого
  токена. Эталонный клиент применяет то же правило.
- Токен берётся только из переменной, названной `--token-env`. Аргументы
  инструментов не могут передать токен, и ни один результат инструмента, список
  endpoint или командная строка запуска его не показывает.
- Запущенный процесс получает токен через своё окружение: в командной строке
  стоит `--<tokenSetting> $TARGET_ENV{<token variable>}`, которое движок
  разрешает, когда приложение читает настройки (см.
  [настройку проекта](../build/project-configuration.md)). `settings` при запуске
  не могут переопределить настройку токена.
- `binaryPath`, `workingDirectory` и имена журналов должны оставаться внутри
  корня проекта или рабочего каталога собственного процесса.
- Адаптер останавливает только деревья процессов, которые запустил сам. Он
  никогда не останавливает процесс по имени образа, не трогает процессы,
  запущенные разработчиком или другим адаптером, а с `--keep-processes-on-exit`
  оставляет свои деревья оператору.

## Профиль запуска

`launch` требует профиля из `--launch-profile` или из проектного модуля. Полный
пример для проекта с `FO_DEV_NAME` `MyGame`, чей клиентский bridge читает
настройки `AiControl.*`:

```json
{
  "schemaVersion": 1,
  "devName": "MyGame",
  "config": "MyGame.fomain",
  "defaultRole": "server_headless",
  "defaultSubConfigs": [],
  "binarySearchDirs": ["Binaries", "Build"],
  "binaryEnvTemplate": "MYGAME_AI_{ROLE}_EXE",
  "nonInteractive": true,
  "bridge": {
    "enabledSetting": "AiControl.Enabled",
    "hostSetting": "AiControl.Host",
    "portSetting": "AiControl.Port",
    "tokenSetting": "AiControl.Token",
    "portStrideSetting": "AiControl.PortStride"
  },
  "roles": {
    "server_headless": {"target": "ServerHeadless", "endpoints": "embedded-clients", "defaultClientCount": 1},
    "client": {"target": "Client", "endpoints": "client"},
    "client_headless": {"target": "ClientHeadless", "endpoints": "client"}
  }
}
```

| Ключ | Значение |
|---|---|
| `schemaVersion` | Должен быть `1` |
| `devName` | `FO_DEV_NAME` проекта; `target` роли превращается в бинарник `<devName>_<target>` (`Server`, `ServerHeadless`, `Client`, `ClientHeadless`, `Mapper`) |
| `config` | Путь к `.fomain` относительно проекта, передаётся как `-ApplyConfig`; без него приложение ищет конфигурацию само |
| `roles` | Имя роли и её `binary` (имя файла) или `target`, `endpoints` (`none`, `client`, `embedded-clients`), `defaultClientCount`, постоянные `settings`. Если ключ опущен, используются пять стандартных ролей, выведенных из `devName` |
| `defaultRole`, `defaultSubConfigs` | Роль, если `launch` её не называет; sub-config, применяемые до `subConfigs` вызова |
| `binarySearchDirs` | Каталоги проекта для рекурсивного поиска; побеждает самый новый подходящий файл, поэтому устаревшая копия никогда не запустится вместо свежей сборки. По умолчанию `Binaries`, `Build`, `Workspace` |
| `binaryEnvTemplate` | Необязательная переменная с явным бинарником; `{ROLE}` заменяется ролью в верхнем регистре |
| `bridge` | Имена настроек bridge проекта. Без этого ключа запуски не регистрируют endpoint |
| `nonInteractive` | Включать `Common.NonInteractive` при включённом bridge, чтобы диалог ошибки не остановил прогон без оператора; по умолчанию `true` |

Неизвестные ключи делают профиль недействительным. Для ролей `embedded-clients`
запускатель задаёт `Server.AutoStartClientOnServer` равным `clientCount` и
регистрирует по endpoint на клиента по адресу
`bridgePort + index * bridgePortStride`; больше одного клиента требует
`portStrideSetting`. Аргументы запуска добавляют `subConfigs`, скалярные
переопределения `settings`, `clientCount`, `bridge: false`, `bridgeHost`,
`bridgePort`, `bridgePortStride`, `binaryPath`, `workingDirectory`,
`waitForBridge`, `timeoutMs` и `selectFirstEndpoint`. Результат содержит
командную строку, endpoint, итог проверки готовности и отметку `.build-hash`,
которую сборка записывает рядом с бинарником.

Владение процессами реализовано отдельным процессом-опекуном на каждый запуск: в
Windows это job object с уничтожением при закрытии, в Linux — child subreaper,
который сигналит только принятым им потомкам. Конец ввода, `SIGTERM`/`SIGINT` и
даже жёсткое уничтожение адаптера останавливают каждое собственное дерево,
включая потомков, ушедших в отдельный сеанс. На других платформах запуск с
владением отклоняется; используйте там `--keep-processes-on-exit` и
останавливайте процессы сами.

## Подключение к задаче и CI

- **Локальная задача.** Направьте конфигурацию MCP-клиента редактора на команду
  выше. Держите вторую задачу, которая запускает ту же команду с `--list-tools`,
  чтобы сломанный модуль или профиль был виден до начала сеанса агента.
- **Сценарные плейтесты.** Раннер запускает адаптер через
  `ai_control_mcp.runner`, который переиспользует одну реализацию MCP-клиента и
  атомарную запись отчётов:

```python
import sys
from pathlib import Path

sys.path.insert(0, str(Path("Engine/BuildTools").resolve()))

from ai_control_mcp import runner

client = runner.McpProcess(runner.build_adapter_command([
    "--project-root", ".",
    "--project-module", "Tools/AiControl/mygame_mcp.py",
    "--launch-profile", "Tools/AiControl/launch-profile.json",
    "--tool-prefix", "mygame_",
]))

try:
    runner.initialize_client(client, "mygame-smoke")
    launched = runner.call_tool(client, "mygame_launch", {"role": "server_headless", "subConfigs": ["AiPlaytest"]})
    endpoint = launched["endpoints"][0]
    runner.wait_ready(client, endpoint, tool_prefix="mygame_", timeout_ms=240000)
    moved = runner.call_tool(client, "mygame_move", {**runner.endpoint_args(endpoint), "x": 10, "y": 12})
    runner.verify_command_payload(moved, "move")
    runner.write_json_report(Path("Workspace/mygame-smoke/report.json"), {"status": "passed", "move": moved})
finally:
    client.close()
```

- **CI.** Запускайте `--list-tools` с проектным модулем при каждом изменении
  модуля или профиля; сборка для этого не нужна. Сценарные плейтесты запускайте
  только в заданиях, которые уже собрали клиент и сервер из одного запекания
  ресурсов, и считайте отсутствие завершения, тайм-аут готовности
  или ненулевой код выхода раннера провалом. Не запускайте адаптер в заданиях
  упаковки и выпуска.

## Расширение в проекте

### Точка расширения через регистрацию

Проектный модуль — обычный Python-код, определяющий `register(registry)`.
Адаптер импортирует его (путь к `.py` или имя через точку, найденное через
`--module-path`), добавляет каталог `BuildTools` движка в путь импорта и один
раз вызывает `register` до начала обслуживания. Полный минимальный модуль для
игры, чьё наблюдение перечисляет `availableActions` и которая принимает команду
`move`:

```python
"""MyGame tools for the Engine AiControl MCP adapter."""

from ai_control_mcp.registry import InvalidArguments, object_schema
from ai_control_mcp.schemas import act_option_properties, endpoint_target_properties


def register(registry):
    registry.set_readiness(fields=["ready"])
    registry.set_instructions("Call mygame_wait_ready first, then mygame_actions, then mygame_move.")
    registry.add_tool(
        "mygame_actions",
        list_actions,
        description="List the actions the player character can take right now.",
        input_schema=object_schema(endpoint_target_properties()),
        annotations={"readOnlyHint": True},
    )
    registry.add_tool(
        "mygame_move",
        move,
        description="Walk the player character to a grid cell and wait until the client finishes the move.",
        input_schema=object_schema(
            {
                **endpoint_target_properties(),
                **act_option_properties(),
                "x": {"type": "integer", "minimum": 0},
                "y": {"type": "integer", "minimum": 0},
            },
            ["x", "y"],
        ),
    )


def list_actions(context, arguments):
    observation = context.observe(arguments)["observation"]
    return {"availableActions": observation.get("availableActions", [])}


def move(context, arguments):
    observation = context.observe(arguments)["observation"]

    if "move" not in observation.get("availableActions", []):
        raise InvalidArguments("move is not available in the current observation")

    options = {"waitForCompletion": True, **arguments}
    return context.act(options, {"type": "move", "x": arguments["x"], "y": arguments["y"]})
```

API реестра:

| Вызов | Действие |
|---|---|
| `add_tool(name, handler, description=, input_schema=, annotations=, replace=False)` или декоратор `@registry.tool(...)` | Регистрирует инструмент. Имена соответствуют `[A-Za-z0-9_.-]{1,128}`; повтор имени завершает запуск ошибкой, если только `replace=True` не заменяет существующий инструмент намеренно |
| `add_resource(uri, reader, name=, description=, mime_type=)` | MCP-ресурс только для чтения, например руководство оператора; `reader(context)` возвращает текст |
| `add_prompt(name, render, description=, arguments=)` | MCP-prompt; `render(arguments)` возвращает текст prompt |
| `set_launch_profile(profile)` | JSON-профиль в виде словаря; `--launch-profile` его заменяет |
| `set_readiness(check=None, fields=())` | Условие готовности: `check(observation, arguments)` возвращает имена невыполненных условий, либо `fields` перечисляет поля наблюдения, которые должны быть истинными. Вызывающий может заменить поля через `requireFields` |
| `set_instructions(text)` | `instructions`, возвращаемые `initialize` |
| `prefix`, `core_name(base)` | Префикс базовых инструментов и полное имя базового инструмента |

Обработчик получает `ToolContext` и проверенные аргументы.
`context.endpoint(arguments)`, `context.observe(arguments)`,
`context.request(arguments, method, params)` и `context.act(arguments, command)`
обращаются к выбранному или названному endpoint через сеанс;
`context.orchestrator` запускает и останавливает собственные процессы, поэтому
проектный инструмент-рецепт собирает `context.orchestrator.launch({...})` из
своих аргументов сцены или аккаунтов. Возвращайте JSON-объект или `ToolResult`
для другого содержимого. Бросайте `InvalidArguments` при ошибке вызывающего и
`ToolFailure`, если инструмент отработал и потерпел неудачу; ошибки bridge
превращаются в ошибки инструмента автоматически.

Схемы входа используют проверяемое подмножество JSON Schema: `type`,
`properties`, `required`, `additionalProperties`, `items`, `enum`, `minimum`,
`maximum`, `minLength`, `maxLength`, `minItems`, `maxItems`, `description`,
`default`, `title`. Схема с другим ключевым словом отклоняется при регистрации,
поэтому адаптер никогда не объявляет ограничение, которое не проверяет.
`object_schema` по умолчанию отклоняет неизвестные аргументы.

### Что остаётся в проекте

В проекте остаются типизированные игровые действия и их предварительные
проверки, проекции и сводки наблюдения, поля готовности, инструменты снимков
экрана на основе проектных команд, рецепты запуска сцен, аккаунтов и
многопользовательских конфигураций, память, профили и автономные политики
агента, prompts и руководства, журналы сеансов и все сценарии плейтестов.
`ai_control_mcp.advisory` даёт небольшие помощники (`task_option`,
`ranked_options`, `clamp_score`) для инструментов, предлагающих следующие
действия; само ранжирование — политика проекта.

Выберите один префикс для базовых и проектных инструментов, чтобы агент видел
одно пространство имён, и никогда не используйте имя базового инструмента для
другого смысла; заменяйте базовый инструмент только через `replace=True` и с тем
же контрактом.

### Переход с собственной копии проекта

Проект, который держал собственный адаптер, переходит на ядро движка одним
изменением:

1. Опишите старый адаптер: какие инструменты общие для протокола (status, ping,
   observe, events, события по курсору, сырой act, ожидание команды, выбор
   endpoint, готовность, запуск, процессы, остановка, журналы), а какие игровые.
2. Удалите собственный клиент bridge и цикл stdio. Передайте `--tool-prefix` со
   старым префиксом, чтобы общие инструменты сохранили имена, и перенесите
   каждый игровой инструмент в проектный модуль через `add_tool`.
3. Переведите старый код запуска в профиль запуска: имена бинарников становятся
   `devName` и целями ролей, файл конфигурации — `config`, настройки bridge —
   `bridge`. Рецепты, которые только заполняют аргументы, становятся небольшими
   проектными инструментами, вызывающими `context.orchestrator.launch`.
4. Замените аргументы `--token` в раннерах и конфигурациях MCP-клиентов
   переменной токена, названной `--token-env`. Перенесите помощники раннеров на
   `ai_control_mcp.runner`.
5. Учтите более строгий контракт: `act` принимает команду вложенным объектом
   `command`, списки endpoint больше не показывают токены, результаты сообщают
   `historyReset`, поля готовности объявляет проект, а повреждённый ответ bridge
   — ошибка инструмента, а не пропуск.
6. Запустите `--list-tools`, сценарные плейтесты проекта и ручной сеанс агента,
   затем удалите старый адаптер, его код управления процессами и его тесты
   общего поведения.

## Контрольный список проверки

1. `--list-tools` с проектным модулем и профилем завершается с кодом `0` и
   перечисляет базовые инструменты с выбранным префиксом и все проектные
   инструменты.
2. На примере протокола `ping`, `observe`, `act` с ожиданием и `next_events`
   проходят, а неверный токен в переменной даёт результат инструмента с
   `AuthorizationError`.
3. `launch` каждой роли профиля запускает самую новую сборку, её командная
   строка содержит `$TARGET_ENV{...}` вместо токена, а `wait_ready` проходит
   проектное условие.
4. Закрытие stdin клиента останавливает каждое запущенное дерево; после этого
   `processes` пуст и ни одного игрового процесса не осталось.
5. Адаптер, проектный модуль и bridge отсутствуют в выпускаемых пакетах.
