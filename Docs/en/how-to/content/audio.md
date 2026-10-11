---
layout: default
title: Audio Resources and Playback
document_id: audio-guide
locale: en
permalink: /Docs/en/how-to/content/audio.html
---

# Audio Resources and Playback

> Engine-owned documentation. This guide describes reusable audio baking,
> runtime decoding, playback, placement, mixing, and validation in
> `cvet/fonline`. A game owns its sound catalog, concept-to-path mapping, music
> state machine, spatial policy, mastering, licenses, and audible acceptance.

Use the generated [audio reference](../../reference/audio/index.md) when exact
stable IDs, source anchors, or machine-readable values matter.

## Source map

- `Source/Tools/AudioBaker.*` accepts authored WAV and Ogg, verifies the input,
  and emits an Ogg Vorbis payload while preserving the authored resource path.
- `Source/Client/AudioManager.*` indexes resource paths and owns decoding,
  streaming, handles, placement, repeat, stop, and live volume state.
- `Source/Scripting/ClientGlobalScriptMethods.cpp` exports `Game.PlaySound`,
  `Game.UpdateSound`, and `Game.PlayMusic`.
- `Source/Frontend/Application.*` owns the SDL audio device, conversion, mixing,
  and callback synchronization. `ApplicationHeadless.cpp` defines the no-audio
  headless boundary.
- `Source/Common/Settings.inc` owns immutable audio startup settings.
- `Source/Tests/Test_AudioBaker.cpp` and `Test_AudioManager.cpp` execute the
  focused native contract.
- `BuildTools/AudioInterface.json` is the checked documentation contract.

## Supported resources

The authoring boundary accepts two source forms:

| Authored suffix | Accepted input | Baker result | Runtime decoder |
|---|---|---|---|
| `.wav` | RIFF/WAVE PCM at 8, 16, 24, or 32 bits; IEEE float at 32 bits | Normalized to interleaved signed 16-bit and encoded as Ogg Vorbis | libvorbisfile |
| `.ogg` | Ogg bitstream containing Vorbis audio | Verified and copied without another lossy pass | libvorbisfile |

There is no ACM runtime path. MP3, FLAC, Opus, AAC, arbitrary SDL formats, and
Ogg containers carrying a codec other than Vorbis are unsupported.

The authored suffix identifies the source path, not the bytes after baking. A
resource named `Sfx/Door.wav` still has that path after baking, but its payload
is Vorbis. Runtime therefore uses one decoder for both preserved `.wav` paths
and native `.ogg` paths; it does not dispatch a codec from the suffix.

## Delivering audio

Include the `Audio` baker in every resource pack that owns client audio:

```ini
[ResourcePack]
Name = Sound
InputDirs = Resources/Sound
IncludePatterns = **
ClientOnly = True
Bakers = Audio
```

Audio is no longer a `RawCopy` resource family. WAV is converted according to
`Baking.AudioVorbisQuality`; authored Ogg is validated before passthrough.
Both routes verify that the output opens as Vorbis. The baker reports encoded
and passthrough counts plus input/output byte totals.

`AudioManager.IndexFiles()` records paths with suffixes listed by
`Audio.SoundFileExtensions` (normally `wav ogg`). The catalog is an inspection
surface; playback itself receives the exact selected resource path.

## Playing effects

`Game.PlaySound(path)` returns a non-reused `uint32` lifetime handle:

```csharp
uint sound = Game.PlaySound("Sfx/DoorOpen.wav");
if (sound == 0) {
    // No live sound was started.
}
```

Pass the exact baked resource path, including the authored extension and case.
There is no extension fallback, lowercase stem normalization, or automatic
concept lookup.

### Numbered variants

The engine does not discover or randomly select numbered variants. If a game
authors `Footstep_1.wav` through `Footstep_4.wav`, project code must choose one
and pass its exact path. This keeps catalog and randomization policy outside the
reusable mixer.

### Placed playback

Use the overload with attenuation and pan when a sound has a current placement:

```csharp
uint sound = Game.PlaySound("Sfx/Generator.wav", attenuation, pan);
```

- attenuation at or below zero returns handle zero before file I/O;
- pan is clamped to `-1..1`; negative values attenuate the right channel and
  positive values attenuate the left;
- the near channel remains at unity, so panning does not boost into clipping.

When the source or listener moves, update the existing instance:

```csharp
bool alive = Game.UpdateSound(sound, attenuation, pan);
```

`false` means the handle is zero, audio is inactive, or playback already
finished. Handles are never reused, so a stale handle cannot target a later
sound. Distance curves, listener selection, occlusion, and recipient filtering
remain project policy.

## Playing music

`Game.PlayMusic(path, repeatTime)` takes an exact path. An empty path stops the
current music and returns success. A new track stops existing music before it
loads the replacement; a failed replacement does not restore the old track.

