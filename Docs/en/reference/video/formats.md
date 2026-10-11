---
title: Video Resource Formats
document_id: generated-video-formats
locale: en
generated: true
---

# Video Resource Formats

> Generated reference. Do not edit directly. Update `BuildTools/VideoInterface.json`, then run `python BuildTools/docs_video.py --write`.

[Index](index.md) | [Formats](formats.md) | [Delivery](delivery.md) | [Decoding](decoding.md) | [Fullscreen](fullscreen.md) | [Embedded](embedded.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/video.json) | [Guide](../../how-to/content/video.md)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-video-format-ogv-3e54cfa6a4"></a><code>video.format.ogv</code> | Ogg video resource | Deliver an Ogg resource containing a Theora video stream through a client-visible RawCopy pack and pass its exact resource path to the video API. | OGV is the stock raw-copy convention; the decoder reads Ogg pages and feeds Theora headers and packets rather than dispatching a generic media framework. | [Source/Common/Settings.inc](https://github.com/cvet/fonline/blob/master/Source/Common/Settings.inc), [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
| <a id="entry-video-format-theora-bccabd3fa3"></a><code>video.format.theora</code> | Theora elementary video in Ogg | Use a decodable Theora stream with valid dimensions, frame-rate metadata, and one of the supported 4:2:0, 4:2:2, or 4:4:4 pixel formats. | The bundled path links libtheora directly and has no alternate codec dispatch. | [BuildTools/cmake/stages/ThirdParty.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/ThirdParty.cmake), [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
