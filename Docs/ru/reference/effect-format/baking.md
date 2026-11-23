---
title: Запекание эффектов и backend-ы
document_id: generated-effect-format-baking
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-effect-format-baking","locale":"ru","source_path":"Docs/en/reference/effect-format/baking.md","source_sha256":"a0ecd5312141d42e5150393719bc2c70206cc2fb385b75f518ace06ece23d7a7"} -->

# Запекание эффектов и backend-ы

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/EffectFormatInterface.json`, затем выполните `python BuildTools/docs_effect_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Состояние рендеринга](render-state.md) | [Ресурсы](resources.md) | [Запекание](baking.md) | [Runtime](runtime.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/effect-format.json) | [Руководство](../../how-to/content/effect-format.md)

Каждый проход создаёт один файл метаданных и семь вариантов для каждой стадии шейдера. Исходный `.fofx` копируется в запечённые ресурсы, потому что runtime продолжает читать из него состояние `[Effect]`.

Варианты стадии: <code>spv</code>, <code>spv_sdl</code>, <code>glsl</code>, <code>glsl_es</code>, <code>hlsl</code>, <code>msl_mac</code>, <code>msl_ios</code>

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-effect-format-baking-compiler-prelude-8ab13cba3b"></a><code>effect-format.baking.compiler-prelude</code> | Сгенерированный пролог шейдера | Перед ShaderCommon и текстом стадии baker добавляет '#version &lt;Version&gt; es', 'precision highp float', MAX_SCRIPT_VALUES, а в 3D-сборках также MAX_BONES и MAX_TEXTURES. | Авторские шейдеры используют единый контракт compile-time формы и не должны дублировать эти директивы. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-baking-compiler-target-4540599eb1"></a><code>effect-format.baking.compiler-target</code> | Целевое окружение glslang | Обе стадии компилируются как GLSL для клиента Vulkan 1.0 и цели SPIR-V 1.0, затем один раз на проход связываются и строят reflection. | Все варианты backend-ов получаются из одной проверенной связанной программы. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-baking-link-interface-6b2005fdf3"></a><code>effect-format.baking.link-interface</code> | Проверка интерфейса стадий | Вершинная и фрагментная стадии каждого прохода должны разбираться, связываться и предоставлять корректную reflection-модель. | Несоответствия location/type и синтаксические ошибки останавливают запекание ресурсов до runtime. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-baking-metadata-3a793719e1"></a><code>effect-format.baking.metadata</code> | Метаданные прохода | Каждый проход создаёт info-артефакт с нативными привязками [EffectInfo] и плотными слотами и числами ресурсов по стадиям в [EffectInfoSdl]. | Runtime-backend-ы используют результаты reflection без запуска glslang. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-baking-native-flavors-0930a361af"></a><code>effect-format.baking.native-flavors</code> | Нативные и кросс-компилированные варианты | Каждая стадия создаёт нативный Vulkan SPIR-V, desktop GLSL 330, GLSL ES 300, HLSL Shader Model 4.0 и исходный Metal для macOS и iOS. | Один исходный эффект поддерживает активные backend-ы рендерера без проектных ответвлений шейдера. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-baking-sdl-remap-216674e85a"></a><code>effect-format.baking.sdl-remap</code> | Переназначение дескрипторов SDL_GPU | Baker копирует нативный SPIR-V, переписывает descriptor set-ы и binding-и под соглашение SDL_GPU для отдельных стадий и сохраняет результат как spv_sdl. Исходный Metal компилируется из переназначенного модуля. | Нативные привязки Vulkan остаются неизменными, а SDL_GPU получает плотные локальные слоты стадий. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-baking-source-copy-cb116df5fd"></a><code>effect-format.baking.source-copy</code> | Запечённая копия исходника | После артефактов всех проходов исходный .fofx записывается в набор запечённых ресурсов. | RenderEffect повторно разбирает состояние [Effect] во время выполнения; исходный файл является частью набора runtime-артефактов. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp), [Source/Frontend/Rendering.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering.cpp) |
| <a id="entry-effect-format-baking-incremental-be6a0c7939"></a><code>effect-format.baking.incremental</code> | Набор инкрементальных выходов | Проверка запекания отслеживает копию исходника, метаданные проходов и каждый артефакт стадии/варианта для всех объявленных проходов. | Отсутствующий или устаревший вариант должен делать эффект кандидатом на повторное запекание. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
