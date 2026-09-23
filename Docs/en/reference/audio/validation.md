---
title: Audio Validation Contract
document_id: generated-audio-validation
locale: en
generated: true
---

# Audio Validation Contract

> Generated reference. Do not edit directly. Update `BuildTools/AudioInterface.json`, then run `python BuildTools/docs_audio.py --write`.

[Index](index.md) | [Formats](formats.md) | [Delivery](delivery.md) | [Decoding](decoding.md) | [Playback](playback.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/audio.json) | [Guide](../../how-to/content/audio.md)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-audio-validation-generated-reference-657f0f8e81"></a><code>audio.validation.generated-reference</code> | Source-backed reference | Regenerate and check the audio model whenever AudioBaker, AudioManager, AppAudio, settings, package delivery, or script entry points change. | The generated model rejects source-extension, runtime-codec, chunk-size, setting, handle, test-inventory, and headless drift before prose can silently become stale. | [BuildTools/docs_audio.py](https://github.com/cvet/fonline/blob/master/BuildTools/docs_audio.py) |
| <a id="entry-audio-validation-raw-copy-3e0a946475"></a><code>audio.validation.raw-copy</code> | Audio baking gate | Bake representative PCM/float WAV and native Ogg inputs, verify authored paths are preserved, inspect the audio counters, and open every output as Vorbis. | Focused tests prove conversion and passthrough, while an embedding-project bake proves that its pack actually selects Audio and ships the resources consumed by its scripts. | [Source/Tests/Test_AudioBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_AudioBaker.cpp), [Source/Tools/AudioBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/AudioBaker.cpp) |
| <a id="entry-audio-validation-decoder-diagnostics-65927ba99d"></a><code>audio.validation.decoder-diagnostics</code> | Decoder diagnostics | Treat AudioBaker exceptions and Ogg open/decode/device-conversion diagnostics as authoring or packaging failures; do not bless a zero playback handle as content validation. | WAV and passthrough Ogg are rejected at bake time, while a resource that exists but cannot decode at runtime logs and enters the debugger instead of becoming silent success. | [Source/Tools/AudioBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/AudioBaker.cpp), [Source/Client/AudioManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/AudioManager.cpp) |
| <a id="entry-audio-validation-headless-boundary-d3aa6fb0d9"></a><code>audio.validation.headless-boundary</code> | Headless boundary | Do not claim audible validation from a headless or stub application; AppAudio is disabled there and playback may report no-op success. | Headless audio deliberately exposes no active device or callback. | [Source/Frontend/ApplicationHeadless.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/ApplicationHeadless.cpp) |
| <a id="entry-audio-validation-native-test-gap-4426613ecf"></a><code>audio.validation.native-test-gap</code> | Focused native coverage | Run Test_AudioBaker and Test_AudioManager for WAV/Ogg baking, passthrough, invalid inputs, panning, placed playback, live updates, handle expiry, and mixer output. | These suites execute the codec and callback contract directly; a visible client is still required to prove a real platform device and audible output. | [Source/Tests/README.md](https://github.com/cvet/fonline/blob/master/Source/Tests/README.md), [BuildTools/docs_audio.py](https://github.com/cvet/fonline/blob/master/BuildTools/docs_audio.py) |
| <a id="entry-audio-validation-visible-client-4feb1d4bfa"></a><code>audio.validation.visible-client</code> | Visible audible validation | On every claimed platform, use a visible client with audio enabled to play baked WAV-path and native-Ogg-path resources, move a placed sound, replace music, exercise delayed and immediate repeats, and test volume endpoints. | Only an active platform audio device can prove conversion, callback scheduling, mixing, and audible output. | [Source/Frontend/Application.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.cpp), [Source/Client/AudioManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/AudioManager.cpp) |
| <a id="entry-audio-validation-project-boundary-66ad789ce4"></a><code>audio.validation.project-boundary</code> | Embedding-project ownership | Keep catalog conventions, spatial/recipient policy, mastering, licenses, attribution, budgets, and gameplay triggers in project documentation and tests. | The engine supplies baking, runtime decoding, a client mixer, and placement parameters, not a complete game audio design or asset-governance system. | [Source/Client/AudioManager.h](https://github.com/cvet/fonline/blob/master/Source/Client/AudioManager.h) |

## Validation commands

```powershell
python BuildTools\docs_audio.py --check
python -m unittest BuildTools.tests.test_docs_audio
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

There is currently no focused native decoder/playback fixture. An embedding project must also bake representative WAV, ACM, and Ogg resources, exercise effect and music calls in a visible client with audio enabled, inspect logs, and verify volume/repeat behavior on every claimed platform.
