---
title: Video Resource Delivery
document_id: generated-video-delivery
locale: en
generated: true
---

# Video Resource Delivery

> Generated reference. Do not edit directly. Update `BuildTools/VideoInterface.json`, then run `python BuildTools/docs_video.py --write`.

[Index](index.md) | [Formats](formats.md) | [Delivery](delivery.md) | [Decoding](decoding.md) | [Fullscreen](fullscreen.md) | [Embedded](embedded.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/video.json) | [Guide](../../how-to/content/video.md)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-video-delivery-raw-copy-065ac8d80c"></a><code>video.delivery.raw-copy</code> | Raw-copy delivery | Keep ogv in Baking.RawCopyFileExtensions and include RawCopy in the client-only resource pack that owns video files. | There is no video baker; playback consumes the delivered bytes. | [Source/Common/Settings.inc](https://github.com/cvet/fonline/blob/master/Source/Common/Settings.inc), [Source/Tools/RawCopyBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/RawCopyBaker.cpp) |
| <a id="entry-video-delivery-exact-path-1964d0511a"></a><code>video.delivery.exact-path</code> | Exact resource path | Pass the complete delivered video path, including its extension; video lookup has no default suffix or normalized stem index. | Both fullscreen and script-owned paths call Resources.ReadFile with the supplied path. | [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp), [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-video-delivery-client-only-c11a2c2a05"></a><code>video.delivery.client-only</code> | Client runtime ownership | Deliver video to client resources and invoke playback on the client or mapper runtime. | Decoding, texture creation, drawing, and exported methods live in the client layer. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-video-delivery-memory-budget-05df3e8e88"></a><code>video.delivery.memory-budget</code> | Whole-resource memory budget | Budget compressed file bytes, one CPU RGBA frame, and one GPU texture for each active playback; the stock path is not streaming from the resource store. | ReadFile.GetData moves the complete resource into VideoClip.RawVideoData before packet decoding. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp), [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
