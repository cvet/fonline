---
layout: default
title: Сборка, упаковка и отладка FOnline на Android
locale: ru
document_id: android-debugging
permalink: /Docs/ru/how-to/platforms/android-debugging.html
---

<!-- docs-translation: {"document_id":"android-debugging","locale":"ru","source_path":"Docs/en/how-to/platforms/android-debugging.md","source_sha256":"0bf03c58c6aaee97beb3359b3cf58e2e1cbc5c78f9a84ea484df4821f535c81f"} -->

# Сборка, упаковка и отладка FOnline на Android

Это принадлежащая Engine инструкция по сборке Android-клиента, созданию и сборке APK, установке через Wi-Fi ADB, подключению к серверу разработки и разделению ошибок сборки, пакета, устройства и runtime. Она опирается на текущую реализацию BuildTools, шаблон Android-проекта, модель поддержки, грамматику пакетов, settings и границу updater. Встраивающий проект отвечает за идентичность приложения, release-политику, парк устройств, серверный профиль, доставку через магазин и evidence приёмки.

## Статус контракта

Это production-контракт переиспользуемого пути Android-клиента на текущей ревизии Engine. Нормативны исходный код Engine, проверяемые модели и тесты. Last Frontier и FOnline TLA служат только закреплённым discovery- и compatibility-evidence; их имена задач, application id, объявления пакетов и CI-покрытие не расширяют поддержку Engine.

Страница автономно применима из checkout встраивающего проекта, где Engine находится в `Engine/`. Заменяйте `<ProjectDevName>`, `<Config>`, `<application-id>` и пути пакетов значениями проекта. Project evidence закреплён в `BuildTools/ExternalProjectEvidence.json`; все переиспользуемые утверждения этой страницы повторно выведены из исходников Engine.

Для Android существуют четыре независимых слоя evidence:

1. cross-build C++-клиента;
2. generated Gradle project и APK;
3. установка и runtime на устройстве или эмуляторе;
4. project release qualification и публикация в магазине.

Успех одного слоя не квалифицирует следующий.

## Область и источники истины

Владельцы контракта в Engine:

- `BuildTools/buildtools.py` и `BuildTools/prepare-workspace.sh` для platform id, подготовки хоста, SDK/NDK pins, native-сборок и локального package helper;
- `BuildTools/PackageInterface.json`, `BuildTools/package.py` и `BuildTools/cmake/stages/Packages.cmake` для грамматики пакетов, ABI staging, генерации Gradle, выбора APK и очистки artifacts;
- `BuildTools/android-project/` для Gradle wrapper, Android plugin, manifest, базового SDL activity, ресурсов и Java-шаблона;
- `BuildTools/android_device.py` для Wi-Fi ADB discovery, выбора endpoint, установки, запуска, остановки и фильтрованных логов;
- `Source/Common/Settings.inc` для зарегистрированных Android package settings;
- `Source/Client/Updater.*` для границы Android native update;
- `ThirdParty/android-sdk`, `ThirdParty/android-ndk` и `ThirdParty/android-api` для закреплённых toolchain inputs;
- `BuildTools/SupportMatrix.json` и `.github/workflows/validate.yml` для ограниченных evidence меток поддержки.

Встраивающий проект отвечает за baking ресурсов, собственные config и server, Android identity и signing material, дополнительные SDK, объявления пакетов, CI, устройства, требования магазина и видимую/runtime-приёмку.

## Матрица поддержки и квалификации

| BuildTools platform | Android ABI | Уровень поддержки Engine | Текущий gate Engine |
|---|---|---|---|
| `android-arm32` | `armeabi-v7a` | build-gated | Ubuntu cross-build client shared-library/package inputs |
| `android-arm64` | `arm64-v8a` | build-gated | Ubuntu cross-build client shared-library/package inputs |
| `android-x86` | `x86` | source-capable | доступный validation target без обязательного Engine CI lane |

Публичная Android-поверхность включает client shared library и package input. Она не означает квалификацию Android server, Mapper, Baker, viewer, service, emulator, physical device, signing, store, graphics driver, input, audio, lifecycle или networking. `android-arm64` является обычным выбором для физического устройства, но это рекомендация workflow, а не более широкое обещание поддержки.

Текущие build gates Engine используют cross-build host на Ubuntu 24.04. Другие хосты могут запускать части CMake, `package.py` или Gradle, но эта матрица их не квалифицирует. Владельцем поддержки является [Матрица поддержки](../../reference/platforms/support-matrix.md); проектные расширения фиксируйте отдельно.

