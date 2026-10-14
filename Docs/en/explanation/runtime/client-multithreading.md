---
layout: default
title: Optional Client Multithreading
locale: en
document_id: client-multithreading
permalink: /Docs/en/explanation/runtime/client-multithreading.html
---

# Optional Client Multithreading

## Source paths inspected

- `Source/Client/WorkScheduler.h` and `Source/Client/WorkScheduler.cpp`
- `Source/Client/Client.cpp`, `SpriteManager.cpp`, `ModelSprites.cpp`, and `ModelInstance.cpp`
- `Source/Client/FontManager.h`, `FontManager.cpp`, and `Source/Tests/Test_FontLayout.cpp`
- `Source/Client/MapLightCalculation.h`, `MapLightCalculation.cpp`, `MapView.cpp`, `FogOfWar.h`, and `FogOfWar.cpp`
- `Source/Common/Settings.inc`
- `Source/Tests/Test_WorkScheduler.cpp`, `Test_ModelAnimationPoseProcedural.cpp`, and `Test_ClientEngine.cpp`

## What this is

`Client.Multithreading` selects serial execution or CPU workers at startup in one binary. [Worker selection](#how-many-workers) derives their count; there is no fixed-count setting, `FO_ENABLE_CLIENT_MULTITHREADING` macro or separate artifact. Compare both modes on one machine with identical resources.

What is implemented today:

- [WorkScheduler](../../../../Source/Client/WorkScheduler.h): bounded CPU batches and owning background tasks over each `ClientEngine`'s persistent workers.
- [ResourcePreparation](../../../../Source/Client/ResourcePreparation.h): owned CPU decoding with owner-only acquisition/publication and bounded static-map lookahead.
- [SpriteManager](../../../../Source/Client/SpriteManager.cpp): two-phase frame updates with declared CPU-only work.
- [SpriteGeometry](../../../../Source/Client/SpriteGeometry.cpp): atlas, region, glyph and map vertex/index kernels.
- [FontManager](../../../../Source/Client/FontManager.cpp): pure measurement and draw-layout preparation with owner-only cache publication.
- [MapView](../../../../Source/Client/MapView.cpp) and [FogShape](../../../../Source/Client/FogOfWar.cpp): dirty light-source and fog-sector calculations over passive obstacle snapshots.
- [ModelInstance](../../../../Source/Client/ModelInstance.cpp): prepare / evaluate / finalize poses; evaluation is one worker stage.

The option does not provide a render thread, GPU recording off the application thread, concurrent scripts, parallel particles or threaded Web. Their prerequisites and proposed order follow below.

No frame-time speedup is claimed. Tests establish pose equality and serial bypass; earlier embedding-project player-path checks do not qualify later revisions or platforms. See [Measuring it](#measuring-it).

## The execution contract

The application thread is the sole owner of client entities, scripts, input, the renderer, the sprite/atlas
managers, and resource publication. A worker only ever runs a **kernel**: a pure CPU computation over state the
batch item exclusively owns, plus immutable data whose lifetime the owner has pinned for the batch.

```text
flowchart LR
    A[Owner: network, input, scripts, map update] --> B[Eligible stage]
    B -->|no workers| S[Direct: the owner calls the kernel itself]
    B -->|workers started| P[Owner prepares a bounded batch]
    P --> W[Workers + owner run the kernels]
    W --> J[Batch drains, first failure is kept]
    S --> C[Owner: apply results, effects, callbacks]
    J --> C
    C --> R[Owner: GPU submission, present]
```

Batch rules:

- **A worker never holds a borrow it did not get through its own item index.** Capturing a client, an entity, a
  manager or the renderer and calling methods through it is out, whatever `const` says about the signature.
- **The owner never publishes anything while a batch is running.** `RunBatch` is synchronous: it returns only
  after every item has completed, so there is no window in which a worker and the owner both touch the frame.

A foreground batch cannot outlive its stage. Owning background resource tasks can remain outstanding outside `RunBatch`; they use the same workers but separate pending/failure accounting. Foreground batches take priority at worker acquisition, and the owner can finish their chunks while workers are busy decoding. Cancel removes queued captures outside the scheduler lock, active work retains ownership until exit, and drain establishes quiescence. Only the constructing owner manages groups and consumption; workers cannot submit or drain nested work. No additional pool or dispatcher is created.

## The scheduler

`ClientEngine::WorkSched` is handed to `SpriteManager` at construction. Embedded clients in one process have separate workers, cancellation and failure state.

```cpp
if (scheduler->ShouldRunParallel(item_count, min_items)) {
    scheduler->RunBatch("StageName", item_count, 1, [&](size_t index) { RunKernel(items[index]); });
}
else {
    for (auto& item : items) {
        RunKernel(item);
    }
}
```

Each eligible stage decides here. `ShouldRunParallel` is false for serial mode, fewer than the caller's threshold or one item. Its ordinary loop creates no job, future, completion counter or scratch buffer.

- **Chunks are coarse.** At most one per participating thread, never smaller than the caller's
  `min_items_per_chunk`. Scheduling cost scales with threads, not with items.
- **The owner participates.** With busy or slow-to-wake workers it can run the whole batch itself, so a chunk is
  never left unclaimed and a saturated machine degrades to serial rather than stalling.
- **A batch is closed before the next opens.** Workers copy the batch descriptor under the mutex and are counted
  while they hold it; `RunBatch` waits for that count to reach zero. Without it, a worker that woke after its
  batch finished would claim a chunk index against the *next* batch and run the previous batch's kernel.
- **Failure is reported once, on the owner.** The first `std::exception` an item throws is captured, the
  remaining chunks stop early, and the exception is rethrown from `RunBatch` at the owner's own consumption
  point. A failed item is never silently rerun on the main thread. `catch (...)` in a worker is a defect handler:
  it raises `FO_UNKNOWN_EXCEPTION()` and the process exits, per [ExceptionSafety.md](../../contributing/coding-contracts/exception-safety.md) §2.1.
- **Nesting is refused.** A batch submitted from inside a batch throws instead of deadlocking against a pool it
  is itself occupying.

## Text-layout batches

`FontManager::PrepareTextLayouts` prepares measurement and final draw formats in two synchronous phases. Each worker owns one `FontFormatInfo` and reads the selected font's immutable metrics; it never receives a client, entity, cache or renderer. The application thread coalesces duplicate keys, updates cache recency, applies brightness and publishes completed formats after each barrier. Draw formats use the measured extent and actual color, preserving the separate measurement/draw cache keys.

Warm caches submit no jobs. The preliminary minimum is 32 distinct misses with at least 16 items per chunk. `Game.CanPrepareTextLayoutsInParallel()` reports worker availability, while `Game.PrepareTextLayouts` also supports explicit serial callers. Consumer scripts retain their existing direct route with no batch lists when workers are unavailable. Font replacement clears the cache; no prepared result exposes a native cache borrow. See [batch preparation](../../how-to/content/font-format.md#batch-preparation).

## Light and fog batches

`MapView::ProcessLighting` coalesces the global reapply request, intensity changes and source dirty flags into one ordered source list. At least eight dirty sources and available workers select `MapLightCalculation`; chunks contain at least two sources. The owner snapshots only a clipped rectangle around each source, including `LightBlocked`, `HasWall`, `HasTransparentWall` and `Corner`. The conservative rectangle includes the square perimeter and adjacent-wall marks. Inputs contain scalar source/daylight values, never `MapView`, `Field`, entities or callbacks. One worker result holds ordered mark events and fan tuples. Only after the complete batch joins does the owner clean and replace each source in the collected order, update visibility/global counters and rebuild render primitives. Removal and intensity interpolation remain on the owner. `UpdateHexLightSources` also batches the sources already marking an obstacle cell when eligible; its False/zero/small branch retains the original copied-map/direct loop. Individual source creation, movement and parameter updates remain immediate owner calculations. Existing obstacle invalidation coverage is unchanged: removing a cell no longer marked by a blocked source does not itself discover that source.

`CalculateMapLight` and the owner-only `CalculateMapLightDirect` use one private calculation with separate obstacle readers and mark sinks. The direct sink publishes marks as before: False, zero workers and small dirty lists build neither obstacle snapshots nor temporary mark-event queues. The fan result is still a temporary vector moved into the source, so direct-path allocation cost requires measurement. The original float stepping, wall orientation, half-intensity neighbors, integer clamps/divisions and fan order are preserved. A repeated mark from one source replaces its whole color when any channel is stronger; the combined cell light takes a per-channel maximum across sources. These are different operations.

`FogShape` separates its passive `CalculationInput` from the owner `Input` containing the direct trace callback. A dirty shape of radius at least 32 uses six hex or four square sectors through `FogGeometry`. Traced sectors receive a bounded immutable shoot-block mask; untraced shapes and overlays ignoring shoot blocks need no mask. `LineTracer` returns the blocked cell itself, including map-edge termination, matching `TraceBullet`'s block result rather than its previous-cell result. Sector results contain positions, colors and offset kinds. The owner concatenates sectors in perimeter order, preserving the center after every two edges, then binds stable draw/base offset borrows and runs transitions. Clean, disabled, small and zero-worker shapes create no snapshot or jobs.

The thresholds are provisional and share `ClientEngine::WorkSched`'s existing worker budget; neither stage adds a pool or a public setting. Failed worker batches join before rethrowing and publish no pending results. Source dirty flags and fog rebuild requests remain set for a later retry; already published geometry stays valid. A direct light failure retains its recorded marks as a cleanable applied source. Native tests use frozen legacy hex mask/color/fan fixtures, hand-derived center and blocked-cell colors, actual dirty consumers, direct/zero/small paths and failure/recovery checks. These do not establish GPU equivalence, square-build qualification, platform qualification or frame-time speedup.

Native consumers must rebuild: `FogShape::Input` now derives from `CalculationInput`, and `Prepare` accepts the optional scheduler and owner obstacle reader. Script declarations, serialized resources, saves, networking and Managed ABI 9 are unchanged by these internal interfaces.

## How many workers

`Client.Multithreading` is a switch, and the worker count behind it is chosen once, at client construction, by
`WorkScheduler::ChooseWorkerCount`. It reads three facts through `ReadWorkerCountInputs` — the logical core count,
whether the build can start threads at all, and whether the CPU is a phone's — and applies these rules in order,
with the three thresholds taken from settings (defaults in brackets):

| Rule | Why |
|---|---|
| No thread support (Web) → no workers | Without `-pthread`, an enabled shared config falls back to serial. |
| Unknown core count → no workers | Serial avoids oversubscription when the machine reports no count. |
| Keep one core for the application thread | The owner also executes chunks; counting it as a worker would double-book a core. |
| Keep another from `Client.MultithreadingHeadroomMinCores` (4) | Headroom serves graphics-driver, audio and managed-runtime threads; smaller machines give that core to the batch. |
| Phone cap: `Client.MultithreadingMaxMobileWorkers` (2) | Mixed cores and thermal limits make the reliable fast cluster small. Dynamically claimed, equal chunks on a slow core delay the batch. This cap never raises the general cap. |
| General cap: `Client.MultithreadingMaxWorkers` (8) | Further splitting small batches adds worker wake/join overhead. Each stage also sets its own minimum chunk size. |

Defaults give 0 workers on 1 core, 1 on 2, 2 on 3 and 4, 4 on 6, 6 on 8, and 8 from 10 cores up; a phone gets at most 2. Zero remains valid with the switch on. The startup log reports the actual count and limiting reason, including serial fallback.

These unmeasured thresholds tune the rule through profiling without rebuilding. Caps accept `0..MAX_WORKER_THREADS` (64); a zero mobile cap keeps phones serial. Headroom cannot be negative. `ChooseWorkerCount` validates them on every platform. Each stage chooses its chunk minimum; `ShouldRunParallel` keeps small batches on the owner.

## The first stage: sprite CPU updates

`SpriteManager::BeginScene` used to walk `_updateSprites` and call `Update()` on each sprite. It now materializes
the live set into a vector first — which also removes a latent hazard, since an `Update()` that starts updating
another sprite used to insert into the map being iterated — and then, **only when the client is parallel**, runs
a preparation pass over it:

1. `Sprite::PrepareUpdate()` on the owner. It decides what this sprite will do this frame and settles everything
   a worker must not touch. Returning `true` means `RunPreparedUpdate()` now holds CPU-only work.
2. `Sprite::RunPreparedUpdate()` — the batch item. It may touch nothing but that sprite's own state.
3. `Sprite::Update()` on the owner, where the GPU and the shared managers exist again.

A serial client keeps the original single-pass `Update()` and its predicate. Preparing all models first changes callback order: advancing a timeline can re-resolve movement and fire animation-init scripts that modify another critter. An already prepared model then sees that modification one frame later. Joint masks are sized at construction and the live set holds every sprite by `shared_ptr` until completion, so nothing dangles. The prior unordered-map walk was not deterministic, but this mode-specific ordering difference still requires visual comparison.

### Model animation poses

`ModelSprite` is the only sprite kind that implements the two phases so far. A model frame pose is now three
phases, and `PoseSpriteFrame` is the three called in a row:

| Phase | Thread | What it does |
|---|---|---|
| `PrepareSpriteFramePose` | owner | Root transformation, body rotation, timeline advance (which can re-resolve a movement animation and reach the client's animation-init callback), track inputs and joint masks read out of the controllers, procedural rotations, ground position pushed down the hierarchy. Recursive. |
| `EvaluateFramePose` | worker | `ModelAnimationRuntimePose::Evaluate` per node, the world-matrix snapshot, the linked-joint overrides, and each child's parent matrix. Recursive. Reads only this hierarchy's own buffers plus the immutable rig. |
| `FinalizeFramePose` | owner | Children first, then attached particle setup and update, the configuration layout refresh, and the animation callbacks. Recursive. |

A whole hierarchy is one item: each child reads its parent's newly posed joint. Per-node jobs would require per-depth barriers with little gain in shallow hierarchies.

Ozz sampling is safe here because every buffer it writes belongs to the pose object — the sampling contexts, the
per-track locals, the joint weights and the model matrices are all per-instance — while the skeleton and the
clips are shared and immutable. The ozz allocator is installed before any pose is constructed and forwards to
`safe_alloc`, which is thread-safe.

The frame-sizing loop in `ModelSpriteFactory::DrawModelToAtlas` consumes the prepared pose as its first pass and
finishes it there; the re-poses that converge the frame size stay serial, since they are rare after warmup and
run inside the GPU-owning stage anyway. A sprite carrying a prepared pose keeps the frame that pose was built
against — settling the frame again would move the root under an already evaluated skeleton.

Direct-draw models (`Render.ModelDirectDraw`), GUI previews and `Prewarm` keep the single-pass path: their poses
happen on demand, inside drawing, where no batch boundary exists.

## Sprite and glyph geometry

`SpriteManager` snapshots atlas rectangles, colors, the selected `SpriteSheet` frame and map transforms on the application thread. It reserves disjoint vertex/index ranges and records `DipData` in draw order. Commands retain offsets rather than buffer spans while the vectors can grow. Immutable mesh topology is held until completion; subsequent frame selection or sprite destruction cannot change a queued mesh.

`AtlasSprite::PrepareGeometry` accepts only the exact built-in atlas type. A subclass that changes `FillData` must explicitly override preparation and may use the protected `PrepareAtlasGeometry` helper; otherwise its owner callback runs after the geometry completion barrier. `ModelSprite` opts in and resolves its crop colors against the lighting width on the application thread, sharing the same read-only calculation with `FillData`. Workers never access a `ModelInstance`.

`WritePreparedSpriteGeometry` writes only its provided spans. The same kernel handles quads, cropped mesh coordinates and color gradients, clipped-region quads and glyphs. Map rotation, projected flattening, standing depth and both transparency-egg flags run after the base geometry, using values captured by the owner. Draw-time glyph iteration, font selection, entity access and effect/texture selection remain on the application thread. Prepared layout can parse UTF-8 and inline colors in CPU workers before the owner consumes the cache.

`FlushBatch` drains geometry before upload, preserving render-target, scissor and effect ordering. `Flush` also completes pending commands when their vertex count is zero, so malformed mesh indices cannot bypass validation. Wireframe buffer reads and unsupported external `Sprite::FillData` implementations also require completion first. `AbortScene` drops descriptors without drawing them. If a kernel throws, `RunBatch` joins all participating workers before rethrowing; the geometry drain discards pending commands, draw records and buffer counts, so no incomplete batch is uploaded or replayed.

Map egg overrides preserve the existing callback contract: when neither egg applies, the transform leaves an external sprite's per-vertex flags intact. When either egg applies, it replaces both slots. Built-in quad and mesh vertices initialize both slots before the transform.

With no workers, draw calls execute the kernel directly without queues or retained mesh owners. With workers, `Render.ParallelSpriteGeometryMinCount` chooses parallel execution at drain (clamped to at least two commands); smaller queues execute on the owner. Its initial value, 128 commands, is provisional and requires workload measurements. The scheduler minimum chunk size is half that threshold, at least one command. Multithreading remains disabled by default.

`SpriteGeometryBatch` is the owner Tracy zone for nonempty geometry drains. Its `WorkScheduler::RunBatch` is named `SpriteGeometry`; existing scheduler batch metadata and join/wait/chunk zones remain unchanged. There is no zone per quad or glyph. `Test_SpriteGeometry.cpp` exercises real sprite/font submission, hand-derived geometry, worker/direct equality, frame/mesh lifetime, output bounds and failure recovery. These tests do not establish a frame-time speedup or a renderer/backend visual qualification.

## What stays on the owner, and why

| Area | Why it is not a batch item |
|---|---|
| `MapView::Process`, `CritterHexView::Process` | Iteration mutates entities, map fields and deletion lists. |
| `MapView::DrawMap` | Rebuilds map data, fires script events between render stages, and records GPU work. |
| Light/fog publication | Map fields, source ownership, visibility/global counters, offset borrows and fog transitions remain owner-only; workers calculate passive marks and geometry. |
| Particles | SPARK and Effekseer share managers, random state and resource callbacks across systems. Middleware threading and client workers also have to be counted against one CPU budget, not enabled independently. |
| Network | Stream decompression is stateful and ordered, and packet handlers mutate client entities. A transport offload needs an ordered byte handoff, not a parallel loop. |
| Resource acquisition/publication | The owner reads bytes and publishes factories, caches, atlas and GPU state. `ResourcePreparation` workers only decode owned bytes; see [limits and lifetime](../../how-to/content/image-format.md#bounded-cpu-resource-preparation). |
| Scripts, renderer, GPU objects | Single-owner by design. Concurrent C# callbacks and a render thread are outside this feature. The frame path no longer reads the GPU back synchronously — a model's hit mask arrives through a non-blocking readback ([FrontendAndRendering.md](../rendering/index.md#reading-a-texture-back)) — so a future render thread would not be stalled by picking. |

SDL documents event polling and GL context selection as main-thread operations, so the portable renderer stays
there regardless of what the CPU workers do.
([SDL_PollEvent](https://wiki.libsdl.org/SDL3/SDL_PollEvent),
[SDL_GL_MakeCurrent](https://wiki.libsdl.org/SDL3/SDL_GL_MakeCurrent).)

## Platforms

| Target | State |
|---|---|
| Windows, Linux | Both modes available. Validate here first. |
| macOS, Android, iOS | Both modes compile and the scheduler is generic, but neither has been validated; do not advertise parallel mode there until it has. Android and iOS get the mobile cap. |
| Web | Serial only. The worker-count rule answers zero there, so `Client.Multithreading` may be on in a shared config and the client still starts serial, saying so in its log. The Emscripten configuration does not enable `-pthread`, and the browser Mono runtime has its own initialization and main-thread attachment rules ([WebDebugging.md](../../how-to/platforms/web-debugging.md), [Emscripten pthreads](https://emscripten.org/docs/porting/pthreads.html)). A threaded Web build is a separate artifact with COOP/COEP isolation and loader selection, and blocking the browser main thread on a worker would stall it — that lane needs non-blocking orchestration of its own. |
| Mapper | Serial always, whatever the setting says. It edits content on one thread and gains nothing. |

## Measuring it

Four lanes, identical resources, settings, renderer, optimization and workload:

| Lane | Purpose |
|---|---|
| A: the revision before this feature | Detect cost introduced by splitting the shared kernels. |
| B: this revision, `Multithreading = False` | The serial path, against A. |
| C: this revision, `Multithreading = True` on a small scene | Scheduler overhead where there is nothing to spread. |
| D: this revision, `Multithreading = True` on a crowded scene | Useful scaling, and whether the chosen count is the one that pays — lane D rerun with `Client.MultithreadingMaxWorkers` lowered shows where more workers stop paying, and that answer revises the defaults. |

Measure median/p95/p99 frame time, CPU critical-path time, GPU time, batch preparation and join wait, allocations
after warmup, RSS, and input-to-present latency. Run both uncapped and frame-capped sessions, and include a
low-core machine so scheduler overhead cannot hide inside a crowd scene. Average FPS proves nothing.

`SpriteManager::UpdateSprites` and `PrepareSpriteCpuUpdates` (`Render`) frame the stage on the application
thread. `SpriteCpuUpdatePrepare` isolates preparation of the eligible sprite set. `WorkScheduler::RunBatch`
(`Threading`) spans submission, the owner's work and participant drain; each participant has a
`WorkScheduler::RunClaimedChunks` zone containing its kernel work. Time outside the owner's `RunClaimedChunks`
also includes submission and closing overhead, so that residual is not a measurement of join wait.

`ClientWorkBatchJoin` spans acquisition of the batch mutex, participant drain and closing the batch.
Its nested `ClientWorkBatchWait` starts after the initial mutex acquisition and covers the participant predicate
loop, condition-variable waits and their mutex reacquisition. A batch already drained still has a short wait
zone; this is elapsed zone time, not a count of sleeping time or proof of lock contention. Every nonempty worker
batch attaches `items=N;chunks=N;workers=N;minItemsPerChunk=N` as RunBatch zone text. It is formatted only when
the `Threading` category is compiled in, after the zero-worker direct return. Per-participant wall-clock durations
may overlap and cannot be summed into the owner's critical path.

The `Engine` category also provides `ClientFrameCallbacks` (scheduled callbacks and time events),
`ClientFrameOnLoop` and `ClientFrameOnRenderIface`. These boundaries remain available with `-Script`, without
installing Mono per-method hooks. They include native calls made by their handlers and do not isolate managed
CPU time. Correlate complete batch zones with intervals between `Application::EndFrame` completions, preserving
capture identity, source paths, owner thread and incomplete-boundary exclusions.

Proposed gates, to pin before promoting the mode: A→B no worse than 1% on median CPU frame time and 2% on
p95/p99, with enough repeated runs to resolve that; for D, a repeatable end-to-end gain on a declared target
workload with no extra buffered frame and no material regression on small scenes. The benefit is bounded by the
serial remainder — `T ≈ T_serial + T_parallel / P + T_prepare + T_join` — so a large speedup of one kernel does
not imply a comparable frame-time gain, especially when the GPU or the scripts dominate.

Structural gates are stricter than the timing ones, and they hold today: no geometry queue or retained mesh owner on the direct path, no
worker access to owner-only state, bounded pending work, and no task left at teardown.

## Tests

The hidden `ClientPoseBatchCost` experiment compares identical baked animation inputs with alternating direct/batch order and exact matrix checks. Its [native input contract](../../../../Source/Tests/README.md#opt-in-pose-batch-cost) separates kernel cost from scene variability; frame and visual validation remain necessary.

- [Test_WorkScheduler.cpp](../../../../Source/Tests/Test_WorkScheduler.cpp) — the worker-count rule against limits of
  the test's own (every row of the table above, a zero cap, the mobile cap never lifting the general one, a limit
  out of range refused, and a sweep over 0..256 cores proving more cores never mean fewer workers and the owner
  always keeps a core), the machine inputs matching the platform, the serial path, every item running exactly once, repeated batches never crossing over, empty and single-item
  batches, the chunk minimum, the parallel threshold, item exceptions reaching the owner with the scheduler still
  usable, nested submission refused, shutdown with workers, two schedulers side by side, serial/parallel producing
  the same result, `ClientMultithreadingFollowsTheSetting` — a real client engine starting no workers when the
  switch is off and exactly the count the rule gives this host when it is on, a lowered `Client.MultithreadingMaxWorkers` reaching
  that count, and running frames in both modes —
  and `ClientSpriteUpdatePhasesFollowTheClientMode`, which
  drives a real frame over recording sprites: a serial client calls only `Update()`, and a parallel one prepares
  every sprite before any evaluation and finishes none until the whole batch has drained. That last one was
  checked against its own falsification (dropping the serial guard fails it).
- [Test_ModelAnimationPoseProcedural.cpp](../../../../Source/Tests/Test_ModelAnimationPoseProcedural.cpp) —
  `ModelAnimationRuntimePosesEvaluateIdenticallyOnClientWorkers`: separate poses over one shared rig, evaluated
  across real worker threads and repeated, give world matrices bit-identical to the serial evaluation. This is the
  claim the whole feature rests on, so it is covered where it can run in every configuration.
- [Test_ClientEngine.cpp](../../../../Source/Tests/Test_ClientEngine.cpp) — `ModelPosePhasesMatchTheSinglePassPose`: the
  split `ModelInstance` phases, interleaved as a batch interleaves them and then run through a real scheduler,
  land on the same pose as the single-pass one. The model fixtures and this native test run with either scripting backend;
  the kernel-level cover above also remains backend-neutral.

Run the native tests under ThreadSanitizer where it is available, and apply
[ThreadSafetyAnalysis.md](../../contributing/coding-contracts/thread-safety-analysis.md) to the scheduler's shared state. Neither proves task lifetime,
exclusive ownership or callback order on its own. Headless tests cannot establish visual parity: a real renderer
still has to be inspected for animation, attachments, shadows, particle timing, bounds and hit testing, map
transitions and GUI previews. An embedding project with a live client harness should also walk one
player-visible surface twice, once per mode, and compare the two verdicts.

## Where this goes next

1. **Profile.** The frame-time lanes above, on the two validated platforms. Everything below is worth doing only
   in the order the profile puts them.
2. **A further stage, chosen by that profile.** Light and fog now have passive calculation boundaries; qualify
   their existing stages before adding more work to the scheduler.
3. **Resource-lane qualification.** Measure loading latency, stalls and memory for the existing bounded prepare/publish lane; qualify other STL implementations before shipping them. The pinned MSVC reservation excludes retained diagnostic exception payloads and is not a physical peak bound.
4. **Platform validation** for macOS and Android, and a decision on whether a threaded Web artifact is worth its
   separate loader and isolation requirements.
