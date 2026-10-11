---
title: Generated Audio Reference
document_id: generated-audio-index
locale: en
generated: true
---

# Generated Audio Reference

> Generated reference. Do not edit directly. Update `BuildTools/AudioInterface.json`, then run `python BuildTools/docs_audio.py --write`.

[Index](index.md) | [Formats](formats.md) | [Delivery](delivery.md) | [Decoding](decoding.md) | [Playback](playback.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/audio.json) | [Guide](../../how-to/content/audio.md)

This reference describes Engine-owned audio resource delivery, decoding, playback, mixing, and script entry points. Project sound catalogs, spatialization policy, music state machines, mastering, and licensing remain outside this contract.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>experimental</code> |
| Support policy | Authored WAV and native Ogg inputs, the baked Ogg payload, sound handles, placement updates, and client playback behavior are revision-pinned by focused baker and mixer tests. |
| Source manifest | [BuildTools/AudioInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/AudioInterface.json) |
| Contract digest | <code>a5b3cb18613c5f67a6be38634f1e162fc652de55e57bf8b14482015ca92c9205</code> |
| Runtime formats | <code>.ogg</code> |
| Missing-suffix fallback | no |
| Runtime side | <code>client</code> |
| Focused native audio tests | 2 |

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Formats](formats.md) | 2 | Accepted containers/codecs and source roles. |
| [Delivery](delivery.md) | 5 | Audio baking, authored paths, indexing, and payload rules. |
| [Decoding](decoding.md) | 7 | Format restrictions, streaming, conversion, and mixing. |
| [Playback](playback.md) | 10 | Script methods, sound handles, placement, music, repeat, and volume. |
| [Validation](validation.md) | 7 | Failure behavior and verification boundaries. |

## Boundary

Included:

- WAV authoring and Ogg Vorbis native resource inputs accepted by AudioBaker
- AudioBaker delivery and AudioManager resource-path indexing
- AudioManager decoding, conversion, streaming, mixing, placement, repeat, and stop behavior
- Game.PlaySound, Game.UpdateSound, and Game.PlayMusic client script entry points
- Audio settings, headless behavior, diagnostics, and project validation boundaries

Excluded:

- project sound catalogs, concept-to-path mappings, variant selection, music state machines, ambient selection, and spatialization policy
- recording, voice chat, capture devices, DSP graphs, buses, ducking, and authored attenuation curves
- audio mastering targets, loudness policy, licensing, attribution, and source provenance
- Effekseer sound nodes, which are not supported by the FOnline Effekseer runtime
- video containers and video-associated audio