## Подготовка хоста и workspace

Linux-пути нужны Python, общие build prerequisites, Java 17 и доступное для записи место в workspace. На новом хосте установите Android package group и подготовьте закреплённые SDK/NDK одним маршрутом:

```bash
bash Engine/BuildTools/prepare-workspace.sh android-packages android-arm64
```

Если системные пакеты уже установлены, подготовьте только локальные для workspace SDK и NDK:

```bash
python3 Engine/BuildTools/buildtools.py prepare-workspace android-sdk android-ndk
```

`prepare-workspace.sh android-arm64` является эквивалентным host wrapper для второго маршрута. Текущие descriptors выбирают Android SDK command-line tools `14742923`, NDK `r29` и native API level `23`. Подготовка SDK принимает лицензии и устанавливает `platform-tools`, build-tools `34.0.0` и platform `android-35` в `Workspace/android-sdk`; NDK устанавливается в `Workspace/android-ndk`.

BuildTools разрешает расположение SDK/NDK в таком порядке:

- подготовленные пути `FO_WORKSPACE`;
- системный Linux SDK `/usr/lib/android-sdk`, если не заданы обе SDK variables;
- явные `FO_ANDROID_HOME` / `FO_ANDROID_SDK_ROOT` и, где применимо, `FO_ANDROID_NDK_ROOT`.

Проверяйте эффективные пути и pins через `python3 Engine/BuildTools/buildtools.py env --summary`. Наличие интерфейса Android Studio само по себе не доказывает готовность toolchain.

### Связь уровней SDK

Package defaults равны `MinSdk = 23`, `TargetSdk = 35` и `CompileSdk = 35`; native build использует независимый pin `ThirdParty/android-api`. Сейчас BuildTools подставляет эти значения, но не проверяет их числовую связь. Проект должен обеспечивать:

```text
native API <= MinSdk <= TargetSdk <= CompileSdk
```

Подготовленный SDK содержит только объявленный набор обязательных platform/build-tools. При изменении `Android.CompileSdk` проект должен явно предоставить соответствующую platform. Успешный C++ cross-build не доказывает, что Gradle найдёт изменённый compile SDK или что магазин примет target SDK.

## Сборка и staging локального debug-пакета

Сначала выполните свежий baking ресурсов встраивающего проекта. Затем соберите native client и создайте отдельное Gradle tree для каждого запрошенного sub-config:

```bash
# Run the embedding project's fresh BakeResources route first.
python3 Engine/BuildTools/buildtools.py build android-arm64 client RelWithDebInfo
python3 Engine/BuildTools/buildtools.py package-android-debug <ProjectDevName> android-arm64 <Config>
```

`package-android-debug` принимает и несколько config names за один вызов. Команда сопоставляет BuildTools platform с Android ABI и запускает `package.py` как `Client`, `Android`, `Raw` с выбранным config. Gradle она не запускает.

Generated project находится здесь:

```text
Workspace/android-debug/<ProjectDevName>-Client-<Config>-Android
```

Packaging выполняет следующие шаги:

1. требует baked client resources и совпадающий build hash;
2. копирует шаблон Android-проекта Engine в чистое output tree;
3. копирует `lib<ProjectDevName>_Client.so` в `app/libs/<abi>/libmain.so` и патчит embedded resources, config, build name и packaged marker;
4. перемещает baked client resource ZIPs в `app/src/main/assets/Resources`;
5. накладывает Android-значения из authored root и выбранного sub-config;
6. патчит Gradle, manifest, activity, strings, icon, дополнительные Java sources и поля SDK/NDK.

Generated tree является одноразовым build output. Меняйте шаблон Engine или authored setting проекта, а не поддерживайте правки внутри `Workspace/android-debug`.

### Разные значения debug для native и Gradle

Приведённая команда собирает native library в `RelWithDebInfo`. Следующий ниже `assembleDebug` выбирает Android/Gradle debug build type и debug signing key. Это независимые решения. Локальный helper намеренно запрашивает у `package.py` `Raw`, а не его binary-selection token `Debug`, поэтому не называйте native library Debug-сборкой C++, если она действительно не была так собрана и упакована.

## Сборка и проверка debug APK

Соберите generated project:

```bash
cd Workspace/android-debug/<ProjectDevName>-Client-<Config>-Android
./gradlew --no-daemon assembleDebug
```

