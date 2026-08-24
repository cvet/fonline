---
layout: default
title: Упаковка и выпуск
locale: ru
document_id: packaging-and-release
permalink: /Docs/ru/how-to/release/packaging.html
---

# Упаковка и выпуск

<!-- docs-translation: {"document_id":"packaging-and-release","locale":"ru","source_path":"Docs/en/how-to/release/packaging.md","source_sha256":"9da1af0d23d70ad0d6015b0d5f93a5b4351b16bd93c732bbea32bdd3a6758e49"} -->

Точная текущая grammar, совместимость target/platform, pack tokens, payloads и
command-line arguments находятся в сгенерированном
[package interface](../../reference/packages/index.md). Перед тем как
называть полученный artifact поддерживаемым, сверяйтесь с
[Матрицей поддержки](../../reference/platforms/support-matrix.md).

## Решение о пакете

Для каждого объявленного application variant соберите точную target, выполните
`ForceBakeResources` для его release configuration, затем вызовите
сгенерированную цель `MakePackage` и проверьте изолированный artifact. Текущие
output-producing packs: `Raw`, `Zip`, `SingleZip`, `Tar`, `TarGz`, `Root`,
`Wix` и `Apk`; допустимые packs всё равно зависят от target и platform.
Реализованный payload или pack является capability, а не support claim.
Встраивающий проект владеет своей package
matrix, release policy, signing credentials, distribution, acceptance, rollout
и rollback. Возможность упаковки Engine не доказывает эти проектные решения.
В частности, `Apk` копирует выбранный signed **или debug** APK; pack token не
доказывает release signing. Описывайте проверенную capability и её evidence, а
не гарантию Engine для каждого host или project.
Не утверждайте, что output packs можно комбинировать с любым target/platform:
используйте только реализованную совместимую строку generated matrix.
`Debug+Apk` выбирает Gradle `assembleDebug`, development artifact которого
подписан Gradle debug key; это debug-signed, а не unsigned и не production
release-signed artifact.

Текущие допустимые targets: `Server`, `Client`, `Mapper`, `Baker`,
`AnimationViewer` и `ParticleViewer`. Упаковка payload реализована для
`Windows`, `Linux`, `Android` и `Web`; `macOS` и `iOS` являются принимаемыми
parser dimensions, package implementations которых имеют статус `unsupported`.
Реализованные payloads: native PE с companion files, native ELF с companion
files, Android Gradle client project с ABI libraries и assets, а также browser
JavaScript/Wasm client с preloaded resources.

Называйте artifact, создаваемый выбранным совместимым output pack: `Raw`
сохраняет staged directory; `Zip` создаёт ZIP; `SingleZip` дополняет один
package-wide ZIP; `Tar` создаёт tar archive; `TarGz` создаёт gzip-compressed tar
archive; `Root` объединяет staged directory с package output root; `Wix`
создаёт MSI; `Apk` копирует выбранный signed или debug APK. Это проверенные
packager capabilities, а не универсальные гарантии Engine или evidence релиза
проекта.

## Знайте, что доказывает Engine

Engine предоставляет переиспользуемую package grammar, staging и patching
бинарных файлов, встроенные resources/configs, архивы, генерацию MSI/APK и
проверочные fixtures. Эти механизмы доказывают только конкретные пути, которые
запущены на конкретной ревизии.

Игровой проект остаётся владельцем:

- фактического release matrix и поддерживаемых host/target combinations;
- product identity, versioning, channels и publication destinations;
- signing credentials, trust policy, timestamping и verification;
- installer/store metadata, deployment, rollout и rollback;
- device/browser/renderer/audio/input/network acceptance;
- database migration, backup/restore и operational readiness;
- лицензий, attribution и происхождения добавленных assets/dependencies.

Не называйте package production-ready только потому, что `package.py` создал
файл. Build capability, packager capability, project qualification и
publication — разные уровни evidence.

## Подготовьте принадлежащую релизу package matrix

До написания `DefinePackage` запишите для каждой release candidate:

