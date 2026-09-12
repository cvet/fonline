---
layout: default
title: Граница интеграции GUI
locale: ru
document_id: gui-runtime-guide
permalink: /Docs/ru/how-to/runtime/gui.html
---

# Граница интеграции GUI

<!-- docs-translation: {"document_id":"gui-runtime-guide","locale":"ru","source_path":"Docs/en/how-to/runtime/gui.md","source_sha256":"3653ca03d660b95b397878634d01c7e483835c84113363b06266eb7554794996"} -->

> Документация движка. Переиспользуемый Engine владеет примитивами rendering, input, window, resources и script exports. Высокоуровневая GUI object model, screen stack, layout library, generated screens и авторские GUI-форматы принадлежат подключающему проекту.

## Текущее состояние

Прежняя Engine library AngelScript `CoreScripts/Gui.fos` и `Input.fos` удалена, когда high-level script libraries во время интеграции Managed C# перешли во владение подключающего проекта. Старый generated GUI runtime reference выведен из эксплуатации вместе с ней, поскольку его source contract больше не существует в Engine.

Это маршрут владения, а не compatibility specification удалённой library. Не восстанавливайте удалённые `.fos` только ради сохранения генератора документации Engine и не выводите Managed C# GUI API из старых AngelScript types.

## Что предоставляет Engine

- application/window services и native input events;
- renderer, sprites, atlases, render targets, fonts, text, effects, video и drawing exports;
- независимые от backend metadata и generated script bindings нативных API;
- client lifecycle hooks, на которые может подписаться GUI проекта;
- backend AngelScript и Managed C#, через которые проект реализует GUI library.

Нативный presentation contract описан во [Frontend и рендеринге](../../explanation/rendering/), backend-neutral bindings — в [Скриптовом runtime](../../explanation/scripting-runtime/), а authoring/runtime C# — в [Скриптах Managed C#](../scripting/managed-csharp.md).

## Что должен документировать проект

Подключающий проект владеет и документирует GUI types, регистрацию screens, правила stack/modal, layout и scaling, drawing order, mouse/keyboard/touch behavior, focus, drag/drop, item views, declarative formats или generators, localization behavior и screen-specific acceptance tests. Эти контракты могут различаться между проектами и языками скриптов.

## Граница проверки

Для изменения нативного render/input export Engine перегенерируйте script API и запустите узкие native tests. Затем скомпилируйте и запеките каждый затронутый scripting backend в представительном подключающем проекте и выполните видимые interaction checks на каждой заявленной платформе. Headless success не является visual acceptance.

Для изменения проектной GUI library храните source-derived reference, tests, generated screens и visual evidence в этом проекте. Engine-only validation не доказывает поведение проектных screens.

## Триггеры сопровождения

Обновляйте этот маршрут, когда Engine добавляет или удаляет нативные presentation/input primitives, меняет предоставляющие их scripting backends или вводит новый переиспользуемый Engine-owned GUI layer. Будущий GUI layer Engine требует реального source owner, tests, generated/reference contract при необходимости, документации EN/RU, evidence проектной интеграции и visual acceptance до заявления о реализации.

## Проверенные пути исходников

- `Source/Frontend/`
- `Source/Client/`
- `Source/Scripting/*ScriptMethods.cpp`
- `Source/Scripting/AngelScript/`
- `Source/Scripting/Managed/`
- `Docs/en/explanation/rendering/`
- `Docs/en/explanation/scripting-runtime/index.md`
