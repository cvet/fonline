---
layout: default
title: Конвейер BuildTools
locale: ru
document_id: buildtools-pipeline
permalink: /Docs/ru/reference/cmake-and-buildtools/pipeline.html
---

# Конвейер BuildTools
<!-- docs-translation: {"document_id":"buildtools-pipeline","locale":"ru","source_path":"Docs/en/reference/cmake-and-buildtools/pipeline.md","source_sha256":"60ade1a45e8c340a86084e751762c9f2363bf9179afd49b031e2a72717b0a730"} -->
Этот документ объясняет поэтапный CMake-конвейер в `BuildTools/cmake/`. Он
дополняет основанное на исходниках руководство [Build Workflow](../../how-to/build/):
в нём описан пользовательский подход к сборке, а здесь — владение реализацией.
Локальные targets и связывание roles описаны в
[ProjectDependencies.md](../../../ProjectDependencies.md), точные публичные
CMake declarations — в [сгенерированном справочнике CMake](../cmake/index.md),
исполняемый синтаксис и владение helper CLI — в
[справочнике helper CLI](../helper-cli/index.md), roles и hooks
нативных расширений — в [справочнике native extensions](../../../generated/native-extension/index.md),
а `DefinePackage` и payload contracts — в
[справочнике package](../packages/index.md).

## Модель владения

Обычно FOnline конфигурируется из встраивающего игрового проекта. Engine
предоставляет CMake stages и helpers; игровой проект задаёт product names,
main config, включённые targets, output paths, packages, scripts и platform
choices.

## Решение об интерфейсе

Используйте существующего владельца, не создавая второй интерфейс. Для CMake
option действует такой приоритет: соответствующая переменная окружения `FO_`,
затем существующее значение CMake cache или `-D`, затем проектное значение
`SetOption`, затем объявленное значение интерфейса по умолчанию. Engine
предоставляет CMake stages и helpers; game project предоставляет значения.

Для команд `BuildTools/buildtools.py create_parser()` владеет основным CLI,
парсеры отдельных helper scripts владеют своими command lines, а `package.py`
вместе с package declaration владеет payload contracts. Helper CLI являются
revision-pinned implementation interfaces: автоматизация обязана фиксировать
ревизию Engine и читать `BuildTools/HelperCliInterface.json` или его
сгенерированный справочник, а не предполагать cross-revision compatibility.

## Проверенные исходные пути

- `BuildTools/Init.cmake`
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/ProjectOptions.cmake`
- `BuildTools/cmake/stages/ThirdParty.cmake`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/cmake/stages/CoreLibs.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/cmake/stages/Packages.cmake`
- `BuildTools/cmake/stages/Finalize.cmake`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/helpers/Commands.cmake`
- `BuildTools/cmake/helpers/Options.cmake`
- `BuildTools/cmake/helpers/RunAndLog.cmake`
- `BuildTools/cmake/helpers/State.cmake`
- `BuildTools/cmake/helpers/WriteBuildHash.cmake`
- `BuildTools/codegen.py`
- `BuildTools/EffekseerEditor/build.ps1`
- `BuildTools/managed_runtime_payload.py`
- `BuildTools/codecoverage.py`
- `BuildTools/android_device.py`
- `BuildTools/web/simple-web-server.py`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/docs_helper_cli.py`
- `BuildTools/docs_cmake.py`
- `BuildTools/PackageInterface.json`
- `BuildTools/docs_package.py`
- `BuildTools/tests/validate_package_interface.cmake`
- `BuildTools/tests/validate_project_interface.cmake`
- `BuildTools/package.py`
- `BuildTools/tests/test_package_include.py`
- `BuildTools/msicreator/createmsi.py`

Важные следствия:

- Не документируйте итоговый список targets одной игры как универсальное
  поведение Engine.
- Предпочитайте обязанности stages и имена options жёстко заданным именам
  сгенерированных targets.
- По возможности проверяйте изменения сборки через preset встраивающего
  проекта.

## Файлы стадий

Поэтапный конвейер находится в `BuildTools/cmake/stages/`. Порядок стадий,
имена entrypoints и проверки hooks во время конфигурации реализованы в
`BuildTools/Init.cmake`. `BuildTools/cmake/ProjectInterface.json` отражает эту
поверхность для сгенерированного [справочника стадий](../cmake/stages.md), а
`validate_project_interface.cmake` отклоняет расхождения в порядке стадий,
entrypoints, hook points и путях к исходникам.

