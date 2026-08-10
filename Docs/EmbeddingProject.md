# Embedding FOnline in a Game Project

FOnline is designed to be embedded as a source submodule. The engine repository supplies reusable technology; the game repository supplies the concrete product.

## Expected repository shape

A typical game repository looks like this:

```text
GameProject/
├── Engine/                 # git submodule pointing to this repository
├── CMakeLists.txt          # project entry point that includes engine build logic
├── CMakePresets.json       # project presets and platform variants
├── GameName.fomain         # master project configuration
├── Scripts/                # game AngelScript modules
├── SourceExt/              # optional project-native C++ extensions
├── Critters/ Items/ Maps/  # game content and prototypes
├── Dialogs/ Texts/         # dialogs and localization
└── Docs/                   # game-specific documentation
```

Folder names vary by project, but the ownership rule should stay stable: reusable engine machinery lives under `Engine/`, while concrete game content and project policy live in the parent repository.

## What belongs in the engine

Keep these in the engine repository:

- Runtime systems shared by multiple games.
- BuildTools and CMake stages used to compose projects.
- Platform package/workspace generation logic.
- Engine resources and reusable tools.
- Public/native API definitions and generated scripting API machinery.
- Documentation about engine behavior, platform mechanics, and reusable contracts.

## What belongs in the game project

Keep these in the embedding project:

- Game rules, content, maps, prototypes, dialogs, localization, and GUI definitions.
- Game-specific AngelScript modules.
- Project-level native extensions.
- Project presets, product identifiers, package names, signing/deployment choices, and CI policy.
- Game design and content workflow documentation.

## Build composition

The game repository should drive the build. In practice this means:

1. Configure from the game repository root, not from inside `Engine/`, unless a specific engine-only workflow says otherwise.
2. Use the game repository's `CMakePresets.json` and tasks so generated paths, target names, and package metadata match the product.
3. Let engine `BuildTools` provide reusable stages and helper functions.
4. Keep generated files out of hand-authored docs unless the generation process is part of the topic.

## Documentation composition

Use this routing:

- Link from game docs into `Engine/Docs/...` for reusable mechanics such as Web/Android debugging, nullability, updater protocol, mapper automation, and native debugging.
- Link from engine docs back to `../../...` only when the example intentionally refers to a real embedding project.
- Avoid duplicating long engine explanations in the game docs. Prefer a short project-specific note plus a link to the owning engine document.

## Validation principle

Validate engine changes through a real embedding project whenever possible. A reusable engine change may compile in isolation but still break generated APIs, project packaging, scripts, or content baking. Choose the narrowest project target that exercises the changed layer.
