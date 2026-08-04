---
layout: default
title: Нативная отладка и отладка AngelScript
locale: ru
document_id: debugging
permalink: /Docs/ru/troubleshooting/debugging.html
---

<!-- docs-translation: {"document_id":"debugging","locale":"ru","source_path":"Docs/en/troubleshooting/debugging.md","source_sha256":"f65f0031b9bad0f90f2ea01eeca496c948647b2e0896626160834f10f497b98c"} -->

# Нативная отладка и отладка AngelScript

Это принадлежащий Engine маршрут для диагностики нативных сбоев, смешанных нативных и скриптовых стеков, фатальных завершений процесса, просмотра данных в Visual Studio и живого выполнения AngelScript. Он следует текущим конфигурациям сборки, платформенным helper-функциям, реализации исключений и стеков, endpoint AngelScript, исходникам комплектного адаптера VS Code, тестам Engine и проверенным evidence встраивающих проектов.

Встраивающий проект отвечает за конкретные имена целей, пути к исполняемым файлам, рабочие каталоги, предварительный bake, sub-config, учётные данные, политику хранения crash-артефактов, установку редактора и сценарий воспроизведения игровой ошибки.

## Быстрый выбор маршрута

- Выбирайте native debugger для crashes, native exceptions, memory, threads или
  mixed stack, чей владеющий frame находится в C++.
- Выбирайте AngelScript debugger для live stepping и переменных скрипта. Текущий
  контракт Engine требует `Script.DebuggerEnabled`, предоставляет TCP endpoint
  на выбираемом для процесса порту из `43000..44999` и использует UDP-порт
  `43001` для discovery; проект владеет настройкой editor и политикой remote access.
- Выбирайте сфокусированный тест Engine или проекта, когда отказ детерминирован и
  изменённый контракт можно наблюдать без интерактивного attach.

Attach является диагностическим свидетельством. Сохраните исходное
воспроизведение и добавьте повторяемый regression route после исправления причины.

## Статус контракта

Страница описывает текущий переиспользуемый контракт Engine. Нормативны исходный код и собственные тесты Engine. Last Frontier и FOnline TLA служат только закреплёнными workflow evidence; их имена launch-профилей, префиксы бинарных файлов, порты сверх стандартных портов Engine, тестовые наборы и продуктовая политика не расширяют поддержку Engine.

У отладки четыре независимых слоя evidence:

1. воспроизводимый сбой и полный исходный лог;
2. соответствующие ему бинарный файл, runtime-библиотеки и нативные символы;
3. evidence живого подключения нативного отладчика или AngelScript к сбойному выполнению;
4. сфокусированный регрессионный тест или повторяемый проектный сценарий после диагностики.

Читаемый стек не доказывает, что исполняемый файл, символы и исходники получены из одной сборки. Успешное подключение не доказывает, что показанная команда отладчика реализована живым транспортом Engine.

## Область действия и ответственность

Engine отвечает за:

- семантику конфигураций сборки, флаги символов компилятора и linker, варианты sanitizer и генерируемые application targets;
- `IsRunInDebugger`, `BreakIntoDebugger`, захват и разрешение нативного стека, exception callbacks, crash handlers и диагностический self-test;
- смешанные слои стека AngelScript/native и текущий runtime endpoint отладчика;
- файлы MSVC Natvis/NatJMC, подключённые к сгенерированным solutions;
- исходники адаптера `BuildTools/angelscript-debugger` и объявленную им схему конфигурации VS Code;
- сфокусированные нативные тесты поведения стеков и исключений.

Встраивающий проект отвечает за:

- выбор запускаемых приложения, конфигурации, набора ресурсов, базы данных, аккаунта и игрового маршрута;
- `.vscode/launch.json`, зависимости от tasks, установку editor extension и именование нескольких процессов;
- сбор нативных dumps, хранение, приватность, загрузку, symbol store и incident policy;
- регрессионные тесты gameplay/scripts и квалификацию release-платформ.

У Web и Android есть дополнительные runtime-границы. После доказательства платформенной специфики симптома используйте [сборку, упаковку и отладку Web](../how-to/platforms/web-debugging.md) или [сборку, упаковку и отладку Android](../how-to/platforms/android-debugging.md).

## Проверенные пути исходников

Текущий контракт заново выведен из:

- `BuildTools/cmake/stages/Init.cmake`, `EngineSources.cmake` и `ThirdParty.cmake`;
- `BuildTools/cmake/helpers/Build.cmake` и `BuildTools/cmake/helpers/State.cmake`;
- `BuildTools/natvis/essentials.natvis`, `unordered_dense.natvis` и `fonline.natjmc`;
- визуализаторов GLM, ImGui, small-vector и ufbx в `ThirdParty/`;
- `Source/Essentials/BasicCore.cpp`, `StackTrace.*`, `ExceptionHandling.*`, `BaseLogging.*` и `Logging.cpp`;
- `Source/Common/DiagnosticSelfTest.cpp` и `Source/Frontend/ApplicationInit.cpp`;
- `Source/Scripting/AngelScript/AngelScriptBackend.cpp`, `AngelScriptContext.cpp` и `AngelScriptDebugger.*`;
- `Source/Common/Settings.inc`;
- `Source/Tests/Test_StackTrace.cpp` и `Test_ExceptionHandling.cpp`;
- `BuildTools/angelscript-debugger/package.json` и его TypeScript-исходников;
- точных снимков проектов в `BuildTools/ExternalProjectEvidence.json`.

