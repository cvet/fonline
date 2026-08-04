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
| <a id="entry-audio-validation-generated-reference-657f0f8e81"></a><code>audio.validation.generated-reference</code> | Source-backed reference | Regenerate and check the audio model whenever SoundManager, ResourceManager, AppAudio, settings, raw-copy delivery, or script entry points change. | The generated model rejects extension, decoder, chunk-size, default, setting, and headless drift before prose can silently become stale. | [BuildTools/docs_audio.py](https://github.com/cvet/fonline/blob/master/BuildTools/docs_audio.py) |
| <a id="entry-audio-validation-raw-copy-3e0a946475"></a><code>audio.validation.raw-copy</code> | Baking gate | Bake a pack containing representative audio and verify the expected relative paths and original bytes reach client resources. | Documentation checks can prove configured extension coverage but cannot prove an embedding project's resource-pack wiring. | [Source/Tests/Test_RawCopyBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_RawCopyBaker.cpp), [Source/Tools/RawCopyBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/RawCopyBaker.cpp) |
| <a id="entry-audio-validation-decoder-diagnostics-65927ba99d"></a><code>audio.validation.decoder-diagnostics</code> | Decoder diagnostics | Treat false playback results and RIFF, PCM, ACM, Ogg, conversion, or unsupported-format log lines as authoring failures. | The loaders report format-specific failure boundaries instead of substituting silence as a valid resource. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-validation-headless-boundary-d3aa6fb0d9"></a><code>audio.validation.headless-boundary</code> | Headless boundary | Do not claim audible validation from a headless or stub application; AppAudio is disabled there and playback may report no-op success. | Headless audio deliberately exposes no active device or callback. | [Source/Frontend/ApplicationHeadless.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/ApplicationHeadless.cpp) |
| <a id="entry-audio-validation-native-test-gap-4426613ecf"></a><code>audio.validation.native-test-gap</code> | Focused native test gap | Keep the absence of focused SoundManager decoder/playback fixtures visible until tests cover valid and invalid WAV, ACM, Ogg, variant, repeat, and replacement behavior. | RawCopyBaker and broad client tests do not execute the codec and callback contract documented here. | [Source/Tests/README.md](https://github.com/cvet/fonline/blob/master/Source/Tests/README.md), [BuildTools/docs_audio.py](https://github.com/cvet/fonline/blob/master/BuildTools/docs_audio.py) |
| <a id="entry-audio-validation-visible-client-4feb1d4bfa"></a><code>audio.validation.visible-client</code> | Visible audible validation | On every claimed platform, use a visible client with audio enabled to play one asset per format, a numbered effect family, a replacement track, delayed and immediate repeats, and volume endpoints. | Only an active platform audio device can prove conversion, callback scheduling, mixing, and audible output. | [Source/Frontend/Application.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.cpp), [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-validation-project-boundary-66ad789ce4"></a><code>audio.validation.project-boundary</code> | Embedding-project ownership | Keep catalog conventions, spatial/recipient policy, mastering, licenses, attribution, budgets, and gameplay triggers in project documentation and tests. | The engine supplies decoding and a global client mixer, not a complete game audio design or asset-governance system. | [Source/Client/SoundManager.h](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.h) |

## Validation commands

```powershell
python BuildTools\docs_audio.py --check
python -m unittest BuildTools.tests.test_docs_audio
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

There is currently no focused native decoder/playback fixture. An embedding project must also bake representative WAV, ACM, and Ogg resources, exercise effect and music calls in a visible client with audio enabled, inspect logs, and verify volume/repeat behavior on every claimed platform.
