---
title: Audio Resource Delivery
document_id: generated-audio-delivery
locale: en
generated: true
---

# Audio Resource Delivery

> Generated reference. Do not edit directly. Update `BuildTools/AudioInterface.json`, then run `python BuildTools/docs_audio.py --write`.

[Index](index.md) | [Formats](formats.md) | [Delivery](delivery.md) | [Decoding](decoding.md) | [Playback](playback.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/audio.json) | [Guide](../../how-to/content/audio.md)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-audio-delivery-raw-copy-f938a33272"></a><code>audio.delivery.raw-copy</code> | AudioBaker delivery | Include the Audio baker in each resource pack that owns runtime audio; it accepts the configured wav and ogg inputs and writes a verified Ogg Vorbis payload. | Audio resources no longer use RawCopy: WAV is normalized and encoded while an already-native Ogg is verified and copied without another lossy pass. | [Source/Tools/AudioBaker.h](https://github.com/cvet/fonline/blob/master/Source/Tools/AudioBaker.h), [Source/Tools/AudioBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/AudioBaker.cpp) |
| <a id="entry-audio-delivery-client-index-a1456e4c25"></a><code>audio.delivery.client-index</code> | Client sound index | Call AudioManager.IndexFiles after client resources are available; it records every path whose suffix occurs in Audio.SoundFileExtensions. | The manager exposes this source-backed path catalog, while Game.PlaySound itself receives the exact selected resource path and performs no concept or variant resolution. | [Source/Client/AudioManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/AudioManager.cpp) |
| <a id="entry-audio-delivery-effect-identity-c2478ffbdd"></a><code>audio.delivery.effect-identity</code> | Exact authored path identity | Pass the exact baked resource path, including its authored extension, to Game.PlaySound and Game.PlayMusic. | AudioBaker preserves the authored path while replacing WAV bytes with Vorbis, and AudioManager reads that path directly without lowercasing or replacing the extension. | [Source/Tools/AudioBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/AudioBaker.cpp), [Source/Client/AudioManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/AudioManager.cpp) |
| <a id="entry-audio-delivery-extension-precedence-ec0db763ec"></a><code>audio.delivery.extension-precedence</code> | Source extension and payload separation | Treat a .wav suffix in baked resources as source identity, not runtime codec identity; all runtime bytes are Ogg Vorbis and .ogg sources remain .ogg. | Preserving authored paths avoids rewriting project references while one runtime decoder and one package payload format remove platform-dependent format behavior. | [Source/Tools/AudioBaker.h](https://github.com/cvet/fonline/blob/master/Source/Tools/AudioBaker.h) |
| <a id="entry-audio-delivery-music-path-22673a0d79"></a><code>audio.delivery.music-path</code> | Music path ownership | Pass music as an exact resource path; unlike effects, music is not resolved through the normalized sound-name index. | PlayMusic forwards the supplied filename directly to AudioManager.Load. | [Source/Client/AudioManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/AudioManager.cpp) |
