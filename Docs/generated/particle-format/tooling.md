---
title: Particle Tooling Contract
document_id: generated-particle-format-tooling
locale: en
generated: true
---

# Particle Tooling Contract

> Generated reference. Do not edit this page directly. Update `BuildTools/ParticleFormatInterface.json`, then run `python BuildTools/docs_particle_format.py --write`.

[Reference index](index.md) | [Source rules](xml.md) | [Formats and backends](objects.md) | [Rendering](renderer.md) | [Tooling](tooling.md) | [Runtime](runtime.md) | [Integration](integration.md) | [Validation](validation.md) | [Canonical JSON model](../particle-format.json)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-particle-format-tooling-mapper-preview-da5cbce04b"></a><code>particle-format.tooling.mapper-preview</code> | Backend-neutral Mapper preview | Preview baked .spk and .efk resources through the Mapper particle window using explicit placement, seed, scale, offset, and optional prewarm. | The preview uses the same ParticleSystem facade and runtime extensions as the game. | [Source/Tools/ParticleEditor.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ParticleEditor.cpp) |
| <a id="entry-particle-format-tooling-spark-editor-2df021aa40"></a><code>particle-format.tooling.spark-editor</code> | Mapper SPARK editor | Edit raw .spark assets in Mapper, save through the SPARK XML saver, then rebake and recreate the .spk preview. | The editor reads source assets while previewing the baked runtime form. | [Source/Tools/SparkParticleEditor.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/SparkParticleEditor.cpp) |
| <a id="entry-particle-format-tooling-effekseer-editor-16877c79de"></a><code>particle-format.tooling.effekseer-editor</code> | Standalone Effekseer Editor | Build and stage the Windows Effekseer authoring tool through buildtools.py build-auxiliary; do not ship it as a game runtime dependency. | The editor has an isolated auxiliary build and staged payload. | [BuildTools/buildtools.py](https://github.com/cvet/fonline/blob/master/BuildTools/buildtools.py)<br>[BuildTools/EffekseerEditor/build.ps1](https://github.com/cvet/fonline/blob/master/BuildTools/EffekseerEditor/build.ps1) |
| <a id="entry-particle-format-tooling-incremental-effekseer-52c6748643"></a><code>particle-format.tooling.incremental-effekseer</code> | Effekseer dependency cache | Let ParticleBaker track compiler-reported dependency snapshots; force a full rebake after changing compiler behavior. | The cache invalidates an effect when its project or dependencies change. | [Source/Tools/ParticleBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ParticleBaker.cpp) |
| <a id="entry-particle-format-tooling-normalized-round-trip-c09c9d8c09"></a><code>particle-format.tooling.normalized-round-trip</code> | Serializer-normalized round trip | Expect Save to rewrite object ordering, optional/default fields, nesting, references, and formatting according to the SPARK graph serializer; review the semantic diff. | The editor saves an object graph, not a token-preserving XML syntax tree. | [ThirdParty/spark/spark/src/Extensions/IOConverters/SPK_IO_XMLSaver.cpp](https://github.com/cvet/fonline/blob/master/ThirdParty/spark/spark/src/Extensions/IOConverters/SPK_IO_XMLSaver.cpp) |