### `Init.cmake`

Устанавливает базовую конфигурацию. Стадия напрямую объявляет все публичные
project options, затем проверяет обязательные значения и создаёт build hash и
общий generation context. `BuildTools/cmake/ProjectInterface.json` фиксирует те
же required inputs, cache types, defaults, allowed values, categories и
override precedence для сгенерированного
[справочника options](../cmake/options.md); структурный тест проверяет наличие
каждой смоделированной option в этой стадии. При изменении публичной option
обновляйте стадию и manifest вместе.

Manifest содержит независимые backends `FO_SPARK_PARTICLES` и
`FO_EFFEKSEER_PARTICLES`. Оба по умолчанию равны `OFF`; при миграции проект
может включить один или оба. Backend source files остаются в стабильных списках
исходников Engine и защищают реализацию соответствующим macro. Отключённый
backend не добавляет third-party target, скомпилированную runtime или Mapper
implementation, runtime resource extensions и baker implementation.

На Linux linker исключает статические архивы LibreSSL Engine из dynamic
symbol table executable. Managed shim OpenSSL открывает системную `libssl`
для cryptography Roslyn; без этой границы библиотека могла бы связать
собственные внутренние вызовы с неверсионированными symbols LibreSSL.

### `ProjectOptions.cmake`

Нормализует и проверяет комбинации project-level options. Текущие примеры:
проверки code coverage, сочетаний build modes и совместимости scripting/tools,
например требование AngelScript support при `FO_BUILD_ASCOMPILER`.

Начинайте здесь, если комбинацию options нужно отклонить или вывести до
создания source lists и targets.

### `ThirdParty.cmake`

Добавляет bundled third-party libraries Engine. Комментарий стадии фиксирует
установку interceptor для `find_package()` до вызовов third-party
`AddSubdirectory()`, чтобы vendored libraries не могли незаметно использовать
систему host.

Подготовка Managed runtime из исходников зависит от target. Перед каждой такой
сборкой `buildtools.py` удаляет repo-local semaphore задач MSBuild, чтобы дерево,
впервые собранное для desktop, не пропустило Android-only task projects и позже
не упало с `MSB4062`. Также передаётся `NuGetAudit=false`: revision runtime
закреплён, warnings являются errors, и новая advisory не должна делать
неизменившийся tag невосстановимым. Shipped payload содержит CLR assemblies, а
не native packages, которые присутствуют лишь в restore graph runtime repo.

Патчи Mono для Windows x86 и Android возвращают constant `false` для свойств
hardware-intrinsic `IsSupported`, не реализованных их JIT. Без fallback тело
property CoreLib рекурсивно вызывает себя и может переполнить stack до первого
кадра. Marker cache Managed runtime включает этот source-patch contract;
prebuilt runtime уже должен его содержать.

