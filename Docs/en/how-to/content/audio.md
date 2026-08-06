---
layout: default
title: Audio Resources and Playback
document_id: audio-guide
locale: en
permalink: /Docs/en/how-to/content/audio.html
---

# Audio Resources and Playback

> Engine-owned documentation. This guide describes reusable audio formats,
> resource delivery, decoding, playback, mixing, and validation in
> `cvet/fonline`. A game owns its sound catalog, music state machine, ambient and
> spatial policy, mastering, licenses, and visible/audible acceptance tests.

Use the generated [audio reference](../../reference/audio/index.md) when exact stable
IDs, source anchors, or machine-readable values matter.

## Source map

- `Source/Client/SoundManager.*` owns decoder selection, playback state,
  streaming, repeat, stop, and the sound/music split.
- `Source/Client/ResourceManager.cpp` builds the effect-name index.
- `Source/Scripting/ClientGlobalScriptMethods.cpp` exports `Game.PlaySound` and
  `Game.PlayMusic`.
- `Source/Frontend/Application.*` owns the SDL audio device, conversion, mixing,
  and callback synchronization.
- `Source/Frontend/ApplicationHeadless.cpp` defines the no-audio headless
  boundary.
- `Source/Tools/RawCopyBaker.*` delivers authored audio bytes unchanged.
- `Source/Common/Settings.inc` owns audio and raw-copy settings.
- `BuildTools/AudioInterface.json` is the checked documentation contract.

## Supported resources

The stock client recognizes three audio suffixes:

| Suffix | Accepted input | Loading model | Recommended use |
|---|---|---|---|
| `.wav` | Narrow RIFF/WAVE PCM profile, 8-bit unsigned or 16-bit signed | Fully decoded and converted before playback | Short effects and test fixtures |
| `.acm` | Legacy Interplay/Fallout ACM | Fully decoded as signed 16-bit; mono for effects, stereo for music, 22050 Hz | Compatibility with existing classic assets |
| `.ogg` | Ogg containing Vorbis audio | Decoded in chunks; short files become fully resident | New music and longer assets |

These are audio-runtime formats, not generic container promises. In particular:

- compressed WAV is rejected;
- WAV chunk handling is sequential and intentionally narrower than a general
  RIFF parser;
- an Ogg stream must contain Vorbis, not another Ogg-carried codec;
- MP3, FLAC, Opus, AAC, and arbitrary SDL-supported formats are not accepted by
  `SoundManager`;
- an explicit unknown suffix is logged and returns failure.

`strex::get_file_extension()` lowercases the suffix, so `.OGG` and `.ACM` reach
the same decoder dispatch. Prefer lowercase authored names for portable,
reviewable paths.

## Delivering audio

There is no dedicated audio baker. Add all runtime formats to
`Baking.RawCopyFileExtensions` and include `RawCopy` in the client resource pack
that owns them:

```ini
Baking.RawCopyFileExtensions = acm ogg wav

[ResourcePack]
Name = Sound
InputDirs = Resources/Sound
IncludePatterns = **
ClientOnly = True
Bakers = RawCopy
```

The default engine setting already includes `acm`, `ogg`, and `wav`; a project
override must preserve every format it ships. `RawCopyBaker` keeps the resource
path and bytes unchanged. The client must receive the pack because playback is
a client responsibility.

After client resources load, `ResourceManager.IndexFiles()` indexes all three
suffixes for effect lookup. The key is the lowercase resource path with only
the final extension removed.

Avoid two files such as `Sfx/Door.wav` and `Sfx/Door.ogg`. Their normalized
effect identity collides. The index uses first-in insertion with format order
WAV, ACM, Ogg, so the WAV entry wins today, but projects should treat duplicate
stems as an authoring error rather than depending on that precedence.

## Playing effects

`Game.PlaySound(name)` is client-side, non-positional playback:

```angelscript
bool played = Game.PlaySound("Sfx/DoorOpen.wav");
verify(played, "Door-open sound could not be started");
```

The caller's extension is removed before lookup. These calls resolve the same
effect identity:

```text
Sfx/DoorOpen
Sfx/DoorOpen.wav
SFX/DOOROPEN.ogg
```

The indexed resource decides which actual format is loaded. Supplying `.ogg`
does not force Ogg when the same normalized stem points to a WAV.

### Numbered variants

If the base identity does not exist, `PlaySound` searches for a contiguous,
one-based family:

```text
Sfx/Footstep_1.wav
Sfx/Footstep_2.wav
Sfx/Footstep_3.wav
```

