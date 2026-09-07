---
title: Video Decoding Contract
document_id: generated-video-decoding
locale: en
generated: true
---

# Video Decoding Contract

> Generated reference. Do not edit directly. Update `BuildTools/VideoInterface.json`, then run `python BuildTools/docs_video.py --write`.

[Index](index.md) | [Formats](formats.md) | [Delivery](delivery.md) | [Decoding](decoding.md) | [Fullscreen](fullscreen.md) | [Embedded](embedded.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/video.json) | [Guide](../../how-to/content/video.md)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-video-decoding-ogg-pages-7e5c1f85e9"></a><code>video.decoding.ogg-pages</code> | Ogg page and packet ingestion | Treat the input as Ogg pages read into the sync layer in 1024-byte portions and support at most ten simultaneously discovered logical streams. | DecodePacket owns a fixed ten-stream state table and copies bounded portions from the in-memory resource. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
| <a id="entry-video-decoding-headers-a17928b083"></a><code>video.decoding.headers</code> | Theora header selection | Provide a stream whose headers are accepted by th_decode_headerin and whose setup can allocate a decoder context. | Construction fails when packet seeking, setup data, or decoder allocation fails. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
| <a id="entry-video-decoding-frame-clock-3f8beb68d4"></a><code>video.decoding.frame-clock</code> | Clock-derived frame selection | Author valid fps numerator and denominator metadata; frame selection derives the target frame from elapsed monotonic time and decoder cost. | RenderFrame multiplies elapsed seconds by the Theora fps ratio and decodes the positive frame difference. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
| <a id="entry-video-decoding-pixel-formats-e2858cfb95"></a><code>video.decoding.pixel-formats</code> | Supported chroma subsampling | Use TH_PF_420, TH_PF_422, or TH_PF_444; any other Theora pixel format stops playback. | The CPU conversion chooses chroma divisors only for those three enum values. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
| <a id="entry-video-decoding-rgba-output-4e80c7f310"></a><code>video.decoding.rgba-output</code> | CPU RGBA output | Expect each decoded frame to be converted from YCbCr to opaque RGBA on the CPU before texture upload. | RenderFrame writes clamped RGB components and alpha 0xFF into RenderedTextureData. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
| <a id="entry-video-decoding-error-stop-fe12a95aeb"></a><code>video.decoding.error-stop</code> | Decode failure stops playback | Treat malformed frame data, color output failure, unsupported pixel format, and end-of-stream as stop conditions and inspect client logs. | Frame errors log and call Stop; end-of-stream also stops the clip. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
| <a id="entry-video-decoding-no-container-audio-baa734faa9"></a><code>video.decoding.no-container-audio</code> | No container-audio decode | Do not rely on an audio stream embedded in the Ogg video; author audio as a separate client music resource when needed. | VideoClip links Theora packet decoding only, while fullscreen pairing starts SoundManager music from the path after a vertical bar. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp), [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp) |
