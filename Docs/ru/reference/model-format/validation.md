---
title: Валидация формата моделей
document_id: generated-model-format-validation
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-model-format-validation","locale":"ru","source_path":"Docs/en/reference/model-format/validation.md","source_sha256":"9508471dc8e15a01db9f0f4ddfcfbcc48e5242562425bd6b4a34a795d4055b2e"} -->

# Валидация формата моделей

> Сгенерированный справочник. Не редактируйте напрямую. Обновите `BuildTools/ModelFormatInterface.json`, затем запустите `python BuildTools/docs_model_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Токены](tokens.md) | [Композиция](composition.md) | [Ресурсы](assets.md) | [Анимация](animation.md) | [Валидация](validation.md) | [Каноническая JSON-модель](../../../generated/model-format.json) | [Руководство](../../how-to/content/model-format.md)

## Правила контракта

| Стабильный ID | Правило | Требование | Причина | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-model-format-rule-lexical-syntax-3a2486e250"></a><code>model-format.rule.lexical-syntax</code> | Разделение по whitespace | Directives и arguments разделяются whitespace; # и ; начинают comments; quoting/escaping для paths с spaces отсутствуют. | Parser удаляет comments и извлекает tokens строки через istringstream. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <a id="entry-model-format-rule-multiple-directives-5e3fe606e6"></a><code>model-format.rule.multiple-directives</code> | Последовательный разбор | Строка может содержать несколько directives; каждая забирает точное число arguments, затем разбор продолжается. | Компактные entries допустимы, но order меняет current link или mesh selector последующих modifiers. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <a id="entry-model-format-rule-selector-order-f53cd0c2a9"></a><code>model-format.rule.selector-order</code> | Порядок selectors | После Layer или Value задайте Root, Attach либо AttachParticles до transforms, materials, disables или cuts. | Layer и Value выбирают dummy link и очищают Mesh; ранние modifiers отбрасываются. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <a id="entry-model-format-rule-template-files-5334d3672b"></a><code>model-format.rule.template-files</code> | Имена templates | Basename include-only files начинается с TEMPLATE_; concrete files не используют этот prefix. | Templates участвуют в timestamps/parsing, но не выдаются как отдельные resources или metadata sections. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <a id="entry-model-format-rule-include-replacements-2c1af91e2a"></a><code>model-format.rule.include-replacements</code> | Scope replacements | Arguments Include являются pairs name/value и заменяют каждое буквальное %name% до tokenization. | Простая текстовая замена не понимает token boundaries; placeholders не должны случайно совпадать. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <a id="entry-model-format-rule-relative-paths-cfa2a858c1"></a><code>model-format.rule.relative-paths</code> | Владение paths | Model, Include, Attach, Cut разрешаются относительно declaring .fo3d; animation files — относительно concrete description, кроме ModelFile; particles/effects глобальны. | Перемещение template или description может изменить contributed asset paths. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <a id="entry-model-format-rule-zero-identity-4b43d7830b"></a><code>model-format.rule.zero-identity</code> | Ноль — transform identity | Нулевые transform и Speed не дают вклада. Варианты +/* инициализируют нулевое поле operand до дальнейших операций. | Runtime SetAnimData пропускает нули, а parser accumulation поддерживает самостоятельные template Scale*/Speed*. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <a id="entry-model-format-rule-layer-zero-fe807a48c3"></a><code>model-format.rule.layer-zero</code> | Нулевой layer value неактивен | Runtime value 0 не выбирает link; authored Root и Attach требуют ненулевого Value. | Composition loop пропускает ноль, baker запрещает links с нулём. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <a id="entry-model-format-rule-parent-materials-3107cf5a6b"></a><code>model-format.rule.parent-materials</code> | Наследование parent material | Используйте Parent или Parent_&lt;mesh&gt; только в attached child; parent mesh должен уже предоставлять нужный texture slot/effect. | Child копирует current material state родителя, не imported default; у root description нет parent context. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <a id="entry-model-format-rule-mesh-before-info-a0aded7029"></a><code>model-format.rule.mesh-before-info</code> | Порядок baker | Запускайте ModelMeshBaker до ModelInfoBaker, чтобы проверить meshes, hierarchy, materials, animations и cuts из baked data. | Built-in orders: 4 для ModelMesh и 6 для ModelInfo. | [Source/Tools/ModelMeshBaker.h](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelMeshBaker.h), [Source/Tools/ModelInfoBaker.h](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.h) |
| <a id="entry-model-format-rule-first-animation-wins-5d78a9978f"></a><code>model-format.rule.first-animation-wins</code> | Побеждает первая animation mapping | Не объявляйте одну state/action Anim pair повторно; только первая mapping проверяется и регистрируется. | Дубликаты пропускаются model-info validation и client registration. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInformation.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInformation.cpp) |
| <a id="entry-model-format-rule-runtime-composition-582952d10d"></a><code>model-format.rule.runtime-composition</code> | Смена layers перестраивает composition | Считайте model-layer arrays состоянием композиции: значение может создать/удалить children и particles, изменить materials/effects, geometry, cuts и combined meshes. | Layer change не является косметическим integer update: он меняет render graph и batching state. | [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <a id="entry-model-format-rule-validation-boundary-7d55b07e20"></a><code>model-format.rule.validation-boundary</code> | Bake и видимая проверка | Требуйте clean resource bake для syntax/asset closure, затем проверяйте scale, pose, composition, attachments, materials, cuts и interaction bounds в visible client scene. | Baking доказывает references и serialized structure; composed visual result доказывает только client renderer. | [Source/Tests/Test_ModelBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ModelBaker.cpp) |

## Удалённые legacy-написания

| Удалённый token | Замена | Текущий контракт |
| --- | --- | --- |
| <code>AnimEqual</code> | <code>StateAnimEqual or ActionAnimEqual</code> | Текущий parser требует явно указать enum domain. |
| <code>CalculateTangentSpace</code> | <code>none</code> | Mesh import настраивает ufbx для generation/normalization отсутствующих normals и tangents; directive .fo3d нет. |
| <code>RenderFrame</code> | <code>none</code> | Текущие model descriptions не создают 2D render frames. |
| <code>RenderFrames</code> | <code>none</code> | Текущие model descriptions не создают sequences 2D render frames. |
| <code>DrawSize</code> | <code>automatic model-sprite layout from baked animation bounds</code> | ModelInfo записывает aggregate/per-animation bounds; client вычисляет offscreen frame активной composition и pose. |
| <code>ViewSize</code> | <code>automatic view/name layout from baked idle-priority bounds</code> | Client проецирует baked model bounds и active child layers вместо authored interaction rectangle. |

Compatibility spelling `Subset` отдельно помечен как deprecated в [токенах](tokens.md): он забирает argument, но не выбирает mesh.

## Команды проверки

```powershell
python BuildTools\docs_model_format.py --check
python -m unittest BuildTools.tests.test_docs_model_format
.\Binaries\Tests-Windows-win64\LF_UnitTests.exe "ModelBaker*"
cmake --build Build\Auto --config RelWithDebInfo --target BakeResources
```

Завершите видимой client scene, которая покрывает все используемые проектом сочетания слоёв, attachments, material overrides, cuts, animations, draw size и interaction bounds.
