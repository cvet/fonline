---
title: Контракт проверки частиц
document_id: generated-particle-format-validation
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-particle-format-validation","locale":"ru","source_path":"Docs/en/reference/particle-format/validation.md","source_sha256":"38371272a8f7af2c8873a076a79e92c94db218c0dec1e18193a5b1d0e63ffe69"} -->

# Контракт проверки частиц

> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/ParticleFormatInterface.json`, затем выполните `python BuildTools/docs_particle_format.py --write`.

[Индекс справочника](index.md) | [Правила исходников](xml.md) | [Форматы и backend-ы](objects.md) | [Отрисовка](renderer.md) | [Инструменты](tooling.md) | [Runtime](runtime.md) | [Интеграция](integration.md) | [Проверка](validation.md) | [Каноническая JSON-модель](../../../generated/particle-format.json) | [Руководство](../../how-to/content/particle-format.md) | [Инструменты авторинга](../../how-to/tools/particle-authoring.md)

| Стабильный ID | Gate | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-particle-format-validation-documentation-7546671f20"></a><code>particle-format.validation.documentation</code> | Контракт документации | После изменения форматов частиц, подсистем, инструментов или интеграций запускайте docs_particle_format.py --check и его сфокусированный модульный тест. | Манифест проверяет каждый якорь исходника и сгенерированную страницу. | [BuildTools/ParticleFormatInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/ParticleFormatInterface.json) |
| <a id="entry-particle-format-validation-baker-tests-f3527f22eb"></a><code>particle-format.validation.baker-tests</code> | Тесты бейкера частиц | После изменения исходных форматов, трансформаций, проверки путей, вывода компилятора или инвалидации зависимостей запускайте модульные тесты ParticleBaker. | Нативный набор проверяет границы бейкинга обеих подсистем. | [Source/Tests/Test_ParticleBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ParticleBaker.cpp) |
| <a id="entry-particle-format-validation-effekseer-runtime-cf357090ac"></a><code>particle-format.validation.effekseer-runtime</code> | Runtime-тесты Effekseer | После изменения геометрии компилятора или runtime, сортировки, пакетирования, текстур, seed или масштаба запускайте сфокусированные runtime-тесты Effekseer. | Набор проверяет детерминированную callback-геометрию и поддерживаемое runtime-поведение. | [Source/Tests/Test_EffekseerParticleRuntime.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_EffekseerParticleRuntime.cpp) |
| <a id="entry-particle-format-validation-project-bake-277a4e8ee2"></a><code>particle-format.validation.project-bake</code> | Бейкинг встраивающего проекта | Повторно запеките затронутый встраивающий проект и отклоняйте авторские .spk/.efk, некорректные исходники, отсутствующие зависимости и устаревшие сгенерированные ресурсы. | Только проектный бейкинг видит полный граф ресурсов и выбранные возможности проекта. | [Source/Tools/ParticleBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ParticleBaker.cpp) |
| <a id="entry-particle-format-validation-visible-scene-6c56437d52"></a><code>particle-format.validation.visible-scene</code> | Видимая runtime-проверка | Проверяйте каждую затронутую подсистему и маршрут интеграции в Mapper и репрезентативной клиентской сцене, включая глубину, отсечение, время жизни, трансформации и производительность. | Успешная компиляция не доказывает визуальную корректность или правильное игровое время. | [Source/Tools/ParticleEditor.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ParticleEditor.cpp) |
| <a id="entry-particle-format-validation-render-routes-20fe741d3d"></a><code>particle-format.validation.render-routes</code> | Оба маршрута рендеринга | Визуально проверяйте частицы в атласе и непосредственно в сцене с реальным порядком отрисовки, углом камеры, масштабом, перекрывающей геометрией глубины и поддерживаемыми backend-рендерерами. | Headless-разбор не доказывает правильность кадрирования, альфа-канала, смешивания, глубины, ориентации или мирового масштаба. | [Source/Client/ParticleSprites.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ParticleSprites.cpp)<br>[Source/Client/SparkExtension.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SparkExtension.cpp) |
| <a id="entry-particle-format-validation-model-route-e7787fd80a"></a><code>particle-format.validation.model-route</code> | Маршрут привязки к модели | Для изменений AttachParticles или Critter.RunParticle визуально проверяйте целевую кость, смещение, время жизни активного слоя, анимацию и профиль FO_ENABLE_3D. | Тесты ParticleSprite не проверяют владение привязкой к кости модели или проекцию. | [Source/Tests/Test_ModelBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ModelBaker.cpp)<br>[Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |

## Команды проверки

```powershell
python BuildTools\docs_particle_format.py --check
python -m unittest BuildTools.tests.test_docs_particle_format
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

Подключаемый проект также должен повторно запечь ресурсы и визуально проверить каждый затронутый backend и маршрут интеграции.
