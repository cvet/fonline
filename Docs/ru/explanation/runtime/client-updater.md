---
layout: default
title: Разделение клиентской среды выполнения и обновление
locale: ru
document_id: client-updater
permalink: /Docs/ru/explanation/runtime/client-updater.html
---

<!-- docs-translation: {"document_id":"client-updater","locale":"ru","source_path":"Docs/en/explanation/runtime/client-updater.md","source_sha256":"4c124f94eeb2a29e1e257e15196079ccb2a174b5fef1333409082c737de3c866"} -->

# Разделение клиентской среды выполнения и обновление

> Документация движка по переиспользуемому ABI между клиентским host и runtime,
> протоколу обновления, контракту упаковки и восстановлению после сбоев. Имена
> бинарных файлов ниже приведены как примеры. Переопределения конфигурации,
> профили пакетов и каналы распространения принадлежат встраивающему проекту.

Нативный клиент поставляется как два артефакта:

- `<client-host>.exe` - тонкое host-приложение из [ClientApp.cpp](../../../../Source/Applications/ClientApp.cpp), которое должно сохранять совместимость между версиями runtime;
- соседняя runtime-библиотека (`<client-host>.dll` в Windows, `.so` в Linux или `.dylib` в macOS) - загружаемая среда выполнения из [ClientLib.cpp](../../../../Source/Applications/ClientLib.cpp), построенная сгенерированной CMake-целью `<ProjectDevName>_ClientLib` и содержащая игровой клиентский движок.

Host загружает runtime через стабильный C ABI из
[ClientRuntimeApi.h](../../../../Source/Client/ClientRuntimeApi.h). Если загрузить
библиотеку нельзя, используется встроенный клиент, слинкованный в host.

На платформах с разделёнными host/runtime сгенерированная цель
`<ProjectDevName>_Client` зависит от `<ProjectDevName>_ClientLib`. Post-build шаг
runtime-цели копирует результат под соседнее имя, производное от имени host.
Поэтому сборка host обновляет оба артефакта, а runtime можно собирать отдельно.
Для headless-целей действует та же зависимость.

**Поддержка платформ.** Разделение host/runtime строится только для Windows,
Linux и macOS. Именно для них `CanSelfUpdateNativeModules()` возвращает `true`,
а `static_assert` в начале `ClientLib.cpp` разрешает сборку. Web, iOS и Android
получают единый самодостаточный клиент: код движка статически включён в бинарный
файл, и ветка загрузки библиотеки в `RunEmbeddedOrLoadedClient()` не выполняется.
Условие CMake находится в
[Applications.cmake](../../../../BuildTools/cmake/stages/Applications.cmake), а
Android дополнительно использует `FO_BUILD_LIBRARY` для Java-loader SDL.

Механизм обновления ресурсов и runtime общий, но поколение updater версионируется
отдельно от игровой совместимости. Поэтому уже выпущенный host может получить
будущий runtime без пересборки самого host, пока сохранён их явный ABI-контракт.

## Статус контракта

Страница является основанным на исходном коде объяснением текущего поведения
host/runtime и updater. Версия C ABI и поколение сетевого протокола - явные
контракты совместимости. Настройки, сгенерированные package metadata и
документированные параметры командной строки сохраняют стабильность, указанную
в их владеющих справочниках. Native-классы, helper-функции, формат cache key и
реализация staging являются внутренними деталями движка, если
[индекс публичных контрактов](../../reference/public-contract/index.md) не говорит обратного. Проект должен
фиксировать точную ревизию Engine и заново проверять пакетные payload после
изменения этих деталей.

## Серверная часть updater

Клиентский updater обслуживается авторитетным сервером. При запуске packaged
server `ServerEngine` создаёт `UpdaterBackend` из `Source/Server/UpdaterBackend.*`.
Backend сканирует клиентские resource packs и native runtime, строит descriptor
для каждой целевой платформы и отвечает на запросы частей файла сообщениями
`NetMessage::UpdateFileData`.

Граница владения намеренная:

- [Серверная среда выполнения](server.md) описывает размещение `UpdaterBackend`, запуск сервера и обработку соединений;
- эта страница описывает ABI host/runtime, staging, переход через restart, проверки совместимости и наблюдаемый клиентом протокол updater.

Детали протокола и host/runtime остаются здесь, а серверный lifecycle и владение
менеджерами - в разделе о серверной среде выполнения.

## Проверенные пути исходного кода

- `Source/Applications/ClientApp.cpp`
- `Source/Applications/ClientLib.cpp`
- `Source/Client/ClientRuntimeApi.h`
- `Source/Client/ClientRuntimeApi.cpp`
- `Source/Client/Updater.h`
- `Source/Client/Updater.cpp`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Server/UpdaterBackend.h`
- `Source/Server/UpdaterBackend.cpp`
- `Source/Server/Server.cpp`
- `Source/Server/Player.h`
- `Source/Server/Player.cpp`
- `Source/Server/ServerConnection.h`
- `Source/Server/ServerConnection.cpp`
- `Source/Common/Common.h`
- `Source/Common/Settings.inc`
- `Source/Essentials/DiskFileSystem.h`
- `Source/Essentials/DiskFileSystem.cpp`
- `Source/Essentials/Platform.h`
- `Source/Essentials/Platform.cpp`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/package.py`
- `BuildTools/msicreator/createmsi.py`
- `BuildTools/tests/test_package_zip_determinism.py`
- `Source/Tests/Test_ClientRuntimeApi.cpp`
- `Source/Tests/Test_DiskFileSystem.cpp`
- `Source/Tests/Test_Platform.cpp`
- `Source/Tests/Test_Settings.cpp`
- `ThirdParty/rpmalloc/rpmalloc/rpmalloc.c`

