---
title: Font Binding Contract
document_id: generated-font-format-binding
locale: en
generated: true
---

# Font Binding Contract

> Generated reference. Do not edit directly. Update `BuildTools/FontFormatInterface.json`, then run `python BuildTools/docs_font_format.py --write`.

[Index](index.md) | [Formats](formats.md) | [FOFNT](fofnt.md) | [BMFont](bmfont.md) | [Binding](binding.md) | [Layout](layout.md) | [Rendering](rendering.md) | [Validation](validation.md) | [Canonical JSON](../font-format.json) | [Guide](../../FontFormat.md)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-binding-extension-dispatch-e6553c421b"></a><code>font-format.binding.extension-dispatch</code> | Exact extension dispatch | Call Game.BindFont with an exact lowercase .fofnt or .fnt path; every other suffix throws a script exception. | Dispatch uses case-sensitive ends_with checks and does not sniff descriptor content. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-font-format-binding-raw-copy-2eb2c5df1d"></a><code>font-format.binding.raw-copy</code> | Descriptor raw-copy boundary | Keep fofnt and fnt in Baking.RawCopyFileExtensions so descriptor bytes reach baked resources unchanged. | There is no dedicated font descriptor baker; RawCopyBaker preserves path and bytes. | [Source/Common/Settings.inc](https://github.com/cvet/fonline/blob/master/Source/Common/Settings.inc), [Source/Tools/RawCopyBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/RawCopyBaker.cpp) |
| <a id="entry-font-format-binding-image-resource-6b1fe7695b"></a><code>font-format.binding.image-resource</code> | Separately baked image | Ship the referenced image as an independently supported image resource in the same pack and preserve its relative path. | The descriptor is raw-copied, but its image is loaded through SpriteManager and the normal image-format pipeline. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-binding-font-slots-3d6d926638"></a><code>font-format.binding.font-slots</code> | Project-extensible font slots | Bind every FontType slot before it is measured or drawn; the Engine names only Default = 0 and embedding scripts may extend the enum through codegen annotations. | FontType is an indexed loaded-font table, and unloaded or out-of-range slots throw. | [Source/Client/FontManager.h](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h) |
| <a id="entry-font-format-binding-iface-atlas-c418ad6baf"></a><code>font-format.binding.iface-atlas</code> | Interface sprite atlas | Script-bound fonts allocate their normal and optional bordered textures in AtlasType::IfaceSprites. | Both Game.BindFont descriptor branches pass the same interface atlas type. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-font-format-binding-bind-time-scale-d2935e9b3d"></a><code>font-format.binding.bind-time-scale</code> | Bind-time downscale | Pass a finite defaultScale in (0, 1]; use a larger authored bitmap and downscale it rather than requesting runtime upscaling. | Binding area-averages glyph pixels and rounds metrics once; values above one and nonpositive or nonfinite values are rejected. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-binding-rebind-4b433dbb8a"></a><code>font-format.binding.rebind</code> | Slot replacement and cache reset | Treat a repeated binding of the same slot as replacement; all cached layouts are discarded before the rebuilt font is used. | StoreFont replaces the optional table entry, rebuilds atlas data, and clears the format cache. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-binding-updater-default-2eff9266bd"></a><code>font-format.binding.updater-default</code> | Updater fallback font | Keep Fonts/Default.fofnt available for the built-in updater path unless the host replaces that resource contract deliberately. | Updater binds the Default slot from that exact path with skip-if-loaded behavior. | [Source/Client/Updater.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Updater.cpp) |
