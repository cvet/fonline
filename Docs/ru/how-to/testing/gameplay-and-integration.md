---
layout: default
title: Gameplay- и integration-тестирование
locale: ru
document_id: gameplay-testing
permalink: /Docs/ru/how-to/testing/gameplay-and-integration.html
---

# Gameplay- и integration-тестирование

<!-- docs-translation: {"document_id":"gameplay-testing","locale":"ru","source_path":"Docs/en/how-to/testing/gameplay-and-integration.md","source_sha256":"c7d9307bc33c73e7a286d3e384222b191f1807108a2ba0481e5e1df707387121"} -->

> Документация принадлежит движку. Руководство задает переиспользуемые правила
> выбора test boundary, детерминированных fixtures, process runner, markers,
> deadline, cleanup и evidence для игр на FOnline.

## Назначение

Используйте этот маршрут, когда одного native unit test уже недостаточно, но
проверка еще не требует packaged release или device lab. Движок поставляет
project-neutral process runner и два уровня доказательства:

- [Gameplay Test Harness](../../../../Examples/GameplayTestHarness/README.md)
  проверяет сам runner без скомпилированной игры;
- [Minimal Multiplayer](../../../../Examples/MinimalMultiplayer/README.ru.md)
  проводит тот же runner через baked content, metadata, headless server/client,
  network, map load, remote calls, replicated state и interaction.

Native Catch2 targets, sanitizers и coverage описаны в
[Testing](../../contributing/testing/). Первый учебный end-to-end путь находится
в [First Automated Test](../../tutorials/first-test.md).

## Решение о тесте

Используйте boundary-based test selection и запускайте проверки narrow-first,
чтобы первый отказ давал focused failure location. Для process smoke объявляйте
`ready_marker`, упорядоченные `required_markers`, `forbidden_markers` и единый
deadline; всегда сохраняйте command lines, logs, причины завершения и результаты
cleanup. Engine владеет этим контрактом runner. Встраивающий проект владеет
своими script test registration API, каталогом fixtures, gameplay assertions,
acceptance thresholds, accounts, databases, scenes и release lanes.

## Выберите минимальную владеющую границу

Выберите первый слой, который действительно наблюдает изменяемый контракт.
Добавляйте более широкое доказательство только при пересечении новой границы.

| Граница изменения | Первое доказательство | Когда расширять |
|---|---|---|
| Native algorithm/data structure | focused Catch2 | construction, serialization, thread или runtime role меняют результат |
| Parser, baker, generated metadata | native test и bake | generated result потребляет runtime |
| Один script module/callback | project-owned side-specific script test | state пересекает module, entity, persistence или client/server |
| Prototype/map/text/resource | bake и semantic assertion | клиент обязан загрузить, показать, воспроизвести или дать взаимодействовать |
| Server lifecycle/world setup | один headless server | нужен network/client-observable результат |
| Protocol/gameplay interaction | ordered headless server/client smoke | заявлены presentation, package, device или backend |
| Package/updater/platform/release | package/platform acceptance | заявлены production infrastructure или recovery |

Это boundary-based selection, а не фиксированная пирамида. Broad smoke не
заменяет focused failure location, а unit test не доказывает process/network/
baked-content границу, в которую не входит. Работайте narrow-first и
останавливайтесь на первом сбое до расширения scope.

## Контракт детерминированной fixture

1. Используйте изолированные working directory, storage namespace, accounts,
   ports и outputs.
2. Выберите checked-in named config/sub-config.
3. Создайте только необходимые world/entities/authored references.
4. Зафиксируйте управляемую случайность или удалите ее с test route.
5. Печатайте low-volume semantic markers после readiness и asserted outcomes.
6. После успеха запросите normal shutdown, проверяя teardown.
7. Ограничьте каждый wait и сохраните process output плюс structured result.

Fixed sleep может только задавать pacing; pass condition обязан быть exit code,
semantic marker, decoded artifact, state query или visible result владельца
границы. Fixture очищает каждый созданный process/resource, использует уникальные
ports либо serialized CI и никогда не обращается к production storage,
credentials, accounts или services.

## Контракт process runner

`BuildTools/gameplay_test_runner.py` выполняет checked JSON manifest без shell.
Command является массивом, а environment-specific values передаются как
`{name}` через повторяемый `--value name=value`.

Schema version 1 содержит:

- root: `name`, `default_timeout_seconds`, optional `forbidden_markers`, ordered
  `scenarios`;
- scenario: stable `id`, optional timeout/forbidden overrides, ordered `processes`;
- process: stable `id`, command array, optional working directory/environment,
  readiness, required/forbidden markers и expected exit code.

Unknown fields, duplicate ids/markers, неверные типы и unresolved placeholders
являются configuration errors. Environment расширяет inherited process env.
Secrets не помещаются в manifest, values, command, markers или report: command
line и process environment могут наблюдаться извне.

```json
{
  "schema_version": 1,
  "name": "project-smoke",
  "default_timeout_seconds": 60,
  "forbidden_markers": ["FATAL ERROR!", "ScriptException"],
  "scenarios": [
    {
      "id": "server-client",
      "processes": [
        {
          "id": "server",
          "command": ["{server}", "-ApplyConfig", "{config}"],
          "ready_marker": "project_server_ready",
          "ready_timeout_seconds": 20,
          "required_markers": ["project_server_ready", "project_server_passed"]
        },
        {
          "id": "client",
          "command": ["{client}", "-ApplyConfig", "{config}"],
          "required_markers": ["project_client_connected", "project_client_passed"]
        }
      ]
    }
  ]
}
```

