---
title: Сгенерированная матрица поддержки
document_id: generated-support-matrix-index
locale: ru
generated: true
generated_by: BuildTools/docs_support_matrix.py
---

<!-- docs-translation: {"document_id":"generated-support-matrix-index","locale":"ru","source_path":"Docs/en/reference/platforms/generated-matrix.md","source_sha256":"43056489c6c769b9eea4b7aeae64383391ada2e3098e54728e8d10d1d4dbd566"} -->

# Сгенерированная матрица поддержки

Эта страница создаётся из проверяемой политики поддержки и актуального реестра целей BuildTools/CI.
Проверка сборки подтверждает конфигурацию и компиляцию; только smoke-проверка подтверждает указанный маршрут запуска процесса.

## Уровни подтверждения

| Уровень | Значение |
|---|---|
| `build_gated` | Конфигурируется и компилируется обязательным workflow проверки при каждом изменении. |
| `smoke_gated` | Проходит проверку сборки и автоматический процессный smoke-тест starter-, native-extension-, многопользовательского проекта или Content Showcase. |
| `source_capable` | Проверяемый реестр BuildTools предоставляет цель, но обязательный workflow её не запускает. |
| `not_in_public_matrix` | Для этой комбинации приложения и платформы не опубликована поддерживаемая цель проверки. |

## Профили платформ