## Двухуровневый запуск клиента

На платформах с self-update host сначала пытается загрузить bundled runtime.
Встроенный движок в host служит fallback, если соседней библиотеки нет или она
не загружается. Это одинаково для обычного и headless-клиента. Настройка
`Client.ForceEmbeddedRuntime`, переданная в командной строке до загрузки settings,
пропускает неявную библиотеку; явный `--ClientLibPath` всё равно имеет приоритет.
Какой бы модуль ни выполнял игру, он запускает единый двухстадийный updater UI:

```text
<client-host> (host)
    │
    │  1. Resolve runtime path (`GetClientRuntimeLivePath()` from current exe name; an installed
    │     client may select a persisted per-user runtime bootstrap; --ClientLibPath overrides both)
    │  2. ApplyStagedBinaryUpdate(<runtime>) — promote pending `<runtime>-staging` over `<runtime>`
    │     (also recovers a crashed-mid-update install on first boot)
    │  3. Platform::LoadModule(<runtime>) → FO_QueryClientRuntimeExports(...)
    │  4. Validate ClientRuntimeExports.Metadata (ABI; compatibility only when explicitly requested)
    │
    ▼
<live runtime module>                 ─── the running module (loaded DLL by default; host module on fallback / ForceEmbeddedRuntime)
    │
    │  RunClientRuntime: InitApp → resource Updater (UI) → ClientEngine → MainLoop
    │  If resource updater reports compat outdated and platform supports self-update:
    │     stage 2: binary Updater (UI) writes or verifies the module at `<runtime>-staging` / `<runtime>`
    │     return ClientRuntimeResult { ReloadRequested, RequestedRuntimePath = <runtime> }
    │
    └─► returns ClientRuntimeResult (Shutdown / ReloadRequested / FatalError)

No sibling DLL / LoadModule fails, or ForceEmbeddedRuntime is set:    ─── embedded fallback
    Embedded client runs the same RunClientRuntime in the host module. After it
    signals ReloadRequested, the host tears down its own Application instance and goes
    to the restart step below.

Restart step (taken on either Case after ReloadRequested) — PromoteStagedReloadForRestart:
    The runtime already asked the user to restart (ShowUpdaterRestartRequired). The host runs
    ApplyStagedBinaryUpdate(RequestedRuntimePath), renaming `<runtime>-staging` over `<runtime>`
    when a staged file exists (atomic .bak rollback). For an installed client it then persists that
    absolute runtime path in the per-user bootstrap selector and EXITS. The update is applied on the
    next launch, which resolves the selector before settings and loads the promoted runtime as its
    single InitApp. The update is not applied in-process. See "Self-update applies on the next launch"
    below for why.
```

`ApplyStagedBinaryUpdate` идемпотентен: при отсутствии `<live>-staging` он
возвращает `true` и ничего не меняет. Один код поэтому обслуживает восстановление
после падения на старте и promotion при выходе после обновления.

Встроенный клиент выполняется в двух случаях:

- bundled runtime не загрузился; при явном `--ClientLibCompatibilityVersion` fallback разрешён только при совпадении с встроенным `FO_COMPATIBILITY_VERSION` host;
- `Client.ForceEmbeddedRuntime` передан без явного `--ClientLibPath`, поэтому host сразу выбирает embedded path.

`RunEmbeddedOrLoadedClient` использует условие
`requested_runtime.ExplicitPath || (!ForceEmbedded && CanSelfUpdateNativeModules(...))`
как для обычной, так и для headless-цели. Поскольку runtime выбирается раньше
обычного чтения конфигурации, `Client.ForceEmbeddedRuntime` должен прийти как
`--ForceEmbeddedRuntime`; значение только из `.fomain` или SubConfig не влияет на
этот pre-init выбор.

Загруженную по live path DLL нельзя безопасно заменить и снова загрузить в том
же процессе. Поэтому host только продвигает staged-файл и завершается. Новый
runtime получает ровно один `InitApp` уже в следующем процессе.

Обычная загрузка bundled DLL специально не сравнивает игровую compatibility
строку runtime со встроенной строкой старого host. Host в установленном клиенте
заморожен, а runtime обновляется; такое сравнение отвергало бы правильный новый
модуль. `--ClientLibCompatibilityVersion <version>` включает строгую проверку
только для тестов и явных probes. При несовпадении embedded fallback запрещён,
чтобы host не скрывал ошибку переходом на старый встроенный код.

Диагностика handoff пишется в обычный `<host>.log`. Host создаёт global data,
открывает лог с truncation, а затем держит дескриптор открытым во время вызова
runtime DLL. Host и DLL содержат независимые экземпляры engine global data и
собственные `std::ofstream`, но разделяют файл без exclusive lock. Каждый write
сначала выполняет seek-to-end, поэтому записи не перетирают друг друга. Runtime
передаёт `AppInitFlags::AppendLogFile` в `InitApp`. Самые ранние строки
`FO_QueryClientRuntimeExports` и `RunClientRuntime` до создания global data DLL
могут попасть только в stdout, но host уже записывает полный load/accept/enter
handoff, а после `InitApp` runtime продолжает тот же файл.

После `ReloadRequested` embedded-модуль выполняет `App.reset()` до возврата host,
чтобы не держать два SDL window одновременно. Runtime из DLL также освобождает
`App` перед `Platform::UnloadModule`. Оба пути вызывают
`ApplicationShutdownHook()`; проект использует hook для остановки process-global
интеграций, например crash handler, до выгрузки модуля.

### Self-update применяется при следующем запуске пользователя

