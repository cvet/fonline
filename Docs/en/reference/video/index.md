---
title: Generated Video Reference
document_id: generated-video-index
locale: en
generated: true
---

# Generated Video Reference

> Generated reference. Do not edit directly. Update `BuildTools/VideoInterface.json`, then run `python BuildTools/docs_video.py --write`.

[Index](index.md) | [Formats](formats.md) | [Delivery](delivery.md) | [Decoding](decoding.md) | [Fullscreen](fullscreen.md) | [Embedded](embedded.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/video.json) | [Guide](../../how-to/content/video.md)

This reference describes the revision-pinned Engine video primitive. It is experimental: a game cinematic system, subtitles, policy, asset ownership, and acceptance evidence remain project concerns.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>experimental</code> |
| Support policy | The current CPU-decoded Ogg/Theora path is revision-pinned while focused native fixtures, production cinematic evidence, and a versioned compatibility policy are missing. |
| Source manifest | [BuildTools/VideoInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/VideoInterface.json) |
| Contract digest | <code>d7ae03dfdb79ca2a61ab6fd9c07a5fd15f3f2c016954653d3d6bee327e360e68</code> |
| Resource | <code>.ogv / Ogg / Theora</code> |
| Whole resource buffered | <code>True</code> |
| Container audio decoded | <code>False</code> |
| Runtime side | <code>client</code> |
| Focused native video tests | 0 |

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Formats](formats.md) | 2 | Container, codec, and pixel-format requirements. |
| [Delivery](delivery.md) | 4 | Raw-copy, exact paths, runtime ownership, and memory. |
| [Decoding](decoding.md) | 7 | Ogg/Theora decode, frame clock, color, and failure rules. |
| [Fullscreen](fullscreen.md) | 9 | Replacement, queue, input, music, drawing, and status. |
| [Embedded](embedded.md) | 7 | Script-owned playback and RenderIface drawing. |
| [Validation](validation.md) | 5 | Visible acceptance gates and known coverage gaps. |

## Boundary

Included:

- OGV raw-copy delivery and exact-path resource loading
- Ogg packetization and Theora header/frame decoding
- CPU YCbCr-to-RGBA conversion and texture upload
- fullscreen playback, queues, input interruption, and separate music pairing
- script-created rectangular VideoPlayback instances
- current diagnostics, limitations, and embedding-project validation

Excluded:

- project cinematic catalogs, story triggers, subtitle systems, localization, skip policy, and save-state consequences
- container audio decoding, voice tracks, audio mixing, and synchronization beyond a separately started music resource
- hardware/platform media decoders, streaming from disk or network, adaptive bitrate, and DRM
- MP4, WebM, AVI, MPEG, animated images, and non-Theora Ogg video
- mastering, accessibility, licensing, attribution, and source provenance
