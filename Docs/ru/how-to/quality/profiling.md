---
layout: default
title: Профилирование
locale: ru
document_id: profiling
permalink: /Docs/ru/how-to/quality/profiling.html
---

# Профилирование

<!-- docs-translation: {"document_id":"profiling","locale":"ru","source_path":"Docs/en/how-to/quality/profiling.md","source_sha256":"2cd4d95b9ba0ddae511a79356f337eaedae9f34e727b7d3213dd54f9c2920739"} -->

> Документация движка о переиспользуемой интеграции Tracy, границах захвата
> и сопоставимых измерениях производительности. Рабочие сцены, оркестрация
> процессов и критерии приемки конкретной игры принадлежат игровому проекту.

## Назначение

Используйте это руководство, когда клиент, сервер, инструмент или baker
работает медленно и timing capture полезнее debugger trace. Движок поставляет
инструментацию Tracy и отдельные конфигурации сборки. Игровой проект задает
топологию процессов и детерминированную нагрузку.

Главное правило: инструментируйте только измеряемый процесс. Профилируемый
клиент должен подключаться к обычному серверу, а профилируемый сервер должен
получать нагрузку от обычного клиента. Тогда каждому capture соответствуют
один endpoint Tracy, одна временная шкала процесса и одна граница атрибуции.

Regular counterpart участвует через обычное игровое соединение client/server.
Он не открывает endpoint Tracy, не владеет им и никогда не служит placeholder,
удерживающим Tracy port.

## Решение о захвате

1. Выбирайте `Profiling_OnDemand` для ограниченного ручного capture или
   `Profiling_Total`, когда требуется включить startup. В single-config generator
   выбирайте profiling configuration при configure, не ожидая, что последующий
   `--config` заменит её.
2. Запускайте один Profiled process и Regular counterpart, чтобы только
   измеряемый процесс владел default Tracy port и timeline. Regular counterpart
   не открывает и не владеет endpoint Tracy. Для client capture запустите
   Regular server и дождитесь readiness, отклоните stale instrumented process
   непосредственно перед запуском Profiled client, затем запустите этот client.
   Для server capture отклоните stale instrumented process непосредственно
   перед запуском Profiled server, дождитесь readiness и только затем нагружайте
   его через Regular client.
3. Фиксируйте revisions, configuration, workload, input, состояние map/data,
   duration и warm-up condition. Записывайте только один capture одновременно и
   повторяйте одинаковый случай не менее трёх раз перед сравнением
   репрезентативного результата.

Role-specific startup order является точным:

| Capture | Порядок запуска |
|---|---|
| Client | Запустите Regular server и дождитесь readiness; отклоните stale process на default Tracy port; запустите Profiled client; затем запустите `tracy-capture`. |
| Server | Отклоните stale process на default Tracy port; запустите Profiled server и дождитесь readiness; запустите Regular client workload driver; затем запустите `tracy-capture`. |

В обоих маршрутах stale-port check выполняется непосредственно перед Profiled
process. В client route не переносите его раньше readiness gate Regular server
или после Profiled process только потому, что сам capture tool запускается
позднее.

## Проверенные исходные пути

- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/ThirdParty.cmake`
- `Source/Essentials/BasicCore.h`
- `Source/Essentials/StackTrace.h`
- `Source/Essentials/Logging.cpp`
- `Source/Essentials/MemorySystem.cpp`
- `Source/Essentials/Threading.h`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Frontend/Application.cpp`
- `Source/Frontend/ApplicationHeadless.cpp`
- `Source/Client/Client.cpp`
- `Source/Server/Server.cpp`
- `Source/Scripting/AngelScript/AngelScriptContext.cpp`
- `Source/Applications/BakerLib.cpp`
- `ThirdParty/tracy/CMakeLists.txt`
- `ThirdParty/tracy/NEWS`
- `ThirdParty/tracy/public/common/TracyVersion.hpp`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/CMakePresets.json`
- `Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain`
- `Examples/MinimalMultiplayer/README.md`
- `Examples/MinimalMultiplayer/run_tutorial_smoke.py`
- upstream-исходники Tracy v0.13.1: [`capture.cpp`](https://github.com/wolfpld/tracy/blob/v0.13.1/capture/src/capture.cpp) и [`csvexport.cpp`](https://github.com/wolfpld/tracy/blob/v0.13.1/csvexport/src/csvexport.cpp)

## Что инструментирует движок

Конфигурации профилирования подключают `TracyClient` к слою essentials движка
и определяют `FO_TRACY=1`. В остальных конфигурациях задано `FO_TRACY=0`, и
макросы профилирования движка исключаются при компиляции.

| Сигнал | Текущее поведение движка |
|---|---|
| Идентификатор процесса | `ApplicationInit` передает Tracy значение `FO_NICE_NAME`. |
| Native CPU zones | `FO_STACK_TRACE_ENTRY()` превращается в `ZoneScoped`, именованная форма — в `ZoneScopedN`. |
| AngelScript CPU zones | Выполненные script-вызовы создают zones с исходными файлом, строкой и declaration. При продолжении suspended context восстанавливает стек script zones. |
| Frames | Видимый и headless-пути `Application::EndFrame()` создают `FrameMark`. |
| Client plot | `ClientEngine::MainLoop()` публикует `Client FPS`. |
| Server plot | Задача серверной статистики публикует `Server jobs per second`. |
| Сообщения журнала | Записи журнала движка также передаются как сообщения Tracy. |
| Потоки | Имена потоков движка видны в Tracy и в помеченных строках журнала. |
| Память | Выделения движка через учитывающий Tracy путь rpmalloc создают события allocation/free. |

Текущая first-party интеграция не добавляет renderer GPU zones или Tracy lock
wrappers. Поэтому CPU capture нельзя представлять как доказательство GPU timing
или lock contention. Для таких вопросов используйте профильные инструменты
renderer-а либо добавляйте узкую scoped-инструментацию.

Обычные выделения памяти C-библиотеки вне allocator wrappers движка не
атрибутируются heap движка автоматически. Перед трактовкой allocation capture
как полного учета памяти процесса прочитайте
[Essentials](../../reference/native/essentials.md#memory-pointers-and-lifetime-utilities).

## Конфигурации сборки

`BuildTools/cmake/stages/Init.cmake` добавляет четыре конфигурации:

| Конфигурация | Основа | Режим Tracy | Назначение |
|---|---|---|---|
| `Profiling_OnDemand` | `RelWithDebInfo` | по запросу | Обычные captures после запуска и прогрева. |
| `Profiling_Total` | `RelWithDebInfo` | непрерывный | Запуск, инициализация и первые frames. |
| `Debug_Profiling_OnDemand` | `Debug` | по запросу | Диагностика инструментации или debug-only поведения, но не performance baseline. |
| `Debug_Profiling_Total` | `Debug` | непрерывный | Диагностика debug-запуска с полной трассировкой. |

Режим on-demand определяет `TRACY_ON_DEMAND`, поэтому трассировка начинается
только после подключения profiler-а. Это основной выбор для воспроизводимых
steady-state измерений. Total mode записывает данные с запуска процесса и
уместен только тогда, когда запуск входит в исследуемый вопрос. Перед новым
total capture перезапускайте процесс.

Для multi-config generator выбирайте профиль параметром `cmake --build
--config`. Для single-config generator задайте profiling-конфигурацию через
`CMAKE_BUILD_TYPE` во время configure. Параметр `--config`, переданный
single-config сборке, не превратит уже настроенный release binary в Tracy
binary.

## Подготовка совместимых инструментов Tracy

Сейчас движок поставляет Tracy `0.13.1`. Checkout намеренно сокращен до
клиентской библиотеки в `ThirdParty/tracy`: upstream-исходников `capture`,
`csvexport` и GUI profiler в нем нет. Протокол захвата должен совпадать,
поэтому используйте [релиз Tracy v0.13.1](https://github.com/wolfpld/tracy/releases/tag/v0.13.1)
или собирайте инструменты из точно этого tag.

Документированные параметры захвата и экспорта проверены по
[`capture.cpp`](https://github.com/wolfpld/tracy/blob/v0.13.1/capture/src/capture.cpp)
и [`csvexport.cpp`](https://github.com/wolfpld/tracy/blob/v0.13.1/csvexport/src/csvexport.cpp)
того же tag, а не перенесены из более новой локальной установки.

Следующие команды создают headless capture и CSV-export tools вне authored
source:

```bash
cmake -E make_directory Workspace/Tracy
git clone --depth 1 --branch v0.13.1 https://github.com/wolfpld/tracy.git Workspace/Tracy/src
cmake -S Workspace/Tracy/src/capture -B Workspace/Tracy/capture -DCMAKE_BUILD_TYPE=Release -DNO_FILESELECTOR=ON
cmake --build Workspace/Tracy/capture --config Release
cmake -S Workspace/Tracy/src/csvexport -B Workspace/Tracy/csvexport -DCMAKE_BUILD_TYPE=Release -DNO_FILESELECTOR=ON
cmake --build Workspace/Tracy/csvexport --config Release
```

`NO_FILESELECTOR=ON` не добавляет зависимости GUI file dialog к двум headless
command-line tools. Каталог результата различается у single-config и
multi-config generators; после сборки найдите `tracy-capture` и
`tracy-csvexport` и добавьте их в `PATH`. Для просмотра сохраненных `.tracy`
используйте GUI profiler того же релиза.

Не подменяйте версию инструмента молча. Tracy отклоняет несовместимый
handshake, а способность инструмента открыть старый файл еще не доказывает
совместимость его live capture protocol с инструментированным процессом.

## Выбор одной границы измерения

| Вопрос | Профилируемый процесс | Обычная парная сторона |
|---|---|---|
| Rendering, visibility, UI, input, client scripts или client networking | desktop client | headless или desktop server |
| Authority, AI, simulation, persistence, server scripts или server networking | headless или desktop server | standalone client workload driver |
| Пропускная способность resource baking | baker application | runtime-пара не требуется |
| Запуск и первый frame | соответствующий процесс в `Profiling_Total` | только минимальные зависимости для запуска |

Не профилируйте embedded server и client в одном процессе, когда вопрос
требует раздельной атрибуции client/server. Не собирайте оба standalone
процесса с Tracy на порту по умолчанию, чтобы затем гадать, какой из них принял
подключение capture.

Сборку и baking выполняйте до окна измерения. Неожиданный in-process rebake,
shader warm-up, заполнение cache или updater operation является частью capture
только тогда, когда именно эта операция объявлена нагрузкой.

## Сборка профилируемого примера

Минимальный multiplayer-проект движка дает командам конкретные target names и
не зависит от закрытой игры. В Windows:

```powershell
Set-Location Examples\MinimalMultiplayer
cmake --preset windows
cmake --build Build\windows --config RelWithDebInfo --target BakeResources FOMM_ServerHeadless
cmake --build Build\windows --config Profiling_OnDemand --target FOMM_Client
```

В Linux:

```bash
cd Examples/MinimalMultiplayer
cmake --preset linux
cmake --build Build/linux --config RelWithDebInfo --target BakeResources FOMM_ServerHeadless
cmake --build Build/linux --config Profiling_OnDemand --target FOMM_Client
```

Так создаются профилируемый клиент и обычный сервер. Для профилирования сервера
поменяйте конфигурации местами:

```bash
cmake --build Build/linux --config RelWithDebInfo --target BakeResources FOMM_Client
cmake --build Build/linux --config Profiling_OnDemand --target FOMM_ServerHeadless
```

Имена project targets выводятся из `FO_DEV_NAME`; имена `FOMM_*` действуют
только для этого примера движка.

## Захват клиента

Используйте отдельные терминалы, а runtime working directory задайте как
`Examples/MinimalMultiplayer/Build/windows` в соответствии с generated target
и smoke-test contract. Сначала запустите обычный сервер:

```powershell
Set-Location Examples\MinimalMultiplayer\Build\windows
.\Binaries\Server-Windows-win64\FOMM_ServerHeadless.exe -ApplyConfig ..\..\FOnlineMinimalMultiplayer.fomain
```

Запустите профилируемый клиент:

```powershell
Set-Location Examples\MinimalMultiplayer\Build\windows
.\Binaries\Client-Windows-win64-Profiling_OnDemand\FOMM_Client.exe -ApplyConfig ..\..\FOnlineMinimalMultiplayer.fomain
```

Из корня example project доведите нагрузку до стабильного состояния и
захватите фиксированное окно:

```powershell
New-Item -ItemType Directory -Force -Path Workspace\Profiling | Out-Null
tracy-capture -o Workspace\Profiling\client.tracy -f -s 30 -m 80 -p 8086
```

В Tracy 0.13.1 параметр `-f` разрешает заменить указанный output, `-s 30`
задает 30 секунд захвата, `-m 80` ограничивает память capture 80 процентами
физической памяти, а `-p 8086` выбирает стандартный порт Tracy. Сохраняйте
stdout capture вместе с результатом: там записаны frame count, time span, zone
count и размер trace.

В Linux действует та же схема каталогов: `Binaries/Server-Linux-x64/` для
обычного сервера и `Binaries/Client-Linux-x64-Profiling_OnDemand/` для
профилируемого клиента.

## Захват сервера

Соберите `FOMM_ServerHeadless` в `Profiling_OnDemand`, а `FOMM_Client` — в
`RelWithDebInfo`. Запустите профилируемый сервер, подключите обычный клиент,
дождитесь стабильности выбранной нагрузки и выполните ту же команду
`tracy-capture`.

Профилируемый Windows server создается в
`Binaries/Server-Windows-win64-Profiling_OnDemand/`, а обычный клиент остается
в `Binaries/Client-Windows-win64/`.

Для startup capture соберите target как `Profiling_Total`, запустите
`tracy-capture` перед процессом и сохраните полный startup log. Не сравнивайте
startup trace из total mode со steady-state trace из on-demand mode.

## Проектирование воспроизводимой нагрузки

Результату производительности нужен контракт нагрузки, а не только файл
`.tracy`. Записывайте:

- точные ревизии движка и игры, состояние dirty worktree, target и profile;
- compiler, host CPU, operating system, renderer, driver, resolution и frame cap;
- идентификатор map/scene или server workload, ревизию контента, seed, число
  actors/clients и подготовку account/database;
- условие warm-up, длительность capture, число frames или server jobs и всю
  input automation;
- остальные процессы, host CPU load, thermal/power mode и каждую отброшенную
  попытку;
- raw capture, stdout захвата, runtime logs, экспортированные таблицы и
  интерпретацию.

Предпочитайте script-сцену или workload driver, принадлежащие проекту. Они
должны начинать с известного состояния, исключать посторонний пользовательский
input, объявлять готовность в журнале, повторять одинаковые действия и чисто
завершаться. Не включайте в профиль visual debug overlays, admin panels,
verbose diagnostics и постороннюю telemetry, если они не являются предметом
измерения.

Выполняйте по одному capture. Проверяйте, что порт Tracy свободен и устаревший
инструментированный процесс не может принять подключение. Дождитесь idle host,
отклоняйте captures с неожиданным числом frames/jobs и сохраняйте не менее
трех сопоставимых попыток для заявления об оптимизации. Публикуйте выбранную
статистику и правило отбора, а не только самый быстрый trace.

## Анализ capture

Начните с timeline и call tree:

1. Подтвердите program name, процесс, build configuration, длительность
   capture и workload marker.
2. Убедитесь, что число frames или server jobs сопоставимо с baseline.
3. Найдите long frames, scheduling gaps и доминирующие потоки до сортировки
   функций.
4. Сравните inclusive time с self time: крупная parent zone может лишь владеть
   дорогими дочерними zones.
5. Рассматривайте AngelScript zones на одной timeline с вызывающим native-кодом.
6. Сопоставьте сообщения журнала, FPS/job plots, allocations и source locations
   с временным окном.
7. Меняйте одну переменную, повторяйте ту же нагрузку и сохраняйте оба raw
   captures.

Совместимый CSV exporter может создать таблицу self time с привязкой к
исходникам:

```bash
tracy-csvexport -e --truncated_mean=95 Workspace/Profiling/client.tracy > Workspace/Profiling/hotspots-self.csv
```

`-e` выбирает self time. 95-percent truncated mean уменьшает влияние самого
медленного хвоста и сохраняет отдельную percentile column; он не заменяет
изучение long frames или tail latency. Для baseline и candidate captures
используйте одинаковые параметры экспорта.

Client captures показывают plot `Client FPS`. Server captures показывают
`Server jobs per second`; текущий event-driven server больше не публикует
удаленные метрики loop time/loops per second. Текущая граница серверной
статистики описана в
[Server Runtime](../../explanation/runtime/server.md#инициализация-и-серверные-задания).

## Добавление узкой инструментации

Сначала используйте существующие zones. Большинство native-функций движка уже
вызывают `FO_STACK_TRACE_ENTRY()`, а AngelScript calls публикуются
автоматически. Если широкой функции нужна стабильная semantic label,
используйте `FO_STACK_TRACE_ENTRY_NAMED` в ее владеющем native scope. Прямые
макросы `TracyPlot`, allocation и message ограждайте `#if FO_TRACY`.

