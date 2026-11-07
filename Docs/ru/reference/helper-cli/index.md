---
title: Сгенерированный справочник вспомогательных CLI
document_id: generated-helper-cli-index
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-helper-cli-index","locale":"ru","source_path":"Docs/en/reference/helper-cli/index.md","source_sha256":"2491dd3973af90f1241b429d37faeecc1360a0e2b0366ba227192ddf42166121"} -->

# Сгенерированный справочник вспомогательных CLI

> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/HelperCliInterface.json` или владеющий исполняемый парсер, затем выполните `python BuildTools/docs_helper_cli.py --write`.

[Индекс справочника](index.md) | [Команды](commands.md) | [Каноническая JSON-модель](../../../generated/helper-cli.json) | [Контракт генерации](../metadata/)

Этот справочник создаётся из объектов `argparse.ArgumentParser`, используемых исполняемыми вспомогательными скриптами движка. Манифест владеет назначением и аудиторией, а исходные парсеры владеют исполняемым синтаксисом.

## Статус контракта

| Поле | Значение |
| --- | --- |
| Стабильность | <code>internal</code> |
| Начиная с версии | Не объявлено |
| Политика поддержки | Командные строки helpers являются привязанными к ревизии интерфейсами реализации; автоматизация должна закреплять ревизию движка. |
| Исходный манифест | [BuildTools/HelperCliInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/HelperCliInterface.json) |
| Digest контракта | <code>b5d6391cbb8dc43b7a7d75ae83e85381cb8af54396fb805b33ace1359248e950</code> |

## Инвентарь

| Стабильный ID | Helper | Владелец | Владелец вызова | Исходный парсер | Команды / глобальные аргументы |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codegen-60abdf415d"></a><code>helper-cli.codegen</code> | [Генерация кода](commands.md#entry-helper-cli-codegen-60abdf415d) | <code>build-release</code> | BuildTools/cmake/stages/Codegen.cmake | [BuildTools/codegen.py](https://github.com/cvet/fonline/blob/master/BuildTools/codegen.py) | 0 / 10 |
| <a id="entry-helper-cli-codecoverage-b014400e5e"></a><code>helper-cli.codecoverage</code> | [Покрытие кода](commands.md#entry-helper-cli-codecoverage-b014400e5e) | <code>quality</code> | BuildTools/cmake/stages/Applications.cmake | [BuildTools/codecoverage.py](https://github.com/cvet/fonline/blob/master/BuildTools/codecoverage.py) | 4 / 0 |
| <a id="entry-helper-cli-gameplay-test-runner-b34ed8deb4"></a><code>helper-cli.gameplay-test-runner</code> | [Запуск игровых тестов](commands.md#entry-helper-cli-gameplay-test-runner-b34ed8deb4) | <code>quality</code> | CMake-цели подключающего проекта и CI smoke-задачи игровых тестов | [BuildTools/gameplay_test_runner.py](https://github.com/cvet/fonline/blob/master/BuildTools/gameplay_test_runner.py) | 0 / 3 |
| <a id="entry-helper-cli-ai-control-client-35184e9731"></a><code>helper-cli.ai-control-client</code> | [Клиент протокола AiControl](commands.md#entry-helper-cli-ai-control-client-35184e9731) | <code>tooling</code> | ИИ-адаптеры проекта, smoke-тесты протокола и непосредственная диагностика разработчиком | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) | 5 / 5 |
| <a id="entry-helper-cli-format-content-55dd5b4af8"></a><code>helper-cli.format-content</code> | [Форматировщик контента](commands.md#entry-helper-cli-format-content-55dd5b4af8) | <code>tooling</code> | BuildTools/buildtools.py format-content, задачи форматирования встраивающего игрового проекта и проверки расхождений в CI | [BuildTools/format_content.py](https://github.com/cvet/fonline/blob/master/BuildTools/format_content.py) | 0 / 8 |
| <a id="entry-helper-cli-fo-formats-9aa60fdd1b"></a><code>helper-cli.fo-formats</code> | [Средство чтения форматов контента движка](commands.md#entry-helper-cli-fo-formats-9aa60fdd1b) | <code>tooling</code> | контентные инструменты встраивающего игрового проекта и прямая диагностика разработчиком | [BuildTools/fo_formats.py](https://github.com/cvet/fonline/blob/master/BuildTools/fo_formats.py) | 3 / 0 |
| <a id="entry-helper-cli-migration-guard-3e5c9b3e72"></a><code>helper-cli.migration-guard</code> | [Контроль миграций идентичности](commands.md#entry-helper-cli-migration-guard-3e5c9b3e72) | <code>quality</code> | локальные задачи проверки встраивающего игрового проекта и контентные гейты CI | [BuildTools/migration_guard.py](https://github.com/cvet/fonline/blob/master/BuildTools/migration_guard.py) | 0 / 11 |
| <a id="entry-helper-cli-ai-control-mcp-f5daba9e92"></a><code>helper-cli.ai-control-mcp</code> | [MCP-адаптер AiControl](commands.md#entry-helper-cli-ai-control-mcp-f5daba9e92) | <code>tooling</code> | конфигурации MCP-клиентов встраивающего проекта, проектные модули инструментов и раннеры ИИ-плейтестов | [BuildTools/ai_control_mcp/cli.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_mcp/cli.py) | 0 / 14 |
| <a id="entry-helper-cli-windows7-import-check-a0c7e4cb59"></a><code>helper-cli.windows7-import-check</code> | [Проверка импортов Windows 7](commands.md#entry-helper-cli-windows7-import-check-a0c7e4cb59) | <code>quality</code> | CI и релизная проверка Windows 7 подключающего проекта | [BuildTools/check_windows7_imports.py](https://github.com/cvet/fonline/blob/master/BuildTools/check_windows7_imports.py) | 0 / 2 |
| <a id="entry-helper-cli-markdown-links-dd3ef679c3"></a><code>helper-cli.markdown-links</code> | [Проверка ссылок Markdown](commands.md#entry-helper-cli-markdown-links-dd3ef679c3) | <code>quality</code> | обёртки документации встраивающего игрового проекта, локальные задачи и задания документации в CI | [BuildTools/markdown_links.py](https://github.com/cvet/fonline/blob/master/BuildTools/markdown_links.py) | 0 / 10 |
| <a id="entry-helper-cli-native-source-audit-b6c759ee81"></a><code>helper-cli.native-source-audit</code> | [Аудиты нативных исходников](commands.md#entry-helper-cli-native-source-audit-b6c759ee81) | <code>quality</code> | CI проверки движка, обёртки нативных аудитов встраивающего игрового проекта и прямое использование разработчиком | [BuildTools/native_source_audit.py](https://github.com/cvet/fonline/blob/master/BuildTools/native_source_audit.py) | 5 / 0 |
| <a id="entry-helper-cli-native-safety-audit-2f4598f1d2"></a><code>helper-cli.native-safety-audit</code> | [Аудиты нативной безопасности](commands.md#entry-helper-cli-native-safety-audit-2f4598f1d2) | <code>quality</code> | workflow validate движка, задачи нативной проверки встраивающего игрового проекта и CI | [BuildTools/native_safety_audit.py](https://github.com/cvet/fonline/blob/master/BuildTools/native_safety_audit.py) | 2 / 0 |
| <a id="entry-helper-cli-android-device-ab99179ae9"></a><code>helper-cli.android-device</code> | [Управление устройством Android](commands.md#entry-helper-cli-android-device-ab99179ae9) | <code>platform</code> | Android-задачи подключающего проекта и непосредственное использование разработчиком | [BuildTools/android_device.py](https://github.com/cvet/fonline/blob/master/BuildTools/android_device.py) | 7 / 1 |
| <a id="entry-helper-cli-simple-web-server-58fbf70798"></a><code>helper-cli.simple-web-server</code> | [Локальный Web-сервер](commands.md#entry-helper-cli-simple-web-server-58fbf70798) | <code>platform</code> | payload WebServer в BuildTools/package.py | [BuildTools/web/simple-web-server.py](https://github.com/cvet/fonline/blob/master/BuildTools/web/simple-web-server.py) | 0 / 2 |
| <a id="entry-helper-cli-artifact-checksums-efe1b1aaa6"></a><code>helper-cli.artifact-checksums</code> | [Контрольные суммы артефактов](commands.md#entry-helper-cli-artifact-checksums-efe1b1aaa6) | <code>build-release</code> | релизные задачи подключающего проекта после остановки последнего записывающего пакеты процесса | [BuildTools/artifact_checksums.py](https://github.com/cvet/fonline/blob/master/BuildTools/artifact_checksums.py) | 0 / 6 |
| <a id="entry-helper-cli-project-version-52ce8849a0"></a><code>helper-cli.project-version</code> | [Генератор версии проекта](commands.md#entry-helper-cli-project-version-52ce8849a0) | <code>build-release</code> | задачи версии, hooks и проверки CI подключающего проекта | [BuildTools/project_version.py](https://github.com/cvet/fonline/blob/master/BuildTools/project_version.py) | 0 / 8 |
| <a id="entry-helper-cli-release-preflight-f5192a7f8b"></a><code>helper-cli.release-preflight</code> | [Предварительная проверка релиза](commands.md#entry-helper-cli-release-preflight-f5192a7f8b) | <code>build-release</code> | ручные релизные задачи подключающего проекта перед созданием тега и публикацией | [BuildTools/release_preflight.py](https://github.com/cvet/fonline/blob/master/BuildTools/release_preflight.py) | 0 / 17 |
| <a id="entry-helper-cli-engine-upgrade-report-a1902fe984"></a><code>helper-cli.engine-upgrade-report</code> | [Отчёт об обновлении Engine](commands.md#entry-helper-cli-engine-upgrade-report-a1902fe984) | <code>documentation</code> | работа подключающего проекта по обновлению Engine, запуск из корня проекта | [BuildTools/engine_upgrade_report.py](https://github.com/cvet/fonline/blob/master/BuildTools/engine_upgrade_report.py) | 0 / 10 |
| <a id="entry-helper-cli-script-binding-check-56e942e304"></a><code>helper-cli.script-binding-check</code> | [Проверка привязок скриптов](commands.md#entry-helper-cli-script-binding-check-56e942e304) | <code>quality</code> | CI движка и гейты качества встраивающего игрового проекта | [BuildTools/check_script_bindings.py](https://github.com/cvet/fonline/blob/master/BuildTools/check_script_bindings.py) | 0 / 7 |
| <a id="entry-helper-cli-angelscript-hygiene-check-b5053583a9"></a><code>helper-cli.angelscript-hygiene-check</code> | [Проверка гигиены кода AngelScript](commands.md#entry-helper-cli-angelscript-hygiene-check-b5053583a9) | <code>quality</code> | CI движка и гейты качества встраивающего игрового проекта | [BuildTools/check_angelscript_hygiene.py](https://github.com/cvet/fonline/blob/master/BuildTools/check_angelscript_hygiene.py) | 0 / 7 |
| <a id="entry-helper-cli-createmsi-18899fd2a5"></a><code>helper-cli.createmsi</code> | [Создание MSI](commands.md#entry-helper-cli-createmsi-18899fd2a5) | <code>build-release</code> | пакет Wix в BuildTools/package.py | [BuildTools/msicreator/createmsi.py](https://github.com/cvet/fonline/blob/master/BuildTools/msicreator/createmsi.py) | 0 / 2 |
| <a id="entry-helper-cli-move-sync-analysis-d49c9c73db"></a><code>helper-cli.move-sync-analysis</code> | [Анализ трейса синхронизации движения](commands.md#entry-helper-cli-move-sync-analysis-d49c9c73db) | <code>networking</code> | плейтесты движения встраивающего игрового проекта и прямая диагностика разработчиком | [BuildTools/move_sync_analysis.py](https://github.com/cvet/fonline/blob/master/BuildTools/move_sync_analysis.py) | 0 / 10 |
| <a id="entry-helper-cli-check-text-packs-fedc98c863"></a><code>helper-cli.check-text-packs</code> | [Проверка текстовых пакетов](commands.md#entry-helper-cli-check-text-packs-fedc98c863) | <code>localization</code> | задачи локализации встраивающего игрового проекта и контентные гейты CI | [BuildTools/check_text_packs.py](https://github.com/cvet/fonline/blob/master/BuildTools/check_text_packs.py) | 0 / 11 |

## Покрытие

Модель содержит 23 helpers, 26 подкоманд, 144 глобальных аргументов и 174 аргументов подкоманд.

Включено:

- принадлежащие движку вспомогательные скрипты Python с фабрикой верхнего уровня create_parser();
- точные usage и вывод справки argparse, аргументы, подкоманды, владельцы, аудитории и контекст вызова.

Исключено:

- отдельно моделируемая командная строка BuildTools/buildtools.py;
- отдельно моделируемая командная строка BuildTools/package.py и контракт объявления пакетов;
- генераторы документации, тесты, библиотечные модули, shell-скрипты и инструменты подключающего проекта.
