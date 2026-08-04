---
title: Audio Decoding Contract
document_id: generated-audio-decoding
locale: en
generated: true
---

# Audio Decoding Contract

> Generated reference. Do not edit directly. Update `BuildTools/AudioInterface.json`, then run `python BuildTools/docs_audio.py --write`.

[Index](index.md) | [Formats](formats.md) | [Delivery](delivery.md) | [Decoding](decoding.md) | [Playback](playback.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/audio.json) | [Guide](../../how-to/content/audio.md)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-audio-decoding-wav-chunk-order-ac41e721f1"></a><code>audio.decoding.wav-chunk-order</code> | WAV chunk order | Place RIFF, WAVE, fmt, optional fact, and data in the exact order expected by LoadWav. | The loader is a focused sequential reader, not a general RIFF chunk walker. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-decoding-wav-pcm-width-2099ecdd6f"></a><code>audio.decoding.wav-pcm-width</code> | WAV PCM widths | Author WAV as uncompressed PCM with either 8-bit unsigned or 16-bit signed samples; channel count and sample rate may vary and are converted by the frontend. | LoadWav maps only those two widths to AppAudio formats before device conversion. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-decoding-acm-shape-b7a4574d99"></a><code>audio.decoding.acm-shape</code> | ACM playback shape | Expect ACM effects to decode as mono and ACM music as stereo signed 16-bit samples at 22050 Hz. | These values are assigned by playback role after CACMUnpacker runs. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-decoding-ogg-streaming-6ff234d02d"></a><code>audio.decoding.ogg-streaming</code> | Ogg streaming | Expect Ogg Vorbis to decode in 64 KiB native chunks and 128 KiB Web chunks; short files are retained fully and release the stream after the initial decode. | SoundManager uses a platform-sized streaming portion and clears OggStream when the first read reaches EOF. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-decoding-device-conversion-c6306e36cc"></a><code>audio.decoding.device-conversion</code> | Device conversion | Let AppAudio convert decoded sample format, channel count, and rate to the active SDL output-device format before playback. | SoundManager does not require authored assets to match one fixed hardware format. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp), [Source/Frontend/Application.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.cpp) |
| <a id="entry-audio-decoding-callback-mixing-f9db5306d1"></a><code>audio.decoding.callback-mixing</code> | Audio callback mixing | Treat playback as client audio-callback work; mutations of the active sound list must hold the audio-device lock. | The SDL stream callback asks SoundManager to fill output while game-thread play/stop operations can add or erase sounds. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp), [Source/Frontend/Application.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.cpp) |
| <a id="entry-audio-decoding-unsupported-extension-a56ebf6594"></a><code>audio.decoding.unsupported-extension</code> | Unsupported extension rejection | Reject any explicit extension other than wav, acm, or ogg and log the unsupported suffix. | Accepting an undecoded empty sound would report false success and enqueue unusable playback state. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
