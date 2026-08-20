---
title: Generated GUI Runtime Reference
document_id: generated-gui-runtime-index
locale: en
generated: true
---

# Generated GUI Runtime Reference

> Generated reference. Do not edit directly. Update `BuildTools/GuiRuntimeInterface.json`, then run `python BuildTools/docs_gui_runtime.py --write`.

[Index](index.md) | [Types](types.md) | [Screen API](screen-api.md) | [Lifecycle](lifecycle.md) | [Layout](layout-rendering.md) | [Input](input.md) | [Integration](integration-validation.md) | [Canonical JSON](../../../generated/gui-runtime.json) | [Guide](../../how-to/runtime/gui.md)

This reference describes the Engine-owned AngelScript GUI runtime. It is not a declarative GUI file-format specification: screen source formats, generators, catalogs, styles, and project hook implementations remain embedding-project concerns.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>experimental</code> |
| Support policy | The CoreScripts GUI runtime is production-used but remains experimental until it has a standalone client example, focused runtime fixtures, and an explicit compatibility policy. |
| Source manifest | [BuildTools/GuiRuntimeInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/GuiRuntimeInterface.json) |
| Contract digest | <code>437603ed47bec7614b368acf6b424a9c27d3d7e9fce56c5211c0fa4a30d1fdbd</code> |
| Runtime side | <code>client</code> |
| Runtime types | 12 |
| Documented type members | 160 |
| Callback signatures | 39 |
| Top-level API overloads | 31 |
| Engine declarative GUI formats | 0 |
| Focused native runtime tests | 0 |

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Types](types.md) | 12 | Object hierarchy, documented members, and callbacks. |
| [Screen API](screen-api.md) | 31 | Registration, stack, focus, lookup, and drag/drop callables. |
| [Lifecycle](lifecycle.md) | 6 | Creation, show/hide, cursor, and refresh behavior. |
| [Layout](layout-rendering.md) | 7 | Coordinates, docking, anchors, crop, frames, scroll, and grids. |
| [Input](input.md) | 7 | Subscriptions, hit order, focus, repeat, drag, and loss. |
| [Integration](integration-validation.md) | 11 | Embedding-project ownership and validation gates. |

## Boundary

Included:

- Gui.fos object types, documented members, callbacks, enums, setting, events, and top-level screen API
- screen registration, creation, stacking, focus, modal, cursor, drag-and-drop, and lifecycle behavior
- anchor, dock, crop, 9-slice, text, grid, item-view, and scroll behavior implemented by the reusable runtime
- Input.fos mouse and keyboard state, dispatch, repeat, and input-loss behavior
- the project hooks required to drive the runtime from client lifecycle and render events

Excluded:

- .fogui, .foguischeme, XML, JSON, or any other declarative GUI authoring format
- project-owned GUI generators, visual editors, generated screen catalogs, layouts, styles, and widget libraries
- project screen identifiers beyond the Engine-owned GuiScreen::None sentinel
- project font bindings, image catalogs, localization policy, input actions, gameplay presentation, and accessibility acceptance
- Dear ImGui developer tooling, server host windows, native application chrome, and touch-to-GUI adaptation
