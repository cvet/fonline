---
layout: default
title: Video Resources and Playback
document_id: video-guide
locale: en
permalink: /Docs/en/how-to/content/video.html
---

# Video Resources and Playback

> Engine-owned documentation. This guide describes the revision-pinned
> Ogg/Theora decoder and presentation primitives in `cvet/fonline`. A game owns
> its cinematic catalog, triggers, recipients, skip policy, subtitles,
> localization, save-state consequences, mastering, provenance, and visible
> acceptance tests.

Use the generated [video reference](../../reference/video/index.md) for stable
contract IDs, checked source anchors, and machine-readable values.

## Contract status

The stock video path is **experimental**. It is useful for controlled project
integration, but it is not yet a versioned production media subsystem:

- only Ogg carrying Theora video is recognized by the implementation;
- the complete compressed resource is loaded into memory before decoding;
- decoding and YCbCr-to-RGBA conversion run on the client CPU;
- the Ogg container's audio is not decoded;
- there is no focused native video decoder, rendering, queue, or loop fixture;
- there is no built-in subtitle, accessibility, aspect-fit, streaming, or
  cinematic-state layer.

Pin the exact Engine revision and re-run visible acceptance tests whenever the
decoder, rendering, resource, input, or audio path changes.

## Source map

- `Source/Client/VideoClip.*` owns Ogg packet ingestion, Theora setup, frame
  timing, pixel conversion, stop/pause state, and the loop flag.
- `Source/Client/Client.*` owns fullscreen playback, queueing, input
  interruption, separate music pairing, draw order, and status.
- `Source/Scripting/ClientGlobalScriptMethods.cpp` exports fullscreen and
  script-owned playback methods.
- `Source/Client/SpriteManager.cpp` defines the target-rectangle behavior used
  by video drawing.
- `Source/Common/Settings.inc` declares `.ogv` as a raw-copy extension.
- `Source/Tools/RawCopyBaker.*` delivers authored bytes without transcoding.
- `BuildTools/cmake/stages/ThirdParty.cmake` links Ogg and Theora into client
  targets.
- `BuildTools/VideoInterface.json` is the checked documentation contract.

## Delivering video

There is no video baker. The default `Baking.RawCopyFileExtensions` contains
`ogv`; an embedding project must also put `RawCopy` in the client-visible
resource pack that owns the files. Prefer a dedicated, client-only video pack
so server and mapper payload decisions stay explicit.

Both playback surfaces use exact resource paths:

```angelscript
Game.PlayVideo("Video/Intro.ogv", true, false);
VideoPlayback video = Game.CreateVideoPlayback("Video/Terminal.ogv", false);
```

Include the `.ogv` suffix. There is no default extension, normalized video-stem
index, language fallback, or filesystem search. Path spelling and case must
match the delivered resource on case-sensitive targets.

The presence of `ogv` in `RawCopyFileExtensions` only permits copying. It does
not select a resource pack, prove that the client received the file, or validate
its container and codec.

## Authoring requirements

Author an Ogg resource with a valid Theora stream. The decoder requires valid
picture dimensions, frame-rate numerator and denominator, setup headers, and
one of these Theora pixel formats:

- `TH_PF_420`;
- `TH_PF_422`;
- `TH_PF_444`.

The Engine does not invoke an authoring tool. A conventional starting point for
a silent asset is:

```powershell
ffmpeg -i input.mov -an -c:v libtheora output.ogv
```

Treat that command as an authoring example, not a compatibility proof. Inspect
the produced stream, bake its exact bytes, and run it in every supported client.
Choose dimensions and frame rate from measured platform budgets. Avoid assuming
that another codec in an Ogg container will work.

The video path does not decode audio from the container. Export sound
separately in a format accepted by [Audio Resources and Playback](audio.md).
The built-in fullscreen path can start one separate music resource, but it does
not provide sample-accurate synchronization, alternate tracks, dialogue
ducking, or language selection.

## Memory and performance

`Resources.ReadFile(...).GetData()` moves the complete compressed file into
`VideoClip`. The implementation does not stream from disk, package storage, or
the network. Each active playback also owns:

- a CPU buffer of one opaque RGBA frame;
- an Ogg/Theora decoder state;
- a GPU texture matching the encoded picture dimensions.

Budget at least `compressed file size + width * height * 4` CPU bytes plus the
GPU texture and decoder overhead for every simultaneous playback. Embedded
instances retain their own copy and texture. Do not preload many long clips by
creating dormant `VideoPlayback` objects.

Ogg input is fed to the parser in 1024-byte portions from the resident buffer.
That implementation detail is not resource streaming. Frame selection uses a
monotonic clock and the encoded frame-rate ratio; when presentation falls
behind, one draw call may decode multiple packets before uploading a frame.
Profile CPU conversion and texture upload on the weakest supported target.

## Fullscreen playback

Call:

```angelscript
Game.PlayVideo(videoName, canInterrupt, enqueue);
bool pending = Game.IsVideoPlaying();
```

The method returns no success value. A missing file leaves no active playback,
and the current implementation emits no dedicated missing-video diagnostic on
that path. Validate resources before entering a transition that depends on
completion.

`Game.IsVideoPlaying()` returns true when a clip is active **or** the fullscreen
queue is non-empty. It does not prove that a frame is currently visible.

### Queue and replacement

When `enqueue` is false, `Game.PlayVideo` destroys the current clip, clears the
entire queue, and then tries to load the requested file. Passing an empty string
therefore acts as a fullscreen stop-and-clear operation.

When `enqueue` is true and a clip is active, the request is appended. When no
clip is active, the same request starts immediately; it does not create a
waiting-only state. Queued entries start sequentially after the previous clip
stops. Test a missing queued entry because loading failure can make the queue
advance without showing a frame.

