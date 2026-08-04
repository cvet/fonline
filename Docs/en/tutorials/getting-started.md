---
layout: default
title: Getting Started with FOnline Engine
locale: en
document_id: getting-started
permalink: /Docs/en/tutorials/getting-started.html
---

# Getting Started with FOnline Engine

This guide is the first route for a developer who opens the engine repository and wants to understand what to read, what to build, and what belongs to the engine versus the game.

## Mental model

FOnline is not a complete game by itself. It is a reusable engine that is normally checked out as an `Engine/` submodule inside a game repository.

The split is:

- **Engine repository:** runtime, tools, build modules, resource pipeline, scripting bridge, platform packaging, third-party code, and engine documentation.
- **Game repository:** content, scripts, project configuration, presets, branding, native game extensions, deployment choices, and game-specific documentation.

If a question is about reusable runtime behavior, engine tools, platform build mechanics, or script/native contracts, document it here in `Engine/Docs/`. If a question is about a concrete game's content, balance, quests, text, maps, or release policy, document it in that game's docs.

## First reading path

1. Read the repository overview in [README.md](../../../README.md).
2. Complete the tested [first headless project tutorial](first-project.md).
3. Run [First Playable Client](first-client.md), then make the
   [first content change](first-content.md) and add the
   [first automated test](first-test.md).
4. Read [Embedding FOnline](../how-to/build/embedding-project.md) to understand how a game project composes the engine.
5. Read [Build Workflow](../how-to/build/) before running other CMake or platform package steps.
6. Open [Source/README.md](../../../Source/README.md) when you need source-tree orientation.
7. Open [BuildTools/README.md](../../../BuildTools/README.md) when touching generated files, CMake stages, packaging, or platform workspaces.

## Common tasks

### I want to create or inspect a game project

Run [First FOnline Headless Project](first-project.md), inspect the complete
[minimal project](../../../Examples/MinimalProject/README.md), and continue through
the playable [Minimal Multiplayer](../../../Examples/MinimalMultiplayer/README.md)
lessons. Then read [Embedding FOnline](../how-to/build/embedding-project.md). The game
repository should own its root CMake files, `.fomain`, content folders,
scripts, and release-specific settings. The engine should stay reusable.

### I want to build or run tests

Start with [Build Workflow](../how-to/build/). Prefer the embedding project's presets and tasks. Engine-only assumptions are easy to get wrong because actual target names, package names, and generated API files are project-dependent.

### I want to debug native code

Use [Native and AngelScript Debugging](../troubleshooting/debugging.md). It covers symbols, mixed stacks, crash diagnostics, native debugger behavior, live script attach, and validation boundaries.

### I want to work on Web or Android

Use the platform docs:

- [Web Build, Packaging, and Browser Debugging](../how-to/platforms/web-debugging.md)
- [Android Build, Packaging, and Device Debugging](../how-to/platforms/android-debugging.md)

### I want to work on updater/runtime split

Use [Client Runtime Split and Updater](../explanation/runtime/client-updater.md). The client host/runtime ABI and updater protocol are subtle enough that they should not be reconstructed from code alone.

### I want to change script/native nullability

Use [Nullability.md](../../Nullability.md). Keep C++ annotations, AngelScript-visible types, runtime checks, and analyzers aligned.

## Documentation rule

Keep docs close to ownership:

- Engine-wide reusable behavior -> `Engine/Docs/`.
- Engine source-tree or build-tool entry points -> `Engine/Source/README.md`, `Engine/BuildTools/README.md`, or focused files under `Engine/Docs/`.
- Game-specific behavior -> the embedding project's `Docs/`.

When changing behavior, update the owning doc in the same change. Do not leave a reader to infer new rules from code.
