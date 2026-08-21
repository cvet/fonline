---
title: Форматы ресурсов шрифтов
document_id: generated-font-format-formats
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-font-format-formats","locale":"ru","source_path":"Docs/en/reference/font-format/formats.md","source_sha256":"a4731dca1978fd6ed18a93ce85ac0d5cf05a093961bfc627091c74c24615849c"} -->

# Форматы ресурсов шрифтов

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/FontFormatInterface.json`, затем выполните `python BuildTools/docs_font_format.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [FOFNT](fofnt.md) | [BMFont](bmfont.md) | [Привязка](binding.md) | [Компоновка](layout.md) | [Отрисовка](rendering.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/font-format.json) | [Руководство](../../how-to/content/font-format.md)

| Стабильный ID | Суффикс | Роль | Контракт | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-format-fofnt-2438a61036"></a><code>font-format.format.fofnt</code> | <code>.fofnt</code> | Runtime-дескриптор | Используйте текстовый дескриптор Engine для явного задания изображения, строки и метрик глифа каждой кодовой точки. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-font-format-format-bmfont-binary-v3-a2f528951c"></a><code>font-format.format.bmfont-binary-v3</code> | <code>.fnt</code> | Runtime-дескриптор | Экспортируйте BMFont binary версии 3 с одной страницей текстуры и однопиксельным отступом с каждой стороны глифа. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-format-bmfc-sidecar-6a547a8922"></a><code>font-format.format.bmfc-sidecar</code> | <code>.bmfc</code> | Вспомогательный файл авторинга, копируемый raw-copy; не runtime-дескриптор | Считайте .bmfc необязательной конфигурацией инструмента BMFont и никогда не передавайте её в Game.BindFont. | [Source/Common/Settings.inc](https://github.com/cvet/fonline/blob/master/Source/Common/Settings.inc), [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |

## Поставляемые дескрипторы

| Суффикс | Файлы |
| --- | --- |
| <code>.fofnt</code> | <code>Big.fofnt</code>, <code>BigNumbers.fofnt</code>, <code>Default.fofnt</code>, <code>Fallout.fofnt</code>, <code>Fat.fofnt</code>, <code>Numbers.fofnt</code>, <code>OldDefault.fofnt</code>, <code>SandNumbers.fofnt</code>, <code>Special.fofnt</code>, <code>Thin.fofnt</code> |
| <code>.fnt</code> | <code>CourierNewSmall.fnt</code>, <code>DefaultExt.fnt</code> |
| <code>.bmfc</code> | <code>CourierNewSmall.bmfc</code>, <code>Settings.bmfc</code> |
