---
title: Компоновка и отрисовка GUI
document_id: generated-gui-runtime-layout-rendering
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-gui-runtime-layout-rendering","locale":"ru","source_path":"Docs/en/reference/gui-runtime/layout-rendering.md","source_sha256":"711d90f396f59630f3375817ce20799cb879d32e078b5dc7bf120f9002963a8d"} -->

# Компоновка и отрисовка GUI

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/GuiRuntimeInterface.json`, затем выполните `python BuildTools/docs_gui_runtime.py --write`.

[Индекс](index.md) | [Типы](types.md) | [API экранов](screen-api.md) | [Жизненный цикл](lifecycle.md) | [Компоновка](layout-rendering.md) | [Ввод](input.md) | [Интеграция](integration-validation.md) | [Канонический JSON](../../../generated/gui-runtime.json) | [Руководство](../../how-to/runtime/gui.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-gui-runtime-layout-parent-coordinate-space-1561c8707a"></a><code>gui-runtime.layout.parent-coordinate-space</code> | Система координат родителя | Интерпретируйте авторские позиции и размеры относительно родителя; корневые объекты используют Settings.View.ScreenWidth и ScreenHeight как текущий размер родителя. | Один алгоритм компоновки обслуживает вложенные виджеты и экраны уровня viewport. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-layout-dock-precedence-890c16c1d4"></a><code>gui-runtime.layout.dock-precedence</code> | Приоритет Dock | Когда Dock не равен None, используйте выбранное правило Left, Right, Top, Bottom или Fill и игнорируйте позиционирование Anchor. | Dock и Anchor являются альтернативными режимами компоновки, а не накапливаемыми ограничениями. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-layout-anchor-center-dba70a3727"></a><code>gui-runtime.layout.anchor-center</code> | Anchor и центрирование | Привязывайте выбранные оси к краям родителя; ось без подходящего edge-bit центрируйте на половину разницы между текущим и авторским базовым размером. | Непривязанные элементы остаются по центру при изменении размера родителя или viewport. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-layout-crop-draw-hit-6d2ea215aa"></a><code>gui-runtime.layout.crop-draw-hit</code> | Crop влияет на отрисовку и hit testing | Когда crop панели включён, ограничивайте отрисовку потомков scissor-областью и отклоняйте hit tests за пределами прямоугольника панели. | Визуальный и интерактивный clipping должны совпадать. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-layout-frame-nine-slice-0f219f68d0"></a><code>gui-runtime.layout.frame-nine-slice</code> | Рамка 9-slice | Используйте SetFrameImage и неотрицательные cap insets в пикселях исходника; среда ограничивает исходные и целевые границы и растягивает края и центр. | Рамка может менять размер без искажения углов или недопустимых диапазонов UV. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-layout-scroll-animation-a0a9fcb831"></a><code>gui-runtime.layout.scroll-animation</code> | Прокрутка Panel | Явно включайте вертикальную и/или горизонтальную автопрокрутку; изменения цели анимируются в течение PanelScrollAnimationDurationMs, а отрисовка обновляет активные tween. | Значения прокрутки являются stateful-смещениями компоновки, а не только преобразованием renderer. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-layout-grid-prototype-9d52c1e6bf"></a><code>gui-runtime.layout.grid-prototype</code> | Клонирование прототипа Grid | Укажите Grid именованный прототип объекта, выберите положительное число столбцов и позвольте ResizeGrid клонировать, индексировать, инициализировать и размещать ячейки. | Прототип остаётся скрытым, а клоны становятся живым повторяемым содержимым. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
