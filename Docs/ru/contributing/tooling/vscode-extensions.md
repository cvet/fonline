---
layout: default
title: Сопровождение расширений VS Code для FOnline
locale: ru
document_id: contributing-vscode-extensions
permalink: /Docs/ru/contributing/tooling/vscode-extensions.html
---

# Сопровождение расширений VS Code для FOnline
<!-- docs-translation: {"document_id":"contributing-vscode-extensions","locale":"ru","source_path":"Docs/en/contributing/tooling/vscode-extensions.md","source_sha256":"f6cbb4bad1fbc5f6026073688d76fa4fb15f05a69e5a45e6590c51d259d8d121"} -->
Это руководство предназначено для участников разработки движка, которые
изменяют `BuildTools/fonline-tools` (FOnline Tools) или
`BuildTools/angelscript-debugger` (отладчик AngelScript для FOnline). Оба -
рабочие инструменты, которыми владеет движок: проекты загружают их как папки с
исходниками из `BuildTools` через отслеживаемые символические ссылки рабочей
области. Руководство описывает структуру исходников, команды сборки и тестов,
какой закоммиченный вывод служит точкой входа во время выполнения, как менять
грамматику меток, не ломая рабочие области проектов, и какие проверки нужны
изменению, включая то, чего автоматические тесты не покрывают.

## Проверенные исходные пути

- `.gitignore`
- `.gitattributes`
- `.github/workflows/validate.yml`
- `BuildTools/fonline-tools/package.json`
- `BuildTools/fonline-tools/tsconfig.json`
- `BuildTools/fonline-tools/.gitignore`
- `BuildTools/fonline-tools/src/extension.ts`
- `BuildTools/fonline-tools/src/jsonc.ts`
- `BuildTools/fonline-tools/src/tests/jsonc.test.ts`
- `BuildTools/fonline-tools/dist/extension.js`
- `BuildTools/angelscript-debugger/package.json`
- `BuildTools/angelscript-debugger/tsconfig.json`
- `BuildTools/angelscript-debugger/.gitignore`
- `BuildTools/angelscript-debugger/src/extension.ts`
- `BuildTools/angelscript-debugger/src/activateFosDebug.ts`
- `BuildTools/angelscript-debugger/src/attachDiscovery.ts`
- `BuildTools/angelscript-debugger/src/debugAdapter.ts`
- `BuildTools/angelscript-debugger/src/fosDebug.ts`
- `BuildTools/angelscript-debugger/src/fosRuntime.ts`
- `BuildTools/angelscript-debugger/src/web-extension.ts`
- `BuildTools/angelscript-debugger/src/tests/adapter.test.ts`
- `BuildTools/angelscript-debugger/src/tests/attachDiscovery.test.ts`
- `BuildTools/angelscript-debugger/dist/extension.js`
- `BuildTools/angelscript-debugger/dist/debugAdapter.js`
- `BuildTools/tests/test_docs_debugging.py`
- `Docs/en/troubleshooting/debugging.md`
- `Docs/en/how-to/project/vscode-workspace.md`
- `Docs/en/contributing/documentation/index.md`
- `Docs/en/how-to/release/versioning.md`

## Структура

Сопровождение расширений находится в `contributing/tooling`, потому что его
аудитория - участники разработки движка, а не игровые команды. Игровые команды
используют руководство
[Настройка рабочей области VS Code для проекта FOnline](../../how-to/project/vscode-workspace.md).

### FOnline Tools

| Путь | Роль |
| --- | --- |
| `package.json` | Манифест расширения: вносимые команды и сочетания клавиш (contributions), активация, scripts. `main` равен `./dist/extension.js`. |
| `src/extension.ts` | Построение меню, фильтрация по платформе и горячие клавиши, один файл примерно из 480 строк. |
| `src/jsonc.ts` | Разборщик `.vscode/tasks.json`: строгий JSON плюс комментарии, завершающие запятые и метка порядка байтов в начале - ослабленный синтаксис, который принимает VS Code. |
| `src/tests/jsonc.test.ts` | Unit-тесты разборщика на `node:test`. |
| `tsconfig.json` | CommonJS, ES2020, `strict`, `noUnusedLocals`, `noEmit`: `tsc` только проверяет типы, бандл пишет esbuild. |
| `dist/extension.js`, `dist/extension.js.map` | Закоммиченный бандл esbuild. Именно его запускает VS Code. |
| `.gitignore` | Игнорирует `test-out/`, куда `npm test` пишет собранные тесты. |