Native self-update не применяется внутри текущего процесса. После staging
runtime updater выводит приглашение перезапустить клиент прямо на update screen
через `Updater::AddText` и `_restartPrompt` из
[Updater.cpp](../../../../Source/Client/Updater.cpp), затем ждёт закрытия клиента.
Runtime возвращает `ReloadRequested`, а
`PromoteStagedReloadForRestart` из
[ClientApp.cpp](../../../../Source/Applications/ClientApp.cpp) продвигает файл и
завершает host. Следующий запуск загружает обновлённый модуль как единственный
`InitApp`.

Сообщение и ожидание отключены для `App->IsHeadless()`: у headless-клиента нет UI
и пользователя, который закроет prompt, поэтому он сразу возвращает результат,
после чего host продвигает файл и выходит. Проверка runtime необходима, так как
`FO_HEADLESS_APP` относится к app-target и не задаётся при компиляции `ClientLib`.

Политика restart защищает не только SDL. Два engine-модуля в одном процессе имеют
отдельные allocator, global data, logging и singleton state. Повторная
инициализация runtime по тому же пути оставляла бы ссылки и thread-local state от
выгруженного модуля. ABI 3 и updater generation 2 отвергают старые host, которые
могли попытаться сделать in-process reload; для них нужен новый полный пакет.

Для установленного клиента после promotion host проверяет, что путь абсолютный,
basename совпадает с runtime, отсутствуют newline/NUL, а live или staging файл
существует. Затем selector записывается через временный файл и rename. При
следующем старте selector читается до settings. Некорректный selector не блокирует
запуск, а возвращает клиента к runtime из install directory.

## Интерфейс командной строки host

```text
<client-host>                                                           # bundled runtime, default compatibility
<client-host> --ClientLibPath <path>                                    # explicit runtime, default compatibility
<client-host> --ClientLibPath <path> --ClientLibCompatibilityVersion <ver>  # explicit runtime, no embedded fallback if ver != built-in
```

Имя bundled runtime вычисляется из basename текущего executable функцией
`GetCurrentClientRuntimeLibraryName()`, с fallback на `FO_DEV_NAME`, если
`Platform::GetExePath()` недоступен. Live path равен
`GetClientRuntimeLivePath() = <exe_dir>/<library_name>`, а platform extension
добавляет `Platform::LoadModule`. Поэтому переименованные или параллельные host
получают свои sibling runtime (`MyAlt.exe` и `MyAlt.dll`) без config patch.
Сгенерированная runtime-цель также копирует canonical output под host-derived
alias, чтобы unpackaged build использовал тот же путь.

## ABI среды выполнения

[ClientRuntimeApi.h](../../../../Source/Client/ClientRuntimeApi.h) - единственный
контракт между host и runtime. Обе стороны согласуют:

- `FO_CLIENT_RUNTIME_HOST_ABI_VERSION = 3`; версия меняется при изменении layout структур или обязательного lifecycle host, а ABI 3 закрепляет promote-and-exit и отклоняет ABI 2;
- `ClientRuntimeMetadata` с именем runtime, build hash и игровой compatibility version;
- `ClientRuntimeExports`, возвращаемый `FO_QueryClientRuntimeExports(host_abi_version, *exports)`;
- `ClientRuntimeResult` со значениями `Shutdown`, `ReloadRequested` и `FatalError`.

После staging runtime выставляет `ResultKind = ReloadRequested` и заполняет
`RequestedRuntimePath`. Host не загружает этот путь снова в текущем процессе:
он продвигает staged-файл, выходит и оставляет загрузку следующему запуску.

Runtime пишет новый модуль как `<live>-staging`, где `<live>` равен
`Updater::GetRuntimeLivePath()` и включает platform extension. Для portable это
`<exe_dir>/<runtime-name>.dll`, для installed -
`<UserWritablePath>/<runtime-name>.dll`. После полного download и проверки hash
updater пытается немедленно заменить live-файл; если он locked, staging остаётся
до прохода host. `RequestedRuntimePath` всегда обозначает итоговый live path, а
не staging. Rename выполняется через `MakeClientRuntimeStagingPath`, после чего
host выходит и только новый процесс вызывает `LoadModule`.

**Изоляция модуля Linux.** Runtime `.so` должен загружаться через `dlopen`, даже
когда host экспортирует собственные символы движка через `-rdynamic`. Поэтому
`AddSharedApplication` в
[Build.cmake](../../../../BuildTools/cmake/helpers/Build.cmake) связывает engine
runtime с `-Wl,-Bsymbolic`: global data, allocator и logging разрешаются внутри
модуля, а не в копии host. Кроме того, `(FOnline Patch)` в vendored rpmalloc не
навязывает initial-exec TLS для Linux; иначе `dlopen` может завершиться ошибкой
`cannot allocate memory in static TLS block`. C ABI копирует строки на границе,
поэтому владение allocation остаётся внутри создавшего её модуля.

В Windows соответствующий PDB называется `<live>.pdb`, например
`<runtime-name>.dll.pdb`, и сначала попадает в `<live>.pdb-staging`. Его обычно
можно продвинуть сразу; при lock debugger host повторяет попытку после DLL swap.
Сбой PDB ухудшает stack traces, но не блокирует атомарную замену DLL с backup и
rollback. Runtime DLL и runtime PDB загружаются вместе только в binaries mode.
Host PDB (`<host_name>.pdb`) используется только для восстановления отсутствующей
локальной копии и никогда не перезаписывает существующую: host exe заморожен, и
PDB другой сборки не должен уничтожить подходящие symbols. Условие
`_binariesMode || CanSelfUpdateNativeModules(...)` позволяет восстановить
отсутствующий host PDB и при обычной resource sync.

## Протокол updater

