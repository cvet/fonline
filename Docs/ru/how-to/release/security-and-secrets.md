---
layout: default
title: Безопасность и секреты
locale: ru
document_id: security-and-secrets
permalink: /Docs/ru/how-to/release/security-and-secrets.html
---

# Безопасность и секреты

<!-- docs-translation: {"document_id":"security-and-secrets","locale":"ru","source_path":"Docs/en/how-to/release/security-and-secrets.md","source_sha256":"1503b15539982e88ef71d5d2d559322d41f7bf5c390c4710ef20093c68e44be7"} -->

Это руководство определяет переиспользуемые границы FOnline для credentials, подстановки конфигурации, подписи пакетов, CI, диагностики и incident response. Оно не выбирает secret manager, поставщика сертификатов, production account, срок хранения или incident policy для подключающей игры.

Используйте [Project Configuration](../build/project-configuration.md) для общего precedence `.fomain`, [Packaging and Release](packaging.md) для производства артефактов и [Client Updater](../../explanation/runtime/client-updater.md) для границы загружаемого native runtime.

## Решение о секрете

Используйте `$ENV{NAME}` только тогда, когда значение может разрешиться на build
или baking host: его конкретное значение может попасть в baked config.
Используйте `$TARGET_ENV{NAME}` для значения, которое должно оставаться
неразрешённым до запуска target application. Лог command-line
override маскирует только узкую поддерживаемую форму и не скрывает target-time
directives, generated files, process state или произвольные logs.

Не помещайте signing material в staged artifacts. Передавайте native signing
через `Packaging.CodeSigningHook`, project-owned процесс которого читает credentials
из своей защищённой среды. Текущий Android packager не имеет host-only secret
input: он читает пароли из baked target config, а затем копирует их в
`FO_ANDROID_RELEASE_STORE_PASSWORD` и `FO_ANDROID_RELEASE_KEY_PASSWORD` для
Gradle. Не помещайте production passwords в этот config; до появления отдельного
handoff используйте project-owned защищённую стадию signing. После утечки ограничьте доступ, отзовите затронутый credential,
выполните rotate замен, пересоберите или разверните заново затронутые artifacts
и проверьте использование. Rewriting git history является очисткой, а не revoke.

## Модель угроз и владение

Защищайте как минимум следующие классы assets:

| Asset | Типичные пути раскрытия | Владелец |
|---|---|---|
| Runtime credentials | tracked config, запечённый internal config, process arguments, logs, crash reports, settings UI, memory | подключающий проект и deployment operator |
| Signing credentials | CI variables, local environment, keystore/certificate files, session signing provider, build logs | release operator |
| Целостность релиза | скомпрометированный runner, изменённый после подписи binary, mutable artifact, непроверенный updater payload | release pipeline проекта |
| Данные игроков и эксплуатации | credentials базы данных, backups, support exports, telemetry payloads | operations проекта |
| Supply chain Engine | source revision, dependencies, actions, SDKs, package tools | maintainers Engine и квалифицирующий проект |

Считайте source проекта, pull-request code, загруженные dependencies, build runners, package staging, artifact storage, deployment hosts и работающий client/server отдельными trust zones. Доступность значения в одной зоне не разрешает копировать его в другую.

Engine предоставляет механику подстановки, одно узкое правило masking логов, package-time handoffs и validation fixtures. Проект владеет созданием secrets, access policy, storage backend, rotation, revocation, audit retention, разделением сред и severity инцидентов. Никогда не помещайте реальные значения в примеры, тесты, документацию, issue text или generated reports Engine.

## Выберите правильную форму конфигурации

`GlobalSettings::SetValue()` распознаёт четыре подстановки. Время их разрешения является границей безопасности, а не только синтаксисом.