В пакете нет README, значка, `.vscodeignore` и скрипта `package`: движок
поставляет расширение как папку с исходниками, а проекты устанавливают его через
символическую ссылку рабочей области, описанную в руководстве по рабочей
области.

### Отладчик AngelScript

| Путь | Роль |
| --- | --- |
| `package.json` | Манифест: язык `fos`, грамматика, тип отладчика `fos`, свойства attach, scripts. `main` равен `./dist/extension.js`. |
| `src/extension.ts`, `src/activateFosDebug.ts`, `src/web-extension.ts` | Точки входа хоста расширений; `activateFosDebug.ts` содержит провайдер конфигураций, который запускает обнаружение целей для attach и применяет фильтр `processId`. |
| `src/debugAdapter.ts`, `src/fosDebug.ts`, `src/fosRuntime.ts`, `src/attachDiscovery.ts` | Адаптер отладки, его демонстрационная среда выполнения (sample runtime) и транспорт attach по UDP/TCP. `attachDiscovery.ts` также содержит выбор цели, общий для провайдера конфигураций и адаптера. |
| `src/tests/` | `adapter.test.ts` с его файлами данных и `attachDiscovery.test.ts`. |
| `dist/extension.js`, `dist/web-extension.js`, `dist/debugAdapter.js` (+ map-файлы) | Закоммиченные бандлы. `contributes.debuggers[].program` равен `./dist/debugAdapter.js`. |
| `sublime-angelscript/`, `language-configuration.json`, `icon.png` | Грамматика и ресурсы редактора, которые расширение вносит в VS Code. |
| `.gitignore` | Игнорирует `test-out/`, куда `npm test` пишет собранные тесты. |

Режим работы адаптера задан константой времени компиляции в `src/extension.ts`
(`runMode`), сейчас это `inline`. Обнаружению целей нужен модуль UDP из Node, и
в браузерном бандле оно недоступно; `src/attachDiscovery.ts` сообщает об этом,
и тогда провайдер конфигураций запрашивает явную конечную точку `tcp://`.

## Какой вывод служит точкой входа

Оба расширения загружают `dist/extension.js` (`main` в каждом `package.json`), и
`dist/` - единственный вывод сборки, который отслеживает каждый пакет. Ни один
`tsconfig.json` ничего не выдаёт: `tsc` запускается только как
`npm run typecheck`, а все бандлы создаёт esbuild. Изменение в `src/` не
действует для пользователей, пока `dist/` не пересобран из него и не закоммичен
вместе с исходниками.

## Предварительные требования и команды

Оба пакета - самостоятельные npm-проекты; ни один из них не собирается ни
CMake, ни каким-либо рабочим процессом Engine. Используйте Node.js 18.18 или
новее с npm: тестовые скрипты опираются на `node:test` и mocha 11. Результаты
ниже получены на Node `24.16.0` и npm `11.13.0` в Windows, во временных копиях
каждого пакета вне репозитория, с TypeScript 4.9.5, esbuild 0.28.2 и
`@types/vscode` 1.89.0.

`package-lock.json` не закоммичен: `.gitignore` репозитория игнорирует
`package-lock.json`, кроме двух названных пакетов для тестов и документации.
Поэтому зависимости не зафиксированы и меняются в пределах объявленных
диапазонов. Два диапазона намеренно узкие: `typescript` равен `^4.9.5`, потому
что исходники используют оператор `satisfies`, а `@types/vscode` равен
`~1.89.0` - минимальной версии редактора из `engines.vscode`, чтобы `typecheck`
отклонял более новый API VS Code.

### FOnline Tools

Выполните из `BuildTools/fonline-tools`:

```bash
npm install
npm test
npm run build
```