- package ID и selected sub-config;
- target, platform, architecture и все binary variants;
- host, compiler/toolchain и workspace image;
- обязательные pack tokens и ожидаемые artifacts;
- signing identity и источник secret без значений secret;
- acceptance job, устройство/browser/service environment;
- updater, network, save/database и rollback compatibility;
- владельца publication и recovery decision.

Один package ID должен объединять только те entries, чьи compiled inputs
доступны в одном `FO_OUTPUT_PATH` и проходят общий процесс владения. Разделяйте
IDs, когда различаются build hosts, credentials, acceptance lanes или
publication destinations.

## Объявите packages

Вызывайте `DefinePackage(...)` после регистрации project sources и до
`BuildPackages()`. Используйте отдельные package IDs, если различаются build
hosts, credentials, acceptance lanes или publication destinations.

```cmake
DefinePackage(ReleaseWindows
    CONFIG PublicRelease
    BINARY Client Windows win64 Raw+Zip+Wix
    BINARY Server Windows win64 Headless+Service+Raw+Zip)

DefinePackage(ReleaseLinux
    CONFIG PublicRelease
    BINARY Server Linux x64 Headless+Daemon+Raw+TarGz)

DefinePackage(ReleaseWeb
    CONFIG PublicRelease
    BINARY Client Web wasm Raw+Zip+WebServer)

DefinePackage(ReleaseAndroidArm64
    CONFIG PublicRelease
    BINARY Client Android arm64 Raw+Apk)
```

Это пример grammar, а не заявление о поддержке и не универсальная production
matrix. Удалите каждую строку, которую игра не собирает и не квалифицирует.

Каждая clause `BINARY` имеет форму:

```text
BINARY <target> <platform> <architecture> <pack+tokens> [POSTFIX <variant>]
```

`CONFIG` выбирает baked sub-config. Для resource-bearing client и server
packages baking должен создать `Baking/Configs/<config>.fomain-client` или
`...-server` через baker `Config`. Package target не создаёт незаметно
отсутствующие application binaries или baked resources.

Используйте `POSTFIX`, когда отдельно собранный binary variant имеет
`FO_BINARY_OUTPUT_POSTFIX`. Значение declaration должно совпадать с build
output. Это согласует выбор package input, packaged runtime identity и имена
server-staged updater payload. Не используйте package name как неявный
variant selector.

Используйте `INCLUDE <source-glob> <target-path>` только для проверенных
распространяемых файлов, уже находящихся под build output. Packager отклоняет
escaping paths и обновляет package root и существующий `SingleZip`. License
notices, attribution и approval third-party payload остаются обязанностями
проекта.

## Соберите, запеките, затем упакуйте

Выполняйте стадии явно и останавливайтесь после первой ошибки:

1. Начните с чистого рекурсивно инициализированного game checkout на release
   revision и проверьте точный Engine SHA.
2. Подготовьте host закреплённой workspace command Engine и preset или CI image
   встраивающего проекта.
3. Сконфигурируйте проект с release-owned cache values. Не переиспользуйте
   необъяснённый developer cache.
4. Соберите каждый application variant из package declaration.
5. Выполните forced bake release resources и configs, если candidate не должен
   зависеть от incremental state.
6. Вызывайте `MakePackage-<package-id>` только после появления binary и baking
   inputs.
7. Сохраните полный package log и отклоняйте assertions, warnings-as-errors,
   signing failures, missing symbols, configs или resource packs.

Конкретные target names выбирает проект. Типичная multi-config sequence:

```bash
cmake --preset release-host
cmake --build Build/release-host --config Release --target <application-targets>
cmake --build Build/release-host --config Release --target ForceBakeResources
cmake --build Build/release-host --config Release --target MakePackage-ReleaseWindows
```

Не запускайте несколько platform entries из одного package ID, если их
compiled inputs недоступны в одном `FO_OUTPUT_PATH`. Отдельные IDs упрощают
владение cross-build и диагностику failures.

Packager изменяет зарезервированные data regions после linking. Он встраивает
resources и выбранный baked config, записывает packaged build name и может
изменять PE PDB paths. Он не генерирует и не исполняет код в этих regions.
Если настроена подпись, она выполняется после patching и до создания archives
или installers.

## Запустите packaging fixture Engine