Из корня embedding project:

```bash
python Engine/BuildTools/gameplay_test_runner.py \
  --manifest Tests/gameplay-smoke.json \
  --value server=Build/bin/Game_ServerHeadless \
  --value client=Build/bin/Game_ClientHeadless \
  --value config=Game.fomain \
  --report Workspace/gameplay-smoke-report.json
```

Пути и target names являются примерами; проект владеет generated executable
names и layout.

## Семантика readiness и markers

Processes запускаются по порядку manifest. При `ready_marker` следующий process
не стартует до marker или timeout; readiness budget ограничен общим scenario
deadline.

Runner объединяет stderr со stdout, декодирует UTF-8 с replacement, добавляет к
строке suite/scenario/process identity и проверяет:

- каждый `required_markers` появился в output этого process;
- ни один process/scenario/suite `forbidden_markers` не появился;
- `expected_exit_code`, по умолчанию 0, совпал точно.

Markers являются стабильными protocol tokens, а не prose. Печатайте их только
после существования asserted state и не переносите в них шумные diagnostics.
Marker доказывает достижение инструментированного состояния, но не
ненаблюдавшиеся pixels, sound, durability, package contents или external
service. Для них нужен владеющий decoder/query/screenshot/audible review/restart/
platform lane.

## Deadlines и cleanup

Scenario имеет один monotonic deadline: readiness и completion расходуют общий
budget. При сбое, timeout или incomplete startup runner идет по запущенным
processes в обратном порядке, вызывает terminate, ждет до пяти секунд и затем
kill-ит неостановившийся process.

Timeout является failure. Сначала выясните, не отсутствовал ли readiness,
dependency, shutdown progress или deterministic timing. Повышайте limit только
по измеренному valid work на supported CI hosts.

## Контракт результата

- `0`: все scenarios прошли;
- `1`: нарушен process/exit/marker/readiness/timeout contract;
- `2`: неверен CLI или manifest.

С `--report` записывается JSON schema version 1 со статусом и duration suite/
scenario, timeout, reasons, process exit codes и missing/forbidden markers.
Commands, environments и full logs намеренно исключены. CI сохраняет report и
prefixed process log вместе.

## Интеграция с CMake и CI

Gameplay smoke оформляется named custom target с dependencies на точные binaries
и baked artifacts. Передавайте target paths через generator expressions,
держите manifest в `SOURCES`, используйте `VERBATIM` и terminal для interleaved
output. Исполняемый образец находится в
[Minimal Multiplayer CMake](../../../../Examples/MinimalMultiplayer/CMakeLists.txt).

Synthetic runner tests запускаются при каждом изменении runner/schema. Хотя бы
один реальный headless server/client example нужен на каждой заявленной
платформе. Product CI добавляет свои script tests, content assertions, packages,
persistence backends, visible checks и release gates.

## Маршрутизация ошибок

| Симптом | Сначала проверить |
|---|---|
| Exit 2 | schema, unknown fields, placeholders и `KEY=VALUE` |
| Process не стартовал | executable/cwd path и host permissions |
| Нет readiness marker | earliest log, config, startup dependencies, marker owner |
| Нет required marker | ближайший assertion, затем remote/entity/content state |
| Forbidden marker | первое появление и native/script source; реальную ошибку не allowlist-ить |
| Exit mismatch после markers | shutdown, teardown callbacks, worker/database drain |
| Timeout | последний marker каждого process, deadlock/dependency/cleanup |
| Локально green, CI red | ports, case, locale, capacity, graphics/audio assumptions, undeclared state |

## Граница ответственности проекта

Движок не определяет script test registry игры, fixture catalog, gameplay
assertions, accounts/databases, filters, authored ids, ports, timing budgets,
package matrix, device lab или acceptance thresholds. Они остаются в проекте.

Project-specific orchestration является evidence, а не Engine API. Primitive
продвигается в движок только с Engine-owned implementation, deterministic tests,
independent guide и real public example.

AiControl/MCP adapter может дать semantic observations/actions внутри project
smoke, но не заменяет deadline, cleanup, marker и report contracts runner-а.
Endpoint schemas и gameplay tools остаются проектными; общий wire lifecycle
описан в [протоколе AiControl](../ai-control-protocol.md). Protocol-only Python
sample не доказывает, что native FOnline client выполнил command или game server
применил normal authority.

## Проверенные исходные пути

- `BuildTools/gameplay_test_runner.py`
- `BuildTools/tests/test_gameplay_test_runner.py`
- `Examples/GameplayTestHarness/fixture_process.py`
- `Examples/GameplayTestHarness/synthetic-smoke.json`
- `Examples/MinimalMultiplayer/tutorial-smoke.json`
- `Examples/MinimalMultiplayer/run_tutorial_smoke.py`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/Scripts/Tutorial.fos`
- `Source/Applications/TestingApp.cpp`
- `BuildTools/cmake/stages/Applications.cmake`
- `.github/workflows/validate.yml`