| Скрипт | Выполняемая команда |
| --- | --- |
| `typecheck` | `tsc -p tsconfig.json --noEmit` |
| `build` | `esbuild ./src/extension.ts --bundle --tsconfig=./tsconfig.json --external:vscode --format=cjs --platform=node --outfile=dist/extension.js --sourcemap --sources-content=false` |
| `watch` | Команда `build` с `--watch` |
| `test` | `typecheck`, затем esbuild собирает `src/tests/jsonc.test.ts` в `test-out/`, и его запускает `node --test` |
| `vscode:prepublish` | Команда esbuild с `--minify` |

Наблюдаемое поведение во временной копии: `npm run typecheck` проходит,
`npm test` проходит пять тестов разборщика, а `npm run build` воспроизводит
закоммиченные файлы `dist/`, если не считать окончаний строк. Тесты покрывают
только разборщик; построению меню, фильтрации по платформе и горячим клавишам
нужен API VS Code. Они проверены запуском бандла против разовой заглушки этого
API, которая не входит в репозиторий.

### Отладчик AngelScript

Выполните из `BuildTools/angelscript-debugger`:

```bash
npm install
npm test
npm run build
```

| Скрипт | Выполняемая команда |
| --- | --- |
| `typecheck` | `tsc -p tsconfig.json --noEmit` |
| `build` | Бандлы `esbuild-base`, `esbuild-web` и `esbuild-adapter` с картами исходников |
| `test` | `typecheck`, `build`, затем esbuild собирает `src/tests/*.test.ts` в `test-out/tests/`, и их запускает `mocha --ui tdd` |
| `watch`, `watch-web` | Сборки бандлов расширения и веб-версии в режиме наблюдения |
| `vscode:prepublish` | `rimraf dist` и минифицированные сборки всех трёх бандлов |
| `package`, `publish`, `publish-pre-release` | Команды `vsce`; движок не упаковывает и не публикует VSIX, проекты используют папку по символической ссылке |

Наблюдаемое поведение во временной копии: `npm run typecheck` проходит, а
`npm test` проходит 13 тестов. Восемь из них - тесты адаптера в
`adapter.test.ts`: они запускают пересобранный `dist/debugAdapter.js` по Debug
Adapter Protocol и прогоняют демонстрационную среду выполнения на файлах данных
Markdown, то есть проверяют обработку протокола адаптером, а не живой endpoint
движка. Пять - unit-тесты выбора цели attach: фильтр не задан, точное
совпадение `processId`, голый идентификатор процесса, который ничему не
соответствует, одинаковые идентификаторы с нескольких хостов и явный
`endpoint`. Поскольку `npm test` пересобирает `dist/` перед тестами адаптера,
запускайте его перед коммитом, чтобы закоммиченные бандлы соответствовали
исходникам. Скрипта lint в пакете нет; его статической проверкой служит
`typecheck` с `strict` и `noUnusedLocals`.