Протокол версионируется константой `FO_UPDATER_VERSION = 2` из
[Common.h](../../../../Source/Common/Common.h). Поколение меняется при изменении
wire format или когда lifecycle старого updater/host больше нельзя безопасно
продолжать. Generation 2 отклоняет generation 1 до передачи descriptor или
binary payload, потому что замороженный старый host мог сделать опасный
in-process reload. Игровая `Settings.CompatibilityVersion` независима и обычно
изменяется с каждой сборкой.

### Handshake

| Направление | Поле | Тип | Назначение |
|-------------|------|-----|------------|
| client → server | `CompatibilityVersion` | `string` | игровая совместимость |
| client → server | `MetadataVersion` | `string` | версия запечённых metadata; empty, пока у updater нет собственных ресурсов |
| client → server | `updater_version` | `uint32` | `FO_UPDATER_VERSION` |
| client → server | `binary_target` | `string` | например, `Windows-win64` или `Android-arm64` из `GetCurrentBinaryUpdateTargetName()` |
| client → server | `in_encrypt_key` | `uint32` | ключи сессии |
| server → client | `compatibility_outdated` | `bool` | несовпадение игровой версии |
| server → client | `updater_outdated` | `bool` | несовпадение `FO_UPDATER_VERSION`, протокол непригоден |
| server → client | `metadata_outdated` | `bool` | client resources запечены из другой ревизии |
| server → client | `MetadataVersion` | `string` | версия metadata, которую сейчас использует server |
| server → client | `out_encrypt_key` | `uint32` | ключи сессии |

`updater_outdated == true` фатален для соединения: дальнейшие сообщения нельзя
интерпретировать по известному контракту. `compatibility_outdated == true`
блокирует игру, но updater всё ещё может доставить ресурсы и native module,
возвращающие клиент к текущей совместимости.