| Хост / цель | Компилятор | Уровень | Приложения | Обязательные цели проверки | Подтверждение runtime | Ограничения |
|---|---|---|---|---|---|---|
| Windows / Windows x64 | MSVC 19.44 или новее | `smoke_gated` | настольный и headless-клиент; сервер, headless-сервер и Windows Service; Mapper, AnimationViewer и ParticleViewer; компилятор AngelScript; Baker ресурсов | `win64-client`, `win64-server`, `win64-mapper`, `win64-ascompiler`, `win64-baker`, `win64-starter-smoke`, `win64-native-extension-smoke`, `win64-tutorial-smoke`, `win64-showcase-smoke`, `win64-tutorial-package`, `win64-package-smoke` | Процессные маршруты starter-, native-extension-, минимального многопользовательского проекта и Content Showcase выполняются проверками BuildTools; маршруты packaging-matrix и tutorial-package собирают архивы, инвентаризируют содержимое и запускают упакованные headless-бинарные файлы клиента и сервера со встроенной конфигурацией. | Успешный smoke-маршрут подтверждает штатное руководство и пути неподписанных тестовых пакетов, но не все сочетания рендерера, драйвера, базы данных, установщика, подписания, магазина, развёртывания или отката. |
| Windows / Windows x86 | MSVC 19.44 или новее | `build_gated` | настольный клиент | `win32-client` | Процессный smoke-маршрут не требуется. | Серверные и инструментальные приложения не входят в опубликованную поверхность проверки x86. |
| Ubuntu 24.04 / Linux x64 | Clang 20 или новее | `smoke_gated` | настольный и headless-клиент; сервер, headless-сервер и daemon; Mapper, AnimationViewer и ParticleViewer; компилятор AngelScript; Baker ресурсов | `linux-client`, `linux-server`, `linux-mapper`, `linux-ascompiler`, `linux-baker`, `linux-starter-smoke`, `linux-native-extension-smoke`, `linux-tutorial-smoke`, `linux-showcase-smoke`, `linux-showcase-capture`, `linux-tutorial-package`, `linux-package-smoke` | Обязательный workflow отвечает за процессные маршруты исходного кода starter-, native-extension-, минимального многопользовательского проекта и Content Showcase, снимок OpenGL Content Showcase на программном Mesa с пиксельной проверкой, а также за упакованные маршруты tutorial и packaging-matrix, которые собирают tar-архивы, инвентаризируют содержимое и запускают упакованные headless-бинарные файлы клиента и сервера со встроенной конфигурацией. | Текущая матрица квалифицирует только Ubuntu 24.04 и пути неподписанных тестовых пакетов после сохранения подтверждений обязательных job; другие дистрибутивы, установщики, подписание, магазины, развёртывание и откат требуют проектной приёмки. |
| Ubuntu 24.04 / Linux x64 | GCC 13 или новее | `build_gated` | настольный и headless-клиент; сервер, headless-сервер и daemon; Mapper, AnimationViewer и ParticleViewer; компилятор AngelScript; Baker ресурсов | `linux-gcc-client`, `linux-gcc-server`, `linux-gcc-mapper`, `linux-gcc-ascompiler`, `linux-gcc-baker` | Отдельный процессный smoke-маршрут для GCC не требуется. | Runtime-квалификация обеспечивается линией Clang; проекту, который зависит от поведения GCC, нужна собственная smoke-линия. |
| runner-ы macOS 26 для Intel и Apple Silicon / macOS x64 и arm64 | AppleClang | `build_gated` | настольный клиент | `mac-client` | Процессный smoke-маршрут клиента не требуется. | Публичная матрица не квалифицирует сервер, Mapper, инструменты, упаковку, подписание или notarization для macOS. |
| runner-ы macOS 26 для Intel и Apple Silicon / клиент iOS | AppleClang | `build_gated` | входные данные библиотеки клиента или пакета приложения | `ios-client` | Smoke-маршрут на симуляторе или устройстве не требуется. | За provisioning, подписание, доставку через App Store, ввод и звук на устройстве, сеть и приёмку lifecycle отвечает проект. |
| хост кросс-сборки Ubuntu 24.04 / Android armeabi-v7a и arm64-v8a | Android NDK Clang | `build_gated` | входные данные динамической библиотеки или пакета клиента | `android-arm32-client`, `android-arm64-client` | Smoke-маршрут на эмуляторе или физическом устройстве не требуется. | За сборку APK, подписание, доставку через магазины, GPU, ввод и звук на устройстве, приостановку и возобновление, а также приёмку сети отвечает проект. |
| хост кросс-сборки / Android x86 | Android NDK Clang | `source_capable` | входные данные динамической библиотеки или пакета клиента | `android-x86-client` | Обязательная линия CI или runtime отсутствует. | Не объявляйте Android x86 поддерживаемой релизной целью без проектной проверки сборки и устройства или эмулятора. |
| хост кросс-сборки Ubuntu 24.04 / WebAssembly | Emscripten Clang | `smoke_gated` | браузерный клиент | `web-client`, `web-showcase-runtime` | Обязательный workflow собирает штатный браузерный клиент и проводит Content Showcase через force-bake на native-хосте, raw/ZIP Web packaging, точные проверки payload, native-сервер, поставку пакета через HTTP, Chromium, настоящий контекст WebGL 2, lifecycle-маркеры и проверку пикселей композитора. | Штатная Web-цель поддерживает только клиент WebGL 2. Content Showcase квалифицирует один детерминированный localhost fixture в закреплённом Chromium; он не квалифицирует все пары браузеров и GPU либо production hosting, заголовки, хранение, активацию звука, развёртывание, CDN, подписание или откат. Эти приёмочные gates остаются ответственностью проекта. |
| Windows / Windows x64 | ClangCL 20 или новее | `source_capable` | настольный клиент; сервер; компилятор AngelScript; Baker ресурсов | `win64-clang-client`, `win64-clang-server`, `win64-clang-ascompiler`, `win64-clang-baker` | Обязательная линия CI или runtime отсутствует. | Используйте MSVC для квалифицированного маршрута Windows, если встраивающий проект не добавил собственную проверку ClangCL. |

## Квалификация рендеринга

| Платформы | Скомпилированные backend | Граница квалификации |
|---|---|---|
| Windows, Linux, macOS | платформенный OpenGL, а также отключаемые Vulkan и SDL_GPU, если исходный backend может инициализироваться | Покрытие сборки не означает покрытие рендерера, runtime или драйвера; каждому поставляемому backend нужна видимая проектная сцена. |
| Android и iOS | OpenGL ES и возможности платформы; Vulkan и SDL_GPU вне Web остаются опциональными на этапе компиляции | Требуется приёмка на устройстве. |
| Web | WebGL 2 | Vulkan и SDL_GPU исключены этапом платформы; требуется приёмка в браузере. |
| headless-приложения | frontend null/headless | Не делайте вывод о поддержке видимого рендеринга или звука по headless smoke-тесту. |

## Сводка

- Профили платформ: **10**
- Профили с проверкой сборки: **8**
- Профили со smoke-проверкой: **3**
- Уникальные обязательные цели проверки CI: **35**

Правила интерпретации релизов и требования приёмки проекта приведены в [матрице поддержки](support-matrix.md).
