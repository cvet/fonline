---
title: Контракт отрисовки шрифтов
document_id: generated-font-format-rendering
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-font-format-rendering","locale":"ru","source_path":"Docs/en/reference/font-format/rendering.md","source_sha256":"875e346b57fca64b74a409c445afe2a6044a375ccb9c7175536dd94fce88cde9"} -->

# Контракт отрисовки шрифтов

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/FontFormatInterface.json`, затем выполните `python BuildTools/docs_font_format.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [FOFNT](fofnt.md) | [BMFont](bmfont.md) | [Привязка](binding.md) | [Компоновка](layout.md) | [Отрисовка](rendering.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/font-format.json) | [Руководство](../../how-to/content/font-format.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-rendering-one-pixel-sampling-border-c99645ca3f"></a><code>font-format.rendering.one-pixel-sampling-border</code> | Однопиксельная рамка выборки | Оставляйте не менее одного пикселя корректного прозрачного отступа вокруг каждого прямоугольника глифа и по краю изображения. | Координаты текстуры и передаваемые quad расширяют каждый видимый глиф на один пиксель со всех сторон. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-rendering-grayscale-tint-2eb10fd758"></a><code>font-format.rendering.grayscale-tint</code> | Нормализация серого и тонирование | Используйте завершающий * в FOFNT Image или бинарный BMFont, когда растр должен нормализоваться в средне-серый и тонироваться цветом отрисовки. | RGB непрозрачных пикселей становится 128/128/128 при сохранении альфа-канала; прозрачные пиксели очищаются. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-rendering-bordered-copy-891143d814"></a><code>font-format.rendering.bordered-copy</code> | Копия атласа с обводкой | Резервируйте прозрачный отступ для однопиксельного чёрного расширения; FontFlag::Bordered выбирает сгенерированную вторую текстуру. | Загрузчик дублирует изображение и заполняет прозрачных соседей видимых пикселей до расчёта UV обводки. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-rendering-bind-time-resampling-2ec561fa06"></a><code>font-format.rendering.bind-time-resampling</code> | Уменьшение усреднением площади | Ожидайте, что defaultScale ниже единицы один раз перепишет область привязанного атласа и целочисленные метрики, без масштаба шрифта для виджета. | Масштабировщик использует усреднение площади с весом альфа-канала, очищает исходный прямоугольник и сохраняет верхнюю левую позицию. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-rendering-font-effect-fdd82b908c"></a><code>font-format.rendering.font-effect</code> | Общий эффект и эффект слота | Шрифты начинают с общего эффекта Engine; переопределение EffectType::Font для слота заменяет его, а null возвращает общий эффект. | Ключ пакетирования отрисовки включает выбранную текстуру и указатель RenderEffect. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp), [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp) |
| <a id="entry-font-format-rendering-inline-color-c56430c987"></a><code>font-format.rendering.inline-color</code> | Встроенные теги цвета | Используйте @color:BBGGRR@ или @color:AABBGGRR@ с необязательным 0x для добавления цвета и @color@ для возврата предыдущего; NoColorize удаляет корректные теги без их применения. | Форматирование удаляет корректные маркеры до переноса и записывает переходы цвета по смещению байтов результата. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-rendering-layout-cache-26a6758f8a"></a><code>font-format.rendering.layout-cache</code> | Кэш компоновки на три кадра | Не полагайтесь на идентичность или срок жизни кэша; ключ включает текст, шрифт, флаги, пропуски, размер прямоугольника, цвет и режим, а запись истекает после трёх неиспользованных кадров. | Кэш является деталью реализации клиента и очищается при сохранении или удалении шрифтов. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
