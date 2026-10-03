---
title: Audio Resource Formats
document_id: generated-audio-formats
locale: en
generated: true
---

# Audio Resource Formats

> Generated reference. Do not edit directly. Update `BuildTools/AudioInterface.json`, then run `python BuildTools/docs_audio.py --write`.

[Index](index.md) | [Formats](formats.md) | [Delivery](delivery.md) | [Decoding](decoding.md) | [Playback](playback.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/audio.json) | [Guide](../../how-to/content/audio.md)

| Stable ID | Suffix | Role | Contract | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-audio-format-wav-005081e1f5"></a><code>audio.format.wav</code> | <code>.wav</code> | Authoring input converted to the runtime Ogg Vorbis payload | Use RIFF/WAVE PCM at 8, 16, 24, or 32 bits, or IEEE float at 32 bits; chunks may appear in any order and unknown chunks are skipped with RIFF padding respected. | [Source/Tools/AudioBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/AudioBaker.cpp) |
| <a id="entry-audio-format-ogg-091dbf2da3"></a><code>audio.format.ogg</code> | <code>.ogg</code> | Native runtime payload and lossless passthrough authoring input | Use an Ogg bitstream containing Vorbis audio; AudioBaker verifies and copies an authored .ogg without lossy re-encoding, and AudioManager streams every baked audio resource through libvorbisfile. | [Source/Client/AudioManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/AudioManager.cpp), [Source/Tools/AudioBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/AudioBaker.cpp) |