`Examples/PackagingMatrix` — исполняемая Engine-owned база для нативных package
mechanics. Она намеренно отделена от читаемых starter и multiplayer tutorials,
поскольку `ConfigBaker` требует инициализации каждого server/client runtime
setting. Checked-in `FOnlinePackagingMatrix.fomain` детерминированно генерируется
из `Source/Common/Settings.inc`; `generate_config.py --check` завершается
ошибкой при расхождении settings и fixture.

В отдельном checkout `Examples/PackagingMatrix` с инициализированным submodule
`Engine` сконфигурируйте host build и соберите принадлежащую fixture цель
`RunPackagingChecks`. Эта цель не зарегистрирована как обязательный lane
Engine `BuildTools validate`.

```bash
cmake --build <packaging-matrix-build-dir> --config Release --target RunPackagingChecks
```

Каждый маршрут собирает client, headless client, server, headless server,
host service/daemon role и baker; принудительно запекает resources и
server/client configs `PackageSmoke`; создаёт raw payloads и ZIP или TAR.GZ;
сравнивает archive members со staged payloads; запускает packaged headless
client против packaged server через реальный updater handshake. Оба процесса
должны увидеть `Common.Packaged`, использовать embedded fixture setting,
вывести success markers и завершиться с code zero.

Verifier записывает `FOPKG-PackageSmoke/packaging-manifest.json` с точной
ревизией Engine, hashes и sizes архивов, полной инвентаризацией payload, наличием
roles и runtime results. Сохраняйте manifest и archives в release lane
подключающего проекта, когда это evidence обязательно; текущий Engine workflow
их не публикует.

Fixture даёт необязательное evidence для Engine package path Windows x64 или
Ubuntu/Linux x64 на host, где он был запущен. Он не квалифицирует
package declaration другой игры, signing, installer, store, deployment host,
database, renderer или rollback. Перенесите evidence pattern в release lane
встраивающего проекта и храните конкретную acceptance там.

### Приёмка package публичного multiplayer-примера

`Examples/MinimalMultiplayer` применяет тот же pattern к читаемым игровым
исходникам. Его package `Tutorial` принудительно запекает automated gameplay
configuration и создаёт нативные raw и ZIP/tar.gz client/server payloads.
Собственный verifier примера проверяет parity archive/payload, записывает
SHA-256 каждого archive и payload file и запускает взаимодействие packaged
headless server/client с картой и предметом через общий gameplay process
runner.

Checked-in `.fomain` генерируется из текущих defaults `Settings.inc` и
проверенных tutorial overrides/sections. `CheckTutorialConfig` выполняется до
baking, поэтому новый или изменённый saved setting обнаруживает stale source,
а не проявляется позднее как неполный packaged config.

Presets `windows-package` и `linux-package` в `Examples/MinimalMultiplayer`
по запросу собирают принадлежащую fixture цель `RunTutorialPackageChecks`.
Сохраняйте archives, package manifest и runtime report в workflow проекта,
когда они обязательны; текущий Engine workflow эти presets не запускает.
Эти evidence уже, чем product release: archives являются
unsigned, headless и audio-disabled fixtures без installer, store, public
deployment, durable backend, upgrade или rollback claim.

Windows x64 lane прошла локально на Engine `fac978a67`: два archives совпали с
raw payloads, inventories client из 28 файлов и server из 37 файлов были
захэшированы, packaged interaction scenario прошёл. Это только local host
evidence. Для Linux support и immutable example-release evidence необходимы
зелёная landed job и проверенный внешний repository commit/tag.

## Выберите artifacts по платформе

### Windows client

- `Raw` сохраняет staged portable directory.
- `Zip` создаёт portable archive из этой directory.
- `Wix` создаёт MSI и требует путь WiX/wixl, используемый
  `BuildTools/msicreator`.
- `OGL` добавляет отдельно собранный OpenGL runtime variant.
- `Lib` выбирает library form там, где target её поддерживает.
- `POSTFIX` не даёт независимо собранным variants, например depot-specific
  client, конфликтовать.

MSI не доказывает, что client подписан, доверен endpoint protection, совместим
при upgrade или принят distribution channel. Проверяйте эти свойства на
финальном emitted artifact.

### Linux client или server