## Слои evidence и матрица поддержки

| Поверхность | Текущая возможность Engine | Граница evidence |
|---|---|---|
| Windows native | Application targets MSVC/clang-cl, PDB во всех конфигурациях кроме `MinSizeRel`, обнаружение отладчика, `DebugBreak`, диагностика SEH через backward-cpp, визуализаторы сгенерированного MSVC-проекта | Engine не создаёт и не хранит minidump-файлы и не обслуживает symbol server. |
| Linux native | Debug information во всех конфигурациях кроме `MinSizeRel`, `-rdynamic`, бинарные файлы для GDB/LLDB, обнаружение отладчика через `/proc/self/status`, диагностика signals/terminate | Включение и сбор core dump, хранение символов, container permissions и retention относятся к политике host/project. |
| macOS native | Debug information во всех конфигурациях кроме `MinSizeRel`, `-rdynamic`, обнаружение через `sysctl(P_TRACED)`, debug trap, signal-диагностика backward-cpp | Репозиторий не поставляет проверенный Engine-профиль LLDB, архив crash reports или release-квалификацию. |
| AngelScript runtime | TCP endpoint с loopback по умолчанию, UDP discovery, line breakpoints, pause/continue/step, скриптовый стек, read-only locals, события stop/abort/error | Нет контракта authentication, encryption, опубликованного VSIX, закреплённого dependency lock, CI живого endpoint, просмотра globals, evaluation выражений или изменения состояния. |
| Смешанный стек в логах | Скриптовые слои и нативные кадры, различение origin/catch, безопасный crash output и локальный для процесса cache разрешения | Качество нативных символов зависит от точных binary, libraries, debug data, platform unwinder и режима выполнения. MemorySanitizer отключает захват нативного стека. |

`Source/Tests` проверяет примитивы стека и исключений. Сейчас он не выполняет реальную TCP/UDP-сессию подключения AngelScript. Статические проверки и launch-профили проекта доказывают форму интеграции, но не живой протокол end to end.

## Быстрый выбор маршрута

| Семейство симптомов | С чего начать | Граница доказательства |
|---|---|---|
| Native assertion, C++ exception, signal, SEH failure или lifecycle invariant | Соответствующие нативные символы, исходный лог, затем минимальная нативная цель под отладчиком | Сфокусированный случай `Source/Tests/Test_*.cpp`, если граница переиспользуема. |
| Ошибка компиляции, binding, remote call или nullability в скрипте | [Scripting Runtime](../explanation/scripting-runtime/) и [Testing](../contributing/testing/) до живого подключения | Минимальная compile/bake fixture или owning test; attach нужен только для вопросов о состоянии выполнения. |
| Breakpoint, stepping, script stack или local value AngelScript | Development-конфигурация с `Script.DebuggerEnabled = True`, затем профиль подключения `fos` | Проверенная остановка в нужном процессе и нужной ревизии исходников. |
| Смешанное исключение script/native | Сначала unified trace в логе Engine, затем нативный отладчик | Сохранить origin throw и catch site; изолировать переиспользуемую границу нативным тестом. |
| Memory corruption, race, uninitialized read или undefined behavior | Узкая поддерживаемая sanitizer-конфигурация до ручного просмотра watch window | Reproducer в owning sanitizer lane; evidence отладчика дополняет её. |
| Ошибка загрузки client host/runtime | [Разделение client runtime и updater](../explanation/runtime/client-updater.md) | Тесты ABI и selector host/runtime до диагностики gameplay. |
| Ошибка браузера или Android | Платформенная инструкция после исключения общего native/script-поведения | Browser/device evidence точного пакета. |

## Конфигурации сборки и символы

`BuildTools/cmake/stages/Init.cmake` задаёт переиспользуемый контракт конфигураций. `expr_DebugInfo` истинен для каждой нативной конфигурации, кроме `MinSizeRel`:

- MSVC-совместимые сборки добавляют `/Zi` и линкуются с `/DEBUG:FULL`, когда включена debug information;
- Linux и macOS используют `AddNativeOptimizationFlags`, добавляющий `-g` при том же условии;
- Linux и macOS добавляют `-rdynamic`, чтобы символы executable были доступны runtime resolver;
- MSVC `Debug` и `RelWithDebInfo` также получают `/JMC`;
- доступный только для MSVC `Release_Debugging` наследуется от `RelWithDebInfo` и добавляет `/dynamicdeopt` и `/DYNAMICDEOPT`.

Не используйте `MinSizeRel` для диагностики, требующей нативных кадров на уровне исходников. Не смешивайте PDB, dSYM/DWARF, executable, client runtime library или native extension из разных сборок, даже если имена и метки commit выглядят одинаково.

### Символы отладки не означают debug-семантику

