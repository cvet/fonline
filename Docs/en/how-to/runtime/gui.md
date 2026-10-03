---
layout: default
title: GUI Integration Boundary
locale: en
document_id: gui-runtime-guide
permalink: /Docs/en/how-to/runtime/gui.html
---

# GUI Integration Boundary

> Engine-owned documentation. The reusable Engine owns rendering, input, window, resource, and script-export primitives. A high-level GUI object model, screen stack, layout library, generated screens, and authored GUI formats belong to the embedding project.

## Current status

The former Engine-owned AngelScript `CoreScripts/Gui.fos` and `Input.fos` library was removed when high-level script libraries moved to the embedding project during the Managed C# integration. The old generated GUI runtime reference was retired with it because its source contract no longer exists in Engine.

This is an ownership route, not a compatibility specification for that removed library. Do not restore the deleted `.fos` files merely to keep an Engine documentation generator alive, and do not infer a Managed C# GUI API from the old AngelScript types.

## What Engine provides

- application/window services and native input events;
- renderer, sprites, atlases, render targets, fonts, text, effects, video, and drawing exports;
- backend-neutral metadata and generated script bindings for native APIs;
- client lifecycle hooks on which a project GUI can subscribe;
- AngelScript and Managed C# backends through which a project implements its GUI library.

Use [Frontend and Rendering](../../explanation/rendering/) for the native presentation contract, [Scripting](../../explanation/scripting-runtime/) for backend-neutral bindings, and [Managed C# Scripting](../scripting/managed-csharp.md) for C# authoring and runtime behavior.

## What the project must document

The embedding project owns and documents its GUI types, screen registration, stack/modal rules, layout and scaling, drawing order, mouse/keyboard/touch behavior, focus, drag/drop, item views, declarative formats or generators, localization behavior, and screen-specific acceptance tests. These contracts may differ between projects and scripting languages.

## Validation boundary

For an Engine-native render/input export change, regenerate the script API and run the focused native tests. Then compile/bake every affected scripting backend in a representative embedding project and perform visible interaction checks on every claimed platform. Headless success is not visual acceptance.

For a project GUI-library change, keep its source-derived reference, tests, generated screens, and visual evidence in that project. Engine-only validation cannot prove project screen behavior.

## Maintenance triggers

Update this route when Engine adds or removes native presentation/input primitives, changes which scripting backends expose them, or introduces a new reusable Engine-owned GUI layer. A future Engine GUI layer needs an actual source owner, tests, generated/reference contract where useful, EN/RU documentation, project integration evidence, and visible acceptance before this page may call it implemented.

## Source paths inspected

- `Source/Frontend/`
- `Source/Client/`
- `Source/Scripting/*ScriptMethods.cpp`
- `Source/Scripting/AngelScript/`
- `Source/Scripting/Managed/`
- `Docs/en/explanation/rendering/`
- `Docs/en/explanation/scripting-runtime/index.md`
