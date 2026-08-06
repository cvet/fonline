---
title: GUI Layout And Rendering
document_id: generated-gui-runtime-layout-rendering
locale: en
generated: true
---

# GUI Layout And Rendering

> Generated reference. Do not edit directly. Update `BuildTools/GuiRuntimeInterface.json`, then run `python BuildTools/docs_gui_runtime.py --write`.

[Index](index.md) | [Types](types.md) | [Screen API](screen-api.md) | [Lifecycle](lifecycle.md) | [Layout](layout-rendering.md) | [Input](input.md) | [Integration](integration-validation.md) | [Canonical JSON](../../../generated/gui-runtime.json) | [Guide](../../how-to/runtime/gui.md)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-gui-runtime-layout-parent-coordinate-space-1561c8707a"></a><code>gui-runtime.layout.parent-coordinate-space</code> | Parent coordinate space | Interpret authored positions and sizes relative to the parent; root objects use Settings.View.ScreenWidth and ScreenHeight as the live parent size. | The same layout algorithm serves nested widgets and viewport-level screens. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-layout-dock-precedence-890c16c1d4"></a><code>gui-runtime.layout.dock-precedence</code> | Dock precedence | When Dock is not None, use the selected Left, Right, Top, Bottom, or Fill rule and ignore Anchor positioning. | Dock and Anchor are alternative layout modes rather than cumulative constraints. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-layout-anchor-center-dba70a3727"></a><code>gui-runtime.layout.anchor-center</code> | Anchor and centering | Anchor selected axes to parent edges; center an axis with no matching edge bit by half the live-size minus authored-base-size delta. | Unanchored controls remain centered when a parent or viewport changes size. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-layout-crop-draw-hit-6d2ea215aa"></a><code>gui-runtime.layout.crop-draw-hit</code> | Crop affects drawing and hit testing | When Panel crop is enabled, scissor descendant drawing and reject hit tests outside the panel rectangle. | Visual clipping and interaction clipping must agree. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-layout-frame-nine-slice-0f219f68d0"></a><code>gui-runtime.layout.frame-nine-slice</code> | 9-slice frame | Use SetFrameImage plus nonnegative cap insets measured in source pixels; the runtime clamps source and destination caps and stretches edges and center. | Frame images can resize without distorting corners or producing invalid UV ranges. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-layout-scroll-animation-a0a9fcb831"></a><code>gui-runtime.layout.scroll-animation</code> | Panel scrolling | Enable vertical and/or horizontal auto-scroll explicitly; target changes animate for PanelScrollAnimationDurationMs and drawing refreshes active tweens. | Scroll values are stateful layout offsets rather than a renderer-only transform. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
| <a id="entry-gui-runtime-layout-grid-prototype-9d52c1e6bf"></a><code>gui-runtime.layout.grid-prototype</code> | Grid prototype cloning | Point Grid at a named object prototype, choose a positive column count, and let ResizeGrid clone, index, initialize, and position the cells. | The prototype remains hidden while clones become the live repeated content. | [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos) |