`metadata_outdated == true` означает, что binaries совпадают, а baked data — нет.
Server и client обязаны использовать metadata из одной bake, потому что entity
payload адресует properties по registration order этих metadata. Несовпадение —
дефект build или deployment, а не поддерживаемый compatibility mode; см.
[Версию metadata](../../reference/metadata/#версия-metadata).

Updater не позволяет запустить client engine с несовместимыми данными:

1. Сначала он подключается, сообщает версию текущих packs (empty при fresh install)
   и синхронизирует все объявленные файлы.
2. Затем он повторно читает версию локальных packs. Если она не совпадает с
   server, updater возвращает `UpdaterResult::MetadataMismatch`, а `ClientEngine`
   не создаётся. Unpackaged dev server без distributed resources нечего проверять,
   поэтому он пропускает этот шаг.
3. Только после проверки создаётся client и отправляет собственный handshake.

Если server redeploy произошёл между sync и client handshake, server сообщает
новое несовпадение, client выбрасывает `ResourcesOutdatedException`, а host снова
запускает sync. Unpackaged client не имеет updater и сообщает mismatch через
обычный exception path.

Повреждённый pre-handshake payload, который не декодируется из buffer, считается
невалидными handshake data. Сервер пишет warning с remote endpoint и выполняет
hard disconnect без exception stack trace. Ошибки decode после handshake идут
через обычный exception-reporting path.

### Начальные данные

`InitData` отправляется один раз после успешного handshake и содержит descriptor
файлов для текущего binary target, global properties и synchronized time.

Каждая запись descriptor имеет поля:

| Поле | Тип | Значение |
|------|-----|----------|
| `name_len` | `int16`, `-1` завершает список | длина client-relative path |
| `name` | `char[name_len]` | client-relative path |
| `size` | `uint64` | полный размер файла |
| `hash` | `uint64` | FNV-1a 64-bit от содержимого |
| `target` | `UpdateFileTarget` (`uint8`) | `ClientResources` или `ClientBinaries` |
| `file_index` | `uint32` | индекс, назначенный сервером для `GetUpdateFile` |

Общие gameplay-resource entries входят в descriptor любого target. Записи
`UpdateFileTarget::ClientBinaries` добавляются только для `binary_target` из
handshake. Затем client фильтрует binary entries по basename runtime, полученному
из host. Поэтому `<client-host>.exe` выбирает `<client-host>.dll`, а
`<alternate-host>.exe` - `<alternate-host>.dll`, даже если CPU/OS target один.

### Возобновляемая передача файла

Клиент последовательно управляет одной передачей:

```text
client → server: GetUpdateFile  { file_index: uint32, start_offset: uint64 }
server → client: UpdateFileData { update_portion: int32, raw bytes[update_portion] }
```

Размер `update_portion` выбирает сервер и ограничивает
`Network.UpdateFileMaxPortionSize`. Значение движка по умолчанию в
[Settings.inc](../../../../Source/Common/Settings.inc) равно 1 000 000 bytes.
Проект может изменить его после измерения throughput и memory pressure одного
message. Следующий запрос передаёт `start_offset = bytes_already_written`, поэтому
после reconnect клиент продолжает с размера временного файла, а серверу не нужно
хранить session state передачи.

Updater connection участвует и в общем протоколе connection stages. После
`InitData` сервер может отправить `NetMessage::HashList` с message id 122, чтобы
передать строки для ранее неизвестных runtime hashes. Updater записывает их в
своё hash storage и продолжает transfer. `HashList` не является payload файла и
не меняет state machine `GetUpdateFile` / `UpdateFileData`.

Серверная валидация в
[UpdaterBackend.cpp](../../../../Source/Server/UpdaterBackend.cpp):

- `file_index` вне range приводит к `LogType::Warning` и `HardDisconnect`;
- `start_offset > file_size` приводит к warning и hard disconnect;
- `update_file_max_portion_size <= 0` считается ошибкой конфигурации и разрывает соединение;
- ошибка чтения disk-mode payload также завершает соединение;
- изменение размера disk-mode файла относительно объявленного descriptor приводит
  к warning и hard disconnect. При `ServerNetwork.UpdateFilesInMemory = False`
  descriptor является startup snapshot, а bytes читаются по требованию, поэтому
  pack нельзя подменить под работающим server и отправить с hash старого файла.

Клиент пишет части в `~<filename>`, после завершения считает streamed
`fs_hash_file` из
[DiskFileSystem.cpp](../../../../Source/Essentials/DiskFileSystem.cpp) и атомарно
заменяет live-файл через `ReplaceFileSafely`. Hash updater - FNV-1a 64-bit; он
отличается от wyhash-backed `hashing_ex::hash`, используемого для hash tables и
`hstring`. Streamed hash chunked-файла совпадает с `fs_hash_data` полного buffer,
поэтому server memory mode и client disk mode согласованы без загрузки multi-GB
pack целиком в память.

`Updater::IsDiskFileHashMatch` кэширует проверку существующих файлов в
`CacheStorage` под `Baking.CacheResources`. Key имеет вид
`<basename>-<path-hash>.hash`: suffix - 16 lowercase hexadecimal digits результата
`hashing::hash<string_view>` от полного path string, переданного проверке. Например,
получается `Embedded.zip-0123456789abcdef.hash`, но реальный suffix зависит от
пути. Digest не содержит недопустимый Windows colon и одновременно разделяет
одноимённые файлы в разных каталогах. Cached value хранит `(size, mtime, hash)`.
Изменение size или mtime инвалидирует запись, а удаление cache entry вызывает
обычный re-hash.

Backward-compatible fallback отсутствует. Старый протокол с server-side session
state, file index и portion counter удалён при введении `FO_UPDATER_VERSION`.
Client и server обязаны использовать одно поколение.

## Серверная сторона: `UpdaterBackend`

[UpdaterBackend.h](../../../../Source/Server/UpdaterBackend.h) принадлежит
`ServerEngine` как `unique_ptr`. В unpackaged dev server `_updaterBackend` равен
null, поэтому запрос `GetUpdateFile` получает `HardDisconnect`: серверу нечего
отдавать.

Текущий native interface является внутренней частью движка, а не стабильным
public API:

```cpp
void LoadFromClientResources(const GlobalSettings& settings);
void ProcessUpdateFile(ptr<Player> player, int32_t update_file_max_portion_size);
auto GetUpdateDescriptor(string_view binary_target_name) const -> const_span<uint8_t>;
```

Descriptor возвращается как borrowed view `const_span<uint8_t>` на storage,
принадлежащий backend; caller не должен сохранять его дольше lifetime storage.

`LoadFromClientResources` проходит `Settings.ClientResources`, выбирает packs из
`Settings.ClientResourceEntries`, кроме `Embedded`, и перечисляет
`Settings.PlatformBinaries/<target>/` для target-specific binaries. По умолчанию
это `PlatformBinaries/`, соседний с `Resources/` в package layout.

Каждый файл хранится как
`UpdateFileData { InMemory, MemoryData?, DiskPath?, Size, Hash }`. В memory mode
весь payload остаётся в RAM на всё время работы server. В disk mode backend
держит только path, size и streamed hash, а `ReadUpdateFilePortion(...)` читает
запрошенный диапазон. Descriptor кэшируется по `binary_target_name`; общие
resource entries объединяются с target-specific, а неизвестный target получает
только common descriptor.

`VerifyClientResourcesMetadata` монтирует client packs и сравнивает их metadata
version с версией, загруженной server. Server исполняет `Settings.ServerResources`,
но распространяет `Settings.ClientResources`, поэтому deployment только одной
стороны теперь завершает startup с `UpdaterException`, где названы обе версии.

## Настройки

| Настройка | Область | Назначение |
|-----------|---------|------------|
| `Network.UpdateFileMaxPortionSize` | top-level | максимум bytes в одном `UpdateFileData`; влияет на throughput и память message, default 1 000 000 |
| `ServerNetwork.UpdateFilesInMemory` | top-level и `[SubConfig]` | `True` держит packaged payload в RAM, `False` читает portions с disk; default `False` |
| `Network.ForceMetadataVersion` | top-level | только для tests: переопределяет metadata version, сообщаемую client, чтобы смоделировать mismatch без второй bake; в shipped configs должна быть empty |
| `Baking.PlatformBinaries` | top-level | каталог чтения и package staging target-specific runtime, default `PlatformBinaries` |
| `Client.UserWritablePath` | client | writable root installed-клиента; empty означает portable, `*` выбирает per-OS user data, иначе нужен explicit absolute path |

Автоматического выбора memory/disk mode в C++ нет. Проект задаёт режим явно для
каждого окружения и подтверждает его нагрузочными измерениями.

## Writable data установленного и portable-клиента

Portable build хранит cache, log и self-update рядом с executable. Это подходит
для zip, распакованного пользователем в writable directory. Installed build в
`Program Files` или `/usr/...` может быть read-only, поэтому записи должны уйти
в per-user root.

`ResolveUserWritablePath(settings)` из `ApplicationInit.cpp` интерпретирует
`Client.UserWritablePath`:

- empty - portable, пути остаются рядом с executable или working directory;
- `*` - `Platform::GetUserDataBase()` и затем `/<Common.GameName>`: `%LOCALAPPDATA%` в Windows, `~/Library/Application Support` в macOS, `$XDG_DATA_HOME` либо `~/.local/share` в Linux;
- explicit path - заданный absolute writable root.

Resolution идемпотентен, создаёт root, `Cache` и `<ClientResources>`. При
невозможности определить или создать directory он пишет warning и безопасно
возвращается к portable, чтобы плохая install-конфигурация не блокировала startup.

Через `fs_make_writable_path(UserWritablePath, relative)` в writable root
перемещаются cache (`CacheStorage`, login keys, secure local storage и local
config), log, resource patches и self-updated native runtime. Read-only base
`ClientResources` остаётся смонтированным, а `<root>/<ClientResources>` добавляется
как overlay с более высоким priority. Поэтому обновлённые файлы выигрывают lookup
без изменения install directory.

Native runtime установленного клиента также обновляется в writable root.
`Updater::_binaryDir` равен `<root>` для installed и exe directory для portable.
Файл `<root>/<runtime_name><ext>` имеет рядом `-staging` и PDB. Self-update не
отключается для installed-клиента на Windows/Linux/macOS.

Host выбирает runtime до settings и не знает `Common.GameName`, поэтому runtime
возвращает writable live path в `ClientRuntimeResult::RequestedRuntimePath`.
Host проверяет absolute path и basename, продвигает файл, записывает selector и
завершается через `GetInstalledClientRuntimeBootstrapPath()` и
`WriteClientRuntimeBootstrapTarget()`. Следующий `INSTALLED` startup читает selector из
`<Platform::GetUserDataBase()>/<FO_NICE_NAME>/ClientRuntimeHost/<runtime><ext>.path`
до settings. Он принимает только корректный путь, для которого существует live
или staging. Missing, oversized, relative, newline/NUL-containing,
wrong-basename и stale selectors возвращают host к install-dir runtime.
`--ClientLibPath` остаётся последним explicit override. Portable-клиент selector
не читает и не пишет.

Installed mode включается marker-файлом `INSTALLED` рядом с executable. Если
`Client.UserWritablePath` empty, marker автоматически выбирает `*`. MSI packager
добавляет marker только во временный Wix payload и удаляет перед продолжением,
поэтому соседние Raw/Zip артефакты остаются portable.

## Упаковка

[package.py](../../../../BuildTools/package.py) обслуживает обе стороны:

- client package содержит host и runtime с одинаковым basename рядом, например `<client-host>.exe` и `<client-host>.dll`;
- соседние обычные и headless client runtime не считаются dependency companions: generic DLL/DSO scan исключает все Engine-owned input и alias `Client`/`ClientLib` и `ClientHeadless`/`ClientLibHeadless`, после чего packager копирует только явно запрошенные variants под packaged basenames; оставшийся headless build output не попадает в обычный portable, ZIP или MSI payload, а token `Headless` по-прежнему добавляет явную headless-пару;
- server package размещает доступные runtime в `<Settings.PlatformBinaries>/<binary_target>/<output_name><runtime_ext>`, чтобы клиенты других платформ могли обновиться;
- Windows Client с `Wix` строит обязательный MSI из staged Raw payload, временно добавляет `INSTALLED`, регистрирует URI scheme через HKCU и падает при отсутствии toolset или ошибке generator;
- Windows runtime PDB называется `<runtime_dll>.pdb`, а host PDB сохраняет `<host_name>.pdb`; package patch CodeView `RSDS` меняет embedded PDB path на итоговое имя, и отсутствие input или неудачный patch считаются ошибкой;
- host PDB staged вместе с runtime payload, но client скачивает его только при отсутствии локального файла и никогда не clobber существующую подходящую копию.

Bundled runtime и server-staged runtime проходят тот же package-time patch, что и
обычные executable: embedded resources, internal config и packaged mark.
Variant config применяется к payload, который реально выполняет игру; например,
Windows OpenGL runtime получает `ForceOpenGL=1`. Embedded zip создаётся с
фиксированными timestamp и permissions, поэтому bundled runtime и соответствующий
server payload остаются byte-identical при раздельной упаковке Server/Client.

Resource zips также используют sorted normalized paths и стабильные metadata.
Incremental baker может touch неизменившийся output, но content-identical repack
не должен менять FNV descriptor hash и заставлять пользователей скачивать pack
заново. Это закрепляет
[test_package_zip_determinism.py](../../../../BuildTools/tests/test_package_zip_determinism.py).

Internal config patch area имеет фиксированную движком ёмкость 10000 bytes;
подключаемые проекты не могут менять её размер. Перед записью bootstrap config
`package.py` читает реальный reserved marker из binary.

`build_runtime_update_target_name` образует `Windows-win64`, `Linux-x64`,
`Linux-arm64`, `macOS-arm64`, `Android-arm64` и другие targets. Profiling получает
`_Profiling`; OGL staged отдельно с `ForceOpenGL=1`. Для
`FO_BINARY_OUTPUT_POSTFIX`, например `Steam`, каждый payload получает `_Steam`,
чтобы варианты не перезаписывались. `extract_binary_entry_postfix` сначала
удаляет как порядок packager (`-Profiling_X-Debug`), так и фактическое имя
multi-config CMake (`-Debug_Profiling_X`), включая именованные release- и
sanitizer-конфигурации, а затем извлекает необязательный postfix. Посторонние
или некорректные каталоги `Client-*` пропускаются и не прерывают корректную
упаковку. Совпадающая package declaration обязана явно задать тот же postfix
через `POSTFIX`: package-wide fallback нет. Client packager отражает
suffix в `bin_out_name`, `PACKAGED_BUILD_NAME` совпадает с именем server payload,
а `Updater.cpp::remap_runtime_name` выбирает правильный runtime. Legacy Win7 target
нормализуется к обычному Windows target, поэтому отдельность обеспечивает именно
explicit postfix.

## Практики встраивающего проекта

Настройки updater следует считать измеряемым release-решением, а не значениями,
которые можно скопировать из другой игры. Держите
`Network.UpdateFileMaxPortionSize` и `ServerNetwork.UpdateFilesInMemory` явными в
каждом deployment profile. Перед изменением измеряйте throughput, peak resident
memory, конкурентную нагрузку updater, disk latency и reconnect. Production может
держать payload в памяти, а development и staging читать его с disk, но release
record должен указывать фактически проверенный путь.

Portable и installed clients требуют разных acceptance lanes. Installed lane
проверяет первый native update, запись selector, загрузку при следующем запуске,
fallback для corrupt/stale selector и второе обновление при locked runtime.
Portable lane доказывает, что selector не читается и не пишется, а sibling runtime
остаётся self-contained. Наличие settings в `.fomain` само по себе не доказывает,
что эти сценарии работают; нужны executable package/updater tests.

Variant identity должна совпадать в package declaration, generated binary name,
`POSTFIX`, `PACKAGED_BUILD_NAME`, server payload и client runtime remapping.
Каждый распространяемый postfix проверяется отдельно, включая отсутствие
clobber между вариантами. Web, Android и iOS должны иметь отдельный store/manual
replacement workflow и не смешиваться с native module self-update Windows,
Linux и macOS.

Минимальный набор project-owned release tests:

- outdated runtime доходит до restart prompt, promotion, exit и успешного следующего запуска;
- corrupt resource/native binary, missing resource pack и отсутствие target payload на server завершаются предсказуемо;
- interrupted transfer возобновляется, stale staging восстанавливается, repeated outdated launch не зацикливается, invalid selector безопасно отклоняется;
- postfix-варианты не конфликтуют, а Windows PDB staged и разрешается debugger, если эти outputs публикуются;
- тестируется точный release package profile и directory server payload, а не только development build tree.

[Внутренний реестр evidence внешних проектов](https://github.com/cvet/fonline/blob/master/Docs/generated/external-project-evidence/index.md)
показывает, какие практики наблюдаются в поддерживаемых играх. Это сравнительное,
не нормативное evidence: переиспользуемый контракт определяют эта страница и
исходный код Engine.

### Граница подписи и доверия

Загруженный native runtime является исполняемым кодом. Transport encryption,
descriptor hashes и atomic promotion обнаруживают повреждение передачи и помогают
recovery, но не подтверждают identity издателя. Проект отвечает за authenticated
distribution, code signing, timestamp, защиту ключей, revocation и incident
response. Подробная граница описана в
[Безопасности и секретах](../../how-to/release/security-and-secrets.md) и
[Упаковке и выпуске](../../how-to/release/packaging.md).

Подписывать нужно окончательные patched host/runtime до archive или installer.
После signing проверяются signature и timestamp, а descriptor hashes связываются
с этими точными bytes. Acceptance запускает packaged server и подтверждает, что
его `PlatformBinaries/<target>/` byte-for-byte совпадает с одобренным артефактом.
Нельзя подписать один файл, а под тем же release identity обслуживать более
позднюю repackaged или config-patched копию.

### Матрица приёмки выпуска

| Ветка | Обязательное evidence |
|-------|-----------------------|
| Native portable client | загрузка sibling runtime, resource update, native update, restart, выполнение при следующем запуске, resume, rollback/staging recovery |
| Native installed client | read-only install, writable overlay, validation/persistence selector, native update, corrupt-selector fallback, повторное обновление |
| Variant/postfix | уникальные package/server names, совпадающий runtime remap, отсутствие cross-variant clobber |
| Windows symbols | восстановление host PDB, обновление runtime PDB, patched CodeView names, проверка signature после final patch |
| Web / Android / iOS | resource update где доступен, явный unsupported-native-update result, инструкция store/manual replacement |
| Packaged server | точный release profile, полный resource list, payload каждого target, memory/disk mode, missing-payload failure |
| Compatibility break | тексты отказа updater generation/runtime ABI, full-client reinstall, rollback и support instructions |

## Жизненный цикл

```text
<client-host> main
    ├── ResolveRequestedClientRuntime(argc, argv)        # Path + CompatibilityVersion + ExplicitPath
    │
    ├── RunClientFromLibrary(argc, argv, requested, *)   # CASE 2: bundled runtime exists
    │     ├── ApplyStagedBinaryUpdate(requested.Path)    # promote <requested>-staging (no-op when missing)
    │     ├── Platform::LoadModule + FO_QueryClientRuntimeExports
    │     ├── Validate exports + metadata
    │     ├── exports.Run(argc, argv, &result)           # DLL drives RunClientRuntime:
    │     │     ├── single Updater (UI) connects to the server. The connect result picks the mode:
    │     │     │     ├── Success         → resources mode → sync ClientResources, finish ResourcesReady
    │     │     │     └── CompatibilityOutdated:
    │     │     │             ├── if !CanSelfUpdate    → finish PlatformUnsupported, caller shows store msg
    │     │     │             └── else                  → binaries mode → write ClientBinaries to
    │     │     │                                          `<live>-staging`, try immediate promote, or verify `<live>`,
    │     │     │                                          finish BinariesStaged
    │     │     ├── On BinariesStaged: set ResultKind = ReloadRequested, RequestedRuntimePath
    │     │     ├── On any other non-success result: ShowUpdaterFailure(result) and quit
    │     │     └── unload of DLL (scope_exit) frees the loaded module
    │     └── If ResultKind == ReloadRequested: PromoteStagedReloadForRestart
    │           └── ApplyStagedBinaryUpdate(requested path), then exit
    │
    └── If LoadModule failed (CASE 1: no DLL yet, packaged install):
          if !CanFallbackToEmbeddedClient(requested): return false
          RunEmbeddedClient(argc, argv, *)               # host-module RunClientRuntime
          (same single-Updater flow as the DLL; host module's App.reset() runs after
           ReloadRequested before the host promotes the runtime and exits)
          if ResultKind == ReloadRequested → promote staged runtime, then exit
```

Один экземпляр `Updater` обслуживает resource и native-binary sync. Он сам
выбирает mode по результату connect; отдельного `BinaryUpdater`, stage-specific
constructor или headless variant нет. Splash UI общий, а terminal state доступен
через `Updater::GetResult()` как `UpdaterResult`.

`CanSelfUpdateNativeModules(GetCurrentUpdatePlatform())` разрешает binary update
для Windows, Linux и macOS. Web не имеет сопоставимого module mechanism, Android
включает runtime в APK, а iOS запрещает произвольный `dlopen`. При outdated
compatibility на этих платформах updater возвращает `PlatformUnsupported`, client
показывает просьбу обновиться через app store и завершается вместо бесконечной
попытки подключиться к игре.

## Проверка

| Симптом | Первый сигнал и действие |
|---------|--------------------------|
| Host не находит runtime и fallback невозможен | embedded updater не может получить module; message box сообщает `Failed to update native client modules for binary target <target>` |
| Поколение updater не совпадает | server log `Connected client X has outdated updater version Y`; старый client требует base package, generation 2+ просит latest full client package |
| Gameplay version outdated на self-update platform | resource pass сообщает compatibility outdated, binaries mode stages runtime, показывает restart prompt, возвращает `ReloadRequested`; host продвигает файл и выходит |
| Gameplay version outdated в Web/iOS/Android | message box `Client outdated, please update via your app store`, затем exit без native self-update |
| Неверный file index/offset | server warning `Wrong file index ...` или `Wrong update file offset ...`, затем disconnect |
| Client data не совпадает с server data | server log `Connected client X runs metadata version A while the server runs B`; updater log называет local version, server version и resource directory. Найдите каталог из другой bake |
| Server распространяет resources, на которых сам не работает | startup server завершается с `Distributed client resources were baked apart from the server resources`, называя оба resource directories и обе версии |
| Resources старше текущего metadata format | metadata-header startup failure: `does not start with the metadata file marker`, `file version does not match the engine` или `carries no version`; выполните full rebake |
| Server не имеет target payload | `Server doesn't provide a native client update for binary target <target>` |
| Остался staging | следующий startup выполняет `ApplyStagedBinaryUpdate` до load runtime |
| Linux каждый раз пишет `LoadModule failed` | проверить `-Wl,-Bsymbolic` и отсутствие initial-exec TLS; standalone `dlopen` выявит `cannot allocate memory in static TLS block`; silent embedded fallback иначе создаёт update loop |
| Update завершился и ждёт на screen | ожидаемое поведение: закрыть client после prompt; host продвинет runtime и выйдет, следующий процесс выполнит единственный `InitApp` |
| Runtime stack trace содержит только addresses | CodeView DLL должен ссылаться на sibling `<live>.dll.pdb`; package.py обязан patch RSDS и падает при ошибке |
| Host frames потеряли symbols, а runtime frames разрешаются | `<host_name>.pdb` не совпадает с frozen exe; восстановить PDB этой сборки или удалить mismatch для recovery, если сервер содержит подходящую копию |

Локальная приёмка:

1. Соберите и запустите generated unit-test target проекта. `Test_ClientRuntimeApi.cpp` проверяет ABI, exports/results, selector round-trip, validation, live selection, staging и fallback; `Test_DiskFileSystem.cpp` - parity hash и writable path; `Test_Platform.cpp` - user data base; `Test_Settings.cpp` - inheritance `UpdateFilesInMemory` и fail-safe `ResolveUserWritablePath`.
2. Соберите generated client host. На native platform зависимость должна построить runtime alias рядом с host. Отдельно соберите runtime target для isolated validation.
3. Запустите host с bundled runtime и подтвердите DLL happy path, resource update и вход в игру.
4. Запустите с `--ClientLibPath <path>` и валидным alternate runtime.
5. Добавьте `--ClientLibCompatibilityVersion <other>` и удалите runtime: host обязан завершиться без embedded fallback.
6. Укажите invalid path без strict compatibility: host должен перейти на embedded client.
7. Соберите project-owned packaged server и проверьте `PlatformBinaries/<target>/<name><ext>` и полный list resource zips.
8. Прервите network mid-download и подключитесь снова: `GetUpdateFile` должен продолжить с temp-file size без полной загрузки.
9. Соедините client со старой `FO_COMPATIBILITY_VERSION` и новый server. После prompt закройте client: host должен заменить `<live>-staging`, выйти без load и загрузить promoted runtime только при следующем запуске.
10. Убейте host во время binary download. При restart полный staging продвигается, а неполный temp продолжается обычным updater session.
11. Для installed smoke добавьте `INSTALLED`, оставьте `Client.UserWritablePath` empty и проверьте per-OS root, cache/log/overlay, selector и повторный launch. Затем испортите selector и подтвердите fallback к install-dir runtime.

Project release gate должен дополнительно запускать точные updater pipeline tests
для распространяемых package profiles: missing/corrupt packs, binary corruption,
server missing payload, repeated outdated launches, selector на каждой
поддерживаемой OS, postfix isolation, staging promotion, Windows PDB и restart.

## См. также

- [Упаковка и выпуск](../../how-to/release/packaging.md) - payload, signing, compatibility, rollout и rollback gates.
- [Процесс сборки](../../how-to/build/) - переиспользуемые точки входа build/package.
- [Архитектура движка](../architecture/) - владение engine, application и build layers.
- [Нативная отладка и отладка AngelScript](../../troubleshooting/debugging.md) - выбор host/runtime binary для debugger.
- Release-документация встраивающего проекта - его package profiles и альтернативные distribution channels.
