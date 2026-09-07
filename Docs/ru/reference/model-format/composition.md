---
title: Композиция моделей
document_id: generated-model-format-composition
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-model-format-composition","locale":"ru","source_path":"Docs/en/reference/model-format/composition.md","source_sha256":"63e8053ca7393caf283f030a984ea5e388798c9423a012f81102321b2a208a0b"} -->

# Композиция моделей

> Сгенерированный справочник. Не редактируйте напрямую. Обновите `BuildTools/ModelFormatInterface.json`, затем запустите `python BuildTools/docs_model_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Токены](tokens.md) | [Композиция](composition.md) | [Ресурсы](assets.md) | [Анимация](animation.md) | [Валидация](validation.md) | [Каноническая JSON-модель](../../../generated/model-format.json) | [Руководство](../../how-to/content/model-format.md)

Runtime-композиция начинается с default `Root`, затем активирует links, у которых `Layer` и `Value` совпадают с текущим model-layer array.

## Процесс композиции слоёв

1. Скопировать предоставленный проектом layer array.
2. Применить точные overrides `AnimLayerValue` для запрошенной animation.
3. Применить default `Root`: transforms, materials, effects, disables и cuts.
4. Активировать совпавшие layer `Root`, child attachments и particle attachments.
5. Удалить children и particles, links которых перестали быть активны.
6. Перегенерировать combined meshes при изменении composition, materials, effects или cuts.

## Директивы композиции

| Directive | Контекст | Контракт авторинга | Runtime-эффект | Источник |
| --- | --- | --- | --- | --- |
| <code>Root</code> | <code>description or selected Layer/Value</code> | Без Layer выбирает default root modifier; при active Layer и ненулевом Value создаёт modifier пары. | Может преобразовать model, изменить speed/materials/effects, отключить meshes/layers и применить cuts без child. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <code>Attach</code> | <code>selected Layer/Value</code> | Создаёт layer-selected child-model link. Path относителен к файлу с directive. | С Link child присоединяется к одной parent bone; без него одноимённые child/parent bones образуют shared-skeleton attachment. У direct FBX/OBJ child нет description-level коррекции scale, поэтому static extent должен находиться между Baking.ModelAttachmentMinExtent и Baking.ModelAttachmentMaxExtent. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <code>AttachParticles</code> | <code>selected Layer/Value</code> | Создаёт layer-selected baked-particle link. Resource path хранится буквально, не относительно description. | Client создаёт particle на Link bone и удаляет её, когда активирующий layer value больше не выбран. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <code>Link</code> | <code>current layer link</code> | Задаёт parent bone текущего non-default link; игнорируется на default или dummy link. | Child целиком присоединяется к bone; particle требует bone. Пустой child-model link не использует её в runtime. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <code>Cut</code> | <code>current link</code> | Добавляет baked cut volumes в выбранные composed-mesh layers. Дефис разделяет lists, - пропускает unskin fields, ~ обращает unskin shape. | Combined geometry внутри или снаружи shapes удаляется; optional paired bones управляют unskin. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <code>RotX</code>, <code>RotY</code>, <code>RotZ</code>, <code>MoveX</code>, <code>MoveY</code>, <code>MoveZ</code>, <code>ScaleX</code>, <code>ScaleY</code>, <code>ScaleZ</code>, <code>Speed</code> | <code>current link</code> | Задаёт transform axis или playback-speed multiplier. Rotation задаётся в градусах. | Ненулевые values умножают model transform или speed chain; ноль не даёт вклада. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <code>Scale</code> | <code>current link</code> | Задаёт одинаковые ScaleX, ScaleY, ScaleZ. | Ненулевое value создаёт uniform scale transform. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <code>RotX+</code>, <code>RotY+</code>, <code>RotZ+</code>, <code>MoveX+</code>, <code>MoveY+</code>, <code>MoveZ+</code>, <code>ScaleX+</code>, <code>ScaleY+</code>, <code>ScaleZ+</code>, <code>Speed+</code> | <code>current link</code> | Прибавляет к transform/speed field; при нуле operand становится initial value. | Include может накладывать additive adjustments без base assignment. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <code>Scale+</code> | <code>current link</code> | Применяет additive rule ко всем scale axes. | Даёт uniform additive adjustment для templates и selected links. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <code>RotX*</code>, <code>RotY*</code>, <code>RotZ*</code>, <code>MoveX*</code>, <code>MoveY*</code>, <code>MoveZ*</code>, <code>ScaleX*</code>, <code>ScaleY*</code>, <code>ScaleZ*</code>, <code>Speed*</code> | <code>current link</code> | Умножает transform/speed field; при нуле operand становится initial value. | Include применяет proportional adjustments, сохраняя ноль как runtime identity sentinel. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <code>Scale*</code> | <code>current link</code> | Применяет multiplicative rule ко всем scale axes. | Даёт uniform proportional scale adjustment. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <code>DisableLayer</code> | <code>current link</code> | Добавляет indices в disabled-layer set; каждое значение проверяется по range. | При active link совпавшие layer slots пропускаются в model instance. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <code>DisableMesh</code> | <code>current link</code> | Добавляет drawable mesh names в disabled set; All хранит empty wildcard. | При active link совпавшие meshes исключаются из combined geometry. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <code>Texture</code> | <code>current link and Mesh selector</code> | Переопределяет texture slot выбранного mesh или всех. Не-Parent names относительны к current model mesh; Parent копирует active parent texture в attached-model context. | Override участвует в mesh batching и adjustment texture-atlas coordinates. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <code>Effect</code> | <code>current link and Mesh selector</code> | Переопределяет draw effect выбранного mesh или всех; Parent копирует active parent effect. | Meshes с разными effects не могут делить combined draw batch. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <code>DisableShadow</code> | <code>description</code> | Отключает shadow rendering всех instances description. | Model-level flag объединяется с per-instance shadow toggle. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |

## Выбор attachment

- Используйте `Attach child.fo3d`, если child нужны собственные description, layers, materials, cuts или animations.
- Используйте `Attach child.fbx` или `Attach child.obj` для прямой baked hierarchy.
- Добавьте `Link Bone`, чтобы поместить весь child под одной parent bone.
- Не задавайте `Link` только когда parent и child намеренно имеют одноимённые bones и child должен следовать parent skeleton.
- Для particles используйте `AttachParticles ... Link Bone`; runtime требует target bone.
