---
title: Цели, платформы и наборы пакетов
document_id: generated-package-matrix
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-package-matrix","locale":"ru","source_path":"Docs/en/reference/packages/matrix.md","source_sha256":"26e9fd81c63624a476df71c977e2f1da8f566709990c91b5955ba21a6290c6c2"} -->

# Цели, платформы и наборы пакетов

> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/PackageInterface.json` или `BuildTools/package.py`, затем выполните `python BuildTools/docs_package.py --write`.

[Индекс](index.md) | [Объявление](declaration.md) | [Матрица](matrix.md) | [Содержимое](payloads.md) | [CLI](cli.md) | [Канонический JSON](../../../generated/package.json)

## Цели

| Стабильный ID | Цель | Ресурсы | Обязательные наборы | Назначение |
| --- | --- | --- | --- | --- |
| <a id="entry-package-target-server-aa345c8bbf"></a><code>package.target.Server</code> | <code>Server</code> | <code>server-and-client</code> | - | Бинарный файл сервера, ресурсы сервера и наборы ресурсов обновления клиента. |
| <a id="entry-package-target-client-6dc545ee39"></a><code>package.target.Client</code> | <code>Client</code> | <code>client</code> | - | Бинарный файл хоста/runtime клиента и ресурсы клиента. |
| <a id="entry-package-target-mapper-2b4270f627"></a><code>package.target.Mapper</code> | <code>Mapper</code> | <code>none</code> | <code>NoRes</code> | Бинарный файл Mapper без упакованных игровых ресурсов. |
| <a id="entry-package-target-baker-a25eccc9ba"></a><code>package.target.Baker</code> | <code>Baker</code> | <code>none</code> | <code>NoRes</code> | Исполняемый файл или библиотека Baker без упакованных игровых ресурсов. |
| <a id="entry-package-target-animationviewer-aefc678e92"></a><code>package.target.AnimationViewer</code> | <code>AnimationViewer</code> | <code>none</code> | <code>NoRes</code> | Автономный инструмент предварительного просмотра анимации без упакованных игровых ресурсов. |
| <a id="entry-package-target-particleviewer-d6fc572302"></a><code>package.target.ParticleViewer</code> | <code>ParticleViewer</code> | <code>none</code> | <code>NoRes</code> | Автономный инструмент предварительного просмотра частиц без упакованных игровых ресурсов. |

## Платформы

| Стабильный ID | Платформа | Статус | Архитектуры | Цели | Содержимое |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-package-platform-windows-8e4a2bee4e"></a><code>package.platform.Windows</code> | <code>Windows</code> | <code>implemented</code> | <code>win32</code>, <code>win64</code>, <code>win32-win7</code>, <code>win64-win7</code>, <code>arm64</code> | <code>Server</code>, <code>Client</code>, <code>Mapper</code>, <code>Baker</code>, <code>AnimationViewer</code>, <code>ParticleViewer</code> | Нативное содержимое PE с необязательными runtime-DLL, символами, архивами и MSI. Ключи -win7 разрешаются в канонические архитектуры бинарных файлов win32/win64 и требуют совпадающего явного POSTFIX, когда выход сборки имеет суффикс. arm64 принимается как существующий вход пакета, но стандартный реестр платформ BuildTools не предоставляет линию сборки Windows arm64. |
| <a id="entry-package-platform-linux-35772b1287"></a><code>package.platform.Linux</code> | <code>Linux</code> | <code>implemented</code> | <code>x64</code>, <code>arm64</code>, <code>x86</code>, <code>arm</code> | <code>Server</code>, <code>Client</code>, <code>Mapper</code>, <code>Baker</code>, <code>AnimationViewer</code>, <code>ParticleViewer</code> | Нативное содержимое ELF с необязательными runtime-библиотеками и архивами. |
| <a id="entry-package-platform-android-0cfd01357a"></a><code>package.platform.Android</code> | <code>Android</code> | <code>implemented</code> | <code>arm32</code>, <code>arm64</code>, <code>x86</code> | <code>Client</code> | Проект клиента Gradle с нативными библиотеками ABI, ресурсами и необязательным APK. |
| <a id="entry-package-platform-macos-a9032ded5f"></a><code>package.platform.macOS</code> | <code>macOS</code> | <code>unsupported</code> | <code>x64</code>, <code>arm64</code> | <code>Client</code> | Принимается argparse, но package_macos сейчас прерывает работу. |
| <a id="entry-package-platform-ios-15d87c2d03"></a><code>package.platform.iOS</code> | <code>iOS</code> | <code>unsupported</code> | <code>arm64</code>, <code>simulator</code> | <code>Client</code> | Принимается argparse, но package_ios сейчас прерывает работу. |
| <a id="entry-package-platform-web-c4af6ceacd"></a><code>package.platform.Web</code> | <code>Web</code> | <code>implemented</code> | <code>wasm</code> | <code>Client</code> | Содержимое браузерного клиента JavaScript/Wasm с предварительно загруженными ресурсами. |

## Токены наборов

| Стабильный ID | Набор | Категория | Статус | Платформы | Цели | Эффект |
| --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-package-pack-raw-d7354ed4ba"></a><code>package.pack.Raw</code> | <code>Raw</code> | <code>artifact</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code>, <code>Android</code>, <code>Web</code> | <code>Server</code>, <code>Client</code>, <code>Mapper</code>, <code>Baker</code>, <code>AnimationViewer</code>, <code>ParticleViewer</code> | Сохраняет подготовленный каталог цели после завершения упаковки. |
| <a id="entry-package-pack-zip-3dcbf3c445"></a><code>package.pack.Zip</code> | <code>Zip</code> | <code>artifact</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code>, <code>Android</code>, <code>Web</code> | <code>Server</code>, <code>Client</code>, <code>Mapper</code>, <code>Baker</code>, <code>AnimationViewer</code>, <code>ParticleViewer</code> | Создаёт детерминированный ZIP рядом с подготовленным каталогом цели. |
| <a id="entry-package-pack-singlezip-0c96b3b8c6"></a><code>package.pack.SingleZip</code> | <code>SingleZip</code> | <code>artifact</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code>, <code>Android</code>, <code>Web</code> | <code>Server</code>, <code>Client</code>, <code>Mapper</code>, <code>Baker</code>, <code>AnimationViewer</code>, <code>ParticleViewer</code> | Добавляет подготовленный каталог цели в единый ZIP всего пакета; одинаковые элементы с совпадающими именами сохраняются один раз, а конфликтующее содержимое приводит к ошибке упаковки. |
| <a id="entry-package-pack-tar-e6de1a9557"></a><code>package.pack.Tar</code> | <code>Tar</code> | <code>artifact</code> | <code>implemented</code> | <code>Linux</code> | <code>Server</code>, <code>Client</code>, <code>Mapper</code>, <code>Baker</code>, <code>AnimationViewer</code>, <code>ParticleViewer</code> | Создаёт несжатый архив tar. |
| <a id="entry-package-pack-targz-41851c8dfc"></a><code>package.pack.TarGz</code> | <code>TarGz</code> | <code>artifact</code> | <code>implemented</code> | <code>Linux</code> | <code>Server</code>, <code>Client</code>, <code>Mapper</code>, <code>Baker</code>, <code>AnimationViewer</code>, <code>ParticleViewer</code> | Создаёт архив tar со сжатием gzip. |
| <a id="entry-package-pack-root-e56dc75506"></a><code>package.pack.Root</code> | <code>Root</code> | <code>artifact</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code>, <code>Android</code>, <code>Web</code> | <code>Server</code>, <code>Client</code>, <code>Mapper</code>, <code>Baker</code>, <code>AnimationViewer</code>, <code>ParticleViewer</code> | Объединяет подготовленный каталог цели с корнем выходного каталога пакета. |
| <a id="entry-package-pack-wix-a1cec8e2e5"></a><code>package.pack.Wix</code> | <code>Wix</code> | <code>artifact</code> | <code>implemented</code> | <code>Windows</code> | <code>Client</code> | Собирает обязательный MSI из подготовленного содержимого клиента Windows; POSTFIX добавляется к базовому имени установщика. |
| <a id="entry-package-pack-apk-961529b696"></a><code>package.pack.Apk</code> | <code>Apk</code> | <code>artifact</code> | <code>implemented</code> | <code>Android</code> | <code>Client</code> | Запускает Gradle и копирует выбранный подписанный или отладочный APK рядом с подготовленным проектом. |
| <a id="entry-package-pack-debug-dca973b711"></a><code>package.pack.Debug</code> | <code>Debug</code> | <code>binary-selection</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code>, <code>Android</code>, <code>Web</code> | <code>Server</code>, <code>Client</code>, <code>Mapper</code>, <code>Baker</code>, <code>AnimationViewer</code>, <code>ParticleViewer</code> | Выбирает отладочные бинарные входы; для Android используется assembleDebug. |
| <a id="entry-package-pack-nores-5895efedf1"></a><code>package.pack.NoRes</code> | <code>NoRes</code> | <code>resource</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code> | <code>Server</code>, <code>Client</code>, <code>Mapper</code>, <code>Baker</code>, <code>AnimationViewer</code>, <code>ParticleViewer</code> | Пропускает подготовку ресурсов и изменение встроенных данных, конфигурации и имени сборки. |
| <a id="entry-package-pack-headless-c204e2e7ca"></a><code>package.pack.Headless</code> | <code>Headless</code> | <code>binary-selection</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code> | <code>Server</code>, <code>Client</code> | Включает обычный и Headless-варианты бинарного файла. |
| <a id="entry-package-pack-service-afbcd6373d"></a><code>package.pack.Service</code> | <code>Service</code> | <code>binary-selection</code> | <code>implemented</code> | <code>Windows</code> | <code>Server</code> | Включает обычный вариант сервера и вариант Windows Service. |
| <a id="entry-package-pack-daemon-7e0a3ed687"></a><code>package.pack.Daemon</code> | <code>Daemon</code> | <code>binary-selection</code> | <code>implemented</code> | <code>Linux</code> | <code>Server</code> | Включает обычный вариант сервера и вариант Linux Daemon. |
| <a id="entry-package-pack-totalprofiling-c82c1f4dfd"></a><code>package.pack.TotalProfiling</code> | <code>TotalProfiling</code> | <code>binary-selection</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code> | <code>Server</code>, <code>Client</code> | Включает обычный вариант бинарного файла и вариант полного профилирования. |
| <a id="entry-package-pack-ondemandprofiling-c1bba3db77"></a><code>package.pack.OnDemandProfiling</code> | <code>OnDemandProfiling</code> | <code>binary-selection</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code> | <code>Server</code>, <code>Client</code> | Включает обычный вариант бинарного файла и вариант профилирования по запросу. |
| <a id="entry-package-pack-ogl-fef4944b2d"></a><code>package.pack.OGL</code> | <code>OGL</code> | <code>binary-selection</code> | <code>implemented</code> | <code>Windows</code> | <code>Client</code> | Включает обычный и OpenGL-варианты клиента и задаёт ForceOpenGL=1. |
| <a id="entry-package-pack-lib-a225228362"></a><code>package.pack.Lib</code> | <code>Lib</code> | <code>binary-selection</code> | <code>implemented</code> | <code>Windows</code> | <code>Client</code>, <code>Baker</code> | Упаковывает приложение в виде динамической библиотеки вместо исполняемого файла. |
| <a id="entry-package-pack-webserver-c35e2bc615"></a><code>package.pack.WebServer</code> | <code>WebServer</code> | <code>payload</code> | <code>implemented</code> | <code>Web</code> | <code>Client</code> | Добавляет локальный помощник simple-web-server.py в содержимое браузерного клиента. |
| <a id="entry-package-pack-appimage-5f139403c1"></a><code>package.pack.AppImage</code> | <code>AppImage</code> | <code>artifact</code> | <code>placeholder</code> | <code>Linux</code> | <code>Client</code> | Зарезервированный токен; текущий упаковщик Linux не выпускает артефакт AppImage. |