Обычный результат находится в `app/build/outputs/apk/debug/app-debug.apk`. Текущий шаблон использует Gradle 8.12 и Android Gradle Plugin 8.7.3, фильтрует native libraries по списку упакованных ABI, не сжимает ZIP assets и использует legacy JNI library packaging. Manifest требует OpenGL ES 3.0 и объявляет permissions для Internet, network state и vibration; touchscreen, gamepad, Bluetooth, USB host и pointer features уровня PC опциональны.

В шаблоне задано `lint.abortOnError = false`. Поэтому сборка APK не является gate для lint, policy, privacy, vulnerabilities или готовности к магазину. Добавляйте project-owned lint и release checks и не считайте `assembleDebug` production-приёмкой.

Перед установкой проверьте generated artifact:

- application id, version code/name, min/target SDK, ABI, activity, permissions и signature;
- ровно один ожидаемый `libmain.so` на объявленный ABI;
- `assets/Resources/Metadata.zip` и ожидаемые resource packs;
- отсутствие keystore passwords, private credentials, local paths, stale configs и нелицензированных SDK payloads.

Для release lane используйте инструменты Android SDK, например `apkanalyzer`, `aapt2` и `apksigner` из подготовленного SDK.

## Конфигурация Android-пакета

Fixed package settings зарегистрированы в `Source/Common/Settings.inc` и экспортируются на стороне server/config baking:

| Setting | Default | Контракт |
|---|---|---|
| `Android.PackageName` | `com.fonline.app` | Gradle namespace/application id и generated Java package. |
| `Android.VersionCode` | `1` | Целочисленный Android package version code. |
| `Android.MinSdk` | `23` | Minimum SDK Gradle; не ниже native API pin. |
| `Android.TargetSdk` | `35` | Target SDK Gradle; store policy принадлежит проекту. |
| `Android.CompileSdk` | `35` | Compile SDK Gradle; соответствующая SDK platform должна быть установлена. |
| `Android.ScreenOrientation` | `landscape` | Значение orientation для activity в manifest. |
| `Android.Icon` | `Engine/Resources/Radiation.png` | Существующий PNG, копируемый во все legacy mipmap densities. |
| `Android.Keystore` | пусто | Путь release keystore, читаемый из baked target config и разрешаемый относительно project config. |
| `Android.KeystorePassword` | пусто | Пароль release store. |
| `Android.KeyAlias` | пусто | Alias release key. |
| `Android.KeyPassword` | пусто | Пароль release key. |

Version name нельзя независимо настроить в текущем packager. Он равен первым восьми символам package build hash или `1.0`, если hash не передан. Если продукту нужен семантический Android version name, расширьте и протестируйте package contract до публикации вместо редактирования generated Gradle files.

Проверка icon подтверждает только наличие файла и PNG signature. Одни и те же байты копируются во все текущие mipmap directories; resizing, adaptive icon generation, foreground/background split и визуальная проверка отсутствуют. Предоставьте project-owned source, приемлемый на всех launcher sizes, и проверяйте его в магазине и на устройствах.

### Package-only семейства расширений

Authored root и выбранный sub-config также могут содержать:

| Prefix | Поведение |
|---|---|
| `Android.ManifestMetaData.<name>` | Создать одно экранированное непустое `<meta-data>` value под `<application>`. |
| `Android.GradleMavenRepository.<name>` | Добавить непустой URL Maven repository. |
| `Android.GradleDependency.<name>` | Вставить непустой project-owned Gradle dependency statement. |
| `Android.JavaSource.<name>` | Скопировать `.java` file в generated application package и заменить `$PACKAGE$` / `$CONFIG$`. |

Keys обрабатываются в sorted order. Basenames Java sources должны быть уникальными: packager запрещает `FOnlineActivity.java`, но не предоставляет общий collision- или class-package-validator. Dependency statements считаются доверенным project input. Перед release проверяйте repositories, dependency locks, licenses, transitive manifests, permissions, exported components, supply-chain provenance и Java source.

Текущий manifest template не содержит общего config family для дополнительных permissions, services, providers, intent filters, network-security policy или adaptive icons. Для них нужно проверяемое расширение template/package contract либо project-owned deterministic packaging step с тестами. Не опирайтесь на ручные правки generated tree.

### Приоритет конфигурации и секреты