- Доступны output forms `Raw`, `Zip`, `Tar` и `TarGz`.
- `Headless` добавляет headless variant к обычной target.
- `Daemon` добавляет Linux daemon server variant.
- `TotalProfiling` и `OnDemandProfiling` добавляют отдельно скомпилированные
  profiling variants там, где они допустимы.

Сохраняйте executable modes, если downstream publication system распаковывает
и переупаковывает archive. Квалифицируйте фактический Linux distribution,
runtime libraries, filesystem paths, process account, signals, logs и service
manager игры.

### Web client

Payload Web client содержит JavaScript, patched Wasm, HTML shell, preloaded
`Resources.data` / `Resources.js` и optional helper `WebServer`. Принадлежащий
Engine-команда Content Showcase `python validate.py --web-runtime` может дать
необязательное evidence для baking на native-хосте,
точный состав raw/ZIP package, localhost HTTP delivery, подключение к native-серверу,
обязательные lifecycle-маркеры, настоящий контекст WebGL 2 и пиксели композитора
для одного детерминированного fixture Content Showcase в закреплённом Chromium.
Она не входит в обязательный Engine workflow и не доказывает публичный browser
deployment подключающей игры.

Для local staging следуйте [сборке, упаковке и отладке в браузере](../platforms/web-debugging.md). Release
lane должна дополнительно проверить HTTPS hosting, MIME types, cache policy,
cross-origin isolation или другие обязательные headers, WebSocket reachability,
browser compatibility, storage persistence, audio activation, UX ошибки
loading и хотя бы одну видимую representative scene.

### Android client

Android payload является сгенерированным Gradle project с одной `libmain.so`
на выбранную ABI и baked resources в application assets. `Apk` запускает
Gradle assembly и копирует полученный APK рядом со staged project.

Закреплённые SDK/NDK workspace, ABI mapping, device connection, resource
staging и configuration fields описаны в
[сборке, упаковке и отладке на Android](../platforms/android-debugging.md). Android ARM32 и ARM64 имеют
build gate; Android x86 остаётся source-capable. Игра владеет emulator/device
gates, GPU/input/audio/network/background behavior, signing identity,
versioning, store policy и rollout.

APK без release keystore settings использует development key Gradle и не
является production release artifact.

### macOS и iOS

Сейчас `package.py` прерывает работу и для `macOS`, и для `iOS`. Не добавляйте
для них строки `DefinePackage`. Support matrix проверяет build inputs client в
более узкой области, но встраивающий проект должен предоставить и сопровождать
application-bundle assembly, resources, entitlements, provisioning, signing,
notarization где применимо, device/simulator checks, store metadata и delivery.
Пока этот путь не станет существующим и повторяемым, описывайте Apple targets
как build-gated inputs, а не packaged или release-supported products.

### Server, service и daemon

Server package включает server resources и client update resource packs. При
наличии совместимых client runtime libraries он также staging-ит platform
runtime payloads для updater. Перед публикацией server с самообновляющимися
clients прочитайте [Client Runtime Split and Updater](../../explanation/runtime/client-updater.md).

`Service` и `Daemon` — binary variants, а не deployment systems. Игра должна
версионировать и проверять:

- process arguments и environment;
- least-privilege account и filesystem permissions;
- service-manager definition и restart limits;
- network exposure и TLS termination;
- database schema, credentials, migration, backup и restore;
- logs, metrics, crash reports, health checks и alerting;
- graceful drain/shutdown и rollback к совместимому binary/config/data set.

Храните эти product и infrastructure details во встраивающем проекте.
[эксплуатация релиза](operations.md) предоставляет
переиспользуемый runbook process, readiness, rollout, shutdown и rollback, не
объявляя инфраструктуру собственностью Engine.

## Воспроизводимость и происхождение

FOnline делает ZIP entries resource pack детерминированными: сортирует
нормализованные paths и фиксирует timestamps и permissions ZIP. Embedded
resource ZIP data использует то же правило. Parser package declarations и
сгенерированный contract детерминированы и проверяются в CI.

