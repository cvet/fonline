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
| <a id="entry-audio-format-wav-005081e1f5"></a><code>audio.format.wav</code> | <code>.wav</code> | Short effects or music decoded fully before playback | Use a RIFF/WAVE file with an immediate fmt chunk, optional fact chunk, an immediate data chunk, PCM format tag 1, and 8-bit unsigned or 16-bit signed samples. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-format-acm-f70b222843"></a><code>audio.format.acm</code> | <code>.acm</code> | Legacy effects and music decoded fully before playback | Supply an Interplay ACM stream accepted by CACMUnpacker; the stock playback contract treats effects as mono and music as stereo signed 16-bit audio at 22050 Hz. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-format-ogg-091dbf2da3"></a><code>audio.format.ogg</code> | <code>.ogg</code> | Modern effects or streaming music | Use an Ogg bitstream containing Vorbis audio; SoundManager decodes signed 16-bit interleaved data and retains the decoder only when more than the first chunk remains. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