`FO_DEBUG=1`, `DEBUG` и `_DEBUG` выдаются только для `Debug`, `Debug_Profiling_Total`, `Debug_Profiling_OnDemand` и `Debug_San_Address`. Другие конфигурации получают `NDEBUG` и `FO_DEBUG=0`, хотя большинство из них содержит debug information.

Это различие существенно:

- `RelWithDebInfo` обычно является лучшим первым воспроизведением release-подобного поведения с символами;
- `Debug` меняет assertions, выбор CRT, оптимизацию и timing и может скрыть или проявить другой сбой;
- `Release_Ext` является маршрутом полной оптимизации/LTO и всё равно содержит нативную debug information, но stepping и просмотр locals могут ухудшиться;
- `Release_Debugging` является MSVC-специфичным маршрутом dynamic deoptimization, а не кроссплатформенным именем конфигурации.

### Windows

Используйте Visual Studio или профиль `cppvsdbg` с точными сгенерированными executable, соседними runtime libraries, native extensions и PDB. Оставляйте рабочий каталог в корне встраивающего проекта, если его сгенерированная конфигурация прямо не требует другого. Останавливайтесь на thrown C++ exceptions только тогда, когда само исключение неожиданно; ожидаемые throw-as-signal пути лучше диагностировать в reporter или на границе invariant.

Сгенерированные MSVC-проекты автоматически включают Natvis и NatJMC Engine. Скопированный executable без соответствующих PDB и библиотек не является полным диагностическим артефактом.

### Linux

Используйте GDB или LLDB с точными executable и shared objects. Сохраняйте исходные environment, рабочий каталог, config, resource paths и выбор allocator/sanitizer. Engine добавляет `-rdynamic`; большинство обычных executable routes используют non-PIE, а baker/client-library и цели, связанные с MemorySanitizer, имеют другие relocation requirements.

Если crash произошёл вне отладчика, сохраните лог Engine до повторного запуска. OS core является дополнительным evidence только тогда, когда host был настроен создавать и сохранять его.

### macOS

Используйте LLDB с соответствующими executable, libraries и debug data. `IsRunInDebugger` проверяет `P_TRACED` через `sysctl`, а `BreakIntoDebugger` использует `__builtin_debugtrap`. Исходники Engine поддерживают диагностику нативных символов и стеков, но репозиторий сейчас не заявляет проверенный editor profile macOS или lane crash artifacts.

### Ограничения sanitizer и платформ

Точную матрицу sanitizer смотрите в [Testing](../contributing/testing/). Для отладки важны следующие взаимодействия:

- MSVC предоставляет `San_Address` и `Debug_San_Address`;
- native Clang предоставляет Address, Memory, Memory-with-origins, Undefined, Thread, DataFlow и Address+Undefined там, где это поддерживает toolchain;
- AddressSanitizer и MemorySanitizer переключают AngelScript на `AS_MAX_PORTABILITY`, чтобы native call trampolines не обходили instrumentation;
- сборки MemorySanitizer компилируют слой stack/exception с `HAS_NATIVE_TRACE=0`; ожидайте diagnostics sanitizer, а не обычный нативный mixed-stack контракт;
- timing, allocation, размер stack и calling convention sanitizer отличаются от release-сборки, поэтому воспроизводите также исходную конфигурацию.

## Нативная отладка

### Запуск, подключение и воспроизведение

1. Запишите точные ревизии Engine и встраивающего проекта, target, configuration, config/sub-config, command line, рабочий каталог и ревизию ресурсов.
2. Сохраните первый сбойный лог и любую OS-диагностику до добавления логирования или смены build mode.
3. Воспроизведите в `RelWithDebInfo` с соответствующими символами, если предметом ошибки не является debug-only семантика.
4. Запускайте под нативным отладчиком, когда важно debugger-aware поведение Engine. Поздний attach позволяет наблюдать процесс, но не обновляет закэшированное Engine решение о присутствии отладчика.
5. Остановитесь на узком invariant, throw site, sanitizer report или faulting instruction. Исследуйте полный набор threads, а не только выбранный frame.
6. Сведите сбой к минимальному тесту Engine или проектному сценарию, который его сохраняет.
7. После исправления повторите исходную конфигурацию; успех только в Debug не является release-like приёмкой.

### Исключения, assertions и ошибки памяти

`ReportExceptionAndContinue` записывает пойманное нефатальное исключение. `ReportExceptionAndExit` и strong assertions записывают диагностику и завершают процесс или передают управление отладчику согласно контракту. Модель уровней exception safety и правила entity-lifecycle throw-as-signal описаны в [Exception Safety](../contributing/coding-contracts/exception-safety.md).

Используйте break-on-throw осмотрительно. AngelScript bindings и lifecycle-код движка могут бросать исключения как часть намеренного reporting path. Начинайте с фиксированного сообщения и context parameters из лога, затем ставьте сфокусированный breakpoint в owning invariant или reporter. Для повреждения памяти приоритетны evidence ASan/MSan/UBSan/TSan и первый некорректный доступ, а не более поздний вторичный assertion.

### Граница core и minidump

Engine записывает crash diagnostics в свой лог. Сейчас он не создаёт Windows minidumps, не настраивает Linux core limits, не собирает macOS crash reports, не загружает dumps и не управляет symbol store.

