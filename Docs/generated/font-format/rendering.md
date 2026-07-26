---
title: Font Rendering Contract
document_id: generated-font-format-rendering
locale: en
generated: true
---

# Font Rendering Contract

> Generated reference. Do not edit directly. Update `BuildTools/FontFormatInterface.json`, then run `python BuildTools/docs_font_format.py --write`.

[Index](index.md) | [Formats](formats.md) | [FOFNT](fofnt.md) | [BMFont](bmfont.md) | [Binding](binding.md) | [Layout](layout.md) | [Rendering](rendering.md) | [Validation](validation.md) | [Canonical JSON](../font-format.json) | [Guide](../../FontFormat.md)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-rendering-one-pixel-sampling-border-c99645ca3f"></a><code>font-format.rendering.one-pixel-sampling-border</code> | One-pixel sampling border | Leave at least one pixel of valid transparent padding around every glyph rectangle and around the image edge. | Texture coordinates and submitted quads expand each visible glyph by one pixel on all sides. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-rendering-grayscale-tint-2eb10fd758"></a><code>font-format.rendering.grayscale-tint</code> | Grayscale normalization and tint | Use a trailing * on FOFNT Image, or binary BMFont, when the bitmap should be normalized to middle gray and tinted by draw color. | Nontransparent RGB becomes 128/128/128 while alpha is preserved; transparent pixels are cleared. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-rendering-bordered-copy-891143d814"></a><code>font-format.rendering.bordered-copy</code> | Bordered atlas copy | Reserve transparent padding for a one-pixel black dilation; FontFlag::Bordered selects the generated second texture. | The loader duplicates the image and fills transparent neighbors of visible pixels before calculating bordered UVs. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-rendering-bind-time-resampling-2ec561fa06"></a><code>font-format.rendering.bind-time-resampling</code> | Area-average downsampling | Expect defaultScale below one to rewrite the bound atlas region and integer glyph metrics once, with no per-widget font scale. | The scaler uses alpha-weighted area averages, clears the original glyph rectangle, and keeps the same top-left position. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-rendering-font-effect-fdd82b908c"></a><code>font-format.rendering.font-effect</code> | Shared and per-slot font effect | Fonts start with the Engine shared font effect; a per-slot EffectType::Font override replaces it and a null override returns to the shared effect. | Draw batching keys include the selected texture and RenderEffect pointer. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp), [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp) |
| <a id="entry-font-format-rendering-inline-color-c56430c987"></a><code>font-format.rendering.inline-color</code> | Inline color tags | Use @color:BBGGRR@ or @color:AABBGGRR@, optionally with a 0x prefix, to push a color and @color@ to restore the previous color; NoColorize strips valid tags without applying them. | Formatting removes valid markers before wrapping and records color transitions by output byte offset. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-rendering-layout-cache-26a6758f8a"></a><code>font-format.rendering.layout-cache</code> | Three-frame layout cache | Do not depend on cached layout identity or lifetime; the cache key includes text, font, flags, skips, rectangle size, color, and mode and expires after three unused frames. | The cache is a client implementation detail and is cleared whenever fonts are stored or cleared. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
