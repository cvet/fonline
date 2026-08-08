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
| Support policy | The three current decoders and client playback behavior are revision-pinned while focused native decoder fixtures and a versioned compatibility policy are still missing. |
| Source manifest | [BuildTools/AudioInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/AudioInterface.json) |
| Contract digest | <code>f323ea212e9ad28ee8e41d2f75b8b930252b22de663b0f939a9a2df2f4ecb6e1</code> |
| Runtime formats | <code>.wav</code>, <code>.acm</code>, <code>.ogg</code> |
| Default missing suffix | <code>acm</code> |
| Runtime side | <code>client</code> |
| Focused native audio tests | 0 |

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Formats](formats.md) | 3 | Accepted containers/codecs and source roles. |
| [Delivery](delivery.md) | 5 | Raw-copy, indexing, naming, and collision rules. |
| [Decoding](decoding.md) | 7 | Format restrictions, streaming, conversion, and mixing. |
| [Playback](playback.md) | 10 | Script methods, effect variants, music, repeat, and volume. |
| [Validation](validation.md) | 7 | Failure behavior and verification boundaries. |

## Boundary

Included:

- WAV, ACM, and Ogg Vorbis resource formats accepted by the stock client
- RawCopyBaker delivery and ResourceManager sound-name indexing
- SoundManager decoding, conversion, streaming, mixing, repeat, and stop behavior
- Game.PlaySound and Game.PlayMusic client script entry points
- Audio settings, headless behavior, diagnostics, and project validation boundaries

Excluded:

- project sound catalogs, path conventions, music state machines, ambient selection, and spatialization policy
- recording, voice chat, capture devices, DSP graphs, buses, ducking, and per-source gain
- audio mastering targets, loudness policy, licensing, attribution, and source provenance
- Effekseer sound nodes, which are not supported by the FOnline Effekseer runtime
- video containers and video-associated audio
