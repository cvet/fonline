---
title: Ресурсы и лимиты моделей
document_id: generated-model-format-assets
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-model-format-assets","locale":"ru","source_path":"Docs/en/reference/model-format/assets.md","source_sha256":"1b5936a053ab7497fc3af65d2a67af80bb859782228f2ae9e42a6af42450a392"} -->

# Ресурсы и лимиты моделей

> Сгенерированный справочник. Не редактируйте напрямую. Обновите `BuildTools/ModelFormatInterface.json`, затем запустите `python BuildTools/docs_model_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Токены](tokens.md) | [Композиция](composition.md) | [Ресурсы](assets.md) | [Анимация](animation.md) | [Валидация](validation.md) | [Каноническая JSON-модель](../../../generated/model-format.json) | [Руководство](../../how-to/content/model-format.md)

`ModelMeshBaker` запекает исходники мешей до того, как `ModelInfoBaker` проверяет и сериализует конкретные `.fo3d`.

## Входные ресурсы

| Стабильный ID | Ресурс | Расширения | Назначение | Требования | Источник |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-model-format-asset-fbx-c5e5ad7161"></a><code>model-format.asset.fbx</code> | FBX mesh | <code>.fbx</code> | Импортирует drawable hierarchy, имена material textures и skin data для mesh payload, а ModelSourceLoader извлекает source skeleton и animation clips, преобразуемые ModelInfoBaker в обязательный runtime rig. | faces должны триангулироваться, а imported face count совпадать с generated triangle count<br>skin clusters должны помещаться в FO_MODEL_MAX_BONES<br>mesh skin references должны разрешаться в physical mesh hierarchy<br>source skeletons и clips должны пройти проверки конечности значений, hierarchy, counts, keys и compatibility | [Source/Tools/ModelMeshBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelMeshBaker.cpp), [Source/Tools/ModelSourceLoader.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelSourceLoader.cpp) |
| <a id="entry-model-format-asset-obj-6c06dcc043"></a><code>model-format.asset.obj</code> | OBJ mesh | <code>.obj</code> | Импортирует static hierarchy и drawable mesh через тот же путь ufbx; отсутствующие vertex attributes получают детерминированные defaults. | для concrete model файл должен содержать хотя бы один drawable mesh<br>OBJ подходит для static attachments и cut volumes, но не для authored skeletal animation stacks | [Source/Tools/ModelMeshBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelMeshBaker.cpp), [Source/Tests/Test_ModelBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ModelBaker.cpp) |
| <a id="entry-model-format-asset-description-c03e9e142e"></a><code>model-format.asset.description</code> | Описание модели | <code>.fo3d</code> | Собирает primary baked mesh с layer-selected root modifiers, child models, particles, materials, effects, cuts и animation mappings. | каждое concrete description должно разрешить directive Model<br>файлы с basename TEMPLATE_ являются include-only и не создаются как модели | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <a id="entry-model-format-asset-texture-e163265b77"></a><code>model-format.asset.texture</code> | Текстура модели | <code>.png</code>, <code>.tga</code>, <code>.dds</code> | Default diffuse textures и явные значения Texture, отличные от Parent, разрешаются относительно owning baked mesh. | каждая imported default diffuse texture должна существовать в baked resources<br>явный target mesh для texture должен быть drawable | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelHierarchy.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelHierarchy.cpp) |
| <a id="entry-model-format-asset-effect-2c95b05b0c"></a><code>model-format.asset.effect</code> | Эффект модели | <code>.fofx</code> | Явное значение Effect, отличное от Parent, является baked-resource path для EffectUsage::Model. | effect resource должен существовать<br>явный target mesh эффекта должен быть drawable | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelHierarchy.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelHierarchy.cpp) |
| <a id="entry-model-format-asset-particle-40ad9e9d94"></a><code>model-format.asset.particle</code> | Присоединённая частица | <code>.spk</code>, <code>.efk</code> | Layer-selected AttachParticles создаёт baked SPARK или Effekseer resource на model bone, пока значение слоя активно. | particle resource должен существовать среди baked resources<br>ссылайтесь на baked .spk или .efk, а не на authoring source .spark или .efkproj<br>задайте непустую Link bone: runtime-создание частицы требует её | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |

## Compile-time лимиты

| Стабильный ID | Опция проекта | Runtime-константа | По умолчанию | Смысл |
| --- | --- | --- | --- | --- |
| <a id="entry-model-format-limit-layers-8359438ceb"></a><code>model-format.limit.layers</code> | <code>FO_MODEL_LAYERS_COUNT</code> | <code>MODEL_LAYERS_COUNT</code> | <code>30</code> | Число slots в каждом model-layer array и исключительная верхняя граница индексов Layer, DisableLayer, AnimLayerValue и layer indices у Cut. |
| <a id="entry-model-format-limit-textures-c365b16c3e"></a><code>model-format.limit.textures</code> | <code>FO_MODEL_MAX_TEXTURES</code> | <code>MODEL_MAX_TEXTURES</code> | <code>8</code> | Число texture slots каждого mesh и исключительная верхняя граница indices Texture. |
| <a id="entry-model-format-limit-bones-b52a056500"></a><code>model-format.limit.bones</code> | <code>FO_MODEL_MAX_BONES</code> | <code>MODEL_MAX_BONES</code> | <code>54</code> | Максимальное число skin-bone matrices в одном combined draw batch. |
| <a id="entry-model-format-limit-bones-per-vertex-e51c03b244"></a><code>model-format.limit.bones-per-vertex</code> | <code>FO_MODEL_BONES_PER_VERTEX</code> | <code>MODEL_BONES_PER_VERTEX</code> | <code>4</code> | Максимальное число imported skin influences, сохраняемых на vertex до нормализации весов. |

Defaults взяты из generated project-interface contract. Проект может их переопределить, но client binaries, baked resources, model layer properties, shaders и packaged content должны использовать одинаковые значения.