`package.py` читает Android settings из baked effective target config. Он не накладывает authored keys `Android.*` и не разрешает `$TARGET_ENV{...}` / `$TARGET_FILE{...}` на packaging host. Поэтому конкретный signing password попадает в baked config, а сохранённая target directive остаётся literal string и не может предоставить signing credentials.

Следующий синтаксис runtime directives намеренно **не** является рабочим package-secret handoff:

```ini
Android.Keystore = $TARGET_ENV{MYGAME_ANDROID_KEYSTORE_PATH}
Android.KeystorePassword = $TARGET_ENV{MYGAME_ANDROID_STORE_PASSWORD}
Android.KeyAlias = $TARGET_ENV{MYGAME_ANDROID_KEY_ALIAS}
Android.KeyPassword = $TARGET_ENV{MYGAME_ANDROID_KEY_PASSWORD}
```

Никогда не помещайте реальные signing values в authored или baked config, документацию, fixtures, logs, generated trees, archives или manifests. Текущий Engine не имеет host-only input для Android signing secrets; оставляйте tuple пустым для development output либо подписывайте в защищённой project-owned стадии. [Безопасность и секреты](../release/security-and-secrets.md) владеет provisioning, redaction, rotation, CI trust и обработкой incidents.

## Runtime bootstrap и staging ресурсов

Generated `FOnlineActivity` наследует SDL `SDLActivity`, загружает только `libmain.so`, готовит ресурсы до старта SDL и самостоятельно строит native argument vector. Он всегда добавляет:

- `--ApplySubConfig <Config>`;
- `--Baking.ClientResources <files-dir>/Resources`;
- `--Baking.CacheResources <files-dir>/Cache`.

Если launch Intent содержит непустой string extra `server_host`, activity также добавляет `--ClientNetwork.ServerHost <value>`. Произвольный command-line extra не поддерживается. Прежде чем заявлять другие runtime overrides, добавьте проверяемый typed bridge.

Packaged assets копируются из `assets/Resources` в private files directory приложения при выполнении хотя бы одного условия:

- `.asset_revision` отличается от `PackageInfo.lastUpdateTime`;
- отсутствует `<files-dir>/Resources/Metadata.zip`.

Activity удаляет прежний resource directory, рекурсивно копирует asset tree и только после успешного копирования записывает новую revision. Ошибки enumeration, создания directory, copy, delete или записи revision вызывают exception и останавливают startup. Cache directory создаётся отдельно и не очищается при APK update. При несовместимом изменении resource formats или cache keys проект должен предоставить явные invalidation и migration evidence.

`adb install -r` сохраняет app data, включая runtime cache и скопированные ресурсы. Очистка app storage или uninstall удаляют эти данные. Перед изменением gameplay code различайте stale APK, retained cache, failed asset copy, updater state и несовместимость server.

Android может обновлять writable resources через обычный client resource path, но `Updater::CanSelfUpdateNativeModules()` возвращает false для Android. Native compatibility mismatch нельзя исправить загрузкой новой shared library на месте: соберите, упакуйте, установите и перезапустите совместимый APK.

## Обнаружение и выбор Wi-Fi ADB-устройства

Включите Developer Options и Wireless debugging, при необходимости выполните pairing устройства с хостом через Android/ADB, разблокируйте его и подтвердите authorization prompt. `android_device.py` обнаруживает connect services, но не выполняет pairing transaction.

Helper ищет `adb` сначала в `Workspace/android-sdk/platform-tools`, затем в `PATH`. Порядок выбора endpoint:

1. явный `--device IP[:port]`;
2. cached `Workspace/android-debug/device-endpoint.txt`, если подключение ещё работает;
3. единственный уже подключённый network serial;
4. записи `adb mdns services`, содержащие `_adb-tls-connect._tcp` или `_adb._tcp`;
5. интерактивный ручной ввод.

Endpoint без port получает `:5555`. В non-interactive job передавайте `--device`, если cached или единственный connected endpoint не гарантирован. Helper ориентирован на Wi-Fi; для USB serial используйте raw ADB либо добавьте протестированный helper path.

Покажите connect endpoints или выберите устройство:

```bash
python3 Engine/BuildTools/android_device.py --workspace-root Workspace discover
python3 Engine/BuildTools/android_device.py --workspace-root Workspace connect --device 192.0.2.10:37199
```

`discover` выводит services, но не подключается. Stale cached endpoint повторно проверяется и затем пропускается; файл перезаписывается только после успеха другого endpoint.

