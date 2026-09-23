---
title: Контракт компоновки текста
document_id: generated-font-format-layout
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-font-format-layout","locale":"ru","source_path":"Docs/en/reference/font-format/layout.md","source_sha256":"4f6997579253178047c34c2637dc97e1ae4699aeb06f4ce8d046f30a7a0a341c"} -->

# Контракт компоновки текста

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/FontFormatInterface.json`, затем выполните `python BuildTools/docs_font_format.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [FOFNT](fofnt.md) | [BMFont](bmfont.md) | [Привязка](binding.md) | [Компоновка](layout.md) | [Отрисовка](rendering.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/font-format.json) | [Руководство](../../how-to/content/font-format.md)

## Значения FontFlag

| Имя | Значение | Поведение исходного кода |
| --- | --- | --- |
| <code>None</code> | <code>0x0000</code> | Без дополнительного поведения |
| <code>NoWrap</code> | <code>0x0001</code> | При переполнении ширины прямоугольника обрезать оставшийся текст вместо переноса на следующую строку |
| <code>TruncateLine</code> | <code>0x0002</code> | При переполнении ширины пропустить оставшиеся глифы до следующего '\n' вместо переноса |
| <code>CenterX</code> | <code>0x0004</code> | Горизонтально центрировать каждую строку в прямоугольнике |
| <code>CenterY</code> | <code>0x0008</code> | Вертикально центрировать блок текста в прямоугольнике |
| <code>AlignRight</code> | <code>0x0010</code> | Выравнивать каждую строку по правому краю прямоугольника |
| <code>AlignBottom</code> | <code>0x0020</code> | Выравнивать блок текста по нижнему краю; также меняет TextFormat::SkipLines с пропуска сверху на пропуск снизу |
| <code>KeepTail</code> | <code>0x0040</code> | Когда блок выше прямоугольника, показывать его хвост, пропуская начальные переполняющие строки |
| <code>NoColorize</code> | <code>0x0080</code> | Удалять встроенные теги цвета (@color:0x...@ / @color@), но отображать текст с неизменным базовым цветом |
| <code>Justify</code> | <code>0x0100</code> | Выравнивать строку по ширине, распределяя дополнительные пробелы между словами |
| <code>Bordered</code> | <code>0x0200</code> | Отрисовывать глифы из обведённого варианта текстуры шрифта вместо обычного |

## Правила компоновки

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-layout-text-format-80e94b41d3"></a><code>font-format.layout.text-format</code> | Значение TextFormat | Передавайте слот Font, битовую маску FontFlag и неотрицательное число SkipLines в TextFormat. | Экспортируемый тип имеет фиксированную 12-байтную раскладку, используемую измерением и отрисовкой. | [Source/Client/FontManager.h](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h) |
| <a id="entry-font-format-layout-wrap-overflow-56e4aad419"></a><code>font-format.layout.wrap-overflow</code> | Переполнение ширины и перенос | При конечной ширине обычная компоновка переносит по последнему пробелу или табуляции и вставляет разрыв перед слишком длинным токеном, когда точки разрыва нет. | Компоновка изменяет свою кэшированную копию текста, задавая границы строк, но не меняет исходную строку вызывающего кода. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-layout-no-wrap-and-truncate-a98195c11b"></a><code>font-format.layout.no-wrap-and-truncate</code> | NoWrap и TruncateLine | NoWrap завершает отрисовку при первом переполнении; TruncateLine удаляет переполняющие глифы до следующего авторского перевода строки. | Эти флаги намеренно выбирают разное поведение потери текста и не являются синонимами. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-layout-horizontal-alignment-db87d0e726"></a><code>font-format.layout.horizontal-alignment</code> | Горизонтальное выравнивание | CenterX и AlignRight независимо размещают каждую строку в прямоугольнике; не сочетайте противоречащие флаги выравнивания. | Начальная и каждая следующая после перевода строки координата X пересчитывается по измеренной ширине этой строки. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-layout-vertical-alignment-e6cce90eb6"></a><code>font-format.layout.vertical-alignment</code> | Вертикальное выравнивание | CenterY центрирует видимый блок, а AlignBottom размещает его у низа прямоугольника с учётом LineHeight и YAdvance. | Вертикальное положение зависит от числа помещающихся строк, а не от длины исходной строки в байтах. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-layout-skip-lines-and-tail-1a695aec23"></a><code>font-format.layout.skip-lines-and-tail</code> | SkipLines и KeepTail | SkipLines обычно удаляет начальные строки, а с AlignBottom конечные; KeepTail отбрасывает начальное переполнение, сохраняя новые видимые строки. | Механизмы обслуживают постраничный вывод и хвост журнала, но используют разные счётчики и условия переполнения. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-layout-justification-280433804e"></a><code>font-format.layout.justification</code> | Выравнивание по ширине | Justify распределяет остаток конечной ширины по доступным для разрыва пробелам перенесённых и не пропущенных строк. | Табуляции фиксированы на четырёх SpaceWidth и не участвуют в выравнивании по ширине. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-layout-utf8-and-missing-glyphs-b65e93ffe1"></a><code>font-format.layout.utf8-and-missing-glyphs</code> | UTF-8 и отсутствующие глифы | Добавляйте каждую требуемую кодовую точку Unicode; некорректный UTF-8 и отсутствующие глифы не занимают ширину и не отображают запасной символ. | Компоновка сопоставляет некорректные последовательности с кодовой точкой ноль, а неизвестные точки с нулевым продвижением; отрисовка пропускает отсутствующие записи. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-layout-measurement-2c9e4eb598"></a><code>font-format.layout.measurement</code> | Измерение соответствует компоновке | Используйте Game.GetTextInfo и функции строк с тем же прямоугольником и TextFormat, что и при отрисовке, но не выводите обрезку NoWrap только для отрисовки из измерений. | Измерение и отрисовка используют GetOrFormat, пропуски, метрики строк и масштаб привязки; обрезка NoWrap намеренно включена только в режиме Draw. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp), [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
