---
title: Проверка форматов изображений
document_id: generated-image-format-validation
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-image-format-validation","locale":"ru","source_path":"Docs/en/reference/image-format/validation.md","source_sha256":"dcfefd0973ca23ccb40fe9dd426a1dc2fc5ce6f1ec87af6b282a2dc27cdee76a"} -->

# Проверка форматов изображений

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/ImageFormatInterface.json`, затем выполните `python BuildTools/docs_image_format.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [FOFRM](fofrm.md) | [Параметры](options.md) | [Запекание](baking.md) | [Runtime](runtime.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/image-format.json) | [Руководство](../../how-to/content/image-format.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-image-format-validation-fofrm-count-068d95fc54"></a><code>image-format.validation.fofrm-count</code> | Положительный count FOFRM | Отклоняйте descriptor, у которого count/Count равен нулю или отрицателен. | До существования runtime frame table descriptor должен разрешить хотя бы одну source reference. | [Source/Tools/ImageBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ImageBaker.cpp) |
| <a id="entry-image-format-validation-fofrm-directions-55fce0e5eb"></a><code>image-format.validation.fofrm-directions</code> | Полные равные направления | Отклоняйте частичные наборы направлений и любое последующее направление, чьё расплющенное число кадров отличается от нулевого. | Каждый direction sheet разделяет один frame-count и timing header. | [Source/Tools/ImageBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ImageBaker.cpp), [Source/Tests/Test_ImageBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ImageBaker.cpp) |
| <a id="entry-image-format-validation-fofrm-nested-shared-106e8e7e2a"></a><code>image-format.validation.fofrm-nested-shared</code> | Без вложенных shared frame records | Отклоняйте ссылку FOFRM, чей дочерний кадр Main уже является shared record. | Реализация flattening копирует concrete child pixels и не перенумеровывает child shared indices. | [Source/Tools/ImageBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ImageBaker.cpp) |
| <a id="entry-image-format-validation-rgba-payload-a6560c29d9"></a><code>image-format.validation.rgba-payload</code> | Точный размер RGBA payload | Отклоняйте каждый concrete frame, чья byte payload не равна в точности width, умноженной на height и четыре. | Client читает фиксированное число байтов RGBA8 без отдельного поля payload length. | [Source/Tools/ImageBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ImageBaker.cpp) |
| <a id="entry-image-format-validation-tga-subset-87f4c5c869"></a><code>image-format.validation.tga-subset</code> | Поддерживаемое подмножество TGA | Не используйте image ID; задавайте bottom-left orientation, TrueColor type 2 или 10 и 24 либо 32 bpp. Indexed/grayscale/другие входы отклоняются или не соответствуют реализованным допущениям orientation. | Loader непосредственно потребляет фиксированный header, затем всегда переворачивает строки и не ветвится по descriptor origin. | [Source/Tools/ImageBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ImageBaker.cpp), [Source/Tests/Test_ImageBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ImageBaker.cpp) |
| <a id="entry-image-format-validation-malformed-inputs-e15cbd5a50"></a><code>image-format.validation.malformed-inputs</code> | Повреждённые входы decoder | Сохраняйте focused failure coverage для повреждённых PNG, TGA, FRM/FRx/RIX/ART/ZAR/TIL/MOS/BAM, SPR и вложенных FOFRM. | Legacy binary parsers должны детерминированно завершаться ошибкой, а не создавать усечённые runtime containers. | [Source/Tests/Test_ImageBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ImageBaker.cpp) |
| <a id="entry-image-format-validation-runtime-container-b6aaaecb88"></a><code>image-format.validation.runtime-container</code> | Guards runtime container | Отклоняйте неверные header/footer magic, нулевые кадры/направления, неподдерживаемое число направлений, недопустимые shared records одного кадра и неверные shared-frame indices. | Повреждённые baked bytes не должны достигать выделения atlas или обновления анимации. | [Source/Common/SpriteResource.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/SpriteResource.cpp), [Source/Client/DefaultSprites.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/DefaultSprites.cpp) |
| <a id="entry-image-format-validation-playback-timing-f10375614b"></a><code>image-format.validation.playback-timing</code> | Ненулевая длительность кадра | Для проигрываемого многокадрового sheet задавайте timing так, чтобы полные AnimTicks, делённые на расплющенное число кадров, были не меньше одной миллисекунды; нулевой fps намеренно отключает playback. | SpriteSheet::Update делит прошедшее время на целые ticks_per_frame, а Play защищает только один кадр или нулевые полные ticks. | [Source/Client/DefaultSprites.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/DefaultSprites.cpp) |
| <a id="entry-image-format-validation-project-visible-c586865990"></a><code>image-format.validation.project-visible</code> | Визуальный gate встраиваемого проекта | После native/documentation checks перезапеките встраиваемый проект и в видимой сцене проверьте изменённые dimensions, alpha edges, mirrors, directions, cadence, offsets, hit masks и нужные client profiles. | Parser/container tests не доказывают framing графики, filtering, gait или выбор project resource pack. | [Source/Tests/Test_ImageBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ImageBaker.cpp), [Source/Tests/Test_TextureAtlas.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_TextureAtlas.cpp) |

## Команды проверки

```powershell
python BuildTools\docs_image_format.py --check
python -m unittest BuildTools.tests.test_docs_image_format
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

Встраиваемый проект также должен перезапечь затронутые ресурсы и проверить каждую изменённую анимацию, направление, alpha edge, hit mask и поддерживаемый client profile.
