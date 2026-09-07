---
title: Жизненный цикл экранов GUI
document_id: generated-gui-runtime-lifecycle
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-gui-runtime-lifecycle","locale":"ru","source_path":"Docs/en/reference/gui-runtime/lifecycle.md","source_sha256":"25ffe9d4ca4c3ab37f543da94675292244d56e8a251277e397393da1d3d531f3"} -->

# Жизненный цикл экранов GUI

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/GuiRuntimeInterface.json`, затем выполните `python BuildTools/docs_gui_runtime.py --write`.

[Индекс](index.md) | [Типы](types.md) | [API экранов](screen-api.md) | [Жизненный цикл](lifecycle.md) | [Компоновка](layout-rendering.md) | [Ввод](input.md) | [Интеграция](integration-validation.md) | [Канонический JSON](../../../generated/gui-runtime.json) | [Руководство](../../how-to/runtime/gui.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-gui-runtime-lifecycle-external-initializer-5e3809e11a"></a><code>gui-runtime.lifecycle.external-initializer</code> | Инициализатор экранов проекта | Предоставьте пространство имён GuiScreens с void InitializeScreens(); EngineCallback_Start вызывает его и не может скомпилироваться или запуститься без этого проектного контракта. | Переиспользуемая среда владеет механикой экранов, но намеренно не владеет каталогом экранов игры. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos), [Docs/en/tutorials/first-project.md](https://github.com/cvet/fonline/blob/master/Docs/en/tutorials/first-project.md) |
| <a id="entry-gui-runtime-lifecycle-registration-precache-3c852bd828"></a><code>gui-runtime.lifecycle.registration-precache</code> | Регистрация и прекэширование | Зарегистрируйте каждый id GuiScreen, отличный от None, с фабрикой; регистрация заменяет прежнюю запись и немедленно создаёт один экран для инициализации и прекэширования ресурсов. | Регистрация выполняет клиентскую стартовую работу, а не пассивно загружает метаданные. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-lifecycle-single-vs-multiinstance-eb5db96192"></a><code>gui-runtime.lifecycle.single-vs-multiinstance</code> | Владение single- и multiinstance | Переиспользуйте зарегистрированный single-instance экран; создавайте новый экран при каждом показе multiinstance и удаляйте этот экземпляр после скрытия. | Флаг меняет семантику создания, сохранения, поиска и teardown. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-lifecycle-show-hide-order-31b3cee1f9"></a><code>gui-runtime.lifecycle.show-hide-order</code> | Порядок callback-функций show и hide | Рассматривайте Show как настройку active-state, обход OnShow, затем обход OnAppear; Hide как OnDisappear, обход OnHide, inactive-state, вызов события и необязательное удаление multiinstance. | Проектные callback-функции часто различают обновление данных и видимость наверху стека. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-lifecycle-cursor-screen-f296275b71"></a><code>gui-runtime.lifecycle.cursor-screen</code> | Владение экраном курсора | Зарегистрируйте GuiScreen::Cursor как выделенный всегда активный CursorScreen и явно рисуйте его через EngineCallback_DrawCursor. | Курсор исключён из обычного стека и обычного прохода отрисовки экранов. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-lifecycle-explicit-refresh-4f661ee276"></a><code>gui-runtime.lifecycle.explicit-refresh</code> | Явное обновление разрешения и языка | Вызывайте Callback_OnResolutionChanged после изменений viewport и Callback_OnLanguageChanged после изменений языка. | Переиспользуемая среда сама не подписывается на эти переходы жизненного цикла проекта. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