Windows Mono также повторяет отказавшие suspension/context reads во время
stop-the-world, пока thread жив, сообщает о каждом отказе и после пяти секунд
завершает процесс вместо пропуска работающего thread. Ready marker
`_suspend_retry` заставляет пересобрать Windows runtime; prebuilt runtime
принимается как есть и уже должен содержать патч. Linux managed builds
подключают OpenSSL cryptography shim вместе с native и globalization shims.
Контракт runtime safety и компиляции fragments описан в
[руководстве Managed C#](../../how-to/scripting/managed-csharp.md#runtime-loading-изоляция-и-shutdown).

Начинайте здесь при добавлении или удалении bundled dependency либо изменении
правил изоляции её сборки.

### `EngineSources.cmake`

Создаёт списки исходников и сгенерированные resource files для последующих
стадий. Сюда добавляются source lists слоёв Essentials, Common, Frontend,
Client, Server, Tools, Scripting и tests. Здесь же готовятся app icon/resource
data, например сгенерированный Windows `.rc`.

Начинайте здесь, когда новый hand-authored source file должен войти в core
library Engine.

### `Codegen.cmake`

Создаёт команду code generation и набор outputs. В `BuildTools/codegen.py`
передаются project и Engine metadata, включая main config, build hash,
generated output path, project names, embedded data capacity, metadata source
files и добавленные common headers.

Стадия создаёт обычную и принудительную цели code generation. Начинайте здесь
при изменении сгенерированных C++/script API metadata.

Связанный документ: [GeneratedApiAndMetadata.md](../metadata/index.md).

### `CoreLibs.cmake`

Создаёт core static libraries из списков, подготовленных в
`EngineSources.cmake`. В зависимости от включённых options текущие обязанности
охватывают Essentials, Common, frontend/headless app layers, scripting
integration libraries, client/server libraries, baker libraries и testing
support.

`EngineSources.cmake` включает нативный модуль `EffekseerCompiler.h/.cpp` в
`BakerLib`. При включённых Effekseer particles `ParticleBaker` напрямую
компилирует через него XML `.efkproj` фиксированной Editor-версии 1.80.5 и
получает список referenced resources каждого проекта для snapshot
path/size/write-time эффекта в `BakeOutput/.baker-cache`. Runtime libraries и
Web clients не зависят от compiler target или host process: они используют
предварительно запечённый `.efk`. Server-only build больше не включает
BakerLib только из-за `FO_BUILD_SERVER`.

Начинайте здесь при изменении группировки исходников, dependencies библиотек
или границ runtime layers.

### `Applications.cmake`

Создаёт executable и shared-library applications из
`Source/Applications/*.cpp`. Используются helpers `AddExecutableApplication`,
`AddSharedApplication`, project variables `FO_DEV_NAME`, output paths,
platform flags и включённые build modes.

В зависимости от options здесь подключаются client, client runtime library,
headless variants client, server variants, Mapper, viewers анимаций и частиц,
baker, AngelScript compiler, Managed script baker и testing app. Универсального приложения Editor и
соответствующей validation target нет.

Effekseer Editor намеренно отсутствует в этой стадии и application target
graph. Его отдельный entrypoint `BuildTools/EffekseerEditor/build.ps1`
конфигурирует и собирает upstream sources независимо от CMake-конфигурации
FOnline во встраивающем проекте.

Для Visual Studio/MSBuild test targets стадия запускает test executable через
`BuildTools/cmake/helpers/RunAndLog.cmake`. Helper сохраняет stdout и stderr в
`<build-dir>/<target>.log` и завершает CMake command с реальным process exit
code. Так ожидаемая диагностика negative tests сохраняется, а MSBuild не
трактует строки со словом `error` как build failures. Другие generators
запускают executable напрямую.

См. [Applications](../applications.md).

### `ScriptsAndBaking.cmake`

Создаёт custom targets для компиляции scripts и baking ресурсов. Текущие
обязанности:

- AngelScript compilation через project AS compiler target при включённом
  AngelScript scripting.
- Компиляция Managed C# через standalone project `ManagedScriptBaker` при
  включённом `FO_MANAGED_SCRIPTING`. `CompileManagedScripts` выполняется после
  `ForceCodeGeneration`, использует настроенные managed source dirs/references/analyzers
  и создаёт target assemblies каждого pack вместе с API/ABI `.gen.cs`, `.gen.csproj` и
  `.gen.sln`. Setup runtime и подготовка payload являются отдельными targets.
- Resource baking через project baker target.
- Поддержка build-hash/write-hash для baked resources.
- Обычные и принудительные bake targets.
- Публичный helper `AddBakingTarget(<target> [SUB_CONFIG <name>] [FORCE] [COMMENT <text>])` для принадлежащих проекту вариантов запекания. Вызывайте его после `SetupScriptsAndBaking()`, когда project baker уже существует; каждая добавленная цель переиспользует стандартные зависимость от codegen, рабочий каталог output, применение конфигурации и обновление resource build hash.

Связанные документы: [Baking Pipeline](../../explanation/content-pipeline/baking.md) и
[Scripting](../../explanation/scripting-runtime/).

### `Packages.cmake`

Создаёт package targets из `FO_PACKAGES` и вызывает `BuildTools/package.py` с
project context: main config, build hash, developer name, nice name,
input/output paths, platform/architecture/config data и optional output postfix
текущей записи `BINARY`.

Clauses декларации `DefinePackage`, допустимые runtime
targets/platforms/architectures, pack tokens, support status и payload effects
моделируются в `BuildTools/PackageInterface.json` и выводятся в
[сгенерированном справочнике пакетов](../packages/index.md). Manifest является
данными документации и проверки; runtime authority остаётся у `DefinePackage`,
`Packages.cmake` и `package.py`. Focused и структурные тесты сопоставляют
смоделированные grammar и dimensions с этими реализациями. Встраивающий проект
всё равно владеет выбором допустимых комбинаций.

`POSTFIX <value>` следует за одной clause `BINARY` и не наследуется соседними
entries. Значение должно совпадать с `FO_BINARY_OUTPUT_POSTFIX`, использованным
при сборке binary: обе стороны участвуют в имени input directory и packaged
runtime identity. Package architecture keys `win32-win7` и `win64-win7`
разрешаются в канонические binary architectures `win32` и `win64`; legacy
toolset выбирается в `buildtools.py`, а явный postfix, например
`POSTFIX Win7`, разделяет имена Raw/Zip/Wix. Перед packaging или publication
запускайте `BuildTools/check_windows7_imports.py` для каждого связанного Win7
PE.

`package.py` владеет переиспользуемой раскладкой package payload и optional
post-processing. Он записывает логические file modes target `0644`/`0755`
независимо от host filesystem, напрямую передаёт их writers ZIP/TAR и объединяет
records Raw/Root разных package parts в versioned package-root publication
handoff `.lf-package-modes.json`. Paths manifest должны быть нормализованными
POSIX-relative payload paths; absolute, drive-qualified, escaping paths и paths
с backslash завершают проверку ошибкой. Publisher raw trees обязан применить
handoff и исключить его из публичного payload.

Для Windows Client package с pack `Wix` `package.py` после staging Raw вызывает
`msicreator/createmsi.py`: MSI получает временный marker `INSTALLED`,
используемый разрешением writable path установленного client, регистрирует
deep-link URI scheme, создаёт Start Menu и Desktop shortcuts и icon в
Add/Remove Programs и всегда показывает редактируемый диалог выбора
installation directory. Оба поддерживаемых build hosts используют одно и то же
inline-описание диалога, поэтому он не пропадает, когда production собирает
через `wixl` на Linux. MSI обязателен при запросе `Wix`. POSIX hosts требуют
`wixl` 0.102 или новее вместе с bundled extension `ui`. Windows ищет
`FO_WIX_ROOT`, подготовленный соседний directory `wix3`, затем `candle`/`light`
в `PATH`; `buildtools.py prepare-workspace wix` загружает закреплённый
`ThirdParty/wix` release WiX v3 в этот workspace с обычными mirror и SHA-256
checks. Windows `light` сначала выполняет обычную ICE validation и повторяет
link один раз с `-sval` только при точном сообщении о недоступности Windows
Installer service; ошибки authoring, linker, обычной ICE и fallback остаются
фатальными. Отсутствие toolset или ошибка generator/build завершает packaging
ошибкой. В Debian/Ubuntu `wixl` поставляется отдельным apt package `wixl`, а не
`msitools`; `common-packages` включает `php-cli` в общий runner contract.
Installer values читаются из
конфига встраивающего проекта, поэтому packager остаётся game-agnostic:

- product/manufacturer/comments name берётся из `Common.GameName` с fallback к
  package nice name;
- `ProductVersion` берётся из `Common.GameVersion`, а `$FILE{...}` разрешается
  относительно main config directory, поэтому `$FILE{VERSION}` даёт реальную
  числовую версию, а не fallback `0.0.0`;
- deep-link URI scheme берётся из `Auth.UriScheme`;
- стабильный WiX `UpgradeCode` берётся из `Packaging.MsiUpgradeCode` и не
  должен меняться после первого выпуска MSI;
- icon Add/Remove Programs берётся из optional `Packaging.AppIcon`;
- имя install directory и MSI base name берутся из package nice name.

Product использует `InstallScope="perUser"`. Компоненты shortcuts Start Menu и
Desktop имеют разные key paths в `HKCU`, регистрация PATH задаёт `System="no"`,
а сгенерированные directory components включают cleanup при uninstall. Явный
`INSTALLDIR`, переданный в `msiexec`, имеет высший приоритет. Иначе первая
интерактивная установка предпочитает путь, запомненный предыдущим MSI, а затем
per-user writable fallback `%LOCALAPPDATA%\<Common.GameName>`. Выбранный путь
сохраняется в `HKCU\Software\<nice-name>\InstallLocation`, а экран выбора
каталога всегда допускает прямое редактирование или обзор, включая явный выбор
`Program Files`. Standalone MSI не обращается к installation infrastructure
Steam или другого магазина.

Обновление MSI намеренно сохраняет запомненный путь в `Program Files` вместо
переноса существующего дерева. Обновлённый marker `INSTALLED` по-прежнему
направляет cache, logs, resources и обновления native runtime в per-user
writable overlay, описанный в
[Client Updater](../../explanation/runtime/client-updater.md), поэтому
сохранённое расположение executable не мешает последующим self-updates.

Portable Raw/Zip artifacts завершаются до MSI и не содержат marker
`INSTALLED`, поэтому остаются portable.

Managed class libraries остаются resource payload, а не binary companions.
Когда несколько binary variants разделяют один updater target, `package.py`
проверяет каждое подготовленное дерево `ManagedRuntime` и выбирает наименее
квалифицированную подходящую binary entry — обычно default Release build — для
единственного target-wide resource pack. Независимо собранные эквивалентные
CoreLib payloads не обязаны быть byte-identical.

Embedding build может задать `FO_RESOURCE_ARCHIVE_CACHE_HELPER` как Python
helper с интерфейсом `restore|store|release --key <sha256> --archive <path>`.
Ключ охватывает стабильные имена и содержимое entries вместе с compression
level. Восстановленный или только что записанный archive всегда проходит ту же
проверку точного entry list и CRC; cache miss и результат недоступности
optional cache переходят к локальному созданию, а прочие failures helper
остаются packaging errors. Повторный идентичный archive в одном процессе
packaging использует первый проверенный результат.

Когда несколько package parts добавляются в один `SingleZip`, байт-идентичные
файлы с одним archive path объединяются в одну entry. Разное содержимое по
одному path является packaging error; packager не создаёт неоднозначные
duplicate ZIP names.

В универсальной package schema нет binary role `EffekseerEditor`. Отдельно
собранные tools объявляются рядом с `BINARY` через
`INCLUDE <source-path-glob> <target-path-in-pack>`. Source glob задаётся
относительно `FO_OUTPUT_PATH`. После сборки обычных binary parts generic
packager заменяет included target tree и обновляет существующий `SingleZip`
без duplicate или stale entries. Этот путь покрыт
`BuildTools/tests/test_package_include.py`.

При изменении wiring package target начинайте в `Packages.cmake`. При изменении
declaration vocabulary, поддерживаемых комбинаций, payload layout, artifact
behavior, packager arguments или package-time installer metadata начинайте в
`BuildTools/PackageInterface.json` и `package.py`.

### `Finalize.cmake`

Выполняет итоговую организацию solution/project и поздний reporting. Текущие
обязанности включают target folder grouping, optional copy настроек ReSharper,
third-party dummy grouping и подробный вывод cache variables при включённом
`FO_VERBOSE_BUILD`.

Начинайте здесь для итоговой организации targets или post-generation
diagnostics, а не для source ownership или проверки build features.

## Вспомогательные файлы

Переиспользуемые helpers находятся в `BuildTools/cmake/helpers/`:

- `Build.cmake` — helpers создания build/target.
- `Commands.cmake` — helpers command targets.
- `Options.cmake` — helpers options/values.
- `RunAndLog.cmake` — внутренний script-mode process runner, сохраняющий test
  output и передающий exit code.
- `State.cmake` — поддержка состояния и hooks поэтапного pipeline.
- `WriteBuildHash.cmake` — записывает состояние build hash для
  generation/baking flows.

Если стадии нужно переиспользуемое поведение, добавляйте helper здесь вместо
копирования логики между stages.

Расположение в helpers не делает command публичной. Документированной
поверхностью встраивающего проекта являются только выбранные команды из
`BuildTools/cmake/ProjectInterface.json`, выведенные в
[справочнике helper](../cmake/helpers.md). Остальные
helper commands остаются внутренними деталями реализации.

## Hooks стадий

Комментарии стадий используют соглашение:

```cmake
AddStageHook(<StageName> Pre|Post <macro-name>)
```

Используйте hooks, когда встраивающему проекту или последующему refactor нужно
расширить поведение стадии без изменения её середины. Документируйте hook рядом
с owning stage или в project docs, если он game-specific.

Документированный inventory поддерживаемых stage names, entrypoints и hook
positions находится в [сгенерированном справочнике стадий и hooks](../cmake/stages.md);
configure-time authority остаётся у `BuildTools/Init.cmake`.

## Маршрутизация изменений

- Новая project option: объявление в `Init.cmake`, проверка сочетаний в
  `ProjectOptions.cmake` и соответствующие документационные данные в
  `BuildTools/cmake/ProjectInterface.json`.
- Новая vendored dependency: `ThirdParty.cmake`.
- Новая project-local dependency или role link:
  [ProjectDependencies.md](../../../ProjectDependencies.md),
  потребляемый список `FO_*_LIBS` закреплённой ревизии и target/package matrix встраивающего проекта.
- Новый engine source file: `EngineSources.cmake` и, возможно,
  `CoreLibs.cmake`.
- Новое generated metadata/API behavior: `Codegen.cmake` и
  [GeneratedApiAndMetadata.md](../metadata/index.md).
- Новая helper command или argument: executable `create_parser()`,
  `BuildTools/HelperCliInterface.json`,
  [справочнике helper CLI](../helper-cli/index.md) и
  `BuildTools/docs_helper_cli.py`.
- Новый project-native source role, hook или binding rule:
  `BuildTools/NativeExtensionInterface.json`,
  [NativeExtensions.md](../../../NativeExtensions.md),
  [generated/native-extension/index.md](../../../generated/native-extension/index.md)
  и `BuildTools/docs_native_extension.py`.
- Новое script compile или resource bake behavior:
  `ScriptsAndBaking.cmake`, [Baking Pipeline](../../explanation/content-pipeline/baking.md),
  [Scripting](../../explanation/scripting-runtime/) и backend-specific руководство AngelScript либо
  [Managed C#](../../how-to/scripting/managed-csharp.md).
- Новый executable/tool entry point: `Applications.cmake` и
  [Applications](../applications.md).
- Рецепты сборки auxiliary tool: `BuildTools/buildtools.py build-auxiliary`,
  `BuildTools/EffekseerEditor/build.ps1` и [Tools.md](../../../Tools.md).
- Новая package declaration, support combination, layout или artifact:
  `BuildTools/PackageInterface.json`, `Packages.cmake`,
  `BuildTools/package.py`,
  [сгенерированном справочнике пакетов](../packages/index.md), при
  необходимости `BuildTools/msicreator/createmsi.py`, а также platform docs.
- Итоговая организация targets или verbose diagnostics: `Finalize.cmake`.

## Чек-лист проверки

Для изменений BuildTools:

1. Выполните `cmake -P BuildTools/tests/validate_project_interface.cmake` при
   изменениях project interface.
2. После регенерации CMake reference выполните
   `python BuildTools/tests/test_docs_cmake.py` и
   `python BuildTools/docs_cmake.py --check`.
3. Выполните configure из корня реального встраивающего проекта.
4. Используйте самый узкий preset, затрагивающий изменённую stage.
5. При изменении source lists проверьте сборку затронутой target.
6. При изменении codegen проверьте generated files и script API consumers.
7. При изменении baking выполните normal и forced bake paths, если применимо.
8. Для изменений Effekseer Editor выполните `buildtools.py build-auxiliary
   effekseer-editor Release` на Windows win64, проверьте staged payload
   managed/native/resources и package `INCLUDE`, если изменилась layout
   developer package.
9. Для package changes выполните затронутую package target и проверьте output
   layout; для WiX/MSI также проверьте generated installer
   config/registry values или соберите installer на host с WiX/wixl.
10. Для изменений package interface выполните
    `python BuildTools/tests/test_docs_package.py`,
    `cmake -P BuildTools/tests/validate_package_interface.cmake` и после
    регенерации `python BuildTools/docs_package.py --check`.
11. Для изменений native extension interface выполните
    `python BuildTools/tests/test_docs_native_extension.py`,
    `cmake -P BuildTools/tests/validate_native_extension_interface.cmake` и
    после регенерации `python BuildTools/docs_native_extension.py --check`.
12. Сравните все регенерированные модели
    API/CMake/main-CLI/package/helper-CLI/native-extension с выбранной base
    через `BuildTools/docs_contract_diff.py` и заполните необходимую
    [contract disposition](../../contributing/contract-change-management.md).
13. При изменении docs запустите проверки ссылок.
14. До сообщения о завершении выполните `git diff --check`.
