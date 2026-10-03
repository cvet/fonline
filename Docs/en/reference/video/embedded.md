---
title: Embedded Video Contract
document_id: generated-video-embedded
locale: en
generated: true
---

# Embedded Video Contract

> Generated reference. Do not edit directly. Update `BuildTools/VideoInterface.json`, then run `python BuildTools/docs_video.py --write`.

[Index](index.md) | [Formats](formats.md) | [Delivery](delivery.md) | [Decoding](decoding.md) | [Fullscreen](fullscreen.md) | [Embedded](embedded.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/video.json) | [Guide](../../how-to/content/video.md)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-video-embedded-create-dd46921a64"></a><code>video.embedded.create</code> | Script-owned playback creation | Use Game.CreateVideoPlayback(exactPath, looped) to create an independent ref-counted playback and texture. | The exported PassOwnership method loads the resource, constructs VideoClip, creates a texture, and stores both in VideoPlayback. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-video-embedded-missing-file-6c5e775f2a"></a><code>video.embedded.missing-file</code> | Creation failure throws | Catch or prevent a missing script-owned video resource; creation throws Video file not found. | Unlike fullscreen PlayVideo, CreateVideoPlayback converts a missing ReadFile result into ScriptException. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-video-embedded-render-event-77026ee7da"></a><code>video.embedded.render-event</code> | RenderIface-only drawing | Call Game.DrawVideoPlayback only from Game.OnRenderIface handling. | The method throws outside the client script draw scope. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-video-embedded-positive-size-1a3e8315de"></a><code>video.embedded.positive-size</code> | Positive target size advances | Pass a positive width and height every frame; zero or negative size skips frame decode, texture upload, and drawing. | RenderFrame is called only inside the positive-size branch. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-video-embedded-rectangle-c7450bb713"></a><code>video.embedded.rectangle</code> | Caller-owned rectangle and aspect | Choose and maintain the target rectangle and aspect policy in project UI code; the Engine draws exactly the supplied position and size. | DrawVideoPlayback constructs irect32 directly from the script arguments. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-video-embedded-stopped-31b532e22e"></a><code>video.embedded.stopped</code> | Stopped field lifecycle | Poll VideoPlayback.Stopped only after continuing to draw the instance; the flag becomes true when DrawVideoPlayback observes that the clip stopped. | The draw method clears resources and sets the exported field after the stop check. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp), [Source/Client/Client.h](https://github.com/cvet/fonline/blob/master/Source/Client/Client.h) |
| <a id="entry-video-embedded-null-noop-12f484cd61"></a><code>video.embedded.null-noop</code> | Null and completed instances are no-ops | Passing null or an instance whose playback resources were cleared performs no draw and does not throw. | DrawVideoPlayback returns early for both states after enforcing render scope. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
