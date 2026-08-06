---
title: Справочник полей FOFNT
document_id: generated-font-format-fofnt
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-font-format-fofnt","locale":"ru","source_path":"Docs/en/reference/font-format/fofnt.md","source_sha256":"cfa2b1f12fd409e24dea253789cf3d28ee044d88863d7954ba21ddb46829f5ae"} -->

# Справочник полей FOFNT

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/FontFormatInterface.json`, затем выполните `python BuildTools/docs_font_format.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [FOFNT](fofnt.md) | [BMFont](bmfont.md) | [Привязка](binding.md) | [Компоновка](layout.md) | [Отрисовка](rendering.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/font-format.json) | [Руководство](../../how-to/content/font-format.md)

| Стабильный ID | Ключ | Синтаксис | Поведение | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-fofnt-version-bac978e025"></a><code>font-format.fofnt.version</code> | Version | <code>Version &lt;integer&gt;</code> | Сделайте Version первым разбираемым ключом; для текущих ресурсов задавайте версию 2. Значения больше 2 отклоняются. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-image-a02f11cb08"></a><code>font-format.fofnt.image</code> | Image | <code>Image &lt;relative-resource&gt;[*]</code> | Укажите непустой ресурс изображения относительно дескриптора; добавьте * для нормализации серого и runtime-тонирования. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-line-height-3ec623f1ea"></a><code>font-format.fofnt.line-height</code> | LineHeight | <code>LineHeight &lt;integer-pixels&gt;</code> | Задайте высоту строки глифов в пикселях или оставьте ноль/не задавайте, чтобы вывести максимальную высоту авторских глифов. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-y-advance-71ad32b001"></a><code>font-format.fofnt.y-advance</code> | YAdvance | <code>YAdvance &lt;integer-pixels&gt;</code> | Задайте дополнительный вертикальный интервал между последовательными строками текста. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-letter-7e4bc3a9d5"></a><code>font-format.fofnt.letter</code> | Letter | <code>Letter '&lt;UTF-8-codepoint&gt;'</code> | Начинайте запись каждого глифа с Letter и одной корректной кодовой точки UTF-8 после первого апострофа. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-position-x-01b533225c"></a><code>font-format.fofnt.position-x</code> | PositionX | <code>PositionX &lt;integer-pixels&gt;</code> | Задайте левую координату прямоугольника глифа в изображении. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-position-y-56c8405277"></a><code>font-format.fofnt.position-y</code> | PositionY | <code>PositionY &lt;integer-pixels&gt;</code> | Задайте верхнюю координату прямоугольника глифа в изображении. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-width-916e35ae75"></a><code>font-format.fofnt.width</code> | Width | <code>Width &lt;integer-pixels&gt;</code> | Задайте ширину видимого прямоугольника глифа без однопиксельной рамки выборки. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-height-01ca83487f"></a><code>font-format.fofnt.height</code> | Height | <code>Height &lt;integer-pixels&gt;</code> | Задайте высоту видимого прямоугольника глифа без однопиксельной рамки выборки. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-offset-x-fcaee963d3"></a><code>font-format.fofnt.offset-x</code> | OffsetX | <code>OffsetX &lt;signed-integer-pixels&gt;</code> | Задайте знаковый горизонтальный вынос в координатах Engine; отрисовка размещает quad в позиции курсора X минус OffsetX. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-offset-y-85bff2a932"></a><code>font-format.fofnt.offset-y</code> | OffsetY | <code>OffsetY &lt;signed-integer-pixels&gt;</code> | Задайте знаковый вертикальный вынос в координатах Engine; отрисовка размещает quad в позиции курсора Y минус OffsetY. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-x-advance-51728242c9"></a><code>font-format.fofnt.x-advance</code> | XAdvance | <code>XAdvance &lt;signed-integer-pixels&gt;</code> | Задайте горизонтальное продвижение курсора после глифа; добавьте явный глиф пробела, если пробелы должны занимать ширину. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-end-and-comments-3a52ef092d"></a><code>font-format.fofnt.end-and-comments</code> | End and comments | <code>End &#124; #comment &#124; ;comment</code> | Завершите полезную часть дескриптора через End; # и ; начинают комментарий только внутри ключа, отделённого пробелами. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