## Установка, запуск, остановка и сбор логов

Используйте fully qualified activity component, выведенный из `Android.PackageName`:

```bash
python3 Engine/BuildTools/android_device.py --workspace-root Workspace install \
  --apk Workspace/android-debug/<ProjectDevName>-Client-<Config>-Android/app/build/outputs/apk/debug/app-debug.apk \
  --device 192.0.2.10:37199
python3 Engine/BuildTools/android_device.py --workspace-root Workspace launch \
  --activity <application-id>/.FOnlineActivity \
  --device 192.0.2.10:37199
python3 Engine/BuildTools/android_device.py --workspace-root Workspace stop \
  --package <application-id> \
  --device 192.0.2.10:37199
python3 Engine/BuildTools/android_device.py --workspace-root Workspace logcat \
  --device 192.0.2.10:37199
```

`install` выполняет `adb install -r`: заменяет APK с сохранением application data и не делает uninstall, clear storage, grant permissions или смену signing compatibility. Для `INSTALL_FAILED_USER_RESTRICTED` выводятся инструкции на устройстве; остальные ошибки ADB возвращаются без преобразования.

`launch` выполняет `am start -n`, а `stop` выполняет `am force-stop`. `logcat` потоково выводит фиксированные filters `SDL:V`, `FOnline:V`, `LF:V` и все сообщения error priority. Он не очищает buffer, не фильтрует по package/PID и не предоставляет custom filter option. Для полной истории, timestamps, PID filters, tombstones или очистки buffer используйте raw `adb logcat`.

## Подключение к серверу разработки

Запустите project-owned server, совпадающий с packaged config и слушающий адрес, доступный устройству. Затем используйте `launch-game`:

```bash
python3 Engine/BuildTools/android_device.py --workspace-root Workspace launch-game \
  --activity <application-id>/.FOnlineActivity \
  --device 192.0.2.10:37199 \
  --server-host 192.0.2.20
```

Helper принудительно останавливает package, запускает activity и передаёт `--es server_host <host>`. Если `--server-host` пропущен, он открывает UDP route к выбранному device endpoint и использует получившийся non-loopback local address. Auto-detection не доказывает, что server слушает этот адрес, firewall разрешает traffic, а project ports и protocol versions совпадают.

Activity преобразует только этот typed extra в `--ClientNetwork.ServerHost`. Проект отвечает за server startup, ports, TLS/proxy policy, account/auth flow, startup scene, public-network threat controls и compatibility. После native- или script-contract changes переустанавливайте APK; подключение старого APK к новому несовместимому server не является корректным Android acceptance test.

## Интеграция release packaging

Переиспользуемое package declaration является project-owned записью `DefinePackage(...)`. Например:

```cmake
DefinePackage(AndroidCandidate
    CONFIG PublicGame
    BINARY Client Android arm64 Raw+Apk)
```

`Apk` запускает Gradle внутри generated tree и копирует выбранный APK рядом с ним как `<target-output-name>.apk`. `Raw` сохраняет Gradle tree; без `Raw` finalization удаляет его после создания запрошенного artifact. `Debug+Apk` выбирает `assembleDebug`; `Apk` без `Debug` выбирает `assembleRelease`. Gradle caches APK изолированы в package-specific `.gradle-user-home` под parent output.

Android packaging поддерживает только target `Client` и требует resources. Одного package declaration недостаточно: CI должен собрать каждый запрошенный ABI, создать совпадающий build hash и свежие baked config/resources, подготовить SDK/NDK/Java, вызвать package target и сохранить logs и artifact evidence.

### Поведение release-подписи

Если непусто любое signing field, обязательны все четыре поля, а keystore file должен существовать. Keystore path и alias патчатся в Gradle; passwords передаются только через `FO_ANDROID_RELEASE_STORE_PASSWORD` и `FO_ANDROID_RELEASE_KEY_PASSWORD`. После этого Gradle подписывает `assembleRelease` заданным release config.

Если все signing fields пусты, шаблон подписывает release build через debug signing config Gradle, чтобы APK можно было установить. `package.py` отклоняет unsigned release APK, но не может превратить debug-key signature в production identity. APK с debug key всегда является development artifact. Production lane должен требовать ожидаемый certificate, проверять его digest после финальной сборки байтов, доказывать monotonic version code и update compatibility и завершаться ошибкой при отсутствии release credentials или verification.

