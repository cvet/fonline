---
title: Сгенерированный справочник GUI Runtime
document_id: generated-gui-runtime-index
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-gui-runtime-index","locale":"ru","source_path":"Docs/en/reference/gui-runtime/index.md","source_sha256":"7b7141352c24b7e4e2ef01400c6cee8ee285c2dc91b5d545a3570f0e4ba726b8"} -->

# Сгенерированный справочник GUI Runtime

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/GuiRuntimeInterface.json`, затем выполните `python BuildTools/docs_gui_runtime.py --write`.

[Индекс](index.md) | [Типы](types.md) | [API экранов](screen-api.md) | [Жизненный цикл](lifecycle.md) | [Компоновка](layout-rendering.md) | [Ввод](input.md) | [Интеграция](integration-validation.md) | [Канонический JSON](../../../generated/gui-runtime.json) | [Руководство](../../how-to/runtime/gui.md)

Этот справочник описывает принадлежащий Engine GUI runtime на AngelScript. Это не спецификация декларативного формата GUI: исходные форматы экранов, генераторы, каталоги, стили и реализации проектных hooks остаются ответственностью подключаемого проекта.

## Состояние контракта

| Поле | Значение |
| --- | --- |
| Стабильность | <code>experimental</code> |
| Политика поддержки | Переиспользуемый GUI runtime поддерживает клиентские интеграции, но остаётся экспериментальным; подключаемые проекты владеют каталогами экранов, генераторами, визуальным оформлением и привязкой hooks. |
| Исходный манифест | [BuildTools/GuiRuntimeInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/GuiRuntimeInterface.json) |
| Дайджест контракта | <code>ecdc56e4e0f650c6af06801b6d63159b9d39ff0d67759ba17751a9884323561a</code> |
| Сторона runtime | <code>client</code> |
| Типы runtime | 12 |
| Документированные члены типов | 159 |
| Сигнатуры callback-функций | 39 |
| Перегрузки верхнеуровневого API | 31 |
| Декларативные GUI-форматы Engine | 0 |
| Целевые нативные тесты runtime | 0 |

| Справочник | Записи | Назначение |
| --- | --- | --- |
| [Types](types.md) | 12 | Иерархия объектов, документированные члены и callbacks. |
| [Screen API](screen-api.md) | 31 | Функции регистрации, стека, фокуса, поиска и drag/drop. |
| [Lifecycle](lifecycle.md) | 6 | Создание, show/hide, курсор и поведение обновления. |
| [Layout](layout-rendering.md) | 7 | Координаты, docking, anchors, crop, рамки, прокрутка и grids. |
| [Input](input.md) | 7 | Подписки, порядок hit, фокус, repeat, drag и потеря ввода. |
| [Integration](integration-validation.md) | 11 | Ответственность подключаемого проекта и gate проверки. |

## Граница ответственности

Включено:

- типы объектов Gui.fos, документированные члены, callbacks, enum-ы, setting, события и верхнеуровневый API экранов
- регистрация, создание и стек экранов, фокус, modal-режим, курсор, drag-and-drop и поведение жизненного цикла
- реализованное переиспользуемым runtime поведение anchor, dock, crop, 9-slice, текста, grid, item-view и прокрутки
- состояние мыши и клавиатуры Input.fos, диспетчеризация, repeat и поведение при потере ввода
- проектные hooks, необходимые для управления runtime из событий жизненного цикла и отрисовки клиента

Исключено:

- .fogui, .foguischeme, XML, JSON и любые другие декларативные форматы авторинга GUI
- проектные генераторы GUI, визуальные редакторы, сгенерированные каталоги экранов, layout-ы, стили и библиотеки виджетов
- идентификаторы экранов проекта за пределами sentinel-значения GuiScreen::None, принадлежащего Engine
- проектные привязки шрифтов, каталоги изображений, политика локализации, действия ввода, gameplay-представление и приёмка доступности
- инструменты разработчика Dear ImGui, окна серверного host, нативное оформление приложения и touch-to-GUI адаптация