Это не делает каждый полный release бит-в-бит воспроизводимым. Linked binaries,
debug symbols, top-level archives, toolchains MSI/APK, signing timestamps,
included files и external SDKs могут содержать host- или time-dependent data.
Заявляйте только ту узкую гарантию, которую действительно проверили.

Для каждого candidate создавайте или сохраняйте artifact manifest как минимум
с такими данными:

- полные SHA игры и Engine;
- dirty-tree status;
- submodule revisions;
- host image, compiler/linker, Python, CMake, версии SDK/NDK/Gradle/WiX;
- package ID, config, target, platform, architecture и pack tokens;
- упорядоченные artifact paths, sizes и SHA-256 hashes;
- signing subject/key alias, результат проверки signature и timestamp status
  без credentials;
- test job/run identifiers и acceptance result;
- third-party license/provenance inventory;
- updater generation/runtime ABI, если artifact участвует в native updates.

Вычисляйте hashes из фактически публикуемых файлов после signing и final
packaging. Храните immutable manifests рядом с immutable artifacts. Сравнение
двух hashes осмысленно только при совпадающих заявленных input и signing
policies.

## Границы подписи и секретов

Windows signing является optional и по умолчанию отключён.
`Packaging.CodeSigningHook` указывает на project-owned executable через
непосредственно используемый несекретный path в project config; packager не
разрешает для него target directives. Затем он вызывает hook по одному
разу для каждого staged `.exe` и `.dll` после binary patching и до создания
archive/MSI. Ненулевой exit hook завершает packaging ошибкой. Hook владеет
signing provider, certificate, timestamp service, retry policy, credential
environment и verification.

Android release fields берутся из `Android.Keystore`,
`Android.KeystorePassword`, `Android.KeyAlias` и `Android.KeyPassword` в baked
effective target config. Password strings передаются Gradle через отдельные
environment variables и не записываются в generated project, но до этого уже
проходят через baked config, а target directives не разрешаются. Поэтому текущий
Engine не имеет host-only handoff Android signing secrets; не помещайте
production credentials в Engine config и используйте защищённую project-owned
стадию signing.

Никогда не коммитьте private key, token, password, keystore password, signing
session, production endpoint secret или decrypted credential в `.fomain`,
package include, log, test fixture, documentation page, artifact manifest или
CI artifact. Используйте secret manager проекта, ограничивайте credentials
packaging job, редактируйте command output и проверяйте, что fork/untrusted jobs
не могут запросить secrets. Записывайте identities и verification results, а
не secret values.

[безопасность и секреты](security-and-secrets.md) владеет substitution
timing, узкой границей masking `Common.SecretSettingTokens`, текущими
ограничениями package secrets, CI isolation, rotation/revocation, incident routing и проверкой
artifact на отсутствие secrets.

Signing доказывает integrity artifact и identity publisher. Он не доказывает
gameplay correctness, отсутствие malware, store acceptance, updater
compatibility или безопасный rollback.

## Матрица приёмки

Сначала выполняйте узкие Engine checks, затем game-owned release checks.
Зелёной сборки недостаточно.

| Слой | Минимальное release evidence |
|---|---|
| Contract | Tests/checks package model и отсутствие необъявленного grammar drift |
| Clean inputs | Точные чистые game/Engine revisions и инициализированные submodules |
| Build | Чистый release configure и все объявленные binary variants |
| Bake | Свежие resources, scripts, metadata и selected client/server config |
| Package | Ожидаемое payload tree и каждый requested artifact |
| Contents | Allowlist/denylist, symbols policy, licenses, отсутствие secrets и stale files |
| Integrity | Final SHA-256 manifest и signature verification, где требуется |
| Install/start | Реальный archive/installer/APK/site/service route на representative target |
| Runtime | Login/connect, representative map/content/UI, save/persistence и clean shutdown |
| Platform | Renderer, input, audio, networking, lifecycle, permissions и update behavior |
| Server operations | Service/daemon lifecycle, database migration, backup/restore и observability |
| Compatibility | Network, save/schema, updater generation/runtime ABI и old-client policy |
| Recovery | Восстановление предыдущих artifact/config/data в пределах отрепетированного time objective |

Повышайте package row до project-qualified только при наличии именованной,
версионируемой, повторяемой и обязательной для release lane. Записывайте failure
как failed candidate; не перезаписывайте прежний immutable artifact под той же
версией.