Встраивающий проект или оператор может добавить эти возможности, но обязан определить:

- точное происхождение executable/library/symbol;
- включение dump и место хранения;
- retention, access control, encryption и deletion;
- обращение с credentials, player data, chat, network buffers и находящимися в памяти secrets;
- поведение при ошибке upload и владельца incident;
- процедуру restore/replay, не требующую production credentials.

Не описывайте стандартный crash reporter платформы как гарантию Engine.

## Обнаружение отладчика и переход в отладчик

`IsRunInDebugger()` кэширует результат при первом вызове в процессе:

- Windows использует `IsDebuggerPresent()`;
- Linux читает `TracerPid` из `/proc/self/status`;
- macOS запрашивает `KERN_PROC_PID` и проверяет `P_TRACED`.

`BreakIntoDebugger()` выполняет `DebugBreak`, `__builtin_debugtrap` или `SIGTRAP`, только если закэшированный результат истинен. Поскольку exception handling задаёт этот вопрос во время ранней инициализации процесса, запуск вне нативного отладчика с последующим attach не гарантирует активацию Engine-triggered breaks.

Когда отладчик обнаружен при запуске, Engine не устанавливает обработку fatal signals/SEH через backward-cpp. Это позволяет нативному отладчику получить fault напрямую, но означает, что обычный out-of-debugger fatal crash-to-log path не является ожидаемым evidence такого запуска. Сохраните отдельный запуск без отладчика, если проверяется сам crash-log контракт.

Отладчик AngelScript не зависит от `IsRunInDebugger`; подключение адаптера `fos` не делает процесс осведомлённым о нативном отладчике.

## Визуализаторы Visual Studio

Сгенерированные MSVC solutions подключают эти визуализаторы без ручного шага установки в Visual Studio:

- `BuildTools/natvis/essentials.natvis`: Engine borrow/owner pointers, `propagate_const`, stack data, engine exceptions, hashed strings, colors, positions и time values;
- `BuildTools/natvis/unordered_dense.natvis`: таблицы и segmented vectors `ankerl::unordered_dense`;
- `BuildTools/natvis/fonline.natjmc`: классификация Engine для Just My Code;
- vendored visualizers для GLM, ImGui, `gch::small_vector` и ufbx.

`BuildTools/cmake/stages/EngineSources.cmake` подключает визуализаторы Engine, а `ThirdParty.cmake` подключает поддерживаемые визуализаторы зависимостей только к MSVC-generated projects. Natvis улучшает просмотр данных, но не меняет lifetime объектов, validity указателей или поведение optimizer.

## Папки solution в Visual Studio

Для MSVC CMake generators target следует создавать, пока активно предназначенное ему значение `CMAKE_FOLDER`. Repository helpers и финальный regrouping pass помещают application, command, core-library и third-party targets в папки сгенерированного solution. Положение папки влияет только на навигацию, но не на symbols или linkage.

## Быстрая проверка

1. Соберите узкую нативную цель не в `MinSizeRel` и подтвердите наличие соответствующего symbol artifact.
2. Запустите её под нативным отладчиком из корня встраивающего проекта.
3. Исследуйте Engine pointer и `StackTraceData`; в MSVC подтвердите загрузку нужного визуализатора.
4. Вызовите контролируемый exception/assertion path или остановитесь в нём и сравните позицию отладчика с логом Engine.
5. Выполняйте отдельный out-of-debugger diagnostic self-test только в изолированном workspace, когда нужно доказать сам маршрут crash-log.

## Архитектура стека

Engine захватывает ограниченный массив нативных return addresses и необязательные заранее разрешённые скриптовые слои в `StackTraceData`. Разрешение нативных символов откладывается до форматирования или явного resolve. Разрешённые нативные кадры кэшируются для всего процесса по instruction address в ограниченном cache, чтобы повторные reports не загружали одинаковую symbol information заново.

`FO_STACK_TRACE_ENTRY()` не является ручным thread-local call stack. Вне конфигураций Tracy он не добавляет stack frame; с Tracy он раскрывается в profiling zone. Нативные call stacks получаются платформенным захватом в момент вызова `GetStackTrace()`.

### Мост AngelScript

`AngelScriptContext.cpp` регистрирует provider скриптового стека, не создавая зависимости Essentials от заголовков AngelScript. Provider проходит активный context и цепочку parent contexts, разрешает declaration каждой функции и исходный `.fos` file/line через preprocessor translator и сохраняет native birth anchors, используемые для вставки вложенного script re-entry в нативный стек.

Скриптовые кадры захватываются сразу, потому что AngelScript context может быть переиспользован или изменён после capture. Захваченные слои находятся в immutable shared storage, поэтому копирование Engine exception остаётся noexcept.

### Порядок объединённых кадров

Форматированный trace идёт от самого нового кадра и может чередовать нативные bridges с вложенными скриптовыми слоями:

```text
[Native] code below the active script/native bridge
[Script] active child context
[Native] bridge between child and parent contexts
[Script] parent context
[Native] caller and process entry
```

Простые traces без native birth anchors помещают скриптовые кадры перед нативным tail. `FormatStackTrace` маркирует каждый frame как `[Script]` или `[Native]`; безопасный crash output использует hexadecimal addresses, если полное разрешение недоступно.