| Форма | Обычное разрешение | Во время `ConfigBaker` | Правильное применение |
|---|---|---|---|
| literal | разбор config | копируется как значение | только публичная несекретная конфигурация |
| `$ENV{NAME}` | процесс, читающий authored config | разрешается на baking host, поэтому конкретное значение может попасть в baked config | несекретный build input, который намеренно встраивается |
| `$FILE{path}` | процесс, читающий authored config; путь относительно owning config directory | читается на baking host, поэтому содержимое файла может попасть в baked config | несекретные generated metadata, например version |
| `$TARGET_ENV{NAME}` | runtime целевого приложения | остаётся directive при baking | runtime secret, который нельзя запекать |
| `$TARGET_FILE{path}` | runtime целевого приложения | остаётся directive при baking | защищённый файл target host, содержимое которого нужно runtime |

Для runtime credentials предпочитайте `$TARGET_ENV{...}`. Используйте `$TARGET_FILE{...}` только когда deployment владеет путём и правами файла; относительный target-file path во встроенном config разрешается из контекста целевого процесса, а не исходного repository.

Не предполагайте, что packager разрешает target directives. Android packaging читает уже запечённый effective target config; сохранённое значение `$TARGET_ENV{...}` остаётся строкой directive, а не package-host lookup секрета. Windows signing читает `Packaging.CodeSigningHook` как путь из project config и также не имеет resolver target directives. Не помещайте package credentials ни в authored, ни в baked config.

```ini
# Values, paths, and aliases below are examples of variable names, not credentials.
Auth.SessionSigningSecret = $TARGET_ENV{MYGAME_AUTH_SESSION_SECRET}

Android.Keystore = $TARGET_ENV{MYGAME_ANDROID_KEYSTORE_PATH}
Android.KeystorePassword = $TARGET_ENV{MYGAME_ANDROID_STORE_PASSWORD}
Android.KeyAlias = $TARGET_ENV{MYGAME_ANDROID_KEY_ALIAS}
Android.KeyPassword = $TARGET_ENV{MYGAME_ANDROID_KEY_PASSWORD}

Packaging.CodeSigningHook = Tools/SignWindowsArtifacts
```

Строки Android выше только иллюстрируют синтаксис runtime directives; текущий packager не разрешает их для signing. Путь Windows hook несекретен. Его executable получает credentials из защищённой среды, которую Engine не исследует.

Не передавайте secret как command-line setting. `Common.SecretSettingTokens` маскирует совпадающие значения только в строке лога `ApplyCommandLine()` вида `Set <name> to <value>`. Raw argument всё ещё может быть виден в shell history, process inspection, `Common.CommandLine`, `Common.CommandLineArgs`, debugger или crash dump.

## Поймите ограничения redaction

Значения `Common.SecretSettingTokens` по умолчанию являются case-insensitive подстроками имени: `secret`, `token`, `password` и `apikey`. Расширяйте список проектными именами, например `dsn`, но считайте это только defense in depth.

Engine **не** определяет тип credential, не шифрует settings, не обнуляет memory, не заменяет значения и не гарантирует redaction за пределами одной строки лога command-line override. В частности:

- config values после разрешения являются обычными strings;
- `GlobalSettings::Save()` выводит применённые значения в baking mode;
- `ConfigBaker` записывает непустые применимые значения в side-specific internal configs;
- `GlobalSettings::Draw()` отображает зарегистрированные setting values;
- project code, third-party libraries, crash handlers, CI shells и signing tools могут логировать свои inputs;
- custom setting, признанный неизвестным во время baking, логируется вместе с текущим значением.

Поэтому target-time directives являются default для runtime secrets, production logs и crash attachments требуют независимого redaction review, а settings/debug UI нельзя давать недоверенным операторам. Никогда не тестируйте redaction реальным credential. Используйте уникальный synthetic canary в изолированной lane, затем удалите logs и artifacts этой lane.

## Упакуйте и подпишите без копирования credentials

### Windows

`Packaging.CodeSigningHook` называет принадлежащий проекту executable. Packager вызывает его как `<hook> <absolute-binary-path>` после binary patching и до создания archive или MSI. Engine не передаёт signing credential аргументом командной строки. Hook получает provider session или credentials из защищённой среды packaging host и должен завершаться ненулевым кодом при ошибке подписи или её проверки.

