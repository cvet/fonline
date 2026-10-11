---
title: Устаревшие параметры имён изображений
document_id: generated-image-format-options
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-image-format-options","locale":"ru","source_path":"Docs/en/reference/image-format/options.md","source_sha256":"6ba60cc17b285a93d17feb0076fb74b3d294dc0d24d3305447a3304ab7261c77"} -->

# Устаревшие параметры имён изображений

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/ImageFormatInterface.json`, затем выполните `python BuildTools/docs_image_format.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [FOFRM](fofrm.md) | [Параметры](options.md) | [Запекание](baking.md) | [Runtime](runtime.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/image-format.json) | [Руководство](../../how-to/content/image-format.md)

| Стабильный ID | Синтаксис | Поведение | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-image-format-option-art-6327308876"></a><code>image-format.option.art</code> | <code>Name$[0-3][T][H][V][Fframe&#124;Ffrom-to].art</code> | Цифры выбирают последнюю запрошенную доступную палитру; T выводит alpha из максимума RGB при сохранении прозрачности нулевого индекса; H/V отражают; F выбирает включительный возрастающий или убывающий ограниченный кадр/диапазон. Регистр букв не важен, неизвестные символы игнорируются. | Параметры разбираются из ссылки на источник без изменения физического имени исходного файла. | [Source/Tools/ImageBaker-Art.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ImageBaker-Art.cpp) |
| <a id="entry-image-format-option-spr-25a63adbd0"></a><code>image-format.option.spr</code> | <code>Name$[part,r,g,b]...Sequence.spr</code> | Ноль или больше записей в скобках задают ограниченные RGB offsets для part 0 other, 1 skin, 2 hair или 3 armor; part вне диапазона применяет RGB ко всем частям, а текст после последней скобки выбирает последовательность без учёта регистра (пустое значение выбирает первую). | Importer компонует palette layers и выбирает анимацию до записи runtime RGBA-кадров. | [Source/Tools/ImageBaker-Spr.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ImageBaker-Spr.cpp) |
| <a id="entry-image-format-option-bam-816a86bae2"></a><code>image-format.option.bam</code> | <code>Name$cycle[-frame].bam</code> | Целое перед - выбирает cycle, а необязательное неотрицательное целое после - — один frame; cycle/frame вне диапазона заменяются нулём, а отсутствие selector кадра импортирует весь cycle. | Источник может публиковать много cycles, тогда как один resource path должен давать детерминированный выход. | [Source/Tools/ImageBaker-Bam.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ImageBaker-Bam.cpp) |
| <a id="entry-image-format-option-art-rotation-45c3b4069f"></a><code>image-format.option.art-rotation</code> | <code>Name$[0-3]r=&lt;digit 0..7&gt;[T][H][V][Fframe&#124;Ffrom-to].art</code> | Загрузчик ART разбирает r= или R= с ровно одной цифрой 0..7 как ротацию по часовой стрелке от севера и создаёт одно направление. Выбор палитры предшествует r=; параметры отражения, alpha и кадров могут идти следом. Неверные значения и последующие символы вне этих параметров отклоняются при запекании. Источник с одной ротацией повторно использует свой кадр с привязкой для выбранной ротации. Без знака равенства r/R игнорируется, а следующая цифра 0..3 по-прежнему выбирает палитру. | Параметры имени источника одинаково работают через Alias и обычные ссылки frm; FOFRM и общая диспетчеризация не разбирают параметры ART и не обходят зарегистрированный загрузчик. | [Source/Tools/ImageBaker-Art.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ImageBaker-Art.cpp) |