Требования к manifest, provenance, publication, staged rollout и rollback описаны в [Упаковке и выпуске](../release/packaging.md).

## Матрица device- и release-приёмки

| Маршрут | Минимальное project evidence | Признак ошибки |
|---|---|---|
| ABI и install | APK содержит только ожидаемые ABI; clean install и `-r` update проходят на представительных API levels | `INSTALL_FAILED_NO_MATCHING_ABIS`, downgrade, signature или policy rejection |
| Cold и warm startup | Resource copy, SDL/native load, применение config и первый rendered frame | crash до `libmain`, нет `Metadata.zip`, black screen или неверный config |
| Rendering | Представительные map, GUI, fonts, images, sprites/models, effects и orientation на GLES 3 devices | shader/driver failure, clipping, corruption, unsupported orientation |
| Input | Touch, back/navigation, keyboard/IME при использовании и каждый заявленный controller class | trapped input, duplicate events, unusable focus, controller mapping drift |
| Audio и lifecycle | Слышимые sound/music, interruption, background/pause/resume, screen lock и process recreation | stuck audio, lost device, duplicate runtime, crash или state loss |
| Networking | Device-to-server route, reconnect, incompatible-version response, latency/loss behavior и firewall policy | loopback/host mismatch, silent timeout, insecure exposure, update loop |
| Resource/cache update | APK update, retained data, изменённые assets, missing metadata и явная cache invalidation | старые assets после update, partial copy, incompatible retained cache |
| Native compatibility | Mismatched native generation сообщает unsupported self-update и восстанавливается заменой APK | предполагается hot-replace Android binary загруженным native module |
| Permissions и privacy | Manifest merge, exported components, runtime permissions, data backup, SDK collection и privacy disclosure | неожиданные permission/component, rejected policy, undeclared data flow |
| Release identity | Release certificate digest, monotonic version code, signed final APK и update с предыдущего поддерживаемого release | debug key, unsigned artifact, неверный alias, non-upgradable package |
| Distribution | Store/internal-track upload, install из реального channel, asset limits, target API и rollback/recovery | локальный sideload успешен, а published channel нет |

CI-матрица Engine намеренно не предоставляет это device evidence. Проект может называть target production-supported только после того, как нужные строки версионированы, воспроизводимы и обязательны в его release gate.

## Диагностика по слоям

| Симптом | Что проверить сначала |
|---|---|
| Ошибка подготовки host | Java 17/system package group, disk permissions, network access, pins SDK/NDK и принятые licenses. Обрезанные архивы Google CDN (`ContentTooShortError`, `Error reading Zip content from a SeekableByteChannel`) повторно загружаются через `download_file` / `run_with_retry`; постоянная ошибка означает проблему host/network, а не отсутствие pin |
| CMake не конфигурирует Android | `FO_ANDROID_NDK_ROOT`, toolchain file, ABI mapping, native API pin, Clang floor и чистый build directory |
| Native build успешен, но package input отсутствует | `FO_OUTPUT`, target/config/build hash, ожидаемый `lib<ProjectDevName>_Client.so` и совпадающий resource bake |
| Packaging отклоняет resources | выбранный sub-config, `Baking.ClientResources`, свежий `Metadata.zip` и отсутствие token `NoRes` |
| Packaging отклоняет icon или Java source | реальная PNG signature, project-relative path, suffix `.java`, уникальный basename и отсутствие override `FOnlineActivity.java` |
| Gradle не видит SDK/NDK | generated `local.properties`, `ANDROID_HOME` / `ANDROID_SDK_ROOT`, patched NDK path/version и установленный compile SDK |
| Ошибка Gradle dependency resolution | generated repositories/dependencies, credentials, dependency locks, proxy/TLS и доступность repository |
| Release build unsigned или debug-signed | полный signing tuple, environment handoff, keystore path/alias, итоговый `apksigner verify` и certificate digest |
| Устройство не обнаруживается | сначала pairing, Wireless debugging, `discover`, `adb devices`, доступность в одной сети, затем явный `--device` |
| Device `unauthorized` или install запрещён | разблокировка устройства, authorization хоста, installer/security prompt и device/vendor policy |
| Ошибка update install | application id, version code, signing certificate, ABI, storage и допустимость clean uninstall для теста |
| Activity не запускается | точный `<application-id>/.FOnlineActivity`, installed package, manifest merge, native library и полный logcat |
| Content отсутствует или устарел | hash/path установленного APK, `lastUpdateTime`, `.asset_revision`, `Metadata.zip`, copy exception, retained cache |
| Client не достигает host | typed extra `server_host`, выбранный LAN address, server bind address, firewall, ports и compatibility version |
| Client запрашивает native update | Android native self-update не поддержан; установите совместимый APK вместо повторов resource updater |
| Ошибка resume/orientation | SDL lifecycle logs, manifest `configChanges`, orientation setting, renderer/device loss и project state restoration |