Инструментация должна:

- исключаться при компиляции non-profiling configurations;
- использовать стабильные имена с низкой cardinality;
- не содержать secrets, account data, chat text или неограниченные content IDs;
- не менять ownership, locking, allocation или scheduling semantics;
- иметь узкий маршрут проверки и обновлять это руководство, если меняется
  публичная интерпретация capture.

Не добавляйте zone только ради большей детализации отчета. Каждая zone должна
отвечать на конкретный вопрос производительности и иметь владельца, способного
ее интерпретировать.

## Частые ошибки

| Симптом | Что проверить |
|---|---|
| Capture tool не подключается | Убедитесь, что процесс собран в одной из четырех profiling-конфигураций, порт верен и его не занял firewall или устаревший процесс. |
| Несовпадение протокола | Используйте инструменты точной версии из `TracyVersion.hpp`. |
| Trace начинается слишком поздно | Используйте `Profiling_Total`, только если startup является целевой нагрузкой; иначе добавьте явный readiness marker и warm-up. |
| Почти пустой trace | Убедитесь, что capture подключился к нужному процессу и нагрузка продолжалась все окно. |
| Client и server zones смешаны или неоднозначны | Профилируйте один standalone-процесс, а парную сторону пересоберите как `RelWithDebInfo`. |
| В профиле преобладает baking или создание cache | Выполните baking и warm-up заранее либо назовите нагрузку измерением startup/baking. |
| Debug build значительно медленнее | Для performance comparisons используйте `Profiling_OnDemand` или `Profiling_Total`; debug profiles предназначены для диагностики. |
| Результат headless client используется как renderer evidence | Null/headless renderer не проверяет производительность видимого rendering. |
| В Linux capture преобладает software rendering | Запишите renderer и driver; не сравнивайте captures программного и аппаратного renderer-а. |
| Allocation totals кажутся неполными | Проверьте third-party/plain C allocations вне границы allocator-а движка. |

