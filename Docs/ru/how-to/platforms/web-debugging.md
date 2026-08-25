---
layout: default
title: Сборка, упаковка и отладка FOnline в браузере
locale: ru
document_id: web-debugging
permalink: /Docs/ru/how-to/platforms/web-debugging.html
---

<!-- docs-translation: {"document_id":"web-debugging","locale":"ru","source_path":"Docs/en/how-to/platforms/web-debugging.md","source_sha256":"2343681bdb6a8074345a94d32a7306599e16dbc10433659caf331fad4eefa63a"} -->

# Сборка, упаковка и отладка FOnline в браузере

Это принадлежащая Engine инструкция по подготовке закреплённого Emscripten toolchain, сборке и упаковке WebAssembly-клиента, его локальной раздаче для диагностики, подключению к серверу проекта и квалификации браузерного deployment. Она следует текущим BuildTools, package shell, Web runtime, networking, renderer, updater, модели поддержки и проверенным project evidence. Встраивающий проект отвечает за bake контента, серверный профиль, аутентификацию, публичный origin, матрицу браузеров, deployment, мониторинг и решение о выпуске.

## Статус контракта

Это production-контракт переиспользуемого пути Web-клиента на текущей ревизии Engine. Нормативны исходный код Engine, проверяемые модели и тесты. Last Frontier и FOnline TLA служат только закреплёнными discovery- и compatibility-evidence; их имена задач, порты, домены, токены, CI jobs и покрытие приёмки не расширяют поддержку Engine.

Страница самостоятельно применима из checkout встраивающего проекта, где Engine находится в `Engine/`. Замените `<ProjectDevName>` и `<Config>` значениями проекта. Project evidence закреплены в `BuildTools/ExternalProjectEvidence.json`; переиспользуемые утверждения заново выводятся из исходников Engine.

Web-доставка имеет четыре отдельных слоя evidence:

1. C++-сборка клиента через Emscripten;
2. сгенерированный браузерный пакет и локальная загрузка по HTTP;
3. реальный браузерный прогон подключения, рендера, ввода, звука, storage и lifecycle;
4. production-хостинг, безопасность, совместимость, наблюдаемость, rollout и rollback.

Успех на одном слое не квалифицирует следующий.

## Область и нормативные источники

Владельцами контракта в Engine являются:

- `ThirdParty/emscripten`, `BuildTools/buildtools.py` и host wrappers для pin toolchain, подготовки workspace, configure и build runners;
- `BuildTools/cmake/stages/Init.cmake` для Web platform tuple, режимов оптимизации, памяти WebAssembly, WebGL, filesystem, исключений и export flags;
- `BuildTools/PackageInterface.json`, `BuildTools/package.py` и `BuildTools/cmake/stages/Packages.cmake` для package grammar, патчинга бинарника, preload ресурсов и выходных артефактов;
- `BuildTools/web/default-index.html` и `BuildTools/web/simple-web-server.py` для штатного shell, диагностики, query-аргументов и сервера разработки;
- `Source/Common/WebRelated.*`, `Source/Frontend/Rendering-OpenGL.cpp` и инициализация приложений для canvas layout, clipboard, загрузки IDBFS, main loop, ошибок и WebGL;
- `Source/Client/NetworkClient-Sockets.cpp`, `Source/Common/Settings.inc` и `Source/Client/Updater.*` для выбора WebSocket и возможностей обновления;
- `Examples/ContentShowcase`, включая контракты пакета/runtime и закреплённый Playwright harness, для переиспользуемого браузерного fixture;
- `BuildTools/SupportMatrix.json` и `.github/workflows/validate.yml` для текущей метки поддержки и build gate Engine.

Встраивающий проект владеет `.fomain`, bake ресурсов, выбранным config, сервером, WebSocket endpoint, login route, настройкой публичного shell, CDN/reverse proxy, заявленной поддержкой браузеров, performance budgets, аналитикой, секретами и release evidence. Не переносите значения проекта в переиспользуемую политику Engine.

## Матрица поддержки и квалификации