Only one music group is active. Music does not use concept lookup or suffix
fallback.

## Repeat timing

A zero `repeatTime` means play once. A nonzero value retains the playback
object and restarts it after completion:

- values greater than one millisecond insert that delay;
- values at or below one millisecond repeat immediately;
- retained Vorbis streams seek back to byte position zero before replay.

The interval begins after playback reaches the end; it is a gap, not a period
that includes the track duration.

## Format details

### WAV authoring

`AudioBaker` walks RIFF chunks in any order, skips unknown chunks, validates
bounds, and respects odd-byte padding. It requires one usable `fmt ` chunk and
a non-empty `data` chunk. Supported formats are PCM (`1`) at 8/16/24/32 bits
and IEEE float (`3`) at 32 bits. Channel count and sample rate must be positive;
block alignment must match the declared frame shape; truncated frames fail the
bake.

Every accepted sample is normalized to signed 16-bit PCM before Vorbis
encoding. Test the exact source export: metadata layout and malformed format
fields are authoring errors even if an editor happens to play the file.

### Ogg Vorbis runtime

Authored Ogg and generated output are opened with libvorbisfile during baking.
At runtime `AudioManager` opens every audio resource as Vorbis and decodes an
initial portion of 64 KiB on native targets or 128 KiB on Web. Short files
become resident; longer files retain `OggStream` and continue decoding from the
audio callback.

## Device conversion and mixing

`AppAudio::ConvertAudio` converts decoded channels, sample rate, and format to
the active SDL output device. The callback starts with silence, asks
`AudioManager` for active data, applies per-instance attenuation and pan, then
mixes using the live sound or music volume.

`Audio.SoundVolume` and `Audio.MusicVolume` are immutable startup defaults.
Use `Game.SetSoundVolume` and `Game.SetMusicVolume` for live changes; the
frontend clamps every mix operation to `0..100`.

Play, stop, and placement update operations synchronize through the audio
device lock. Native extensions must not mutate mixer storage from callbacks.

## Disabled and headless behavior

`Audio.DisableAudio = true`, an unavailable SDL device, and headless/stub
frontends leave audio inactive. In that state `PlayMusic` is a successful no-op
and `PlaySound` returns handle zero.

These results are not a resource-existence check. Baking proves that a path and
payload are valid; a visible client with an active device proves conversion,
callback scheduling, mixing, and audible output.

## Recommended project practice

1. Put authored WAV/Ogg in a client pack that selects the `Audio` baker.
2. Keep exact paths in project-owned catalogs; resolve concepts and variants
   before calling the engine.
3. Use WAV as an editable/master input and native Ogg when avoiding another
   lossy encode matters.
4. Centralize distance curves, listener choice, ambient scheduling, cooldowns,
   and music transitions in project code.
5. Keep server-authoritative gameplay decisions separate from local playback.
6. Store masters, licenses, attribution, and redistribution provenance outside
   baked output.
7. Test representative assets and placement motion on every supported platform.

## Diagnostics

Treat all `AudioBaker` exceptions as content failures. Typical causes are a
missing or truncated RIFF chunk, unsupported WAV encoding/width, inconsistent
block alignment, an empty Ogg, or an Ogg stream without Vorbis.

At runtime, Ogg open/decode and device-conversion failures are logged and enter
the debugger. Handle zero only says that no live effect started; it does not
replace the baker gate.

## Validation workflow

Run documentation and focused native checks:

```powershell
python BuildTools\docs_audio.py --check
python -m pytest BuildTools\tests\test_docs_audio.py
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

`Test_AudioBaker` covers PCM/float WAV conversion, native Ogg passthrough, and
invalid inputs. `Test_AudioManager` covers decoder/mixer output, pan, placed
starts, live updates, handle expiry, and synchronization-relevant behavior.

Then bake the embedding project's real pack. Confirm preserved paths, inspect
baker counters, and open every output as Vorbis. In a visible client, play a
WAV-path and native-Ogg-path resource, move a placed sound, replace music,
exercise immediate/delayed repeats, and test volume endpoints on each claimed
platform.

## Project boundary

An embedding project owns catalogs, concepts, variants, server/client routing,
distance and occlusion policy, ambient and music state, concurrency budgets,
loudness/accessibility choices, masters, licenses, attribution, and audible
acceptance. The engine supplies baking, one decoder path, a client mixer,
handles, and placement parameters.

## Maintenance

Changes to accepted inputs, baker conversion, preserved paths, Vorbis
streaming, handles, placement, repeat timing, script signatures, live volume,
device conversion, package delivery, or headless behavior are
documentation-bearing. Update `BuildTools/AudioInterface.json` and this guide,
regenerate the reference, run focused tests and the aggregate contract diff,
and include migration guidance for public behavior changes.