Calling `Game.PlaySound("Sfx/Footstep")` selects uniformly from the three. The
rules are exact:

1. A base `Sfx/Footstep.*` always wins and disables variant selection.
2. Numbering starts at `_1`.
3. Discovery stops at the first missing number.
4. `_1` and `_3` without `_2` form a one-entry selectable family; `_3` is not
   discovered.
5. Each numbered identity must itself be unique across formats.

Validate variant families as authored data. A missing middle file does not
produce a runtime error because the shorter prefix remains valid.

### Spatial and gameplay policy

Stock `SoundManager` does not store a world position, radius, listener, pan, or
per-instance gain. Every active sound is mixed globally into the local client's
output. A multiplayer game implements spatial policy by deciding which clients
receive a playback request, whether distance permits it, and which asset or
volume tier to select.

That policy is not an engine audio-format feature. Keep recipient filtering,
ambient emitters, occlusion, cooldowns, and gameplay triggers in project code
and project tests.

## Playing music

`Game.PlayMusic(path, repeatTime)` takes an exact resource path:

```angelscript
bool played = Game.PlayMusic("Music/Exploration.ogg", Time::Milliseconds(1));
verify(played, "Exploration music could not be started");
```

Music does not use the normalized effect index. If the path has no extension,
`SoundManager` appends `.acm` for legacy compatibility.

Only one music group is active. A new call removes all current music before it
tries to load the replacement. If replacement loading fails, the previous track
is not restored. Projects that need fallback should validate the target
resource before the transition or explicitly choose a known fallback after a
false result.

An empty music name stops current music and returns success:

```angelscript
Game.PlayMusic("", Time::Milliseconds(0));
```

## Repeat timing

A zero `repeatTime` means play once. A nonzero value keeps the sound object and
restarts it after completion:

- values greater than one millisecond wait for the authored interval;
- values at or below one millisecond repeat immediately;
- retained Ogg streams rewind to byte position zero before replay.

The delay begins after decoded playback reaches the end, not when playback
starts. It is therefore a gap between iterations, not a target period that
includes track duration.

## Format details

### WAV

`LoadWav` expects, in order:

1. `RIFF`;
2. RIFF size;
3. `WAVE`;
4. `fmt `;
5. a format block at least 16 bytes long;
6. optional `fact`;
7. `data`;
8. sample bytes.

The format tag must be `1` (PCM). Only 8- and 16-bit sample widths are mapped.
Channel count and sample rate are passed to frontend conversion. Because the
loader does not walk arbitrary RIFF chunks, metadata chunks inserted between
the expected blocks can make an otherwise valid WAV fail.

For predictable authoring, export plain PCM WAV without extra chunks and test
the exact delivered file rather than only the editor source.

### ACM

`CACMUnpacker` decodes the complete source. `SoundManager` then supplies the
legacy playback shape:

- signed 16-bit samples;
- one channel for `PlaySound`;
- two channels for `PlayMusic`;
- 22050 Hz.

Use ACM for compatibility. Prefer Ogg Vorbis or PCM WAV for new assets so the
source and playback properties are visible to standard tools.

### Ogg Vorbis

The loader uses libvorbisfile callbacks over the engine `FileSystem`. The first
decode portion is:

- 64 KiB on native targets;
- 128 KiB on Web.

If that first read reaches end-of-file, the decoder is released and the
converted sound is fully resident. Longer streams keep `OggStream` and decode
subsequent portions as the audio callback consumes them.

An Ogg file with no Vorbis data, a bad header, a version mismatch, read failure,
or decode failure returns false and writes a diagnostic.

## Device conversion and mixing

Decoded data keeps its source format, channel count, and rate only until
`AppAudio::ConvertAudio`. The SDL frontend converts it to the active output
device's format. This is why supported WAV rates and channel counts need not
match one hard-coded device profile.

The application audio callback:

1. fills an output buffer with the device's silence value;
2. asks `SoundManager` for every active sound;
3. chooses `Audio.SoundVolume` or `Audio.MusicVolume`;
4. clamps the value to `0..100`;
5. mixes each converted buffer through SDL.

Adding and removing playback objects holds the audio-stream lock. Native
extensions must not bypass this synchronization or mutate `SoundManager`
storage from their own callbacks.

## Disabled and headless behavior

`Audio.DisableAudio = true`, an unavailable SDL device, the headless frontend,
and the stub frontend leave `SoundManager` inactive.

