---
title: Проверка формата эффектов
document_id: generated-effect-format-validation
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-effect-format-validation","locale":"ru","source_path":"Docs/en/reference/effect-format/validation.md","source_sha256":"1ea6815f6a2b0fc74a66b106a205d16396b947a25dd3bf1316c58b7254160163"} -->

# Проверка формата эффектов

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/EffectFormatInterface.json`, затем выполните `python BuildTools/docs_effect_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Состояние рендеринга](render-state.md) | [Ресурсы](resources.md) | [Запекание](baking.md) | [Runtime](runtime.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/effect-format.json) | [Руководство](../../how-to/content/effect-format.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-effect-format-validation-required-sections-7eb31d9eec"></a><code>effect-format.validation.required-sections</code> | Обязательное содержимое исходника | Отклоняйте отсутствующую секцию Effect или проход без пригодного вершинного либо фрагментного текста. | Частичный исходник не может создать полную связанную программу или runtime-состояние. | [Source/Tests/Test_EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_EffectBaker.cpp) |
| <a id="entry-effect-format-validation-compiler-diagnostics-cc1a443e82"></a><code>effect-format.validation.compiler-diagnostics</code> | Диагностика компилятора | Ошибки разбора, связывания или reflection шейдера останавливают запекание эффекта и нормализуют диагностику компилятора до одной строки лога. | Логи сборки остаются машиночитаемыми и сохраняют сообщение компилятора шейдера. | [Source/Tests/Test_EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_EffectBaker.cpp), [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-validation-buffer-layout-0086cb6c8e"></a><code>effect-format.validation.buffer-layout</code> | Layout uniform buffer-а | Отклоняйте известные uniform buffer-ы, отражённый размер которых отличается от соответствующей структуры RenderEffect, и любой неизвестный uniform block. | Все backend-ы загружают фиксированные нативные структуры без адаптации layout под отдельный шейдер. | [Source/Tests/Test_EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_EffectBaker.cpp), [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-validation-bindings-b0fad0d441"></a><code>effect-format.validation.bindings</code> | Привязки дескрипторов | Отклоняйте отсутствующие явные binding-и, повторяющиеся binding-и одной стадии, мёртвые объявления descriptor-ов и превышение лимита стадии SDL_GPU. | Плотное переназначение слотов SDL должно быть полным и детерминированным. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-validation-output-flavors-5e48dca94f"></a><code>effect-format.validation.output-flavors</code> | Полнота запечённых вариантов | Проверяйте исходник, метаданные, нативный SPIR-V, SDL SPIR-V, GLSL, GLSL ES, HLSL и оба выхода MSL для каждой стадии/прохода. | Ресурс может пройти один backend и остаться неполным для другого пакета. | [Source/Tests/Test_EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_EffectBaker.cpp) |
| <a id="entry-effect-format-validation-metadata-bindings-6620bcd7a8"></a><code>effect-format.validation.metadata-bindings</code> | Метаданные reflection | Проверяйте значения нативных binding-ов [EffectInfo] и числа/слоты SDL [EffectInfoSdl] на репрезентативных эффектах. | Backend-ы зависят от идентичности метаданных даже при успешной компиляции самого шейдера. | [Source/Tests/Test_EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_EffectBaker.cpp) |
| <a id="entry-effect-format-validation-cross-backend-3867b45fe8"></a><code>effect-format.validation.cross-backend</code> | Видимая кросс-backend проверка | Проверяйте проектные эффекты на каждом профиле рендерера/backend-а, поставляемом проектом, включая сцены, чувствительные к depth/blend, и минимальный поддерживаемый профиль шейдера. | Кросс-компиляция не доказывает поведение драйвера, поддержку feature level, визуальный результат или производительность. | [Source/Frontend/Rendering-OpenGL.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering-OpenGL.cpp), [Source/Frontend/Rendering-Direct3D.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering-Direct3D.cpp), [Source/Frontend/Rendering-Vulkan.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering-Vulkan.cpp), [Source/Frontend/Rendering-SDLGpu.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering-SDLGpu.cpp) |
| <a id="entry-effect-format-validation-project-ownership-bdef2e2a3d"></a><code>effect-format.validation.project-ownership</code> | Проверка проектного контракта | Подключаемый проект должен проверять каждый путь эффекта, назначение EffectType/subtype, реестр слотов ScriptValue, writer buffer-а, переопределение resource pack и видимый fallback, которыми он владеет. | Engine может проверить механику формата, но не способен вывести проектную семантику шейдеров или художественный замысел. | [Source/Client/EffectManager.h](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h), [Source/Frontend/Rendering.h](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering.h) |

## Команды проверки

```powershell
python BuildTools\docs_effect_format.py --check
python -m unittest BuildTools.tests.test_docs_effect_format
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

Завершите проверку в подключаемом проекте запеканием его ресурсов и видимыми проверками на каждом поддерживаемом проектом профиле рендерера/backend-а.
