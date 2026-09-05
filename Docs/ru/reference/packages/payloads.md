---
title: Содержимое пакетов и артефакты
document_id: generated-package-payloads
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-package-payloads","locale":"ru","source_path":"Docs/en/reference/packages/payloads.md","source_sha256":"4083db1f6a534b87d3a93186526435378e28e646fab25e6dda0c6a995a71924b"} -->

# Содержимое пакетов и артефакты

> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/PackageInterface.json` или `BuildTools/package.py`, затем выполните `python BuildTools/docs_package.py --write`.

[Индекс](index.md) | [Объявление](declaration.md) | [Матрица](matrix.md) | [Содержимое](payloads.md) | [CLI](cli.md) | [Канонический JSON](../../../generated/package.json)

Упаковщик подготавливает содержимое одной цели, применяет преобразования бинарных файлов и ресурсов, выпускает артефакты в фиксированном порядке завершения и удаляет промежуточный каталог, если токен `Raw` не требует его сохранить.

## Содержимое для платформ

| Стабильный ID | Платформа | Статус | Содержимое |
| --- | --- | --- | --- |
| <a id="entry-package-payload-windows-6b9618ed08"></a><code>package.payload.Windows</code> | <code>Windows</code> | <code>implemented</code> | Изменённый исполняемый файл PE или DLL, сопутствующие runtime-файлы, PDB при наличии и каталоги ZIP с ресурсами, если не выбран NoRes. Ключи win32-win7 и win64-win7 читают каноническую запись бинарного файла win32/win64, выбранную тем же POSTFIX отдельной записи. |
| <a id="entry-package-payload-linux-92e6941cc5"></a><code>package.payload.Linux</code> | <code>Linux</code> | <code>implemented</code> | Изменённый исполняемый файл или динамическая библиотека runtime, сопутствующие файлы и каталоги ZIP с ресурсами, если не выбран NoRes. |
| <a id="entry-package-payload-android-094c0cd541"></a><code>package.payload.Android</code> | <code>Android</code> | <code>implemented</code> | Сгенерированный проект Gradle с libmain.so для каждого ABI и запечёнными ресурсами в assets приложения; Apk при необходимости выпускает соседний APK. |
| <a id="entry-package-payload-web-04b1334012"></a><code>package.payload.Web</code> | <code>Web</code> | <code>implemented</code> | JavaScript, изменённый Wasm, оболочка HTML, файлы предварительной загрузки Resources.data/Resources.js и необязательный помощник локального сервера. |
| <a id="entry-package-payload-macos-acb0bdca5d"></a><code>package.payload.macOS</code> | <code>macOS</code> | <code>unsupported</code> | В текущем состоянии репозитория содержимое не выпускается. |
| <a id="entry-package-payload-ios-c0357994eb"></a><code>package.payload.iOS</code> | <code>iOS</code> | <code>unsupported</code> | В текущем состоянии репозитория содержимое не выпускается. |

## Наборы, создающие выходные артефакты

| Стабильный ID | Набор | Статус | Выход | Эффект |
| --- | --- | --- | --- | --- |
| <a id="entry-package-pack-raw-d7354ed4ba"></a><code>package.pack.Raw</code> | <code>Raw</code> | <code>implemented</code> | <code>&lt;target-output&gt;</code> | Сохраняет подготовленный каталог цели после завершения упаковки. |
| <a id="entry-package-pack-zip-3dcbf3c445"></a><code>package.pack.Zip</code> | <code>Zip</code> | <code>implemented</code> | <code>&lt;target-output&gt;.zip</code> | Создаёт детерминированный ZIP рядом с подготовленным каталогом цели. |
| <a id="entry-package-pack-singlezip-0c96b3b8c6"></a><code>package.pack.SingleZip</code> | <code>SingleZip</code> | <code>implemented</code> | <code>&lt;output&gt;/&lt;output-name&gt;.zip</code> | Добавляет подготовленный каталог цели в единый ZIP всего пакета; одинаковые элементы с совпадающими именами сохраняются один раз, а конфликтующее содержимое приводит к ошибке упаковки. |
| <a id="entry-package-pack-tar-e6de1a9557"></a><code>package.pack.Tar</code> | <code>Tar</code> | <code>implemented</code> | <code>&lt;target-output&gt;.tar</code> | Создаёт несжатый архив tar. |
| <a id="entry-package-pack-targz-41851c8dfc"></a><code>package.pack.TarGz</code> | <code>TarGz</code> | <code>implemented</code> | <code>&lt;target-output&gt;.tar.gz</code> | Создаёт архив tar со сжатием gzip. |
| <a id="entry-package-pack-root-e56dc75506"></a><code>package.pack.Root</code> | <code>Root</code> | <code>implemented</code> | <code>&lt;output&gt;</code> | Объединяет подготовленный каталог цели с корнем выходного каталога пакета. |
| <a id="entry-package-pack-wix-a1cec8e2e5"></a><code>package.pack.Wix</code> | <code>Wix</code> | <code>implemented</code> | <code>&lt;output&gt;/&lt;nice-name&gt;[_&lt;postfix&gt;].msi</code> | Собирает обязательный MSI из подготовленного содержимого клиента Windows; POSTFIX добавляется к базовому имени установщика. |
| <a id="entry-package-pack-apk-961529b696"></a><code>package.pack.Apk</code> | <code>Apk</code> | <code>implemented</code> | <code>&lt;output&gt;/&lt;target-output-name&gt;.apk</code> | Запускает Gradle и копирует выбранный подписанный или отладочный APK рядом с подготовленным проектом. |

Вызов упаковщика должен выбрать хотя бы один реализованный набор, создающий выходной артефакт. Списки только из модификаторов, а также неизвестные, повторяющиеся, резервные, неподдерживаемые платформой или несовместимые с целью токены отклоняются до подготовки выходных данных.