| Слой | Текущие evidence Engine | Что доказано | Что остаётся проекту |
|---|---|---|---|
| Toolchain | pin Emscripten `6.0.8` и workspace preparer | Воспроизводимый выбранный SDK input | Host image, cache, mirrors и восстановление после сбоев |
| Build | обязательный CI lane `web-client` на Ubuntu 24.04 | Браузерный клиент компилируется и линкуется в `Web-wasm` | Game bake, пакет, сервер и поведение браузера |
| Renderer | строго WebGL 2; Vulkan и SDL_GPU исключены | Скомпилированный графический контракт | Матрица браузер/GPU/driver и видимая корректность |
| Package | Web `Client` + `wasm`, ресурсы обязательны | Можно выпустить штатный shell, patched wasm и preloaded resources | Публичный хостинг и неизменяемый release artifact |
| Runtime | Engine canvas, clipboard, загрузка IDBFS, WebSocket и main-loop code | Переиспользуемые механизмы существуют | User gesture, storage, reconnect, lifecycle и game-flow acceptance |
| Browser automation | локальный Playwright harness в `Examples/ContentShowcase` | Необязательный детерминированный fixture пакета, сети, WebGL 2, lifecycle и пикселей композитора | Обязательный CI и выпускные gates проекта для браузеров, GPU, устройств и game flow |

Поддерживаемое Engine приложение — браузерный клиент. Не выводите поддержку Web server, Mapper, Baker или других приложений из веток исходников, которые случайно могут собраться через Emscripten. Метка `build_gated` квалифицирует компиляцию браузерного клиента; текущий реестр проверок не требует process smoke в браузере.

## Подготовка host и workspace

`ThirdParty/emscripten` закрепляет `6.0.8`. Preparer удаляет и заново клонирует `Workspace/emsdk`, устанавливает и активирует именно эту версию с `--build=Release --shallow`, а BuildTools запускает configure/build внутри её `emsdk_env`. Произвольный системный `emcc` не используется.

На свежем Linux host установите Node.js, Java, общие build packages и SDK:

```bash
bash Engine/BuildTools/prepare-workspace.sh web-packages web
```

Если Linux host packages уже установлены, подготовьте только SDK:

```bash
bash Engine/BuildTools/prepare-workspace.sh web
```

На Windows используйте проверенный PowerShell wrapper или прямую host-workspace command с feature `web`. Windows-подготовка устанавливает workspace SDK, но не обеспечивает все host prerequisites. В текущей host map у macOS нет проверенного Web workspace preparer, поэтому он не заявлен как Web build host.

Выбранный toolchain — `Workspace/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`. BuildTools получает `FO_EMSDK` из выбранного workspace, если этот каталог существует; для другого пересоздаваемого workspace задайте `FO_WORKSPACE`, а не направляйте `FO_EMSDK` на посторонний SDK. Windows использует Ninja Multi-Config, Linux — Unix Makefiles. Считайте SDK и каталоги build/output пересоздаваемыми, но проверяйте pin в исходниках.

## Конфигурации сборки и ограничения Web

Соберите браузерный клиент с полезной debug information:

```bash
python3 Engine/BuildTools/buildtools.py build web client RelWithDebInfo
```

Используйте `Debug`, когда нужны assertions Emscripten и stack-overflow checks уровня 2. `RelWithDebInfo` содержит `-g3` и оптимизированную non-Debug линковку. `Release` и `Release_Ext` добавляют `-O3 -flto`; переходите к ним после исправного диагностического маршрута. Успешная native-сборка не заменяет эту target-сборку.

Текущий Web link contract включает:

- target tuple `Web-wasm`, browser OS, wasm architecture и executable suffix `.js`;
- stack 16 MiB, initial memory 256 MiB, maximum 4 GiB, разрешённый growth и LZ4 для ресурсов;
- минимальный и максимальный WebGL версии 2, режим OpenGL ES, отсутствие Vulkan, SDL_GPU и linked SDL library (`-sUSE_SDL=0`);
- принудительный filesystem и поддержку IDBFS, отключённый dynamic execution, строгие JavaScript checks и строгие правила undefined symbols/unimplemented syscalls для shipping client;
- экспорт `_main`, `_malloc`, `_free` и runtime methods, необходимых штатному shell и resource loader;
- перехват WebAssembly exceptions и abort-on-wasm-exception behavior.

Только wasm unit-test target ослабляет проверки undefined symbols и unimplemented syscalls, потому что linked server/database code ссылается на недоступные POSIX operations, которые исполняемые тесты не вызывают. Никогда не переносите это исключение в shipping client.

Memory growth не отменяет ceiling 4 GiB, ограничения browser process, allocation spikes или GPU resource limits. Установите project budgets на представительном контенте и длительных браузерных прогонах.