## Чек-лист выпуска

1. Зафиксируйте game revision, точную Engine revision, dependency graph,
   package declarations и release config.
2. Убедитесь, что target rows package-capable и их support labels актуальны.
3. Начните с clean workspace и запишите версии toolchain/SDK.
4. Соберите каждый объявленный application variant без warnings.
5. Выполните force bake и проверку точных release configs и content.
6. Упакуйте каждый host-owned package ID отдельно и сохраните полные logs.
7. Проверьте payload contents, license/provenance records, writable-path
   behavior, symbols и отсутствие secrets.
8. Где требуется, подпишите и проверьте signatures, затем захэшируйте final
   artifacts и запишите manifest.
9. Установите или разверните emitted artifact через реальный distribution path
   и выполните declared acceptance lane.
10. Проверьте updater, network, save/database и rollback compatibility с
    версиями, которые игра продолжает поддерживать.
11. До широкого rollout отрепетируйте или проверьте
    [backup/restore](backup-and-recovery.md) и rollback предыдущего
    release.
12. Публикуйте immutable artifacts и manifests, наблюдайте staged rollout и
    сохраняйте предыдущий совместимый release до завершения acceptance.

## Маршрутизация ошибок

| Симптом | Сначала проверьте |
|---|---|
| `Config file not found` | Baker `Config`, package `CONFIG`, `Baking/Configs` и fresh bake output |
| Отсутствует binary input | Built target/variant, platform architecture key, `POSTFIX` и общий `FO_OUTPUT_PATH` |
| Unknown или invalid pack token | Generated package matrix, а не запомненные комбинации |
| Package успешен, но requested artifact отсутствует | Output-producing pack token и final package output path |
| Signing пропущен | Разрешение project config и `Packaging.CodeSigningHook` или Android keystore fields |
| Signing завершился ошибкой | Isolated signing hook/Gradle job, credentials, timestamp service и final-byte order |
| Web files загружаются, но игра не подключается | HTTP/browser diagnostics, WebSocket endpoint и embedded release config |
| APK устанавливается, но resources отсутствуют | Generated assets, runtime staging, app update/version behavior и storage logs |
| Service или daemon сразу завершается | Сначала package payload, затем project-owned account/config/database/network/log policy |
| Update зациклен или загружает неверный runtime | Package `POSTFIX`, staged runtime name, updater generation/ABI и server payload inventory |
| Одни sources дают разные hashes | Сравните полный input/tool/signing manifest до заявления о недетерминизме packager |

## Проверенные исходные пути

- `BuildTools/PackageInterface.json`
- `BuildTools/package.py`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/stages/Packages.cmake`
- `BuildTools/tests/test_docs_package.py`
- `BuildTools/tests/validate_package_interface.cmake`
- `BuildTools/tests/test_package_include.py`
- `BuildTools/tests/test_package_security.py`
- `BuildTools/tests/test_package_zip_determinism.py`
- `BuildTools/tests/test_packaging_matrix.py`
- `BuildTools/tests/test_minimal_multiplayer_package.py`
- `BuildTools/msicreator/createmsi.py`
- `BuildTools/check_windows7_imports.py`
- `BuildTools/SupportMatrix.json`
- `.github/workflows/validate.yml`
- `Examples/PackagingMatrix/`
- `Examples/MinimalProject/`
- `Examples/MinimalMultiplayer/`

## См. также

- [Generated Package Interface](../../reference/packages/index.md)
- [Build Workflow](../build/)
- [Project-Local Dependencies](../../../ProjectDependencies.md)
- [Матрица поддержки](../../reference/platforms/support-matrix.md)
- [Конфигурация проекта](../build/project-configuration.md)
- [Generated Content Workflow](../build/generated-content.md)
- [Сборка, упаковка и отладка в браузере](../platforms/web-debugging.md)
- [Сборка, упаковка и отладка на Android](../platforms/android-debugging.md)
- [Client Runtime Split and Updater](../../explanation/runtime/client-updater.md)
- [Эксплуатация релиза](operations.md)
- [Обновление Engine](../migration/engine-upgrade.md)
