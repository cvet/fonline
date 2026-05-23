# Getting Started with FOnline Engine

This guide is the first route for a developer who opens the engine repository and wants to understand what to read, what to build, and what belongs to the engine versus the game.

## Mental model

FOnline is not a complete game by itself. It is a reusable engine that is normally checked out as an `Engine/` submodule inside a game repository.

The split is:

- **Engine repository:** runtime, tools, build modules, resource pipeline, scripting bridge, platform packaging, third-party code, and engine documentation.
- **Game repository:** content, scripts, project configuration, presets, branding, native game extensions, deployment choices, and game-specific documentation.

If a question is about reusable runtime behavior, engine tools, platform build mechanics, or script/native contracts, document it here in `Engine/Docs/`. If a question is about a concrete game's content, balance, quests, text, maps, or release policy, document it in that game's docs.

## First reading path

1. Read the repository overview in [../README.md](../README.md).
2. Read [EmbeddingProject.md](EmbeddingProject.md) to understand how a game project composes the engine.
3. Read [BuildWorkflow.md](BuildWorkflow.md) before running CMake or platform package steps.
4. Open [../Source/README.md](../Source/README.md) when you need source-tree orientation.
5. Open [../BuildTools/README.md](../BuildTools/README.md) when touching generated files, CMake stages, packaging, or platform workspaces.

## Common tasks

### I want to create or inspect a game project

Start with [EmbeddingProject.md](EmbeddingProject.md). The game repository should own its root CMake files, `.fomain`, content folders, scripts, and release-specific settings. The engine should stay reusable.

### I want to build or run tests

Start with [BuildWorkflow.md](BuildWorkflow.md). Prefer the embedding project's presets and tasks. Engine-only assumptions are easy to get wrong because actual target names, package names, and generated API files are project-dependent.

### I want to debug native code

Use [Debugging.md](Debugging.md). It covers stack traces, debugger helpers, Visual Studio-specific flow, and validation notes.

### I want to work on Web or Android

Use the platform docs:

- [WebDebugging.md](WebDebugging.md)
- [AndroidDebugging.md](AndroidDebugging.md)

### I want to work on updater/runtime split

Use [ClientUpdater.md](ClientUpdater.md). The client host/runtime ABI and updater protocol are subtle enough that they should not be reconstructed from code alone.

### I want to change script/native nullability

Use [Nullability.md](Nullability.md). Keep C++ annotations, AngelScript-visible types, runtime checks, and analyzers aligned.

## Documentation rule

Keep docs close to ownership:

- Engine-wide reusable behavior -> `Engine/Docs/`.
- Engine source-tree or build-tool entry points -> `Engine/Source/README.md`, `Engine/BuildTools/README.md`, or focused files under `Engine/Docs/`.
- Game-specific behavior -> the embedding project's `Docs/`.

When changing behavior, update the owning doc in the same change. Do not leave a reader to infer new rules from code.