Состояние поставки адаптера (приватный пакет, папка с исходниками,
устанавливаемая через символическую ссылку, нет lock-файла, нет VSIX, тесты
адаптера только на демонстрационной среде выполнения) зафиксировано в разделе
[Состояние поставки адаптера](../../troubleshooting/debugging.md#состояние-поставки-адаптера).
При изменении пакета поддерживайте согласованность этой страницы и её теста
`BuildTools/tests/test_docs_debugging.py`.

## Безопасное расширение грамматики меток

Грамматика реализована только в `BuildTools/fonline-tools/src/extension.ts`.
Проекты зависят от каждой детали, описанной в
[руководстве по рабочей области](../../how-to/project/vscode-workspace.md#как-fonline-tools-строит-меню).
Изменение тега, правила приоритета или правила горячих клавиш меняет меню
каждого проекта, использующего расширение, а версионируемого контракта для
этого у Engine нет.

Порядок изменения грамматики:

1. Изучите текущее поведение по руководству по рабочей области и функции,
   которые оно описывает. Фильтрация по платформе - одна функция,
   `matchesCurrentPlatform`, общая для задач, конфигураций запуска и составных
   конфигураций (compounds). Теги из метки удаляет `stripPlatformTags`, а
   группировку выполняет `parseGroupedTaskName`.
2. Измените исходники. Путь задач (`getTaskPicks`) и путь запусков
   (`getLaunchPicks`) дублируют код группировки; обновите оба. Меню и горячие
   клавиши используют один список, `getTopLevelPicks`, который каждое нажатие
   горячей клавиши строит заново, поэтому отдельного кэша горячих клавиш,
   требующего синхронизации, нет. Сохраняйте в `src/jsonc.ts` приём всего, что
   VS Code принимает в `tasks.json`, и дополняйте `src/tests/jsonc.test.ts` при
   его изменении.
3. Держите теги внутри метки или имени. Удаление тега из хранимой строки
   сломало бы ссылки `dependsOn`, `preLaunchTask` и составных конфигураций в
   существующих проектах.
4. При изменении горячей клавиши или команды обновите `contributes.commands` и
   `contributes.keybindings` в `package.json`; `MAX_SORT_HOTKEYS` в исходниках и
   девять вносимых команд должны совпадать.
5. Выполните `npm test` и `npm run build`, затем закоммитьте `src/` и
   пересобранный `dist/` вместе. Изменение без пересобранного `dist/` не
   действует для пользователей, потому что точкой входа служит `dist/`.
6. Обновите [руководство по рабочей области](../../how-to/project/vscode-workspace.md)
   и его русское зеркало, включая таблицу грамматики меток, правила платформ и
   контрольный список проверки. Обновляйте
   [руководство по отладке](../../troubleshooting/debugging.md), только если
   изменилось поведение attach.
7. Добавьте запись в историю изменений по правилам из
   [версионирования](../../how-to/release/versioning.md#каждое-обновление-master)
   и опишите изменение поведения для встраивающих проектов, потому что
   существующий `tasks.json` может перестать совпадать.

Автоматические тесты FOnline Tools покрывают только разборщик `tasks.json`.
Само меню проверяется вручную: откройте временную рабочую область с
`tasks.json` и `launch.json`, выполните `FOnline: Show Tools` и несколько
горячих клавиш, измените метку с `[sort:N]` и снова нажмите её горячую клавишу,
а затем пройдите контрольный список из руководства по рабочей области на каждой
платформе хоста, поддержку которой вы заявляете.

## Контрольный список изменения

1. Изменение сделано в `src/`, а `dist/` пересобран через `npm run build` из тех
   же исходников.
2. `npm test` проходит в каждом изменённом пакете; он включает `typecheck`,
   который не допускает ошибок.
3. `dist/` не правился вручную.
4. Поведение грамматики меток, разбора `tasks.json` и горячих клавиш
   перепроверено по исходникам и отражено в руководстве по рабочей области на
   английском и русском.
5. Метки из `tasks.json` и `launch.json` в руководстве по рабочей области
   по-прежнему дают то же меню.
6. Вклады (contributions) в `package.json` соответствуют исходникам.
7. Для отладчика: свойства attach в `package.json`, провайдер конфигураций и
   руководство по отладке согласованы, а формулировки о состоянии поставки в
   руководстве по отладке и в `test_docs_debugging.py` по-прежнему верны.
8. Проверки документации из
   [сопровождения документации](../documentation/index.md#контрольный-список-проверки)
   проходят, и запись в истории изменений существует.

## Сводка покрытия

| Область | Автоматическое покрытие |
| --- | --- |
| Проверка типов обоих пакетов | `npm run typecheck`, часть каждого `npm test` |
| Разборщик `tasks.json` в FOnline Tools | `npm test`: `src/tests/jsonc.test.ts` |
| Меню, фильтрация по платформе и горячие клавиши FOnline Tools | Нет; ручная проверка выше |
| Обработка протокола адаптером отладчика | `npm test`: `src/tests/adapter.test.ts` на демонстрационной среде выполнения через пересобранный `dist/debugAdapter.js` |
| Выбор цели attach в отладчике | `npm test`: `src/tests/attachDiscovery.test.ts` |
| Метаданные пакета отладчика | Тест документации `BuildTools/tests/test_docs_debugging.py` |
| UDP-обнаружение и живой attach к процессу Engine | Нет |

Тесты пакетов запускаются на машине разработчика.
`.github/workflows/validate.yml` не запускает ни один из них: его единственная
настройка Node.js относится к job браузерной проверки документации, который
устанавливает зависимости из закоммиченного lock-файла, а у пакетов расширений
lock-файла нет.