## Автоматизация на стороне проекта

Встраивающий проект должен автоматизировать повторяемые части, не меняя
контракт движка. Production runner должен:

- выбирать объявленный workload ID и точную измеряемую сторону client/server;
- собирать с Tracy только измеряемый процесс;
- заранее выполнять baking и staging binaries/resources;
- сериализовать доступ к runtime logs и порту Tracy;
- ждать готовности процесса и нагрузки;
- проверять quiet host и минимальный объем работы;
- запускать `tracy-capture`, экспортировать стабильную таблицу и копировать все
  logs;
- сохранять provenance и отклонять шумные или неполные попытки.

Scene IDs, config names, executable names, ожидаемые FPS/job thresholds,
database fixtures, renderer policy и report destinations принадлежат проекту.
Их нельзя переносить в это руководство движка как значения по умолчанию.

## Порядок проверки

После изменения profiling configurations, инструментации или этого руководства
выполните:

```bash
python BuildTools/tests/test_docs_profiling.py
python BuildTools/docs_snippets.py --check --external
python BuildTools/docs_validate.py
```

Затем соберите один on-demand target и убедитесь, что короткий capture содержит:

- ожидаемые program name и процесс;
- native и AngelScript zones;
- frame marks;
- соответствующий client/server plot;
- сообщения журнала и именованные потоки;
- ненулевое число frames/jobs и читаемый сохраненный trace.

