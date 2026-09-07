---
title: GUI Screen Lifecycle
document_id: generated-gui-runtime-lifecycle
locale: en
generated: true
---

# GUI Screen Lifecycle

> Generated reference. Do not edit directly. Update `BuildTools/GuiRuntimeInterface.json`, then run `python BuildTools/docs_gui_runtime.py --write`.

[Index](index.md) | [Types](types.md) | [Screen API](screen-api.md) | [Lifecycle](lifecycle.md) | [Layout](layout-rendering.md) | [Input](input.md) | [Integration](integration-validation.md) | [Canonical JSON](../../../generated/gui-runtime.json) | [Guide](../../how-to/runtime/gui.md)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-gui-runtime-lifecycle-external-initializer-5e3809e11a"></a><code>gui-runtime.lifecycle.external-initializer</code> | Project screen initializer | Provide namespace GuiScreens with void InitializeScreens(); EngineCallback_Start calls it and cannot compile or start without that project contract. | The reusable runtime owns screen mechanics but intentionally does not own a game screen catalog. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos), [Docs/en/tutorials/first-project.md](https://github.com/cvet/fonline/blob/master/Docs/en/tutorials/first-project.md) |
| <a id="entry-gui-runtime-lifecycle-registration-precache-3c852bd828"></a><code>gui-runtime.lifecycle.registration-precache</code> | Registration and precache | Register each non-None GuiScreen id with a factory; registration replaces the old entry and constructs one screen immediately for initialization and resource precache. | Registration is executable client startup work, not passive metadata loading. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-lifecycle-single-vs-multiinstance-eb5db96192"></a><code>gui-runtime.lifecycle.single-vs-multiinstance</code> | Single and multiinstance ownership | Reuse a registered single-instance screen; create a fresh screen for each multiinstance show and remove that instance after hide. | The flag changes construction, persistence, lookup, and teardown semantics. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-lifecycle-show-hide-order-31b3cee1f9"></a><code>gui-runtime.lifecycle.show-hide-order</code> | Show and hide callback order | Treat Show as active-state setup, OnShow traversal, then OnAppear traversal; treat Hide as OnDisappear, OnHide traversal, inactive state, event fire, and optional multiinstance removal. | Project callbacks often distinguish data refresh from top-of-stack visibility. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-lifecycle-cursor-screen-f296275b71"></a><code>gui-runtime.lifecycle.cursor-screen</code> | Cursor screen ownership | Register GuiScreen::Cursor as the dedicated always-active CursorScreen and draw it explicitly through EngineCallback_DrawCursor. | The cursor is excluded from the ordinary screen stack and ordinary screen draw pass. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-lifecycle-explicit-refresh-4f661ee276"></a><code>gui-runtime.lifecycle.explicit-refresh</code> | Explicit resolution and language refresh | Call Callback_OnResolutionChanged after viewport changes and Callback_OnLanguageChanged after language changes. | The reusable runtime does not subscribe these project lifecycle transitions itself. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