## Bake, build и package

Это отдельные стадии. Сначала выполните bake актуальных ресурсов/config выбранного проекта через документированный project bake target. Затем соберите соответствующий Web client. После этого создайте по одному локальному browser package для каждого config:

```bash
python3 Engine/BuildTools/buildtools.py package-web-debug <ProjectDevName> <Config> [<Config> ...]
```

Helper не выполняет build или bake. Он использует project git `HEAD` как build hash, берёт бинарники из настроенного output и project inputs, вызывает `package.py` как `Client Web wasm Raw+WebServer` и пишет:

```text
Workspace/web-debug/<ProjectDevName>-Client-<Config>-Web/
```

Если изменились source, scripts, resources, `.fomain`, совместимость Engine или выбранный config, заново выполните bake/build/package затронутого слоя. Refresh браузера не исправляет устаревшие generated inputs.

## Контракт браузерного пакета

Штатный raw Web package содержит:

- `index.html` из Engine shell;
- `<ProjectDevName>_Client.js` и `<ProjectDevName>_Client.wasm`;
- `Resources.data` и `Resources.js`, созданные закреплённым Emscripten `file_packager.py` с `--preload` и `--lz4`;
- необязательный `web-loading-image.<ext>` из `Web.LoadingImage`;
- `web-server.py`, когда выбран pack token `WebServer`;
- также архив, если project package declaration выбирает `Zip`.

Web packaging поддерживает только target `Client`, только architecture `wasm` и требует ресурсы; `NoRes` отклоняется. Packager копирует JavaScript/WebAssembly, патчит embedded resources, effective config и packaged build name в wasm, помещает настроенный каталог client resources в preload и удаляет этот распакованный каталог после создания `Resources.data`.

`Web.LoadingImage` разрешается относительно главного `.fomain`, и packaging завершается ошибкой, если настроенный файл отсутствует. По умолчанию `Web.BackgroundColor` равен `rgb(0, 0, 0)`. Считайте output directory сгенерированным и неизменяемым: изменяйте owning template/config и пересобирайте, а не правьте release files вручную.

Для release declarations используйте проверенную package grammar и [Packaging and Release](../release/packaging.md). Package declaration всё равно требует свежего bake, совпадающего build hash, полного артефакта, provenance и browser acceptance.

## Аргументы shell и граница секретов

Штатный shell преобразует каждую непустую query-пару URL `key=value` в `--key value` внутри `Module.arguments`. Это позволяет typed launch overrides, например:

```text
http://localhost:7000/index.html?ClientNetwork.WebSocketHost=127.0.0.1&Network.WebSocketPort=4026&Network.SecuredWebSockets=False
```

Без query клиент использует свой packaged effective configuration. Shell не содержит allowlist query keys; нормативной остаётся проверка settings в Engine.

Полный URL виден в browser history, screenshots, скопированных ссылках, reverse proxies, access logs, telemetry, referrers, support tools и любому пользователю со страницей. Никогда не передавайте в query parameters пароли, долгоживущие bearer tokens, signing material, database strings или повторно используемые administrator credentials. Если проект использует browser login token, он должен определить узкую audience, короткий lifetime, single-use/revocation behavior, transport protection, logging redaction и incident response в собственной threat model. См. [Security and Secrets](../release/security-and-secrets.md).

Shell загружает `Resources.js` раньше main client JavaScript, меняет virtual working directory на `/`, предоставляет `#canvas` и хранит последние 200 console entries для встроенной log/error panel. `F8` переключает panel. `window.foShowError`, JavaScript errors, unhandled promise rejections и stderr с текстом error/exception попадают в видимую diagnostic surface; stdout и stderr также дублируются в browser console.

## Локальная раздача пакета

Запускайте сгенерированный development helper из любого working directory; он всегда раздаёт каталог, в котором находится сам:

```bash
python3 Workspace/web-debug/<ProjectDevName>-Client-<Config>-Web/web-server.py --port 7000
```

Откройте `http://localhost:7000/index.html`. Не используйте `file://`: WebAssembly, fetch packaged resources, browser security, clipboard, storage и networking требуют HTTP origin.

Helper намеренно минимален:

- по умолчанию использует port `7000` и отправляет no-store/no-cache headers;
- использует threaded Python server и bind `('', port)`, то есть открывает listener на всех host interfaces, разрешённых OS/firewall;
- `--fork` выполняет fork только там, где существует `os.fork`, а на Windows фактически ничего не делает;
- не имеет TLS, authentication, access control, явного wasm MIME override, COOP/COEP policy, compression, health check или production hardening.

