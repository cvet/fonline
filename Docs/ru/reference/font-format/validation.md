---
title: Контракт проверки шрифтов
document_id: generated-font-format-validation
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-font-format-validation","locale":"ru","source_path":"Docs/en/reference/font-format/validation.md","source_sha256":"71641654096278dba6c5e727c9a7d876f1f9eb8582fcc2e507360fa3730000d4"} -->

# Контракт проверки шрифтов

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/FontFormatInterface.json`, затем выполните `python BuildTools/docs_font_format.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [Привязка](binding.md) | [Авторинг](authoring.md) | [Компоновка](layout.md) | [Отрисовка](rendering.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/font-format.json) | [Руководство](../../how-to/content/font-format.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-validation-face-loading-f937748005"></a><code>font-format.validation.face-loading</code> | Сбои загрузки лица | Ожидайте, что привязка бросит исключение и оставит прежний шрифт, если файл лица отсутствует, FreeType не может открыть его как лицо (.fofnt, .fnt или неподдерживаемый контейнер), у него нет карты символов Unicode или его нельзя установить в заданный размер в пикселях. | Менеджер шрифтов сообщает об отсутствующем ресурсе, а растеризатор сообщает о каждом отказе FreeType собственным фиксированным сообщением до любого изменения состояния. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp), [Source/Client/FontRasterizer.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontRasterizer.cpp) |
| <a id="entry-font-format-validation-default-font-presence-2b4d5ccbe0"></a><code>font-format.validation.default-font-presence</code> | Шрифт по умолчанию в каждом runtime-стенде | Предоставляйте Fonts/FOnline-Regular.ttf в любом наборе ресурсов, запускающем клиент, Mapper или updater, включая тестовые стенды, и не допускайте его исключения при удалении неиспользуемых ресурсов. | Запуск привязывает шрифт по умолчанию по пути и бросает исключение, если его нет, а сборщик использования ресурсов добавляет шрифт по умолчанию как используемый ресурс. | [Source/Tools/ResourceUsage.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ResourceUsage.cpp), [Source/Tests/Test_BakerHelpers.h](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_BakerHelpers.h), [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-validation-converter-current-d7a6288548"></a><code>font-format.validation.converter-current</code> | Преобразованные лица актуальны | После изменения bitmap-источника или конвертера запускайте python BuildTools/bitmap_font_to_ttf.py --manifest Resources/FontSources/Fonts.json --check и python BuildTools/tests/test_bitmap_font_to_ttf.py; каждое закоммиченное лицо должно совпадать со свежим преобразованием. | Лицо — генерируемые данные, поэтому ручная правка или устаревшее преобразование иначе были бы поставлены незамеченными. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py), [BuildTools/tests/test_bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/tests/test_bitmap_font_to_ttf.py) |
| <a id="entry-font-format-validation-generated-contract-40d35172b7"></a><code>font-format.validation.generated-contract</code> | Расхождение сгенерированного контракта | Пересоздавайте и проверяйте модель font-format при любом изменении привязки лиц, шрифта по умолчанию, значений raw-copy по умолчанию, обработки цветных лиц, констант конвертера, enum шрифтов, кеша или поставляемых лиц. | Проверяемая модель заставляет CI отклонить незаметное расхождение исходников и документации. | [BuildTools/docs_font_format.py](https://github.com/cvet/fonline/blob/master/BuildTools/docs_font_format.py) |
| <a id="entry-font-format-validation-engine-tests-0ba5ebbb02"></a><code>font-format.validation.engine-tests</code> | Регрессионные gate Engine | После изменений FontManager, FontRasterizer или шрифта по умолчанию запускайте целевой документационный тест и полную генерируемую цель модульных тестов Engine. | Структурные проверки фиксируют контракты, выведенные из исходного кода, а нативные тесты покрывают растеризацию лиц, цветные лица, листы глифов и пути создания клиента. | [Source/Tests/Test_FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_FontManager.cpp), [Source/Tests/Test_FontRasterizer.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_FontRasterizer.cpp) |
| <a id="entry-font-format-validation-embedding-project-3dcc84a5cf"></a><code>font-format.validation.embedding-project</code> | Запекание и видимая проверка проекта | Запеките лица, запустите тесты измерения для каждого привязанного размера в пикселях и визуально проверьте обычный, обведённый, цветной, перенесённый, выровненный, локализованный текст и случаи отсутствующих глифов. | Успешный raw-copy не доказывает покрытие глифов, хинтинг, типографику, поведение бэкенда рендеринга или вписывание в GUI. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp), [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |

## Команды проверки

```powershell
python BuildTools\docs_font_format.py --check
python -m unittest BuildTools.tests.test_docs_font_format
python BuildTools\bitmap_font_to_ttf.py --manifest Resources\FontSources\Fonts.json --check
python -m unittest BuildTools.tests.test_bitmap_font_to_ttf
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

Подключаемый проект также должен запечь лица, запустить регрессию измерения текста и проверить в видимом клиенте репрезентативный обычный, обведённый, цветной, перенесённый и локализованный текст.
