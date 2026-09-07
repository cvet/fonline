---
title: GUI Integration And Validation
document_id: generated-gui-runtime-integration-validation
locale: en
generated: true
---

# GUI Integration And Validation

> Generated reference. Do not edit directly. Update `BuildTools/GuiRuntimeInterface.json`, then run `python BuildTools/docs_gui_runtime.py --write`.

[Index](index.md) | [Types](types.md) | [Screen API](screen-api.md) | [Lifecycle](lifecycle.md) | [Layout](layout-rendering.md) | [Input](input.md) | [Integration](integration-validation.md) | [Canonical JSON](../../../generated/gui-runtime.json) | [Guide](../../how-to/runtime/gui.md)

## Integration rules

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-gui-runtime-integration-no-declarative-parser-04472fe1a3"></a><code>gui-runtime.integration.no-declarative-parser</code> | No Engine declarative GUI format | Treat Gui.fos and Input.fos as an AngelScript runtime API; do not claim that Engine parses .fogui, .foguischeme, or another layout format. | Declarative sources and generators belong to embedding projects or a separately versioned companion tool. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos), [Docs/en/tutorials/first-project.md](https://github.com/cvet/fonline/blob/master/Docs/en/tutorials/first-project.md) |
| <a id="entry-gui-runtime-integration-client-hook-bridge-408d0ce04a"></a><code>gui-runtime.integration.client-hook-bridge</code> | Client hook bridge | Call EngineCallback_Start, EngineCallback_Loop, EngineCallback_Draw, and optionally EngineCallback_DrawCursor from the embedding client's corresponding events. | The runtime implements these hooks but does not subscribe project render/lifecycle events for itself. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-integration-render-scope-1f4e127e33"></a><code>gui-runtime.integration.render-scope</code> | Script render scope | Draw GUI only from the client OnRenderIface scope where script drawing is enabled; place the cursor pass at the project-selected top layer. | Native ClientEngine enables script drawing around OnRenderIface and processes video before ending the scene. | [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp) |
| <a id="entry-gui-runtime-integration-font-image-dependencies-b5f29233a2"></a><code>gui-runtime.integration.font-image-dependencies</code> | Font and image dependencies | Bind every FontType used by Text nodes and deliver every sprite referenced by Panel or Button before showing affected screens. | The GUI runtime delegates text layout and sprite loading/drawing to the reusable font and image systems. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-integration-project-enum-extension-187b9ea6aa"></a><code>gui-runtime.integration.project-enum-extension</code> | Project enum extension | Extend GuiScreen and CursorType through project metadata without redefining the Engine sentinels. | Engine supplies reusable baseline values while each game owns its screen and cursor catalog. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-integration-touch-boundary-9cf9415fbe"></a><code>gui-runtime.integration.touch-boundary</code> | Touch adaptation boundary | Do not assume Input.fos routes native touch events into GUI mouse callbacks; provide and validate project/platform adaptation when touch interaction is required. | ClientEngine emits separate touch events while the reusable Input module subscribes only mouse, key, and input-loss events. | [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp), [Source/Scripting/AngelScript/CoreScripts/Input.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Input.fos) |

## Validation rules

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-gui-runtime-validation-generated-contract-2038572ded"></a><code>gui-runtime.validation.generated-contract</code> | Generated runtime contract | Regenerate and check gui-runtime.json and its reference pages whenever Gui.fos, Input.fos, client input/render dispatch, or this interface manifest changes. | The model fails on class/API/callback/annotation/timing/source drift before prose can silently become stale. | [BuildTools/GuiRuntimeInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/GuiRuntimeInterface.json) |
| <a id="entry-gui-runtime-validation-client-compilation-42203652a1"></a><code>gui-runtime.validation.client-compilation</code> | Embedding client compilation | Compile and bake an embedding client that supplies project enum values, GuiScreens::InitializeScreens, screen factories, and lifecycle/render hook calls. | Engine-only source checks cannot prove the required cross-module AngelScript symbols or generated screen code. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-validation-visible-layout-input-750a995a35"></a><code>gui-runtime.validation.visible-layout-input</code> | Visible layout and input | Validate representative screens at every supported resolution and input profile, including anchors, docks, crop, 9-slice, focus, close-on-miss, modal ordering, repeat, drag, and input loss. | Compilation and headless execution do not prove geometry, draw order, alpha hit testing, clipping, or user interaction. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-validation-language-font-fac44a9019"></a><code>gui-runtime.validation.language-font</code> | Language and font acceptance | Switch every shipped language in a visible client, run Callback_OnLanguageChanged, and inspect wrapping, clipping, glyph coverage, focus colors, text input, and message feeds. | The GUI runtime refreshes text callbacks and geometry but cannot validate project translations or bound font coverage. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-validation-fixture-gap-26f30e9d86"></a><code>gui-runtime.validation.fixture-gap</code> | Focused fixture gap | Keep the surface experimental until Engine has a self-contained client fixture that constructs screens, drives input, renders pixels, changes resolution/language, and proves teardown. | Current native tests do not directly execute the CoreScripts GUI runtime. | [Source/Tests/README.md](https://github.com/cvet/fonline/blob/master/Source/Tests/README.md) |

## Validation commands

```powershell
python BuildTools\docs_gui_runtime.py --check
python -m unittest BuildTools.tests.test_docs_gui_runtime
python BuildTools\docs_validate.py
```

These checks prove source/model/reference consistency. They do not replace compiling an embedding client or visibly testing layout, draw order, input, language, fonts, and teardown.
