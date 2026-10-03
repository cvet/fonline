---
title: Font Resource Formats
document_id: generated-font-format-formats
locale: en
generated: true
---

# Font Resource Formats

> Generated reference. Do not edit directly. Update `BuildTools/FontFormatInterface.json`, then run `python BuildTools/docs_font_format.py --write`.

[Index](index.md) | [Formats](formats.md) | [FOFNT](fofnt.md) | [BMFont](bmfont.md) | [Binding](binding.md) | [Layout](layout.md) | [Rendering](rendering.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/font-format.json) | [Guide](../../how-to/content/font-format.md)

| Stable ID | Suffix | Role | Contract | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-format-fofnt-2438a61036"></a><code>font-format.format.fofnt</code> | <code>.fofnt</code> | Runtime descriptor | Use the Engine text descriptor when authoring explicit image, line, and per-codepoint glyph metrics. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-font-format-format-bmfont-binary-v3-a2f528951c"></a><code>font-format.format.bmfont-binary-v3</code> | <code>.fnt</code> | Runtime descriptor | Export BMFont binary version 3 with one texture page and one-pixel padding on every side of each glyph. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-format-bmfc-sidecar-6a547a8922"></a><code>font-format.format.bmfc-sidecar</code> | <code>.bmfc</code> | Raw-copied authoring sidecar; not a runtime descriptor | Treat .bmfc as an optional BMFont tool configuration file and never pass it to Game.BindFont. | [Source/Common/Settings.inc](https://github.com/cvet/fonline/blob/master/Source/Common/Settings.inc), [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |

## Bundled descriptors

| Suffix | Files |
| --- | --- |
| <code>.fofnt</code> | <code>Big.fofnt</code>, <code>BigNumbers.fofnt</code>, <code>Default.fofnt</code>, <code>Fallout.fofnt</code>, <code>Fat.fofnt</code>, <code>Numbers.fofnt</code>, <code>OldDefault.fofnt</code>, <code>SandNumbers.fofnt</code>, <code>Special.fofnt</code>, <code>Thin.fofnt</code> |
| <code>.fnt</code> | <code>CourierNewSmall.fnt</code>, <code>DefaultExt.fnt</code> |
| <code>.bmfc</code> | <code>CourierNewSmall.bmfc</code>, <code>Settings.bmfc</code> |