Используйте его только с нечувствительными development artifacts в доверенной сети, закрывайте listener после работы и применяйте host firewall policy. Для automated local runs предпочитайте project test server с bind на `127.0.0.1`. Никогда не публикуйте `web-server.py` как production origin.

## Подключение к серверу проекта

Web client всегда использует `ClientNetwork.WebSocketHost` и `Network.WebSocketPort`. `Network.SecuredWebSockets` выбирает `ws://` или `wss://`. Native `ClientNetwork.ServerHost` / `Network.ServerPort` не являются Web transport endpoint, UDP отключён Web build, а native proxy path недоступен.

Запустите совместимый project server с WebSocket listener, затем проверьте endpoint через browser DevTools. Страница по HTTPS обычно должна подключаться через `wss://`; небезопасный запрос `ws://` является mixed content и ожидаемо блокируется. Проект владеет certificate names, TLS termination, reverse-proxy upgrade headers, origin policy, firewall exposure, rate limits, authentication, compatibility errors и reconnect behavior.

Разделяйте следующие отказы:

1. HTTP не загружает package files;
2. падает инициализация JavaScript/WebAssembly;
3. не проходит WebSocket DNS/TCP/TLS/upgrade;
4. не проходит Engine handshake или compatibility;
5. после подключения ломаются login, scene или gameplay scripts.

Общий loading screen не превращает их в одну проблему. Сохраняйте browser console/network evidence и соответствующие server/proxy logs.

## Поведение runtime в браузере

### Canvas и rendering

Runtime нацелен на `#canvas` и создаёт WebGL2 context. Он слушает изменения window, `visualViewport` и fullscreen. Adaptive sizing ограничивает page dimensions, настроенные min/max, height percentage, aspect factor и position factors; fullscreen центрирует canvas. `Module.foScreenWidth` и `Module.foScreenHeight` могут переопределить вычисленные dimensions, если custom shell задаёт ненулевые значения.

Штатный shell сообщает о WebGL context loss и требует reload. Engine не обещает прозрачное context restoration. Квалифицируйте resizing, fullscreen, high-DPI behavior, orientation, browser zoom, context loss, background/resume и representative rendering в поддерживаемой матрице браузеров проекта.

### Main loop, audio и input

Web main loop устанавливается через `emscripten_set_main_loop_arg(..., 0, 1)`. Browser scheduling, throttling background tabs, visibility changes и правила user gesture остаются поведением браузера. Экспортированный runtime содержит helper Emscripten для возобновления audio context, но проект всё равно должен доказать first-use audio activation, mute/unmute, interruption, resume и device changes через реальное взаимодействие.

Canvas copy events используют Engine clipboard. Runtime запрашивает clipboard-read permission при первом pointer interaction, если API доступно, и перехватывает неповторный `Ctrl+V`; при ошибке navigator access используется Engine clipboard. Clipboard API зависят от secure contexts, permissions, focus и user gestures, а несколько ошибок намеренно подавляются. Проверяйте paste/copy визуально, а не считайте отсутствие exception успехом.

### Persistent data

Runtime создаёт `/PersistentData`, монтирует IDBFS, вызывает `FS.syncfs(true)` и задерживает обычный startup до завершения начальной загрузки из браузера в virtual filesystem. Сейчас callback отмечает готовность даже при ненулевом `err`. Проверенный generic path не доказывает автоматическую обратную синхронизацию после каждого последующего изменения.

Поэтому не обещайте durable saves только на основании mount IDBFS. Проект должен определить, какие данные там находятся, когда выполняется flush записей, поведение quota/eviction, private/incognito mode, schema/version migration, recovery после corruption, удаление пользовательских данных и multi-tab conflict policy. Проверяйте cold reload и browser restart на точном production origin; storage привязан к origin.

### Логи и fatal errors

File logging и asynchronous file-log writing отключены на Web. Основные reusable evidence — browser console и встроенная panel штатного shell. Собирайте console entries, JavaScript errors, unhandled rejections, page crashes, network traces, server logs и точные revisions артефактов. Скопированная error panel полезна, но не заменяет предшествующую console/network timeline.

## Диагностика в браузере

Используйте DevTools в таком порядке:

1. **Console:** Engine stdout/stderr, JavaScript exceptions, WebAssembly aborts, failed assertions и WebGL messages.
2. **Network:** `index.html`, main `.js`, `.wasm`, `Resources.js`, `Resources.data`, status, MIME, content encoding, cache headers, redirects и WebSocket upgrade/frames.
3. **Sources:** generated JavaScript, поддержка WebAssembly debugging и source artifacts, реально выпущенные выбранной Emscripten configuration. Сам по себе `-g3` не обещает отдельный source-map file в каждом package.
4. **Application/storage:** origin, состояние IndexedDB/IDBFS, quota, cache/service-worker state, добавленный проектом, и данные после reload.
5. **Performance/memory:** long tasks, frame pacing, heap growth, GPU pressure, download/decompression ресурсов и background throttling после установления корректности.

Сохраняйте page URL без секретов, browser/version, OS/GPU, package hash, revisions Engine/project, server config, proxy headers и шаги воспроизведения. Сравнивайте локальный raw package с public origin, чтобы отделить hosting от runtime.

## Native updater и повторный deployment

Updater определяет платформу как `Web` / `Web-wasm`, использует `/` как virtual runtime root и возвращает false из `CanSelfUpdateNativeModules`. Browser client не может исправить несовместимое поколение native/WebAssembly скачиванием replacement module на месте.

Публикуйте совпадающие `index.html`, main JavaScript, wasm и пару ресурсов как один versioned artifact. Не допускайте deployment window, в котором cached shell загружает новый wasm со старыми ресурсами или наоборот. Используйте versioned directories или другой atomic switch, явную cache policy, health/smoke checks и rollback на полный предыдущий artifact. Для native changes выполняйте reload/redeploy совместимого package; не смешивайте это с project resource-update path.

## Production-хостинг и безопасность

Production origin должен обеспечить как минимум:

| Область | Обязательное project decision/evidence |
|---|---|
| MIME | `.wasm` отдаётся как `application/wasm`; корректны типы JavaScript/data/image; нет HTML fallback для отсутствующих artifacts |
| TLS | HTTPS origin, корректная certificate chain/name, game route `wss://`, secure redirects и отсутствие mixed content |
| WebSocket proxy | Upgrade/connection forwarding, timeouts, frame/body limits, origin policy, граница доверия client IP и полезные failure logs |
| Artifact atomicity | shell/JS/wasm/resources одной revision, versioned URL или atomic switch, integrity/provenance и полный rollback |
| Cache | short/no-cache policy для mutable entry points; проверенная immutable policy только для content-addressed/versioned assets |
| Compression | проверенная в browser/proxy policy Brotli/gzip без double compression или corruption wasm/data; range behavior, если используется |
| Isolation headers | осознанная COOP/COEP/CORP/CORS policy, когда SharedArrayBuffer/threads или project assets этого требуют; проверенная совместимость third-party |
| Security headers | CSP и embedding/frame policy, совместимые с Emscripten и разрешёнными integrations; проверенные referrer и permissions policy |
| Secrets/privacy | нет reusable secrets в package/query/logs; документированы storage, telemetry, consent, retention и deletion |
| Operations | health checks, synthetic browser smoke, error/performance telemetry, alerting, staged rollout и упражнение rollback |

Штатный helper не удовлетворяет этой таблице. Hosting headers могут сломать package, который работает локально, поэтому квалифицируйте реальный public route и CDN/proxy configuration.

## Матрица браузерной и release-приёмки