Путь hook должен быть непосредственно используемым несекретным путём; packager не разрешает в нём `$TARGET_ENV{...}`. Держите executable вне workspace, доступного недоверенной записи, pin или hash его, ограничьте право замены и заставьте его проверять итоговую signature и timestamp. Текущий hook Engine подписывает staged `.exe` и `.dll`; подпись внешнего installer или publication metadata остаётся ответственностью проекта.

### Android

Android release packaging читает `Android.Keystore`, `Android.KeystorePassword`, `Android.KeyAlias` и `Android.KeyPassword` из baked effective target config. Если задан любой member, требуется полный tuple. Затем `package.py` передаёт две строки password в Gradle через `FO_ANDROID_RELEASE_STORE_PASSWORD` и `FO_ANDROID_RELEASE_KEY_PASSWORD`; generated `build.gradle` читает эти environment variables и не содержит passwords.

Этот Gradle handoff предотвращает подстановку password в `build.gradle`, но не делает input host-only: конкретный пароль уже прошёл через baked config, а directive `$TARGET_ENV{...}` не разрешается. Поэтому текущий Engine не предоставляет безопасную для production границу Android signing secrets. Оставляйте tuple пустым для development output либо используйте защищённую project-owned стадию Android signing, credentials которой никогда не попадают в authored или baked Engine config.

Keystore path и key alias встраиваются в generated project и не считаются passwords. Защищайте сам keystore, generated Gradle tree, `GRADLE_USER_HOME`, process memory и worker logs. APK, созданный через fallback к debug key, является development artifact, а не production release.

### Артефакты и updater payloads

Подпись не доказывает отсутствие secrets в артефакте или публикацию правильных bytes. После подписи:

1. проверьте каждую обязательную signature и timestamp;
2. составьте inventory raw payloads и archives, включая client-runtime updater libraries;
3. просканируйте staged tree и archive members на forbidden files, private keys, config dumps и synthetic canaries;
4. вычислите hash финальных bytes и свяжите hashes с source, ревизией Engine, toolchain, config и package identity;
5. публикуйте immutable artifacts только после успешной install/deploy и updater acceptance.

Не печатайте реальное значение для его поиска. Предпочитайте правила forbidden-path и file-type, secret-scanning tools, entropy checks с проверенными allowlists и synthetic canaries, созданные только для test lane.

## Границы доверия CI

Переиспользуемый validation workflow Engine использует верхнеуровневое разрешение `contents: read` и не выполняет release signing или deployment. Его единственная текущая ссылка на GitHub secret — coverage upload token. Это состояние repository является evidence для validation Engine, а не шаблоном безопасности release workflow подключающего проекта.

Project release lane должна обеспечивать всё следующее:

- недоверенный pull-request code никогда не получает release, deployment, database или signing credentials;
- signing и deployment выполняются только из проверенных protected revisions и protected environments;
- revisions workflow и third-party actions закреплены согласно policy проекта;
- self-hosted runners считаются постоянными privileged hosts, очищаются между jobs и изолируются от untrusted builds;
- secret-bearing steps отключают shell tracing и никогда не выводят environment, command lines, generated config или provider responses;
- caches никогда не содержат keystores, credentials, signed-session state, private config или production database material;
- artifacts имеют явные retention и access policy, а upload paths не могут случайно включить весь workspace;
- release credentials ограничены одной средой и least privilege; development, staging и production не используют общие значения;
- события approval, signing, publication и rollback оставляют очищенный audit trail.

Маскирование repository secret не является content scanner. Преобразованное, разделённое, encoded, короткое или выведенное инструментом значение может избежать masking, а malicious build способен вывести значение наружу, ничего не печатая.

## Выдавайте, заменяйте и отзывайте

Ведите принадлежащий проекту inventory с несекретным identifier, purpose, owner, storage location, consumers, environments, privilege, creation date, rotation rule и revocation procedure. Не помещайте само значение в inventory.

Для плановой rotation:

