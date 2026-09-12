---
title: Generated CMake Project Interface
document_id: generated-cmake-index
locale: en
generated: true
---

# Generated CMake Project Interface

> Generated reference. Do not edit this page directly. Update `BuildTools/cmake/ProjectInterface.json`, then run `python BuildTools/docs_cmake.py --write`.

[Reference index](index.md) | [Canonical JSON model](../../../generated/cmake.json) | [Generation contract](../metadata/)

This reference describes the project-facing CMake surface consumed by an embedding game repository. The manifest is a checked documentation model of the current CMake declarations; the implementation in `BuildTools/Init.cmake` and the stage/helper files remains authoritative at configure time.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>experimental</code> |
| Since | Not declared |
| Support policy | No versioned support line is declared yet; embedding projects should pin an engine revision. |
| Source manifest | [BuildTools/cmake/ProjectInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/ProjectInterface.json) |

## Coverage

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Project options](options.md) | 44 | Required inputs, defaults, and override precedence. |
| [Stages and hooks](stages.md) | 10 | Strict project-generation order and extension boundaries. |
| [Project helpers](helpers.md) | 6 | Selected commands intended for embedding projects. |

## Option override precedence

1. matching FO_ environment variable
2. existing CMake cache or -D value
3. SetOption value supplied by the embedding project
4. declared interface default

The first defined source wins. Required options have no interface default and must be supplied by the project.

## Boundary

Included:

- project cache options declared during the Init stage
- ordered project-generation stage entrypoints and hooks
- selected embedding-project helper commands

Excluded from this slice:

- internal CMake implementation helpers
- package declaration grammar and payload layouts
- BuildTools command-line interfaces
- toolchain files and platform preset policy

Package declarations and payloads are documented by the separate [package interface reference](../packages/index.md). The main BuildTools command line is documented by the separate [parser-backed CLI reference](../buildtools/index.md).