| Маршрут | Минимальные project evidence | Сигнал отказа |
|---|---|---|
| Artifact load | свежий origin загружает HTML, JS, wasm и ресурсы одной revision с правильными MIME/status | 404, HTML вместо wasm, stale mixed revision, decompression failure |
| Startup | wasm инициализируется, hydration IDBFS завершается, появляется первый rendered frame | abort, постоянный loading state, memory ceiling, storage exception |
| Rendering | representative GUI, fonts, images, models/sprites, effects, resizing, fullscreen и context-loss policy | blank/corrupt frame, clipping, shader failure, unrecoverable resize |
| Input/clipboard | mouse, keyboard, wheel, touch, если заявлен, focus, paste/copy и modal interaction | duplicate/lost input, blocked clipboard, unusable focus |
| Audio | user-gesture activation, playback, mute, interruption, background/resume и заявленные browsers | suspended context, silence, duplicate playback, lost device |
| Networking | connect через `wss://`, authentication, compatibility rejection, reconnect, latency/loss, proxy timeout и server restart | mixed-content block, failed upgrade, silent timeout, reconnect loop |
| Persistence | cold reload/restart, simulation quota/eviction, migration, recovery corruption, private mode и deletion | lost/stale save, startup hang, cross-version corruption |
| Lifecycle | hidden/background tab, throttling, resume, browser navigation, refresh и multi-tab policy | runaway loop, stale socket, duplicated session, unrecoverable state |
| Performance | download/startup budget, frame time, memory growth, длительная representative session и low-end supported device | budget regression, unbounded heap/GPU growth, tab kill |
| Security | headers, origin/CORS, token lifetime/redaction, dependency/shell review и abuse/rate-limit behavior | leaked query/token, permissive origin, blocked required asset |
| Rollout | canary/synthetic smoke на реальном origin, observability, полный prior artifact и practiced rollback | local pass при public failure, mixed release, no recovery path |

Engine build lane намеренно не предоставляет эти browser/release evidence. Проект может называть Web production-supported только тогда, когда применимые строки воспроизводимы и обязательны в его release gate.

## Диагностика по слоям

| Симптом | Что проверять сначала |
|---|---|
| Workspace prepare падает | host Node/Java/common packages, network, disk, точный pin `6.0.8`, `Workspace/emsdk` и activation logs |
| Configure выбирает не тот compiler | BuildTools platform `web`, путь workspace toolchain, `emsdk_env` и stale build directory |
| Link падает только на Web | strict undefined/unimplemented syscall output, unsupported native dependency, Web platform guards и exception flags |
| Package отсутствует | успешные bake/build, точные dev name/config, project git revision, `FO_OUTPUT` и log `package-web-debug` |
| Package отклоняет ресурсы | current effective config, `Baking.ClientResources`, свежие metadata, отсутствие `NoRes` и loading-image path |
| `index.html` загружается, а wasm/data нет | раздаваемый package directory, status/MIME, HTML fallback, proxy rewrite, compression и cache |
| Браузер показывает старый код | точный origin/path, добавленный проектом service worker или CDN, entry-point cache, versioned artifact и package hash |
| WebGL2 context не запускается | наличие browser/WebGL2, GPU/driver/blocklist, software-rendering policy, console и context attributes |
| Client не подключается | `WebSocketHost`, `WebSocketPort`, secure flag, DNS/TLS, mixed content, proxy upgrade, firewall и server listener |
| Socket подключён, но login завис | protocol/compatibility, authentication, server/client logs, затем project script/UI flow |
| Clipboard пуст | secure context, permission, user gesture, focus, browser policy, navigator fallback и видимое Engine state |
| Данные исчезают после reload | production origin, hydration IDBFS, явный later flush path, quota/eviction/private mode и schema migration |
| Audio остаётся без звука | user gesture, suspended audio context, tab visibility, browser autoplay policy, mixer/device и project assets |
| Release падает, а local helper работает | публичные MIME/TLS/cache/compression/COOP/COEP/CSP/CORS/proxy headers и mixed artifact revisions |
| Client просит несовместимое native update | Web native self-update не поддерживается; разверните и перезагрузите совместимый полный browser artifact |

Храните evidence по слоям: workspace/configure/build log, package log и file inventory, HTTP headers, browser console/network trace, WebSocket frames, server/proxy logs, storage state, screenshot/video для визуального поведения и точные revisions. Не сводите все отказы к «Web не работает».

## Project evidence и правила извлечения

`Examples/ContentShowcase` является переиспользуемой базовой линией, принадлежащей Engine. Его локальный маршрут `python validate.py --web-runtime` выполняет force-bake на native-хосте, собирает и проверяет raw/ZIP Web payload, запускает native-сервер и сгенерированный HTTP-сервер, требует успешные ответы `index.html`, JavaScript, WebAssembly и ресурсов, наблюдает маркеры готовности клиента/сервера, создаёт буфер WebGL 2 размером 1280 x 800 в закреплённом Chromium, отклоняет ошибки консоли, страницы, сети и Engine и проверяет снимок композитора по областям контента. Сохранённый WebGL-снимок и машинная запись являются локальным доказательством fixture; текущий Engine workflow не требует этот маршрут. Они не квалифицируют production headers, публичный origin, аутентификацию, хранение, активацию звука, длительные сессии или поддерживаемую игрой матрицу браузеров/GPU.

