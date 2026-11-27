---
title: Контракт авторинга шрифтов
document_id: generated-font-format-authoring
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-font-format-authoring","locale":"ru","source_path":"Docs/en/reference/font-format/authoring.md","source_sha256":"5bf25304cd3aaf75716042e7e8e9028466f8d48ba29d5b959002146045de90ab"} -->

# Контракт авторинга шрифтов

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/FontFormatInterface.json`, затем выполните `python BuildTools/docs_font_format.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [Привязка](binding.md) | [Авторинг](authoring.md) | [Компоновка](layout.md) | [Отрисовка](rendering.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/font-format.json) | [Руководство](../../how-to/content/font-format.md)

## Параметры конвертера

| Поле | Значение |
| --- | --- |
| Исходные форматы | <code>.fofnt</code>, <code>.fnt</code> |
| Единиц шрифта на пиксель bitmap | <code>100</code> |
| Диапазон родного размера | 1..255 px |
| Нейтральный серый глифа | <code>128</code> |
| Поставляемых лиц | <code>9</code> |
| Пакеты Python | <code>fontTools</code>, <code>numpy</code>, <code>PIL</code>, <code>potrace</code> |

## Правила авторинга

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-authoring-converter-d7b0700993"></a><code>font-format.authoring.converter</code> | Конвертер bitmap-шрифтов | Преобразуйте созданный bitmap-шрифт командой python BuildTools/bitmap_font_to_ttf.py SOURCE OUTPUT.ttf --family NAME; ему нужны fontTools, Pillow, numpy и potracer, а runtime и документационные gate не требуют ни одного из них. | Конвертер — единственный читатель источников .fofnt и BMFont v3; он пишет TrueType-лицо, результат которого побайтово одинаков между запусками. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py) |
| <a id="entry-font-format-authoring-native-size-metrics-0e220541ef"></a><code>font-format.authoring.native-size-metrics</code> | Родной размер и метрики | Привязывайте преобразованное лицо в его родном размере в пикселях, равном высоте строки источника плюс межстрочный зазор, чтобы воспроизвести bitmap-компоновку; лицо несёт 100 единиц шрифта на пиксель bitmap, а родной размер вне диапазона 1..255 отклоняется. | Конвертер записывает единицы на em как произведение родного размера на пиксельную единицу и отказывается от размера, который встроенный растровый страйк вместить не может. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py) |
| <a id="entry-font-format-authoring-single-color-0af62a9423"></a><code>font-format.authoring.single-color</code> | Одноцветные шрифты | Ожидайте, что одноцветный источник станет 8-битным растровым страйком EBDT/EBLC в родном размере, который FreeType рисует пиксель в пиксель, плюс контурами, трассированными по покрытию глифов, для всех остальных размеров; пустая программа prep не даёт FreeType автоматически хинтовать лицо. | Страйк сохраняет авторские пиксели в родном размере при любом режиме растеризации, а трассированные контуры позволяют тому же лицу масштабироваться на другие размеры. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py) |
| <a id="entry-font-format-authoring-multi-color-d867c9c701"></a><code>font-format.authoring.multi-color</code> | Многоцветные шрифты | Ожидайте, что источник с цветами помимо нейтрального серого станет слоями COLR v0 поверх палитры CPAL: по одному слою на каждый различный цвет пикселя из целопиксельных прямоугольников, а базовыми глифами будут трассированные силуэты. | Цветные слои сохраняют авторские цвета точными в родном размере и масштабируют пиксельную сетку в других размерах, а рендереры без поддержки цвета всё равно рисуют силуэты. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py) |
| <a id="entry-font-format-authoring-skipped-codepoints-3eee9d8e9a"></a><code>font-format.authoring.skipped-codepoints</code> | Пропускаемые управляющие коды | Не ожидайте глифов для управляющих кодов C0 и C1 в преобразованном лице; задайте в источнике явную букву пробела, потому что клиент отвергает цепочку лиц без неё. | Управляющие коды являются командами компоновки движка, поэтому конвертер их пропускает, а менеджер шрифтов требует букву пробела, чтобы измерять и рисовать текст. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py), [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-authoring-check-688d69791f"></a><code>font-format.authoring.check</code> | Проверка актуальности | Преобразуйте одно лицо командой python BuildTools/bitmap_font_to_ttf.py SOURCE OUTPUT.ttf --family NAME [--style Regular]; добавьте --check, чтобы получить ошибку, когда OUTPUT отсутствует или отличается от свежего преобразования, вместо записи. | --check сравнивает байты результата, что устойчиво, потому что конвертер фиксирует метки времени шрифта, поэтому закоммиченное лицо, переставшее соответствовать источнику, обнаруживается. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py) |
| <a id="entry-font-format-authoring-sources-not-packed-7c85123d50"></a><code>font-format.authoring.sources-not-packed</code> | Источники остаются вне Engine | Храните bitmap-источники вне любых пакетов ресурсов и поставляйте только сгенерированные лица; Engine поставляет свои лица из Resources/Embedded/Fonts для шрифта по умолчанию и из Resources/Core/Fonts для остальных, а исходные bitmap-источники этих лиц хранятся в репозитории fonline-historical-stuff (папка FontSources), что является указателем на происхождение, а не зависимостью сборки. | Runtime не нужны ни исходные дескрипторы, ни их изображения, поэтому дерево Engine несёт только конвертер и его сгенерированные лица. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py) |

## Команды конвертера

```powershell
python BuildTools\bitmap_font_to_ttf.py SOURCE.fofnt OUTPUT.ttf --family "Name"
python BuildTools\bitmap_font_to_ttf.py SOURCE.fofnt OUTPUT.ttf --family "Name" --check
```

Преобразованные лица являются генерируемыми данными: правьте bitmap-источник или конвертер, пересоздавайте лицо и никогда не редактируйте его вручную. Режим `--check` завершается ошибкой, если OUTPUT отличается от свежего преобразования. Исходные bitmap-источники лиц, которые поставляет Engine, хранятся вне него, в репозитории `fonline-historical-stuff` (папка `FontSources`).
