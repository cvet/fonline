---
title: Директивы анимации моделей
document_id: generated-model-format-animation
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-model-format-animation","locale":"ru","source_path":"Docs/en/reference/model-format/animation.md","source_sha256":"30660479504552b03bf7cafd742bce42f0e1d892cdc6063b2942ffeba70be450"} -->

# Директивы анимации моделей

> Сгенерированный справочник. Не редактируйте напрямую. Обновите `BuildTools/ModelFormatInterface.json`, затем запустите `python BuildTools/docs_model_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Токены](tokens.md) | [Композиция](composition.md) | [Ресурсы](assets.md) | [Анимация](animation.md) | [Валидация](validation.md) | [Каноническая JSON-модель](../../../generated/model-format.json) | [Руководство](../../how-to/content/model-format.md)

Здесь перечислены directives `.fo3d`, участвующие в выборе анимации и композиции позы движения. Effective durations, materialization aliases и script lookup описаны в [Model Animation](../../how-to/content/model-animation.md).

| Directive | Синтаксис | Контракт авторинга | Runtime-эффект |
| --- | --- | --- | --- |
| <code>Anim</code> | <code>Anim &lt;state&gt; &lt;action&gt; &lt;ModelFile&#124;animation-mesh&gt; &lt;clip&#124;~clip&#124;Base&gt;</code> | Сопоставляет state/action pair с source clip. ModelFile выбирает primary source, ~ обращает playback, Base выбирает первый clip до runtime-rig conversion. | Регистрируется первая declaration пары; model-specific lookup и substitutions описаны в ModelAnimation.md. |
| <code>AnimSpeed</code> | <code>AnimSpeed &lt;state&gt; &lt;action&gt; &lt;positive-float&gt;</code> | Задаёт authored playback speed mapped pair. | Speed умножает runtime playback и делит common effective duration. |
| <code>AllowAnimationGeometry</code> | <code>AllowAnimationGeometry &lt;external-animation-file&gt;</code> | Временно разрешает drawable geometry в точном внешнем source Anim, пока source исправляется. Path разрешается от final concrete description. | Только validation, без serialization. Duplicate, unselected, duplicate-resolved и stale exceptions останавливают bake; удалите строку с исправлением export. |
| <code>AnimLayerValue</code> | <code>AnimLayerValue &lt;state&gt; &lt;action&gt; &lt;layer&gt; &lt;value&gt;</code> | Переопределяет layer value при запросе точной authored state/action pair. | Override применяется до redundant-call detection и model composition. |
| <code>FastTransitionBone</code> | <code>FastTransitionBone &lt;bone&gt;</code> | Помечает base-model bone для immediate transition reset, когда новый child использует её как Link. | Следующий body-animation track сбрасывает transition state attachment bone. |
| <code>StateAnimEqual</code> | <code>StateAnimEqual &lt;from-state&gt; &lt;to-state&gt;</code> | Определяет одношаговый state-animation alias. | Alias применяется один раз до exact lookup и имеет приоритет над exact source-key entry. |
| <code>ActionAnimEqual</code> | <code>ActionAnimEqual &lt;from-action&gt; &lt;to-action&gt;</code> | Определяет одношаговый action-animation alias. | Alias применяется один раз до exact lookup и имеет приоритет над exact source-key entry. |
| <code>DisableAnimationInterpolation</code> | <code>DisableAnimationInterpolation</code> | Отключает keyframe interpolation model animation controller. | Registered controller выполняет sampling без interpolation. |
| <code>DisableBackwardAnim</code> | <code>DisableBackwardAnim</code> | Отключает выбор WalkBack и RunBack для movement-pose animation. | Movement всегда использует forward walk/run, а SetMoveDir выравнивает look direction. |
| <code>RotationBone</code> | <code>RotationBone &lt;bone&gt;</code> | Выбирает проверенную torso/body rotation bone и включает movement overlay controller. | Look/move directions могут различаться; body и настроенные head bones получают directional rotation, пока movement/turn animations идут на overlay controller. |

## Отделение от 2D root motion

Эти directives управляют 3D skeletal clips и model composition. Offsets `NextX` / `NextY` и выбор кадров по направлению движения принадлежат [Sprite Root Motion](../../how-to/content/sprite-root-motion.md).
