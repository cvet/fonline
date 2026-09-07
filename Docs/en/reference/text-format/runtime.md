---
title: Runtime Text API
document_id: generated-text-format-runtime
locale: en
generated: true
---

# Runtime Text API

> Generated reference. Do not edit directly. Update `BuildTools/TextFormatInterface.json`, then run `python BuildTools/docs_text_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Languages](languages.md) | [Prototype text](proto-text.md) | [Runtime](runtime.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/text-format.json) | [Guide](../../how-to/content/text-and-localization.md)

The generated API reference owns the exact exported signatures. This page explains selection, missing-data behavior, and side availability.

## Script methods

| Stable ID | Signature | Sides | Behavior | Missing or invalid input | Source |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-text-format-runtime-get-language-3fe5c8a9cf"></a><code>text-format.runtime.get-language</code> | <code>LanguageName Game.GetLanguage()</code> | <code>server</code>, <code>client</code>, <code>mapper</code> | Returns the current Engine Language setting as a strong LanguageName wrapper. | Not applicable. | [Source/Scripting/CommonGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/CommonGlobalScriptMethods.cpp) |
| <a id="entry-text-format-runtime-get-current-text-5e96e49ab2"></a><code>text-format.runtime.get-current-text</code> | <code>string Game.GetText(TextPackKey textKey)</code> | <code>client</code>, <code>mapper</code> | Returns the first variant from the current client language. | A missing key returns an empty string. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-text-format-runtime-get-indexed-text-1c9e1cc0c1"></a><code>text-format.runtime.get-indexed-text</code> | <code>string Game.GetText(TextPackKey textKey, int32 textIndex)</code> | <code>client</code>, <code>mapper</code> | Returns the zero-based variant selected by textIndex from the current client language. | A missing key or out-of-range index returns an empty string; a negative index throws. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-text-format-runtime-get-language-text-eeccc868d4"></a><code>text-format.runtime.get-language-text</code> | <code>string Game.GetText(string langName, TextPackKey textKey)</code> | <code>client</code>, <code>mapper</code> | An empty langName or the current language uses the current pack; another langName loads or reuses a cached pack and returns its first variant. | An absent or unsupported non-empty language has no automatic fallback and therefore yields an empty string for the key. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp), [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp) |
| <a id="entry-text-format-runtime-get-count-ce8c833d65"></a><code>text-format.runtime.get-count</code> | <code>int32 Game.GetTextCount(TextPackKey textKey)</code> | <code>server</code>, <code>client</code>, <code>mapper</code> | Returns the number of variants stored under the complete key. | Returns zero when the key is absent. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp), [Source/Scripting/ServerGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ServerGlobalScriptMethods.cpp) |
| <a id="entry-text-format-runtime-is-present-3705f3473d"></a><code>text-format.runtime.is-present</code> | <code>bool Game.IsTextPresent(TextPackKey textKey)</code> | <code>server</code>, <code>client</code>, <code>mapper</code> | Reports whether at least one variant exists under the complete key. | Returns false when the key is absent. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp), [Source/Scripting/ServerGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ServerGlobalScriptMethods.cpp) |
| <a id="entry-text-format-runtime-change-language-f4b573ddec"></a><code>text-format.runtime.change-language</code> | <code>void Game.ChangeLanguage(string langName)</code> | <code>client</code>, <code>mapper</code> | Loads langName into the current client pack and writes the Language setting. | The Engine does not validate the identifier, apply another fallback, or invoke a project GUI refresh callback. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp), [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp) |
| <a id="entry-text-format-runtime-server-boundary-2b4580120e"></a><code>text-format.runtime.server-boundary</code> | <code>server: IsTextPresent and GetTextCount only</code> | <code>server</code> | The server loads one pack for Settings.Language and exposes presence and count queries. | There is no server script Game.GetText overload in the Engine API. | [Source/Server/Server.cpp](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp), [Source/Scripting/ServerGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ServerGlobalScriptMethods.cpp) |

## Renderer-owned inline tags

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-text-format-rendering-inline-color-bd3b5426c1"></a><code>text-format.rendering.inline-color</code> | Inline color tags | The client font renderer recognizes @color:HEX@ to push a six- or eight-hex-digit color and @color@ to restore the previous color. | Color tags are renderer syntax, independent from text-pack parsing and project lexem formatting. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-text-format-rendering-no-colorize-a6ac56231f"></a><code>text-format.rendering.no-colorize</code> | NoColorize | FontFlag.NoColorize strips valid inline color tags while rendering all glyphs with the base color. | The formatting pass always removes recognized tags, but writes per-glyph colors only when colorization is enabled. | [Source/Client/FontManager.h](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h), [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |

The Engine does not interpret game lexems such as player-name, gender, argument, nested-text, or random-choice tags. An embedding project that adds them owns their grammar, tests, diagnostics, and ordering relative to renderer color tags.