### Поверхность API

| Функция | Назначение |
|---|---|
| `GetStackTrace()` | Захватить нативные адреса и доступные сейчас скриптовые слои. |
| `GetStackTraceEntry(deep)` | Разрешить один объединённый frame по zero-based depth. |
| `ResolveStackTrace(st)` | Разрешить и объединить все захваченные кадры. |
| `FormatStackTrace(st)` | Создать читаемый смешанный trace. |
| `SafeWriteStackTrace(st)` | Записать через low-allocation crash/log path с fallback на адреса. |
| `ClearResolvedStackTraceCache()` | Очистить process-wide разрешённые нативные entries. |
| `GetResolvedStackTraceCacheSize()` | Получить текущий размер cache для тестов и диагностики. |
| `SetScriptStackTraceProvider(provider)` | Установить или удалить provider скриптового слоя. |
| `HasScriptStackTraceProvider()` | Проверить регистрацию provider в тестах. |

`BaseEngineException` захватывает origin trace при создании. Поэтому последующий catch/report сохраняет throw site, а не заменяет его только стеком reporter.

### Reporting исключений и отложенное форматирование

`MakeErrorStackTrace()` создаёт `CatchedStackTraceData`: необязательный origin из `BaseEngineException` и новый trace catch site. Форматирование использует origin при его наличии и отмечает catch location; у исключений не из Engine есть только trace catch site.

Exception callback получает message, уже захваченный `CatchedStackTraceData` и fatal flag. Интеграции, пересылающие диагностику, должны разрешать или копировать данные, пока известно их происхождение, и сохранять identity script/native кадров.

### Примитивы logging и crash path

Обычные exception callbacks используют structured logging path. Следующие подряд одинаковые сообщения исключений сворачиваются в отложенный count. Fatal и low-memory paths используют synchronous base logging и `SafeWriteStackTrace`; если formatting или symbol resolution завершается ошибкой, raw addresses сохраняются вместо удаления report.

`Common.AsyncLogWrite` управляет обычной асинхронной доставкой log. Fatal crash output приостанавливает её и выполняет синхронный flush, чтобы headless process не зависел от `stderr` или незавершённого writer thread.

### Гарантия crash-to-log и self-test

Вне нативного отладчика backward-cpp обрабатывает поддерживаемые Windows SEH failures и POSIX fatal signals/termination на Windows, Linux и macOS. Engine добавляет причину crash, захватывает stack, переключается на synchronous log writes и завершает процесс по crash path. Долгоживущие Engine worker threads устанавливают POSIX alternate signal stack, чтобы диагностике stack overflow хватило места. Threads, созданным сторонними библиотеками, требуется такая же настройка до выполнения глубоко рекурсивной работы Engine.

`FO_SELFTEST_CRASH` является destructive diagnostic hook, задаваемым только через environment и запускаемым при инициализации приложения после готовности logging и exception callbacks. Поддержаны базовые режимы `main_null_read`, `main_null_write`, `main_wild_write`, `main_stack_overflow`, `main_fpe`, `main_abort`, `main_noexcept_throw`, `main_throw` и `main_strong_assert`; замените `main_` на `thread_` для соответствующего worker-style thread route.

Запускайте его только для изолированного одноразового процесса и workspace. Он намеренно приводит процесс к crash или termination. Неизвестный режим записывает warning и продолжает работу. Сам репозиторий Engine не предоставляет subprocess acceptance runner; проверенное evidence Last Frontier исполняет Linux headless route, но этот проектный тест не является нормативным доказательством Engine.

### Покрытие

`Source/Tests/Test_StackTrace.cpp` покрывает регистрацию provider, порядок script layers, вложенное native/script interleaving, truncation, formatting, reuse/eviction cache, поиск отдельного entry, safe writing и containment бросающего provider. `Test_ExceptionHandling.cpp` покрывает payload Engine exceptions, поведение origin/catch, замену callback и inputs fatal/non-fatal reporter.

Текущий набор Engine не открывает TCP/UDP endpoint AngelScript, не подключает адаптер VS Code, не проверяет Natvis в Visual Studio и не запускает каждый crash mode как subprocess. Это явные integration gaps, а не неявное следствие успешных native unit tests.

## Отладчик AngelScript

### Включение и стоимость runtime

Устанавливайте `Script.DebuggerEnabled = True` только в development config или command-line override. Значение по умолчанию равно `False`. При включении `AngelScriptBackend` сохраняет line cues, отключает bytecode optimization, создаёт endpoint и устанавливает line callback в script contexts.

Это меняет характеристики сборки и выполнения скриптов и добавляет обработку каждой строки. Не включайте отладчик в production, benchmarks или acceptance runs, претендующих на обычную script performance. Compile-time define AngelScript `AS_DEBUG` следует нативным Debug-конфигурациям и не связан с runtime-setting `Script.DebuggerEnabled`.

### Контракт endpoint и discovery

Runtime:

- привязывает TCP к `Script.DebuggerBindHost`, стандартное значение Engine равно `127.0.0.1`;
- выбирает порт из `43000..44999`, начиная с `process_id % 2000`;
- объявляет newline-delimited JSON protocol версии `1`;
- отвечает на UDP probe `fos-debug-discover-v1` на порту `43001`;
- объявляет process id в виде `<pid>:<tcp-port>`, а target role как `server`, `client` или `mapper`;
- принимает одновременно одну активную TCP debug session.

Конфигурация VS Code attach принимает `processId`, прямой `endpoint` вида `tcp://127.0.0.1:43042`, `discoveryPort` со стандартным значением `43001` и `discoveryTimeoutMs` со стандартным значением `800`. Desktop discovery требует поддержки UDP в Node.js. Текущий endpoint Engine работает только через TCP, хотя parser адаптера также распознаёт строки endpoint для pipe и Unix socket, предназначенные другим transports.

### Матрица возможностей attach

| Действие VS Code | Состояние live Engine attach | Примечания |
|---|---|---|
| Найти и выбрать server, client или mapper | Поддерживается | При нескольких instances используйте объявленный `<pid>:<port>`. |
| Line breakpoint | Поддерживается | Engine индексирует breakpoints по basename исходного файла, поэтому одинаковые имена `.fos` неоднозначны. |
| Pause / continue | Поддерживается | Pause срабатывает на следующем line callback AngelScript, а не в период, когда скриптовая строка не выполняется. |
| Step in / over / out | Поддерживается | Работает с глубиной script context и исходными строками, разрешёнными preprocessor. |
| Script stack trace | Поддерживается в остановленном состоянии | Ответ attach содержит script frames; для объединённого нативного стека используйте лог Engine или native debugger. |
| Local variables | Read-only, поддерживаются в остановленном состоянии | Значения являются форматированными snapshots каждого script frame. |
| Script globals | Не реализованы | Scope Globals адаптера содержит attach metadata, а не живые globals AngelScript. |
| Hover/evaluate/expression | Не является live Engine-контрактом | Текущий attach mode может использовать adapter-local/mock поведение. Не считайте результат evidence процесса. |
| Set variable/expression, чтение/запись памяти, data/instruction/function breakpoints, reverse execution | Не реализованы live attach | Некоторые controls объявлены shared adapter, потому что их поддерживает его mock launch runtime; ошибки или placeholder behavior в attach mode не расширяют возможности Engine. |
| Stop по exception/abort/error | Поддерживается как runtime events | Полное исключение и mixed trace смотрите в логе Engine. |

Редактор исходников использует обычную one-based нумерацию строк; адаптер и endpoint преобразуют её во внутреннюю zero-based protocol line. Сейчас breakpoint verification подтверждает принятую line number, но не уникальность basename исходника и не достижимость строки в активном module.

### Граница безопасности

У debugger protocol нет authentication, authorization, confidentiality или integrity protection. Discovery также раскрывает роль процесса и attach endpoint. Сохраняйте `Script.DebuggerBindHost = 127.0.0.1`, если иной bind не разрешён явной временной проверкой trusted network.

Никогда не открывайте TCP `43000..44999` или UDP `43001` в публичный Internet, недоверенную LAN, production pod/service или shared CI runner. Для remote work оставляйте Engine на loopback, используйте принадлежащий оператору authenticated transport и настраивайте явный локальный endpoint. Не передавайте credentials в debugger config или log evidence.

### Состояние поставки адаптера

`BuildTools/angelscript-debugger` сейчас является пригодным для сборки из исходников инструментом, но не распространяемым production-продуктом редактора:

- `package.json` имеет `private`, версию `0.1.0` и scripts typecheck/build/package;
- в репозитории нет dependency lock адаптера, checked VSIX, записи marketplace publication или обязательного adapter build job;
- TypeScript test проверяет sample/mock runtime адаптера, а не живой endpoint Engine;
- transport attach требует desktop Node.js debug-adapter runtime.

Встраивающий проект может собрать и проверить локальный VSIX, но обязан владеть выбранными версиями Node/npm, разрешённым dependency lock, хэшем extension artifact, маршрутом install/upgrade и совместимостью editor. Пока Engine не добавит эти артефакты и live attach gate, нельзя называть установку адаптера воспроизводимой или release-qualified.

### Выбор нескольких процессов

Client, server и mapper используют общий UDP discovery port `43001` и выбирают разные TCP-порты в диапазоне на основе процесса. Предпочитайте process-specific selection вместо подключения к первому ответу. Для детерминированной автоматизации прочитайте строку `AngelScript debugger TCP endpoint` в runtime log и используйте explicit endpoint.

Используйте уникальные имена скриптов среди source roots, участвующих в отладке. Поскольку таблица breakpoints Engine использует только выделенное имя файла, пути наподобие `Scripts/Admin/State.fos` и `Scripts/Client/State.fos` нельзя независимо адресовать текущим transport.

### Устранение неполадок attach

