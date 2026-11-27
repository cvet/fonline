---
title: Форматы ресурсов шрифтов
document_id: generated-font-format-formats
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-font-format-formats","locale":"ru","source_path":"Docs/en/reference/font-format/formats.md","source_sha256":"6791f8eb35ceaded94c0b5923b0105db474e9e13024a2d7d48ef3280b9e4803c"} -->

# Форматы ресурсов шрифтов

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/FontFormatInterface.json`, затем выполните `python BuildTools/docs_font_format.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [Привязка](binding.md) | [Авторинг](authoring.md) | [Компоновка](layout.md) | [Отрисовка](rendering.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/font-format.json) | [Руководство](../../how-to/content/font-format.md)

| Стабильный ID | Суффикс | Роль | Контракт | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-format-truetype-3db4652bb8"></a><code>font-format.format.truetype</code> | <code>.ttf</code> | Единственный runtime-формат текста, растеризуется FreeType | Поставляйте каждое лицо текста как TrueType-шрифт и привязывайте его перегрузкой Game.BindFont со списком лиц; клиент рисует текст только так. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp), [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-format-truetype-colored-bda5c5725e"></a><code>font-format.format.truetype-colored</code> | <code>.ttf</code> | TrueType-лицо с цветными слоями COLR версии 0 поверх палитры CPAL | Создавайте многоцветное лицо как слои COLR v0 с палитрой CPAL; клиент рисует его собственными цветами и умножает их на цвет текста. | [ThirdParty/freetype/include/freetype/config/ftoption.h](https://github.com/cvet/fonline/blob/master/ThirdParty/freetype/include/freetype/config/ftoption.h), [Source/Client/FontRasterizer.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontRasterizer.cpp) |
| <a id="entry-font-format-format-bitmap-source-352a427410"></a><code>font-format.format.bitmap-source</code> | <code>.fofnt, .fnt</code> | Только вход конвертера; runtime их не загружает | Храните созданный bitmap-шрифт как текстовый дескриптор .fofnt с изображением или бинарный BMFont v3 .fnt вне всех пакетов ресурсов и преобразуйте его конвертером в TrueType-лицо до поставки. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py), [Source/Client/FontRasterizer.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontRasterizer.cpp) |

## Поставляемые лица

| Пакет | Лица |
| --- | --- |
| <code>Embedded</code> | <code>FOnline-Regular.ttf</code> |
| <code>Core</code> | <code>FOnline-Big.ttf</code>, <code>FOnline-BigNumbers.ttf</code>, <code>FOnline-Fat.ttf</code>, <code>FOnline-Numbers.ttf</code>, <code>FOnline-Old.ttf</code>, <code>FOnline-SandNumbers.ttf</code>, <code>FOnline-Special.ttf</code>, <code>FOnline-Thin.ttf</code> |

## Поставляемые цветные лица

| Таблицы | Лица |
| --- | --- |
| <code>COLR</code>, <code>CPAL</code> | <code>FOnline-BigNumbers.ttf</code>, <code>FOnline-Numbers.ttf</code>, <code>FOnline-SandNumbers.ttf</code> |
