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
| <a id="entry-audio-delivery-raw-copy-f938a33272"></a><code>audio.delivery.raw-copy</code> | Raw-copy delivery | Keep acm, ogg, and wav in Baking.RawCopyFileExtensions and include RawCopy in every resource pack that owns runtime audio. | There is no dedicated audio baker; runtime decoders require the original bytes and relative path. | [Source/Common/Settings.inc](https://github.com/cvet/fonline/blob/master/Source/Common/Settings.inc), [Source/Tools/RawCopyBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/RawCopyBaker.cpp) |
| <a id="entry-audio-delivery-client-index-a1456e4c25"></a><code>audio.delivery.client-index</code> | Client sound index | Call ResourceManager.IndexFiles after client resources are available; it indexes every wav, acm, and ogg path for effect-name lookup. | Game.PlaySound resolves through the prebuilt name map rather than scanning the filesystem on each call. | [Source/Client/ResourceManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ResourceManager.cpp) |
| <a id="entry-audio-delivery-effect-identity-c2478ffbdd"></a><code>audio.delivery.effect-identity</code> | Effect identity | Treat an effect identity as the lowercase resource path with its final extension removed. | PlaySound erases the caller suffix and lowercases the name before querying ResourceManager's identically normalized index. | [Source/Client/ResourceManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ResourceManager.cpp), [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-delivery-extension-precedence-ec0db763ec"></a><code>audio.delivery.extension-precedence</code> | Duplicate-stem precedence | Do not ship multiple wav/acm/ogg resources with the same normalized effect stem; when they collide, the first indexed extension wins in wav, acm, ogg order. | ResourceManager uses map::emplace while iterating the fixed extension array, so later formats do not replace an existing stem. | [Source/Client/ResourceManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ResourceManager.cpp) |
| <a id="entry-audio-delivery-music-path-22673a0d79"></a><code>audio.delivery.music-path</code> | Music path ownership | Pass music as an exact resource path; unlike effects, music is not resolved through the normalized sound-name index. | PlayMusic forwards the supplied filename directly to SoundManager.Load. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
