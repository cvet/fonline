---
title: Синтаксис файла эффекта
document_id: generated-effect-format-syntax
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-effect-format-syntax","locale":"ru","source_path":"Docs/en/reference/effect-format/syntax.md","source_sha256":"582440c1f21e9ede868b7525b09cab58a9b584a454cee45cb6631d80c1b8d164"} -->

# Синтаксис файла эффекта

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/EffectFormatInterface.json`, затем выполните `python BuildTools/docs_effect_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Состояние рендеринга](render-state.md) | [Ресурсы](resources.md) | [Запекание](baking.md) | [Runtime](runtime.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/effect-format.json) | [Руководство](../../how-to/content/effect-format.md)

Минимальный однопроходный эффект содержит обязательную секцию конфигурации и одну пару вершинного/фрагментного шейдера:

```ini
[Effect]

[VertexShader]
layout(set = 0, binding = 0, std140) uniform ProjBuf { mat4 ProjMatrix; };
layout(location = 0) in vec3 InPosition;
void main(void) { gl_Position = ProjMatrix * vec4(InPosition, 1.0); }

[FragmentShader]
layout(location = 0) out vec4 FragColor;
void main(void) { FragColor = vec4(1.0); }
```

| Стабильный ID | Секция | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-effect-format-section-effect-2883b72871"></a><code>effect-format.section.effect</code> | [Effect] | Каждый исходный файл .fofx должен содержать секцию Effect. В ней хранятся число проходов, версия шейдера, выбор теневого прохода и состояние рендеринга по умолчанию или для отдельных проходов. | И baker, и runtime-загрузчик отклоняют исходный файл без этой секции. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp), [Source/Frontend/Rendering.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering.cpp) |
| <a id="entry-effect-format-section-shader-common-3bb6a58153"></a><code>effect-format.section.shader-common</code> | [ShaderCommon] | ShaderCommon содержит необязательный исходный текст шейдера, добавляемый после пролога Engine перед обеими стадиями каждого прохода. | Это единственный блок повторного использования на уровне формата; parser .fofx не поддерживает директиву include. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-section-vertex-0fcaf320c3"></a><code>effect-format.section.vertex</code> | [VertexShader] and [VertexShader PassN] | Для каждого объявленного прохода нужен непустой текст вершинного шейдера. Baker сначала читает VertexShader PassN, а при пустой секции прохода использует общую секцию VertexShader. | Общая вершинная стадия может обслуживать все проходы, а выбранные проходы могут её переопределять. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-section-fragment-1425c78deb"></a><code>effect-format.section.fragment</code> | [FragmentShader] and [FragmentShader PassN] | Для каждого объявленного прохода нужен непустой текст фрагментного шейдера. Baker сначала читает FragmentShader PassN, а при пустой секции прохода использует общую секцию FragmentShader. | Многопроходные эффекты часто совместно используют обработку геометрии и различаются только фрагментной стадией. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