1. Подтвердите, что выбранный процесс действительно получил `Script.DebuggerEnabled = True`; одно имя compound launch не включает endpoint.
2. Подтвердите наличие в логе строк TCP endpoint и UDP discovery port.
3. Проверьте, что bind остаётся loopback, если remote exposure не прошло явную проверку.
4. Если discovery ничего не находит, используйте записанный в логе прямой TCP endpoint и проверьте local firewall/extension-host UDP.
5. Если найдено несколько targets, осознанно выберите объявленные role и `<pid>:<port>`.
6. Подтвердите соответствие editor sources ревизии baked scripts, загруженной процессом.
7. Переименуйте повторяющиеся basenames `.fos`, прежде чем доверять line breakpoints.
8. Считайте отсутствие globals, mutation, memory, hover/evaluate и advanced DAP controls текущими ограничениями transport.
9. Если stepping меняет поведение, воспроизведите ещё раз с отключённым отладчиком, потому что line cues и bytecode optimization различаются.

## Интеграция отладчика во встраивающем проекте

Проекту следует предоставить независимые маршруты для:

- нативного запуска под отладчиком с точными сгенерированными executable и symbols;
- нативного attach, когда отладчик не может владеть запуском процесса, с документированным ограничением cached detection;
- attach AngelScript к уже запущенному development process;
- compound native launch и `fos` attach, когда нужны оба представления;
- Web/Android launch только для platform-specific симптомов;
- изолированного запуска unit tests и destructive crash-diagnostic subprocesses.

Оставляйте binary prefixes, paths, tasks, databases, accounts, ports и игровые sub-configs в project-owned files. Переиспользуемым требованием является контракт полей и проверки, а не конкретное имя `.vscode`.

## Checklist launch-профиля проекта

Поддерживаемый нативный профиль фиксирует:

- target, configuration, executable, runtime libraries, источник symbols и рабочий каталог;
- config/sub-config и каждый command-line override;
- предварительные configure/build/bake и могут ли они создать clean build tree;
- тип отладчика (`cppvsdbg`, GDB/LLDB через `cppdbg` или другой проверенный frontend);
- environment variables без secrets в исходниках и reports;
- поведение launch/attach и ограничение late attach;
- узкий сценарий, доказывающий профиль.

Поддерживаемый профиль AngelScript дополнительно фиксирует:

- как `Script.DebuggerEnabled = True` применяется к нужному процессу;
- политику loopback для `Script.DebuggerBindHost`;
- discovery port/timeout или выбор explicit endpoint;
- выбор multi-instance и политику duplicate filenames;
- версию adapter, происхождение dependencies/artifact и маршрут установки;
- поддержанные attach controls и live acceptance с breakpoint, stack и local value.

Статическая проверка должна отклонять отсутствующие ссылки task/compound, устаревшие setting names, non-loopback default и профили с `fos` attach без включения endpoint.

## Проверка тестами Engine

Для переиспользуемой нативной регрессии:

1. выберите или добавьте минимальный случай `Source/Tests/Test_*.cpp`;
2. соберите сгенерированный unit-test target встраивающего проекта с соответствующими symbols;
3. запустите точный Catch2 case, воспроизводящий сбой;
4. запустите более широкий Engine unit-test target, если менялись Essentials, scripting, threading или shared runtime behavior;
5. запустите подходящий sanitizer lane для дефектов memory/concurrency/undefined behavior;
6. после успешного теста повторите исходный application scenario.

Game scripts, content, bake commands, process names и gameplay fixtures принадлежат проекту. Проектный тест может доказывать compatibility, но не может быть единственным нормативным доказательством поведения Engine.

## Проверка host и runtime клиента

Нативные клиенты могут использовать небольшой host executable и соседнюю client runtime library. Диагностируйте загрузку host/runtime отдельно от gameplay:

1. соберите host и runtime из одной revision/configuration;
2. подтвердите, что ожидаемый runtime alias и соответствующие symbols находятся рядом с host;
3. запустите собранную пару;
4. проверьте явный совместимый `--ClientLibPath`;
5. проверьте несовместимый `--ClientLibCompatibilityVersion` и убедитесь в ошибке вместо тихой загрузки неверной библиотеки;
6. проверьте неверный alternate path и документированный embedded fallback;
7. после изменений ABI или selector запустите `Source/Tests/Test_ClientRuntimeApi.cpp`.

Package layout и rollout updater принадлежат [Packaging and Release](../how-to/release/packaging.md) и [разделению client runtime и updater](../explanation/runtime/client-updater.md).

## Project evidence и правила извлечения

`BuildTools/ExternalProjectEvidence.json` закрепляет оба снимка проектов. Текущее evidence показывает:

- Last Frontier хранит нативные launch-профили Windows/Linux, явный профиль `fos` attach, compounds с запуском через `--Script.DebuggerEnabled True`, loopback base bind и проверяемый static workflow validator. Его Linux pipeline также исполняет crash self-test modes Engine. Это сильные проектные практики, но они остаются project-owned.
- FOnline TLA независимо содержит нативные профили Windows/Linux и `fos` compounds. В закреплённой ревизии сами compounds не включают `Script.DebuggerEnabled`, а base config отключает отладчик и привязывает его к `0.0.0.0`. Это полезное negative compatibility evidence, а не рекомендуемый шаблон.

Переиспользуемые правила заново выведены из исходников Engine. Никогда не копируйте имена targets Last Frontier в документацию Engine, не продвигайте wildcard bind TLA и не выводите live attach coverage из статического launch file. Изменение ревизии проекта требует повторной проверки всех указанных файлов до обновления evidence decision.