Inactive `PlaySound` and `PlayMusic` calls intentionally report success without
loading a resource. `PlaySound` also returns success without lookup when
`Audio.SoundVolume == 0`. This supports silent/headless runs, but it means:

> A true playback result is not a resource-existence check unless real audio is
> active and, for effects, sound volume is nonzero.

Use baking/resource validation for existence and a visible client for audible
behavior. Do not use headless test success as proof that a codec, device,
conversion, mix, or path works.

## Recommended project practice

1. Use a dedicated client-only `RawCopy` pack for audio.
2. Prefer Ogg Vorbis for music and long assets, PCM WAV for short effects, and
   ACM only when preserving legacy content.
3. Keep one normalized effect stem per resource.
4. Validate numbered families for contiguous `_1.._N` membership.
5. Pass exact paths for music and check replacement failures.
6. Keep server-authoritative gameplay decisions separate from client playback.
7. Put distance, recipient, cooldown, ambient, and transition policy in one
   project-owned audio module.
8. Clamp project UI values to `0..100` even though the mixer also clamps.
9. Store original masters and redistribution provenance outside baked output.
10. Test representative assets on every platform the game claims to support.

Avoid loading the same long Ogg repeatedly as a rapid effect. Each request owns
decoder and converted-buffer state. Use bounded gameplay triggering and choose
asset lengths appropriate to the expected concurrency.

## Diagnostics

Common failures and their likely causes:

| Diagnostic or result | Likely cause |
|---|---|
| false with no matching effect | No base identity and no `_1` variant in the client index |
| `Unsupported sound format` | Explicit path uses a suffix other than WAV, ACM, or Ogg |
| `'RIFF' not found` / `'WAVE' not found` | File is not the expected WAV container |
| `'fmt ' not found` / `Unknown format2` | Unsupported WAV chunk order |
| `Compressed files not supported` | WAV format tag is not PCM |
| `Decode Acm error` | ACM decoder did not produce the expected byte count |
| `Bitstream does not contain any Vorbis data` | Ogg uses another codec or is corrupt |
| SDL conversion failure | Source parameters cannot be converted to the active device |
| true but no sound in headless/silent run | Expected no-op success boundary |

When replacing music, remember that the old track is stopped before the new
file is loaded. A silent result after a false return is expected transition
behavior, not evidence that `StopMusic` failed.

## Validation workflow

Run the checked documentation contract:

```powershell
python BuildTools\docs_audio.py --check
python -m unittest BuildTools.tests.test_docs_audio
python BuildTools\docs_validate.py
```

Run native tests for raw-copy and broad frontend regressions:

```powershell
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

Then validate an embedding project:

1. Bake a pack with representative WAV, ACM, and Ogg files.
2. Confirm the client resource paths and bytes.
3. Start a visible client with audio enabled and nonzero volumes.
4. Play one asset per format.
5. Exercise an absent-base numbered family and a deliberate numbering gap.
6. Replace valid music with valid and invalid paths.
7. Check play-once, immediate repeat, and delayed repeat.
8. Check volume `0`, an in-range value, and `100`.
9. Inspect logs for every decoder and SDL diagnostic.
10. Repeat on each claimed native/Web/mobile platform.

The Engine currently has no focused native `SoundManager` codec/playback test
file. `Test_RawCopyBaker.cpp` proves byte delivery, while documentation tests
pin parser and runtime source structure. Until focused fixtures exist, audible
client validation is a required residual gate.

## Project boundary

An embedding project owns:

- the audio resource tree and naming taxonomy;
- which content properties point to audio;
- server-to-client recipient and distance logic;
- ambient scheduling and music-state transitions;
- concurrency budgets and anti-spam rules;
- loudness, mastering, accessibility, and user options;
- source masters, licenses, attribution, and redistribution approval;
- gameplay and audible acceptance tests.

Do not copy those policies from Last Frontier or another game into engine
documentation. Reuse only behavior supported by the source-backed contract
above.

## Maintenance

Changes to any of these areas are documentation-bearing:

- accepted extensions or decoder dispatch;
- WAV, ACM, or Ogg parsing;
- streaming chunk size or repeat timing;
- normalized identity or variant selection;
- script method signatures or return conventions;
- audio settings, conversion, mixing, or headless behavior;
- raw-copy defaults or resource indexing.

Update `BuildTools/AudioInterface.json` and this guide, regenerate the model and
pages, run focused tests and the aggregate contract diff, and record migration
guidance for any public behavior change in the same change.