Храните evidence отдельно для каждого слоя: CMake/build log, package log и generated tree, Gradle log и inspection APK, output команд ADB, полный logcat/tombstone, server log и точные revisions. Не сводите все сбои к формулировке «Android не работает».

## Project evidence и правила извлечения

Закреплённый snapshot Last Frontier демонстрирует project-owned Android settings, локальный ARM64 task graph VS Code для build/bake/package/install/remote server, raw Android package declarations и nightly/manual cross-platform build matrix. Его официальные production package declarations сейчас не создают Android APK. Это integration evidence, а не release promise Engine.

Закреплённый snapshot FOnline TLA независимо содержит Android package settings, CMake presets для ARM32/ARM64/x86 и CI client builds для ARM32/ARM64, но аудит не обнаружил квалификации Android APK/device. Это подтверждает, что identity и build wiring принадлежат игре; оно не оправдывает копирование release practices или заявление поддержки x86.

Повышайте project observation до контракта только после переноса переиспользуемого механизма и focused tests в Engine. Проекту принадлежат:

- application id, display name, icon, version policy, signing identity, SDK integrations и permissions;
- выбранный sub-config, server route, ports, authentication, startup scene и content packs;
- Gradle dependency allowlist, privacy/security review, device matrix, performance budgets и store policy;
- CI job names, editor tasks, device endpoints, credentials, artifacts, rollout, monitoring и rollback.

Отсутствие device lane в любом из проектов является evidence пробела, а не evidence приемлемого device behavior.

## Триггеры сопровождения

Повторно проверяйте эту инструкцию в том же изменении при изменении:

- Android platform ids, ABI aliases, support labels, CI matrix, host prerequisites, SDK/NDK/API pins или environment resolution;
- Android CMake flags, client output name, native library rename, binary patching, package target/pack grammar, output paths или Gradle invocation;
- package setting declarations/defaults, sub-config precedence, host directives, signing fields, secret handoff, icon/metadata/dependency/Java-source handling;
- Gradle wrapper/plugin, manifest features/permissions/activity, SDL Java base, `FOnlineActivity`, resource revision/copy/cache behavior или Intent extras;
- ADB discovery markers, endpoint cache/selection, install flags, activity command, server-host detection, force-stop или logcat filters;
- Android updater capability, native compatibility behavior, renderer/input/audio/lifecycle requirements или release acceptance policy;
- revision/path evidence Last Frontier или TLA, особенно после изменения Android config, package declarations, CI или device workflows в любом проекте.

Запускайте focused Android documentation test, package/security tests, generated documentation gates и затронутые build/package/device lanes. При наблюдаемом изменении поведения обновляйте project integration docs в той же revision.

## Маршруты проверки

Из корня Engine выполните source-backed documentation checks:

```bash
python3 BuildTools/tests/test_docs_android_debugging.py
python3 BuildTools/tests/test_package_security.py
python3 BuildTools/tests/test_docs_package.py
python3 BuildTools/tests/test_docs_support_matrix.py
python3 BuildTools/docs_validate.py
```

Для Android-изменения дополнительно запустите самый узкий затронутый native validation target (`android-arm32-client`, `android-arm64-client` или source-capable x86 target), свежий resource bake, сборку generated project, inspection APK, install/update, cold и warm launch, полный сбор логов и нужные строки device acceptance matrix. Host-only documentation или package test не заменяет visible/device evidence.

## См. также

- [Матрица поддержки](../../reference/platforms/support-matrix.md)
- [Процесс сборки](../build/)
- [Встраивающий проект](../build/embedding-project.md)
- [Конвейер BuildTools](../../reference/cmake-and-buildtools/pipeline.md)
- [Упаковка и выпуск](../release/packaging.md)
- [Безопасность и секреты](../release/security-and-secrets.md)
- [Разделение client runtime и updater](../../explanation/runtime/client-updater.md)
- [Сборка, упаковка и отладка в браузере](web-debugging.md)
