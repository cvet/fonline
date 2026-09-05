---
title: Интеграция и проверка GUI
document_id: generated-gui-runtime-integration-validation
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-gui-runtime-integration-validation","locale":"ru","source_path":"Docs/en/reference/gui-runtime/integration-validation.md","source_sha256":"d111a7e90407373b60344b2c69374ea26ee857cde8b9224b6dcd5e04623406b8"} -->

# Интеграция и проверка GUI

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/GuiRuntimeInterface.json`, затем выполните `python BuildTools/docs_gui_runtime.py --write`.

[Индекс](index.md) | [Типы](types.md) | [API экранов](screen-api.md) | [Жизненный цикл](lifecycle.md) | [Компоновка](layout-rendering.md) | [Ввод](input.md) | [Интеграция](integration-validation.md) | [Канонический JSON](../../../generated/gui-runtime.json) | [Руководство](../../how-to/runtime/gui.md)

## Правила интеграции

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-gui-runtime-integration-no-declarative-parser-04472fe1a3"></a><code>gui-runtime.integration.no-declarative-parser</code> | Нет декларативного GUI-формата Engine | Рассматривайте Gui.fos и Input.fos как runtime API AngelScript; не заявляйте, что Engine разбирает .fogui, .foguischeme или другой формат компоновки. | Декларативные исходники и генераторы принадлежат встраиваемым проектам или отдельно версионируемому сопутствующему инструменту. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos), [Docs/en/tutorials/first-project.md](https://github.com/cvet/fonline/blob/master/Docs/en/tutorials/first-project.md) |
| <a id="entry-gui-runtime-integration-client-hook-bridge-408d0ce04a"></a><code>gui-runtime.integration.client-hook-bridge</code> | Мост клиентских hooks | Вызывайте EngineCallback_Start, EngineCallback_Loop, EngineCallback_Draw и при необходимости EngineCallback_DrawCursor из соответствующих событий встраиваемого клиента. | Среда реализует эти hooks, но сама не подписывается на проектные события render/lifecycle. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-integration-render-scope-1f4e127e33"></a><code>gui-runtime.integration.render-scope</code> | Область скриптовой отрисовки | Рисуйте GUI только в клиентской области OnRenderIface, где разрешена скриптовая отрисовка; размещайте cursor pass на выбранном проектом верхнем слое. | Нативный ClientEngine разрешает скриптовую отрисовку вокруг OnRenderIface и обрабатывает видео до завершения сцены. | [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp) |
| <a id="entry-gui-runtime-integration-font-image-dependencies-b5f29233a2"></a><code>gui-runtime.integration.font-image-dependencies</code> | Зависимости шрифтов и изображений | Привяжите каждый FontType, используемый узлами Text, и доставьте каждый спрайт, на который ссылается Panel или Button, до показа затронутых экранов. | Среда GUI делегирует компоновку текста и загрузку/отрисовку спрайтов переиспользуемым системам шрифтов и изображений. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-integration-project-enum-extension-187b9ea6aa"></a><code>gui-runtime.integration.project-enum-extension</code> | Расширение enum проектом | Расширяйте GuiScreen и CursorType через проектные метаданные, не переопределяя sentinel-значения Engine. | Engine предоставляет переиспользуемые базовые значения, а каждая игра владеет каталогом экранов и курсоров. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-integration-touch-boundary-9cf9415fbe"></a><code>gui-runtime.integration.touch-boundary</code> | Граница touch-адаптации | Не предполагайте, что Input.fos направляет нативные touch-события в mouse callbacks GUI; предоставьте и проверьте адаптацию проекта/платформы, когда требуется touch-взаимодействие. | ClientEngine создаёт отдельные touch-события, а переиспользуемый модуль Input подписан только на события мыши, клавиш и потери ввода. | [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp), [Source/Scripting/AngelScript/CoreScripts/Input.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Input.fos) |

## Правила проверки

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-gui-runtime-validation-generated-contract-2038572ded"></a><code>gui-runtime.validation.generated-contract</code> | Сгенерированный runtime-контракт | Перегенерируйте и проверяйте gui-runtime.json и его справочные страницы при изменении Gui.fos, Input.fos, клиентской диспетчеризации input/render или этого интерфейсного манифеста. | Модель выявляет drift классов/API/callback/аннотаций/таймингов/исходников до незаметного устаревания текста. | [BuildTools/GuiRuntimeInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/GuiRuntimeInterface.json) |
| <a id="entry-gui-runtime-validation-client-compilation-42203652a1"></a><code>gui-runtime.validation.client-compilation</code> | Компиляция встраиваемого клиента | Скомпилируйте и запеките встраиваемый клиент, который предоставляет проектные enum-значения, GuiScreens::InitializeScreens, фабрики экранов и вызовы lifecycle/render hooks. | Проверки только исходников Engine не доказывают обязательные межмодульные символы AngelScript или сгенерированный код экранов. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-validation-visible-layout-input-750a995a35"></a><code>gui-runtime.validation.visible-layout-input</code> | Видимые компоновка и ввод | Проверьте репрезентативные экраны при каждом поддерживаемом разрешении и профиле ввода, включая anchors, docks, crop, 9-slice, фокус, close-on-miss, modal ordering, repeat, drag и потерю ввода. | Компиляция и headless-выполнение не доказывают геометрию, порядок отрисовки, alpha hit testing, clipping или взаимодействие пользователя. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-validation-language-font-fac44a9019"></a><code>gui-runtime.validation.language-font</code> | Приёмка языка и шрифта | Переключите каждый поставляемый язык в видимом клиенте, выполните Callback_OnLanguageChanged и проверьте перенос, clipping, покрытие глифов, цвета фокуса, ввод текста и ленты сообщений. | Среда GUI обновляет text callbacks и геометрию, но не может проверять проектные переводы или покрытие привязанных шрифтов. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-validation-fixture-gap-26f30e9d86"></a><code>gui-runtime.validation.fixture-gap</code> | Пробел целевого fixture | Сохраняйте экспериментальный статус, пока в Engine не появится самодостаточный клиентский fixture, который создаёт экраны, управляет вводом, рисует пиксели, меняет разрешение/язык и доказывает teardown. | Текущие нативные тесты напрямую не выполняют среду GUI из CoreScripts. | [Source/Tests/README.md](https://github.com/cvet/fonline/blob/master/Source/Tests/README.md) |

## Команды проверки

```powershell
python BuildTools\docs_gui_runtime.py --check
python -m unittest BuildTools.tests.test_docs_gui_runtime
python BuildTools\docs_validate.py
```

Эти проверки доказывают согласованность исходников, модели и справочника. Они не заменяют компиляцию подключаемого клиента и видимую проверку компоновки, порядка отрисовки, ввода, языка, шрифтов и teardown.