Изменения памяти также требуют узкого allocation capture. Изменения script
context требуют capture, который выполняет, приостанавливает, продолжает и
завершает цепочку AngelScript calls. Изменения frontend требуют проверки и
видимого, и headless-пути frame marks, пока поддерживаются оба.

## Сопровождение

Обновляйте эту страницу в том же изменении, когда:

- меняются имена profiling configurations или их base configurations;
- меняется wiring `FO_TRACY`, `TRACY_ENABLE` или `TRACY_ON_DEMAND`;
- меняется версия Tracy или состав сокращенного vendored payload;
- меняются frame marks, имена program/thread, logs, plots, allocation tracking
  или AngelScript zones;
- появляется first-party граница GPU/lock instrumentation;
- меняется target/config/output layout примера движка.

При обновлении Tracy проверяйте клиент и совместимые upstream capture/export
tools вместе. До изменения документированной версии или утверждения о
протоколе повторите по одному client и server capture.

## См. также

- [Тестирование](../../contributing/testing/) — выбор test boundary, sanitizers и coverage.
- [Нативная отладка и отладка AngelScript](../../troubleshooting/debugging.md) для native и AngelScript debugger workflows.
- [Процесс сборки](../build/) — ownership сборки встраивающего проекта.
- [Frontend и рендеринг](../../explanation/rendering/) — границы renderer/runtime.
- [Server Runtime](../../explanation/runtime/server.md) — текущая диагностика job throughput.
- [ThirdParty Maintenance](../../../ThirdPartyMaintenance.md) — обновление vendored version и pruning.
