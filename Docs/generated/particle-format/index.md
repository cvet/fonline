---
title: Generated Particle Format Reference
document_id: generated-particle-format-index
locale: en
generated: true
---

# Generated Particle Format Reference

> Generated reference. Do not edit this page directly. Update `BuildTools/ParticleFormatInterface.json`, then run `python BuildTools/docs_particle_format.py --write`.

[Reference index](index.md) | [Source rules](xml.md) | [Formats and backends](objects.md) | [Rendering](renderer.md) | [Tooling](tooling.md) | [Runtime](runtime.md) | [Integration](integration.md) | [Validation](validation.md) | [Canonical JSON model](../particle-format.json)

This reference describes the optional SPARK and Effekseer authoring, baking, runtime, Mapper, integration, and validation contract.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>experimental</code> |
| Support policy | SPARK and Effekseer are independent optional backends. Embedding projects must pin an Engine revision and explicitly enable, test, and support the formats they ship. |
| Source manifest | <code>BuildTools/ParticleFormatInterface.json</code> |
| Contract digest | <code>307018de62c9576171e256c3e0b7400ff310fa777f979e96f169a2f75b44a80a</code> |
| Authored extensions | <code>spark</code>, <code>efkproj</code> |
| Runtime extensions | <code>spk</code>, <code>efk</code> |
| Runtime side | <code>client</code> |

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Source rules](xml.md) | 12 | Authored XML and dependency boundaries. |
| [Formats and backends](objects.md) | 4 | Optional backends and source-to-runtime forms. |
| [Rendering](renderer.md) | 16 | Backend rendering routes and fields. |
| [Tooling](tooling.md) | 5 | Mapper and standalone authoring workflows. |
| [Runtime](runtime.md) | 10 | Composition, routing, seed, scale, and prewarm. |
| [Integration](integration.md) | 6 | Sprite, model, script, and project boundaries. |
| [Validation](validation.md) | 7 | Documentation, native, bake, and visible gates. |

## Boundary

Included:

- SPARK .spark authoring and baked .spk delivery
- Effekseer .efkproj authoring and baked .efk delivery
- backend-neutral particle runtime and sprite integration
- Mapper preview and SPARK authoring tools
- resource, model, and client-script integration
- source-backed validation and production gates

Excluded:

- embedding-project particle catalogs, filenames, visual policy, and budgets
- project-selected effects, textures, models, and acceptance scenes
- unsupported Effekseer renderer families and advanced capabilities
- upstream editor behavior that is not present in the bundled tools
- generated .spk and .efk files as authored source