Закреплённый snapshot Last Frontier демонстрирует project-owned Web settings и secure deployment profiles, локальные VS Code build/package/server/Chrome tasks, reusable package declarations и обязательный nightly/manual Linux-Web pipeline. Его project runner выбирает случайный loopback port, принудительно задаёт `application/wasm`, добавляет COOP/COEP и no-cache headers, запускает Playwright Chromium с software WebGL, собирает console/page errors/crashes и проверяет packaged WebSocket login, token login и deterministic rendering/combat workload. Это сильный паттерн project qualification, а не обещание поддержки Engine.

Закреплённый snapshot FOnline TLA независимо содержит Emscripten CMake presets и Web settings, но этот аудит не нашёл сопоставимого checked browser-package/Playwright qualification lane. Это полезные configuration/build-discovery evidence, а не browser release standard.

Переносите project observation только после появления переиспользуемого механизма и focused tests в Engine. Оставляйте проекту:

- domain, ports, certificates, proxy/CDN configuration, browser allowlist, auth/token policy и public shell integrations;
- CI job names, editor launch tasks, scenes, accounts, gameplay markers, telemetry, budgets, artifacts, rollout и rollback;
- storage schema/flush policy, reconnect semantics, application lifecycle, accessibility, privacy и product security review.

Отсутствие project browser lane — evidence пробела, а не evidence приемлемого поведения браузера.

## Триггеры сопровождения

Повторно проверяйте эту инструкцию в том же change, когда меняется что-либо из следующего:

- pin Emscripten, host feature map/packages, workspace layout, environment wrapper, generator, configure/build command, platform tuple или support/CI label;
- Web compile/link flags, memory/stack limits, exception policy, exports/runtime methods, filesystem, WebGL/backend selection или исключение wasm tests;
- Web package platform/target/arch/pack grammar, binary patching, resource preloading/LZ4, shell substitutions, loading image/background, output naming или archive behavior;
- query parsing штатного shell, load order, log/error panel, canvas, stdout/stderr, context-loss behavior или bind/cache/MIME/header behavior development server;
- canvas layout/settings, clipboard permissions/events, IDBFS mount/sync readiness, main loop, audio activation, logging или fatal-error reporting;
- выбор WebSocket host/port/secure, доступность UDP/proxy, server WebSocket/TLS contract, updater platform/root или native self-update capability;
- контракты package/runtime/markers/pixels Content Showcase, закреплённая версия Playwright, требования production hosting/security/acceptance либо revision/path evidence Last Frontier/TLA.

Обновляйте project integration docs в той же revision, где меняется наблюдаемое поведение проекта. Запускайте focused Web documentation test, package/security/support tests, generated documentation gates, Engine Web build lane и каждый затронутый project browser/package/deployment route.

## Маршруты проверки

Из корня Engine запустите source-backed checks:

```bash
python3 BuildTools/tests/test_docs_web_debugging.py
python3 BuildTools/tests/test_docs_package.py
python3 BuildTools/tests/test_package_security.py
python3 BuildTools/tests/test_docs_support_matrix.py
python3 BuildTools/docs_validate.py
```

Запускайте `python validate.py --web-runtime` из `Examples/ContentShowcase` для необязательного переиспользуемого fixture пакета и браузера после установки закреплённых зависимостей `WebTests`. Для изменения поведения также подготовьте закреплённый SDK, соберите Web client во всех затронутых configurations, заново выполните bake/package публичного minimal project, проверьте каждый output и HTTP header и выполните применимые строки browser/release acceptance matrix против реального project server и production-like origin. Host-only documentation test, успешный Emscripten link или localhost fixture не заменяет browser evidence проекта.

## См. также

- [Матрица поддержки](../../reference/platforms/support-matrix.md)
- [Build Workflow](../build/)
- [BuildTools Pipeline](../../reference/cmake-and-buildtools/pipeline.md)
- [Упаковка и выпуск](../release/packaging.md)
- [Безопасность и секреты](../release/security-and-secrets.md)
- [Разделение Client Runtime и Updater](../../explanation/runtime/client-updater.md)
- [Networking и Authority](../../explanation/authority-and-networking/)
- [Gameplay- и integration-тестирование](../testing/gameplay-and-integration.md)
- [Сборка, упаковка и отладка FOnline на Android](android-debugging.md)