### Interruption

With `canInterrupt = true`, the current clip stops on:

- key-down;
- mouse-down;
- touch down, move, up, tap, double-tap, scroll, or zoom.

This is a broad transport primitive, not a complete skip policy. A game that
requires hold-to-skip, a protected first interval, confirmation, input
debouncing, or mandatory story state must implement that policy around its own
cinematic controller.

### Separate music

The fullscreen string accepts one optional music path after `|`:

```angelscript
Game.PlayVideo("Video/Intro.ogv|Sound/Intro.ogg", true, false);
```

The Engine loads the first component as video and uses only the second component
as one-shot music. Additional separators do not form a playlist. Starting a
paired request first stops current music. Completion or interruption of every
fullscreen clip also calls `StopMusic()` unconditionally, even when that clip
did not start paired music.

Do not place gameplay-critical music restoration behind an assumption that the
video path preserves the previous music group. A project controller should
record and restore the intended state explicitly.

### Drawing and aspect

Fullscreen frames are uploaded and drawn after `Game.OnRenderIface`. Drawing
without source and target regions fills the complete current render target with
alpha blending disabled. The encoded image is stretched when its aspect ratio
differs from the target.

The built-in path has no letterboxing, pillarboxing, safe-area, crop, caption,
overlay, or transition policy. Use a project-owned embedded presentation when
those requirements matter.

## Embedded playback

Create a script-owned instance with:

```angelscript
VideoPlayback video = Game.CreateVideoPlayback("Video/Terminal.ogv", false);
```

Unlike fullscreen playback, a missing resource throws `Video file not found`.
The returned ref-counted object owns independent decoder state and a texture.

Draw it only during `Game.OnRenderIface`:

```angelscript
void RenderTerminalVideo()
{
    Game.DrawVideoPlayback(video, ipos(120, 80), isize(640, 360));
}
```

The exact event subscription syntax belongs to the embedding script module.
The target width and height must be positive. A non-positive size skips frame
decode, upload, and drawing, so a hidden instance does not advance through this
API.

The Engine draws exactly the requested rectangle and does not preserve aspect
ratio. Compute fit, crop, bars, safe area, and responsive layout in project UI
code. Passing null or an instance whose resources were released is a no-op.

`VideoPlayback.Stopped` becomes true only when a subsequent
`Game.DrawVideoPlayback` call observes the stopped clip, clears its resources,
and updates the field. Continue drawing or polling through a controller until
that cleanup transition has occurred.

## Looping

`Game.CreateVideoPlayback(path, true)` exposes the `VideoClip` loop flag. At
end-of-stream, the current source calls `Stop()` followed by `Resume()`, but
there is no focused fixture proving decoder rewind, packet-state reset, frame
counter reset, or uninterrupted multi-cycle output.

Treat looping as experimental even within this experimental subsystem. Do not
promise a production ambient loop until a visible test has completed several
cycles for the exact asset and every claimed platform. Prefer an explicit
project fallback when continuity matters.

## Diagnostics

Construction and frame decode can report failures such as:

- packet seek or Theora header decode failure;
- missing setup data or decoder allocation failure;
- malformed encoded frame data;
- color-buffer output failure;
- unsupported Theora pixel format.

Inspect the client log around the first attempted frame. For fullscreen missing
files, independently inspect baked resources because that lookup currently
returns silently. A successful constructor still does not prove sustained
rendering, correct aspect, synchronized audio, cleanup, or loop continuity.

## Validation workflow

Run the source-backed documentation checks:

```powershell
python BuildTools\docs_video.py --check
python -m unittest BuildTools.tests.test_docs_video
python BuildTools\docs_validate.py
```

Then validate each representative asset in a visible client:

1. Bake and confirm the exact client resource path and byte identity.
2. Show the first frame and sustained motion at the intended resolution.
3. Reach natural completion and verify resource cleanup.
4. Exercise every allowed interruption input and the project's skip policy.
5. Exercise replacement, multiple queued entries, and a deliberately missing
   path.
6. Verify target resize, aspect policy, safe area, overlays, and captions.
7. Verify separate music start, stop, restoration, and acceptable drift.
8. Profile memory, CPU decode/conversion, frame pacing, and texture upload.
9. For looped playback, observe several complete cycles.
10. Repeat on every native, web, Android, and mapper target the project claims.

There is currently no focused native video fixture. Source-backed checks keep
the documentation honest; they do not replace player-visible evidence.

## Project boundary

The Engine owns:

- exact-path client resource loading;
- Ogg/Theora packet and frame decoding;
- CPU RGBA conversion and texture upload;
- fullscreen replacement, queue, interruption, and separate music pairing;
- script-owned playback creation and rectangular interface drawing.

An embedding game owns:

- a video resource pack and asset catalog;
- cinematic triggers, recipients, authority, and replay rules;
- skip/queue policy and save-state consequences;
- subtitles, localization, accessibility, aspect, safe area, and overlays;
- music/voice strategy and restoration;
- source assets, licenses, attribution, and provenance;
- file, memory, CPU, GPU, package, and download budgets;
- visible acceptance tests and platform support claims.

Project documentation may link here for Engine mechanics. It must document its
own integration rather than treating another game's scripts or assets as an
Engine contract.

## Maintenance

When video behavior changes:

1. update `BuildTools/VideoInterface.json` and its source anchors;
2. run `python BuildTools/docs_video.py --write`;
3. update this guide when authoring or operational meaning changed;
4. add or update focused native tests when the decoder becomes testable;
5. run the documentation contract diff and disposition workflow;
6. require embedding projects to repeat visible acceptance for affected assets
   and targets.

The generated reference is machine-checkable evidence for the current revision,
not a substitute for a compatibility policy or production media tests.
