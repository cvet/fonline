---
title: Контракт инструментов частиц
document_id: generated-particle-format-tooling
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-particle-format-tooling","locale":"ru","source_path":"Docs/en/reference/particle-format/tooling.md","source_sha256":"2444259db3476ef80bdc30512b3e240cd7a323e4bb4fa8ebd5e72a8c8647dc70"} -->

# Контракт инструментов частиц

> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/ParticleFormatInterface.json`, затем выполните `python BuildTools/docs_particle_format.py --write`.

[Индекс справочника](index.md) | [Правила исходников](xml.md) | [Форматы и backend-ы](objects.md) | [Отрисовка](renderer.md) | [Инструменты](tooling.md) | [Runtime](runtime.md) | [Интеграция](integration.md) | [Проверка](validation.md) | [Каноническая JSON-модель](../../../generated/particle-format.json) | [Руководство](../../how-to/content/particle-format.md) | [Инструменты авторинга](../../how-to/tools/particle-authoring.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-particle-format-tooling-mapper-preview-da5cbce04b"></a><code>particle-format.tooling.mapper-preview</code> | Независимый от подсистемы предпросмотр в Mapper | Просматривайте запечённые ресурсы .spk и .efk через окно частиц Mapper, явно задавая размещение, seed, масштаб, смещение и необязательный prewarm. | Предпросмотр использует тот же фасад ParticleSystem и те же runtime-расширения, что и игра. | [Source/Tools/ParticleEditor.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ParticleEditor.cpp) |
| <a id="entry-particle-format-tooling-spark-editor-2df021aa40"></a><code>particle-format.tooling.spark-editor</code> | Редактор SPARK в Mapper | Редактируйте исходные ресурсы .spark в Mapper, сохраняйте их через XML-сериализатор SPARK, затем повторно запекайте и пересоздавайте предпросмотр .spk. | Редактор читает исходные ресурсы, а предпросмотр показывает запечённую runtime-форму. | [Source/Tools/SparkParticleEditor.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/SparkParticleEditor.cpp) |
| <a id="entry-particle-format-tooling-effekseer-editor-16877c79de"></a><code>particle-format.tooling.effekseer-editor</code> | Отдельный Effekseer Editor | Собирайте и размещайте Windows-инструмент авторинга Effekseer командой buildtools.py build-auxiliary; не поставляйте его как runtime-зависимость игры. | У редактора изолированная вспомогательная сборка и отдельный размещаемый payload. | [BuildTools/buildtools.py](https://github.com/cvet/fonline/blob/master/BuildTools/buildtools.py)<br>[BuildTools/EffekseerEditor/build.ps1](https://github.com/cvet/fonline/blob/master/BuildTools/EffekseerEditor/build.ps1) |
| <a id="entry-particle-format-tooling-incremental-effekseer-52c6748643"></a><code>particle-format.tooling.incremental-effekseer</code> | Кеш зависимостей Effekseer | Позвольте ParticleBaker отслеживать сообщаемые компилятором снимки зависимостей; после изменения поведения компилятора принудительно выполните полный повторный бейкинг. | Кеш инвалидирует эффект при изменении его проекта или зависимостей. | [Source/Tools/ParticleBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ParticleBaker.cpp) |
| <a id="entry-particle-format-tooling-normalized-round-trip-c09c9d8c09"></a><code>particle-format.tooling.normalized-round-trip</code> | Нормализованный сериализатором round trip | Учитывайте, что Save переписывает порядок объектов, необязательные поля и поля по умолчанию, вложенность, ссылки и форматирование по правилам сериализатора графа SPARK; проверяйте семантический diff. | Редактор сохраняет граф объектов, а не сохраняющее токены синтаксическое дерево XML. | [ThirdParty/spark/spark/src/Extensions/IOConverters/SPK_IO_XMLSaver.cpp](https://github.com/cvet/fonline/blob/master/ThirdParty/spark/spark/src/Extensions/IOConverters/SPK_IO_XMLSaver.cpp) |