1. создайте replacement у owning provider;
2. предоставьте его узкой целевой среде;
3. разверните consumers, умеющие использовать replacement, разрешая проверенное overlap только когда этого требует protocol;
4. проверьте нормальную работу и отрицательное поведение старого значения;
5. отзовите старое значение;
6. удалите устаревшие копии с runners, hosts, caches, backups где позволяет policy, и с машин операторов;
7. запишите очищенное evidence и следующий rotation trigger.

При подозрении на раскрытие остановите затронутые publication/deployment, сначала выполните revoke, если это безопасно для сервиса, замените каждый credential, полученный из раскрытого материала или хранившийся рядом, и сохраните очищенное forensic evidence. Уберите раскрытые logs и artifacts из обычного доступа, но считайте каждую скачанную копию сохранившейся. Переписывание git history или удаление CI log не возвращает доверие старому значению. До возобновления rollout пересоберите и переподпишите из известной рабочей revision и runner.

## Процедура проверки

После изменения substitution, signing или security guidance выполните сфокусированные переиспользуемые проверки:

```bash
python BuildTools/tests/test_package_security.py
python BuildTools/tests/test_docs_security_and_secrets.py
python BuildTools/tests/test_docs_package.py
python BuildTools/docs_snippets.py --check --external
python BuildTools/docs_validate.py
```

Затем квалифицируйте затронутую project lane с непроизводственными credentials:

1. проверьте authored `.fomain` и выбранный sub-config на literals;
2. выполните bake и убедитесь, что runtime secrets остаются directives `$TARGET_*` в `Baking/Configs`, а package credentials отсутствуют;
3. создайте development package и проверьте generated tree, raw package, archives, logs и manifest;
4. убедитесь, что Windows signing hook получает credentials из своей защищённой среды, либо проверьте project-owned Android signing stage без помещения секретов в Engine config;
5. установите или разверните, запустите packaged application и проверьте updater/network behavior;
6. отзовите или удалите synthetic credentials и очистите isolated evidence согласно test policy.

Успешные Engine tests доказывают runtime substitution и текущие границы packager, включая отсутствие Android host-only handoff. Они не доказывают безопасность provider account, runner, secret manager, application log, database, distribution store или incident process.

Используйте [Release Operations](operations.md) для preflight целевого хоста, staged rollout, сохранения evidence и rollback. Используйте [Backup and Recovery](backup-and-recovery.md) для зашифрованных recovery sets, доступа restore role, key identifiers и sidecar manifests без секретов. Не помещайте credentials и чувствительный incident material в обе эксплуатационные записи.

## Маршрутизация отказов

| Симптом | Что проверить сначала |
|---|---|
| Конкретный credential появился в `Baking/Configs` | замените `$ENV`/`$FILE` на target form и повторите bake из чистого output |
| Command-line log показывает credential | имя setting и `Common.SecretSettingTokens`; замените значение, поскольку сохраняются другие пути раскрытия arguments |
| Packaging сообщает об отсутствующем path/file | baked target config или root project config, выбранный sub-config и permissions файла; не помещайте туда secret |
| Android release использует debug key | полный signing tuple из четырёх полей и выбранный package config |
| Generated Gradle file содержит password | regression реализации packaging; остановитесь и выполните rotation до publication |
| Windows hook пропущен | literal path `Packaging.CodeSigningHook` в project config и доступность hook file |
| Signature отсутствует после успешного hook | contract проверки и failure hook и порядок post-sign mutation |
| Secret появился в logs, artifacts, cache или history | сначала revoke/rotate, ограничьте доступ, сохраните очищенное evidence, затем удалите копии |
| Недоверенный CI job достигает protected material | workflow event, permissions, environment approval, runner isolation и scope cache/artifact |

## Проверенные пути исходников

- `Source/Common/Settings.inc`
- `Source/Common/Settings.cpp`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Tools/ConfigBaker.cpp`
- `Source/Tests/Test_Settings.cpp`
- `BuildTools/foconfig.py`
- `BuildTools/package.py`
- `BuildTools/android-project/app/build.gradle`
- `BuildTools/tests/test_package_security.py`
- `.github/workflows/validate.yml`