## Устранение неполадок по слоям

| Наблюдение | Вероятный слой | Следующее действие |
|---|---|---|
| Breakpoints пустые и строк endpoint нет | Endpoint не включён или startup завершился ошибкой | Проверьте effective `Script.DebuggerEnabled`, затем startup logs и доступность портов. |
| Discovery пуст, но TCP endpoint записан | Проблема UDP/firewall/extension host | Подключитесь к точному записанному `tcp://127.0.0.1:<port>` endpoint. |
| Останавливается неверный client/server/mapper | Выбор multi-instance | Выберите объявленные role и `<pid>:<port>`; не автоматизируйте первый ответ. |
| Breakpoint срабатывает в другом одноимённом файле | Коллизия basename | Переименуйте один `.fos`; текущие Engine breakpoints индексируются по basename. |
| Globals или hover values выглядят искусственными | Возможность adapter превышает live attach transport | Используйте read-only locals, logs или native inspection; не считайте значение evidence Engine. |
| Нативные frames представлены только адресами | Нет или не совпадают symbols либо ограничен resolver | Сопоставьте binary/libraries/debug data и проверьте доступность platform unwinder. |
| Crash виден в отладчике, но в логе нет `FATAL ERROR!` | Процесс запущен под native debugger | Это ожидаемый debugger-aware route; для проверки crash logging один раз воспроизведите вне отладчика. |
| Engine-triggered break не срабатывает после attach | Присутствие отладчика закэшировано до late attach | Перезапустите процесс под native debugger. |
| В trace MemorySanitizer нет нативных кадров | Намеренная конфигурация `HAS_NATIVE_TRACE=0` | Используйте MSan report и соответствующее symbolized воспроизведение без MSan. |
| Debug build проходит, но release-like build падает | Различие semantics/optimization/timing | Воспроизведите в `RelWithDebInfo`, затем в sanitizer или `Release_Debugging`, где он поддержан. |
| Dump/core отсутствует | Host/project collection не настроен | Настройте принадлежащий OS/operator маршрут dump; Engine гарантирует только документированный log path. |

## Триггеры сопровождения

Повторно проверяйте эту страницу в том же change при изменении:

- имён configurations, `expr_DebugInfo`, `expr_DebugBuild`, symbol/linker flags, sanitizer wiring, PIE/LTO или output layout;
- `IsRunInDebugger`, `BreakIntoDebugger`, capture/resolution/cache стека, exception callbacks, crash handlers, logging flush, alternate signal stacks или режимов `FO_SELFTEST_CRASH`;
- Engine или third-party Natvis/NatJMC и их подключения CMake;
- `Script.DebuggerEnabled`, `Script.DebuggerBindHost`, line cues/optimization AngelScript, настройки context, портов/protocol/commands/events endpoint, breakpoint keys, stack/locals или security boundary;
- схемы adapter, discovery/transport, DAP capability mapping, поставки dependency/toolchain, тестов или публикации;
- файлов launch/evidence проекта, указанных в `ExternalProjectEvidence.json`.

Обновляйте канонические английскую и русскую страницы вместе, меняйте normalized source hash перевода, перегенерируйте external evidence, snippets, locale/site/search/routes и AI delivery, затем запускайте focused debugging gate и aggregate documentation validation. Изменения runtime дополнительно требуют owning native, TypeScript/adapter, process и project integration tests.

## Checklist проверки

1. Запустите `BuildTools/tests/test_docs_debugging.py` и aggregate documentation tests.
2. После изменений native stack/exception запустите `Source/Tests/Test_StackTrace.cpp` и `Test_ExceptionHandling.cpp`.
3. После изменения соответствующих границ запустите sanitizer lanes и `Test_ClientRuntimeApi.cpp`.
4. Подтвердите PDB/DWARF artifacts и MSVC visualizers в заново сгенерированном проекте.
5. Докажите один native launch под отладчиком и один out-of-debugger crash-log route на каждой изменённой платформе.
6. Докажите один live AngelScript attach: endpoint log, осознанный выбор процесса, breakpoint, pause/step, script stack и read-only locals.
7. Подтвердите, что advanced adapter controls описаны согласно live Engine transport, а не mock runtime.
8. Подтвердите loopback bind отладчика, отсутствие credentials и соблюдение project privacy policy для dump/log evidence.
9. Повторно проверьте точное закреплённое project evidence и не переносите project-specific names в процедуру Engine.

## См. также

- [Testing](../contributing/testing/) для границ unit, sanitizer, coverage и integration.
- [Profiling](../how-to/quality/profiling.md) для Tracy capture после понимания correctness boundary.
- [Scripting Runtime](../explanation/scripting-runtime/) для ownership и execution AngelScript.
- [Exception Safety](../contributing/coding-contracts/exception-safety.md) для invariant и termination policy.
- [Разделение client runtime и updater](../explanation/runtime/client-updater.md) для диагностики host/runtime.
- [Сборка, упаковка и отладка Web](../how-to/platforms/web-debugging.md) и [сборка, упаковка и отладка Android](../how-to/platforms/android-debugging.md) для платформенных маршрутов.
