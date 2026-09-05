//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "Common.h"

#include "EngineBase.h"
#include "ImGuiStuff.h"

FO_BEGIN_NAMESPACE

static auto PrepareInputBuffer(string_view text, uint32_t max_length) -> vector<char>
{
    FO_STACK_TRACE_ENTRY();

    if (text.size() > numeric_cast<size_t>(max_length)) {
        throw ScriptException("Text arg length must be less or equal to maxLength arg");
    }

    auto initial_len = std::max(numeric_cast<size_t>(max_length), text.size());
    vector<char> buffer(initial_len + 1);

    if (!text.empty()) {
        std::copy_n(text.data(), text.size(), buffer.data());
    }

    buffer[text.size()] = '\0';
    return buffer;
}

static auto ToImVec2(fpos32 pos) -> ImVec2
{
    FO_STACK_TRACE_ENTRY();

    return {pos.x, pos.y};
}

static auto ToImVec2(fsize32 size) -> ImVec2
{
    FO_STACK_TRACE_ENTRY();

    return {size.width, size.height};
}

static auto ToImVec2(isize32 size) -> ImVec2
{
    FO_STACK_TRACE_ENTRY();

    return {numeric_cast<float32_t>(size.width), numeric_cast<float32_t>(size.height)};
}

static auto ToFPos32(const ImVec2& pos) -> fpos32
{
    FO_STACK_TRACE_ENTRY();

    return {pos.x, pos.y};
}

static auto ToFSize32(const ImVec2& size) -> fsize32
{
    FO_STACK_TRACE_ENTRY();

    return {size.x, size.y};
}

static auto ToColorComp(float32_t value) -> uint8_t
{
    FO_STACK_TRACE_ENTRY();

    return numeric_cast<uint8_t>(std::clamp(iround<int32_t>(value * 255.0f), 0, 255));
}

static void ColorToFloat3(ucolor color, float32_t (&values)[3])
{
    FO_STACK_TRACE_ENTRY();

    values[0] = numeric_cast<float32_t>(color.comp.r) / 255.0f;
    values[1] = numeric_cast<float32_t>(color.comp.g) / 255.0f;
    values[2] = numeric_cast<float32_t>(color.comp.b) / 255.0f;
}

static void ColorToFloat4(ucolor color, float32_t (&values)[4])
{
    FO_STACK_TRACE_ENTRY();

    values[0] = numeric_cast<float32_t>(color.comp.r) / 255.0f;
    values[1] = numeric_cast<float32_t>(color.comp.g) / 255.0f;
    values[2] = numeric_cast<float32_t>(color.comp.b) / 255.0f;
    values[3] = numeric_cast<float32_t>(color.comp.a) / 255.0f;
}

static void StoreColor3(ucolor& color, const float32_t (&values)[3])
{
    FO_STACK_TRACE_ENTRY();

    color = ucolor(ToColorComp(values[0]), ToColorComp(values[1]), ToColorComp(values[2]), color.comp.a);
}

static void StoreColor4(ucolor& color, const float32_t (&values)[4])
{
    FO_STACK_TRACE_ENTRY();

    color = ucolor(ToColorComp(values[0]), ToColorComp(values[1]), ToColorComp(values[2]), ToColorComp(values[3]));
}

// Returns the Engine ImGui script facade during an active ImGui frame; throws when no context or frame scope is available.
///@ ExportMethod GlobalGetter
FO_SCRIPT_API ptr<ScriptImGui> Common_Game_ImGui(ptr<BaseEngine> engine)
{
    auto imgui_context = ImGui::GetCurrentContext();

    if (!imgui_context) {
        throw ScriptException("ImGui context is not available");
    }

    if (!imgui_context->WithinFrameScope) {
        throw ScriptException("You can use this function only in active ImGui frame");
    }

    return engine->GetImGui();
}

// Returns whether an ImGui context exists and is currently inside an active frame scope.
///@ ExportMethod
FO_SCRIPT_API bool Common_Game_IsImGuiAvailable(ptr<BaseEngine> engine)
{
    ignore_unused(engine);

    auto imgui_context = ImGui::GetCurrentContext();
    return imgui_context && imgui_context->WithinFrameScope;
}

// Begins a required nonempty-label window and returns whether its contents should be submitted; always pair the call with End, even when it returns false.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_Begin([[maybe_unused]] ptr<ScriptImGui> self, string_view label, ImGui_WindowFlags flags = ImGui_WindowFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Window label arg is empty");
    }

    return ImGui::Begin(string(label).c_str(), nullptr, static_cast<ImGuiWindowFlags>(flags));
}

// Begins a required nonempty-label closable window, updates opened when the close control is used, and returns whether to submit contents; always pair with End.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_Begin([[maybe_unused]] ptr<ScriptImGui> self, string_view label, bool& opened, ImGui_WindowFlags flags = ImGui_WindowFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Window label arg is empty");
    }

    return ImGui::Begin(string(label).c_str(), &opened, static_cast<ImGuiWindowFlags>(flags));
}

// Ends the current window begun by Begin; this is required regardless of Begin's return value.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_End([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::End();
}

// ReSharper disable once CppInconsistentNaming
// Pushes a required nonempty string component onto the ImGui ID stack.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PushID([[maybe_unused]] ptr<ScriptImGui> self, string_view strId)
{
    if (strId.empty()) {
        throw ScriptException("Id arg is empty");
    }

    ImGui::PushID(string(strId).c_str());
}

// ReSharper disable once CppInconsistentNaming
// Pushes an integer component onto the ImGui ID stack.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PushID([[maybe_unused]] ptr<ScriptImGui> self, int32_t intId)
{
    ImGui::PushID(intId);
}

// ReSharper disable once CppInconsistentNaming
// Pops the most recently pushed component from the ImGui ID stack.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PopID([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::PopID();
}

// Pushes an RGBA override for the selected ImGui style color; restore it with PopStyleColor.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PushStyleColor([[maybe_unused]] ptr<ScriptImGui> self, ImGui_Col colorId, float32_t r, float32_t g, float32_t b, float32_t a)
{
    ImGui::PushStyleColor(static_cast<ImGuiCol>(colorId), ImVec4(r, g, b, a));
}

// Pops count entries from the ImGui style-color stack.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PopStyleColor([[maybe_unused]] ptr<ScriptImGui> self, int32_t count = 1)
{
    ImGui::PopStyleColor(count);
}

// Pushes a scalar override for a scalar-valued ImGui style variable; restore it with PopStyleVar.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PushStyleVar([[maybe_unused]] ptr<ScriptImGui> self, ImGui_StyleVar styleVar, float32_t value)
{
    ImGui::PushStyleVar(static_cast<ImGuiStyleVar>(styleVar), value);
}

// Pushes a two-component override for a vector-valued ImGui style variable; restore it with PopStyleVar.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PushStyleVarVec2([[maybe_unused]] ptr<ScriptImGui> self, ImGui_StyleVar styleVar, float32_t x, float32_t y)
{
    ImGui::PushStyleVar(static_cast<ImGuiStyleVar>(styleVar), ImVec2(x, y));
}

// Pops count entries from the ImGui style-variable stack.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PopStyleVar([[maybe_unused]] ptr<ScriptImGui> self, int32_t count = 1)
{
    ImGui::PopStyleVar(count);
}

// Sets the next window's screen position subject to the supplied ImGui condition.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowPos([[maybe_unused]] ptr<ScriptImGui> self, ipos32 pos, ImGui_Cond cond = ImGui_Cond::None)
{
    ImGui::SetNextWindowPos(ImVec2(numeric_cast<float32_t>(pos.x), numeric_cast<float32_t>(pos.y)), static_cast<ImGuiCond>(cond));
}

// Sets the next window's size subject to the supplied ImGui condition.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowSize([[maybe_unused]] ptr<ScriptImGui> self, isize32 size, ImGui_Cond cond = ImGui_Cond::None)
{
    ImGui::SetNextWindowSize(ImVec2(numeric_cast<float32_t>(size.width), numeric_cast<float32_t>(size.height)), static_cast<ImGuiCond>(cond));
}

// Sets whether the next window is collapsed subject to the supplied ImGui condition.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowCollapsed([[maybe_unused]] ptr<ScriptImGui> self, bool collapsed, ImGui_Cond cond = ImGui_Cond::None)
{
    ImGui::SetNextWindowCollapsed(collapsed, static_cast<ImGuiCond>(cond));
}

// Constrains the next window's size between the supplied minimum and maximum dimensions.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowSizeConstraints([[maybe_unused]] ptr<ScriptImGui> self, isize32 sizeMin, isize32 sizeMax)
{
    ImGui::SetNextWindowSizeConstraints(ImVec2(numeric_cast<float32_t>(sizeMin.width), numeric_cast<float32_t>(sizeMin.height)), ImVec2(numeric_cast<float32_t>(sizeMax.width), numeric_cast<float32_t>(sizeMax.height)));
}

// Sets the explicit content size of the next window, controlling its scrollable extent independently of the window size.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowContentSize([[maybe_unused]] ptr<ScriptImGui> self, isize32 size)
{
    ImGui::SetNextWindowContentSize(ImVec2(numeric_cast<float32_t>(size.width), numeric_cast<float32_t>(size.height)));
}

// Requests focus for the next window begun in the current frame.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowFocus([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::SetNextWindowFocus();
}

// Sets the next window's initial scroll coordinates; negative components leave the corresponding axis unchanged.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowScroll([[maybe_unused]] ptr<ScriptImGui> self, fpos32 scroll)
{
    ImGui::SetNextWindowScroll(ToImVec2(scroll));
}

// Overrides the background alpha of the next window.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowBgAlpha([[maybe_unused]] ptr<ScriptImGui> self, float32_t alpha)
{
    ImGui::SetNextWindowBgAlpha(alpha);
}

// Renders text verbatim without interpreting it as a printf-style format string.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Text([[maybe_unused]] ptr<ScriptImGui> self, string_view text)
{
    ImGuiTextUnformatted(text);
}

// Renders text using ImGui's disabled-text style.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TextDisabled([[maybe_unused]] ptr<ScriptImGui> self, string_view text)
{
    ImGui::TextDisabled("%s", string(text).c_str());
}

// Renders text with wrapping at the current content-region boundary.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TextWrapped([[maybe_unused]] ptr<ScriptImGui> self, string_view text)
{
    ImGui::TextWrapped("%s", string(text).c_str());
}

// Measures text using the current font, optionally hiding the ID suffix after ## and wrapping at the supplied width.
///@ ExportMethod
FO_SCRIPT_API fsize32 Common_ImGui_CalcTextSize([[maybe_unused]] ptr<ScriptImGui> self, string_view text, bool hideTextAfterDoubleHash = false, float32_t wrapWidth = -1.0f)
{
    string source = string(text);
    return ToFSize32(ImGui::CalcTextSize(source.c_str(), nullptr, hideTextAfterDoubleHash, wrapWidth));
}

// Advances the current line baseline so subsequent text aligns vertically with framed widgets.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_AlignTextToFramePadding([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::AlignTextToFramePadding();
}

// Returns elapsed ImGui time in seconds since context initialization.
///@ ExportMethod
FO_SCRIPT_API float64_t Common_ImGui_GetTime([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::GetTime();
}

// Returns the ImGui frame counter narrowed to int32.
///@ ExportMethod
FO_SCRIPT_API int32_t Common_ImGui_GetFrameCount([[maybe_unused]] ptr<ScriptImGui> self)
{
    return numeric_cast<int32_t>(ImGui::GetFrameCount());
}

// Returns the current font's text-line height without inter-line spacing.
///@ ExportMethod
FO_SCRIPT_API float32_t Common_ImGui_GetTextLineHeight([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::GetTextLineHeight();
}

// Returns the current font's text-line height including ImGui item spacing.
///@ ExportMethod
FO_SCRIPT_API float32_t Common_ImGui_GetTextLineHeightWithSpacing([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::GetTextLineHeightWithSpacing();
}

// Returns the standard framed-widget height for the current font and style.
///@ ExportMethod
FO_SCRIPT_API float32_t Common_ImGui_GetFrameHeight([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::GetFrameHeight();
}

// Returns the standard framed-widget height plus vertical item spacing.
///@ ExportMethod
FO_SCRIPT_API float32_t Common_ImGui_GetFrameHeightWithSpacing([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::GetFrameHeightWithSpacing();
}

// Returns whether the current window has just appeared after being hidden, inactive, or newly created.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsWindowAppearing([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::IsWindowAppearing();
}

// Returns the current window's top-left position in screen coordinates.
///@ ExportMethod
FO_SCRIPT_API fpos32 Common_ImGui_GetWindowPos([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ToFPos32(ImGui::GetWindowPos());
}

// Returns the current window's size.
///@ ExportMethod
FO_SCRIPT_API fsize32 Common_ImGui_GetWindowSize([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ToFSize32(ImGui::GetWindowSize());
}

// Returns whether a rectangle of the supplied size at the current cursor position overlaps the current clipping rectangle.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsRectVisible([[maybe_unused]] ptr<ScriptImGui> self, fsize32 size)
{
    return ImGui::IsRectVisible(ToImVec2(size));
}

// Returns whether the supplied screen-space rectangle overlaps the current clipping rectangle.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsRectVisible([[maybe_unused]] ptr<ScriptImGui> self, fpos32 rectMin, fpos32 rectMax)
{
    return ImGui::IsRectVisible(ToImVec2(rectMin), ToImVec2(rectMax));
}

// Returns the current window width.
///@ ExportMethod
FO_SCRIPT_API float32_t Common_ImGui_GetWindowWidth([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::GetWindowWidth();
}

// Returns the current window height.
///@ ExportMethod
FO_SCRIPT_API float32_t Common_ImGui_GetWindowHeight([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::GetWindowHeight();
}

// Returns the current window's horizontal scroll offset.
///@ ExportMethod
FO_SCRIPT_API float32_t Common_ImGui_GetScrollX([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::GetScrollX();
}

// Returns the current window's vertical scroll offset.
///@ ExportMethod
FO_SCRIPT_API float32_t Common_ImGui_GetScrollY([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::GetScrollY();
}

// Sets the current window's horizontal scroll offset.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetScrollX([[maybe_unused]] ptr<ScriptImGui> self, float32_t scrollX)
{
    ImGui::SetScrollX(scrollX);
}

// Sets the current window's vertical scroll offset.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetScrollY([[maybe_unused]] ptr<ScriptImGui> self, float32_t scrollY)
{
    ImGui::SetScrollY(scrollY);
}

// Returns the maximum horizontal scroll offset of the current window.
///@ ExportMethod
FO_SCRIPT_API float32_t Common_ImGui_GetScrollMaxX([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::GetScrollMaxX();
}

// Returns the maximum vertical scroll offset of the current window.
///@ ExportMethod
FO_SCRIPT_API float32_t Common_ImGui_GetScrollMaxY([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::GetScrollMaxY();
}

// Adjusts horizontal scrolling so the current cursor position appears at the requested viewport ratio.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetScrollHereX([[maybe_unused]] ptr<ScriptImGui> self, float32_t centerXRatio = 0.5f)
{
    ImGui::SetScrollHereX(centerXRatio);
}

// Adjusts vertical scrolling so the current cursor position appears at the requested viewport ratio.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetScrollHereY([[maybe_unused]] ptr<ScriptImGui> self, float32_t centerYRatio = 0.5f)
{
    ImGui::SetScrollHereY(centerYRatio);
}

// Adjusts horizontal scrolling so a local X position appears at the requested viewport ratio.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetScrollFromPosX([[maybe_unused]] ptr<ScriptImGui> self, float32_t localX, float32_t centerXRatio = 0.5f)
{
    ImGui::SetScrollFromPosX(localX, centerXRatio);
}

// Adjusts vertical scrolling so a local Y position appears at the requested viewport ratio.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetScrollFromPosY([[maybe_unused]] ptr<ScriptImGui> self, float32_t localY, float32_t centerYRatio = 0.5f)
{
    ImGui::SetScrollFromPosY(localY, centerYRatio);
}

// Returns whether any ImGui item is currently hovered.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsAnyItemHovered([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::IsAnyItemHovered();
}

// Returns whether any ImGui item is currently active.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsAnyItemActive([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::IsAnyItemActive();
}

// Emits a standalone bullet and advances the cursor on the same line for following content.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Bullet([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::Bullet();
}

// Draws a separator carrying a required nonempty label.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SeparatorText([[maybe_unused]] ptr<ScriptImGui> self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Separator label arg is empty");
    }

    ImGui::SeparatorText(string(label).c_str());
}

// Returns the horizontal spacing from a tree-node arrow or bullet to its label.
///@ ExportMethod
FO_SCRIPT_API float32_t Common_ImGui_GetTreeNodeToLabelSpacing([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::GetTreeNodeToLabelSpacing();
}

// Sets the next tree node or collapsing header's open state subject to the supplied ImGui condition.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextItemOpen([[maybe_unused]] ptr<ScriptImGui> self, bool isOpen, ImGui_Cond cond = ImGui_Cond::None)
{
    ImGui::SetNextItemOpen(isOpen, static_cast<ImGuiCond>(cond));
}

// Returns the horizontal space available from the current cursor to the content-region boundary.
///@ ExportMethod
FO_SCRIPT_API float32_t Common_ImGui_GetContentRegionAvailX([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::GetContentRegionAvail().x;
}

// Returns the vertical space available from the current cursor to the content-region boundary.
///@ ExportMethod
FO_SCRIPT_API float32_t Common_ImGui_GetContentRegionAvailY([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::GetContentRegionAvail().y;
}

// Returns the current cursor's local X coordinate within the window.
///@ ExportMethod
FO_SCRIPT_API float32_t Common_ImGui_GetCursorPosX([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::GetCursorPosX();
}

// Returns the current cursor's local Y coordinate within the window.
///@ ExportMethod
FO_SCRIPT_API float32_t Common_ImGui_GetCursorPosY([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::GetCursorPosY();
}

// Returns the current cursor position in screen coordinates.
///@ ExportMethod
FO_SCRIPT_API fpos32 Common_ImGui_GetCursorScreenPos([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ToFPos32(ImGui::GetCursorScreenPos());
}

// Sets the current cursor position in window-local coordinates.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetCursorPos([[maybe_unused]] ptr<ScriptImGui> self, float32_t x, float32_t y)
{
    ImGui::SetCursorPos(ImVec2(x, y));
}

// Sets the current cursor position in screen coordinates.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetCursorScreenPos([[maybe_unused]] ptr<ScriptImGui> self, fpos32 pos)
{
    ImGui::SetCursorScreenPos(ToImVec2(pos));
}

// Pushes a local X coordinate at which subsequent text wraps; zero uses the content-region boundary.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PushTextWrapPos([[maybe_unused]] ptr<ScriptImGui> self, float32_t wrapLocalPosX = 0.0f)
{
    ImGui::PushTextWrapPos(wrapLocalPosX);
}

// Restores the previous text-wrapping position.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PopTextWrapPos([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::PopTextWrapPos();
}

// Requests keyboard focus for a focusable item at the supplied offset relative to the next submitted item.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetKeyboardFocusHere([[maybe_unused]] ptr<ScriptImGui> self, int32_t offset = 0)
{
    ImGui::SetKeyboardFocusHere(offset);
}

// Draws a required nonempty-label button and returns whether it was activated.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_Button([[maybe_unused]] ptr<ScriptImGui> self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Button label arg is empty");
    }

    return ImGui::Button(string(label).c_str());
}

// Places the next item on the current line with optional start offset and spacing overrides.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SameLine([[maybe_unused]] ptr<ScriptImGui> self, float32_t offsetFromStartX = 0.0f, float32_t spacing = -1.0f)
{
    ImGui::SameLine(offsetFromStartX, spacing);
}

// Overrides the width of the next submitted item.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextItemWidth([[maybe_unused]] ptr<ScriptImGui> self, float32_t itemWidth)
{
    ImGui::SetNextItemWidth(itemWidth);
}

// Pushes a default width for subsequent items; restore it with PopItemWidth.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PushItemWidth([[maybe_unused]] ptr<ScriptImGui> self, float32_t itemWidth)
{
    ImGui::PushItemWidth(itemWidth);
}

// Restores the previous default item width.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PopItemWidth([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::PopItemWidth();
}

// Resolves and returns the width ImGui would use for the next item.
///@ ExportMethod
FO_SCRIPT_API float32_t Common_ImGui_CalcItemWidth([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::CalcItemWidth();
}

// Submits an invisible layout item of the supplied size.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Dummy([[maybe_unused]] ptr<ScriptImGui> self, isize32 size)
{
    ImGui::Dummy(ImVec2(numeric_cast<float32_t>(size.width), numeric_cast<float32_t>(size.height)));
}

// Increases the current line's horizontal indentation by the supplied amount or the style default when zero.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Indent([[maybe_unused]] ptr<ScriptImGui> self, float32_t indentW = 0.0f)
{
    ImGui::Indent(indentW);
}

// Decreases the current line's horizontal indentation by the supplied amount or the style default when zero.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Unindent([[maybe_unused]] ptr<ScriptImGui> self, float32_t indentW = 0.0f)
{
    ImGui::Unindent(indentW);
}

// Begins grouping subsequent items into one composite layout and interaction bounding box.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_BeginGroup([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::BeginGroup();
}

// Ends the current group and exposes it as the last submitted item.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndGroup([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::EndGroup();
}

// Draws a required nonempty-label checkbox, updates value on interaction, and returns whether it changed.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_Checkbox([[maybe_unused]] ptr<ScriptImGui> self, string_view label, bool& value)
{
    if (label.empty()) {
        throw ScriptException("Checkbox label arg is empty");
    }

    return ImGui::Checkbox(string(label).c_str(), &value);
}

// Draws a required nonempty-label checkbox that toggles the selected signed flag bits and returns whether flags changed.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_CheckboxFlags([[maybe_unused]] ptr<ScriptImGui> self, string_view label, int32_t& flags, int32_t flagsValue)
{
    if (label.empty()) {
        throw ScriptException("Checkbox label arg is empty");
    }

    return ImGui::CheckboxFlags(string(label).c_str(), &flags, flagsValue);
}

// Draws a required nonempty-label checkbox that toggles the selected unsigned flag bits and returns whether flags changed.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_CheckboxFlags([[maybe_unused]] ptr<ScriptImGui> self, string_view label, uint32_t& flags, uint32_t flagsValue)
{
    if (label.empty()) {
        throw ScriptException("Checkbox label arg is empty");
    }

    return ImGui::CheckboxFlags(string(label).c_str(), &flags, flagsValue);
}

// Draws a required nonempty-label signed integer input, updates value, and returns whether editing changed it.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputInt([[maybe_unused]] ptr<ScriptImGui> self, string_view label, int32_t& value, int32_t step = 1, int32_t stepFast = 100, ImGui_InputTextFlags flags = ImGui_InputTextFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    return ImGui::InputInt(string(label).c_str(), &value, step, stepFast, static_cast<ImGuiInputTextFlags>(flags));
}

// Draws a required nonempty-label two-component integer input and writes both components back only when editing changes them.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputInt2([[maybe_unused]] ptr<ScriptImGui> self, string_view label, int32_t& valueX, int32_t& valueY, ImGui_InputTextFlags flags = ImGui_InputTextFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    int values[2] {valueX, valueY};
    bool changed = ImGui::InputInt2(string(label).c_str(), values, static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

// Draws a required nonempty-label three-component integer input and writes all components back only when editing changes them.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputInt3([[maybe_unused]] ptr<ScriptImGui> self, string_view label, int32_t& valueX, int32_t& valueY, int32_t& valueZ, ImGui_InputTextFlags flags = ImGui_InputTextFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    int values[3] {valueX, valueY, valueZ};
    bool changed = ImGui::InputInt3(string(label).c_str(), values, static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

// Draws a required nonempty-label four-component integer input and writes all components back only when editing changes them.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputInt4([[maybe_unused]] ptr<ScriptImGui> self, string_view label, int32_t& valueX, int32_t& valueY, int32_t& valueZ, int32_t& valueW, ImGui_InputTextFlags flags = ImGui_InputTextFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    int values[4] {valueX, valueY, valueZ, valueW};
    bool changed = ImGui::InputInt4(string(label).c_str(), values, static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

// Draws a required nonempty-label float drag editor with speed, bounds, three-decimal display, and slider flags; updates value and reports changes.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragFloat([[maybe_unused]] ptr<ScriptImGui> self, string_view label, float32_t& value, float32_t speed, float32_t minValue, float32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    return ImGui::DragFloat(string(label).c_str(), &value, speed, minValue, maxValue, "%.3f", static_cast<ImGuiSliderFlags>(flags));
}

// Draws a required nonempty-label integer drag editor with speed, bounds, and slider flags; updates value and reports changes.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragInt([[maybe_unused]] ptr<ScriptImGui> self, string_view label, int32_t& value, float32_t speed, int32_t minValue, int32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    return ImGui::DragInt(string(label).c_str(), &value, speed, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));
}

// Draws a required nonempty-label two-component float drag editor and writes both components back only when they change.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragFloat2([[maybe_unused]] ptr<ScriptImGui> self, string_view label, float32_t& valueX, float32_t& valueY, float32_t speed, float32_t minValue, float32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    float32_t values[2] {valueX, valueY};
    bool changed = ImGui::DragFloat2(string(label).c_str(), values, speed, minValue, maxValue, "%.3f", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

// Draws a required nonempty-label three-component float drag editor and writes all components back only when they change.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragFloat3([[maybe_unused]] ptr<ScriptImGui> self, string_view label, float32_t& valueX, float32_t& valueY, float32_t& valueZ, float32_t speed, float32_t minValue, float32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    float32_t values[3] {valueX, valueY, valueZ};
    bool changed = ImGui::DragFloat3(string(label).c_str(), values, speed, minValue, maxValue, "%.3f", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

// Draws a required nonempty-label four-component float drag editor and writes all components back only when they change.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragFloat4([[maybe_unused]] ptr<ScriptImGui> self, string_view label, float32_t& valueX, float32_t& valueY, float32_t& valueZ, float32_t& valueW, float32_t speed, float32_t minValue, float32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    float32_t values[4] {valueX, valueY, valueZ, valueW};
    bool changed = ImGui::DragFloat4(string(label).c_str(), values, speed, minValue, maxValue, "%.3f", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

// Draws a required nonempty-label two-component integer drag editor and writes both components back only when they change.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragInt2([[maybe_unused]] ptr<ScriptImGui> self, string_view label, int32_t& valueX, int32_t& valueY, float32_t speed, int32_t minValue, int32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    int values[2] {valueX, valueY};
    bool changed = ImGui::DragInt2(string(label).c_str(), values, speed, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

// Draws a required nonempty-label three-component integer drag editor and writes all components back only when they change.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragInt3([[maybe_unused]] ptr<ScriptImGui> self, string_view label, int32_t& valueX, int32_t& valueY, int32_t& valueZ, float32_t speed, int32_t minValue, int32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    int values[3] {valueX, valueY, valueZ};
    bool changed = ImGui::DragInt3(string(label).c_str(), values, speed, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

// Draws a required nonempty-label four-component integer drag editor and writes all components back only when they change.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragInt4([[maybe_unused]] ptr<ScriptImGui> self, string_view label, int32_t& valueX, int32_t& valueY, int32_t& valueZ, int32_t& valueW, float32_t speed, int32_t minValue, int32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    int values[4] {valueX, valueY, valueZ, valueW};
    bool changed = ImGui::DragInt4(string(label).c_str(), values, speed, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

// Draws a required nonempty-label bounded float slider, updates value, and returns whether it changed.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderFloat([[maybe_unused]] ptr<ScriptImGui> self, string_view label, float32_t& value, float32_t minValue, float32_t maxValue)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    return ImGui::SliderFloat(string(label).c_str(), &value, minValue, maxValue);
}

// Draws a required nonempty-label bounded integer slider with the supplied flags, updates value, and reports changes.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderInt([[maybe_unused]] ptr<ScriptImGui> self, string_view label, int32_t& value, int32_t minValue, int32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    return ImGui::SliderInt(string(label).c_str(), &value, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));
}

// Draws a required nonempty-label two-component bounded float slider and writes both components back only when they change.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderFloat2([[maybe_unused]] ptr<ScriptImGui> self, string_view label, float32_t& valueX, float32_t& valueY, float32_t minValue, float32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    float32_t values[2] {valueX, valueY};
    bool changed = ImGui::SliderFloat2(string(label).c_str(), values, minValue, maxValue, "%.3f", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

// Draws a required nonempty-label three-component bounded float slider and writes all components back only when they change.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderFloat3([[maybe_unused]] ptr<ScriptImGui> self, string_view label, float32_t& valueX, float32_t& valueY, float32_t& valueZ, float32_t minValue, float32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    float32_t values[3] {valueX, valueY, valueZ};
    bool changed = ImGui::SliderFloat3(string(label).c_str(), values, minValue, maxValue, "%.3f", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

// Draws a required nonempty-label four-component bounded float slider and writes all components back only when they change.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderFloat4([[maybe_unused]] ptr<ScriptImGui> self, string_view label, float32_t& valueX, float32_t& valueY, float32_t& valueZ, float32_t& valueW, float32_t minValue, float32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    float32_t values[4] {valueX, valueY, valueZ, valueW};
    bool changed = ImGui::SliderFloat4(string(label).c_str(), values, minValue, maxValue, "%.3f", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

// Draws a required nonempty-label two-component bounded integer slider and writes both components back only when they change.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderInt2([[maybe_unused]] ptr<ScriptImGui> self, string_view label, int32_t& valueX, int32_t& valueY, int32_t minValue, int32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    int values[2] {valueX, valueY};
    bool changed = ImGui::SliderInt2(string(label).c_str(), values, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

// Draws a required nonempty-label three-component bounded integer slider and writes all components back only when they change.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderInt3([[maybe_unused]] ptr<ScriptImGui> self, string_view label, int32_t& valueX, int32_t& valueY, int32_t& valueZ, int32_t minValue, int32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    int values[3] {valueX, valueY, valueZ};
    bool changed = ImGui::SliderInt3(string(label).c_str(), values, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

// Draws a required nonempty-label four-component bounded integer slider and writes all components back only when they change.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderInt4([[maybe_unused]] ptr<ScriptImGui> self, string_view label, int32_t& valueX, int32_t& valueY, int32_t& valueZ, int32_t& valueW, int32_t minValue, int32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    int values[4] {valueX, valueY, valueZ, valueW};
    bool changed = ImGui::SliderInt4(string(label).c_str(), values, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

// Draws a required nonempty-label vertical float slider of the supplied size and bounds, updates value, and reports changes.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_VSliderFloat([[maybe_unused]] ptr<ScriptImGui> self, string_view label, fsize32 size, float32_t& value, float32_t minValue, float32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    return ImGui::VSliderFloat(string(label).c_str(), ToImVec2(size), &value, minValue, maxValue, "%.3f", static_cast<ImGuiSliderFlags>(flags));
}

// Draws a required nonempty-label vertical integer slider of the supplied size and bounds, updates value, and reports changes.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_VSliderInt([[maybe_unused]] ptr<ScriptImGui> self, string_view label, fsize32 size, int32_t& value, int32_t minValue, int32_t maxValue, ImGui_SliderFlags flags = ImGui_SliderFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    return ImGui::VSliderInt(string(label).c_str(), ToImVec2(size), &value, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));
}

// Draws a separator appropriate to the current horizontal or vertical layout context.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Separator([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::Separator();
}

// Adds one standard vertical spacing unit to the current layout.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Spacing([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::Spacing();
}

// Ends the current line and advances the layout cursor to a new line.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_NewLine([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::NewLine();
}

// Begins a required nonempty-ID child region with optional border and supplied size; always pair with EndChild regardless of the return value.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginChild([[maybe_unused]] ptr<ScriptImGui> self, string_view strId, isize32 size, bool border)
{
    if (strId.empty()) {
        throw ScriptException("Child id arg is empty");
    }

    return ImGui::BeginChild(string(strId).c_str(), ImVec2(numeric_cast<float32_t>(size.width), numeric_cast<float32_t>(size.height)), border ? ImGuiChildFlags_Borders : ImGuiChildFlags_None);
}

// Begins a required nonempty-ID child region with explicit child and window flags; always pair with EndChild regardless of the return value.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginChild([[maybe_unused]] ptr<ScriptImGui> self, string_view strId, isize32 size, ImGui_ChildFlags childFlags, ImGui_WindowFlags windowFlags)
{
    if (strId.empty()) {
        throw ScriptException("Child id arg is empty");
    }

    return ImGui::BeginChild(string(strId).c_str(), ImVec2(numeric_cast<float32_t>(size.width), numeric_cast<float32_t>(size.height)), static_cast<ImGuiChildFlags>(childFlags), static_cast<ImGuiWindowFlags>(windowFlags));
}

// Ends the current child region begun by BeginChild; this is required regardless of BeginChild's return value.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndChild([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::EndChild();
}

// Draws a required nonempty-label collapsing header and returns whether its contents are visible; it does not push a tree scope.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_CollapsingHeader([[maybe_unused]] ptr<ScriptImGui> self, string_view label, ImGui_TreeNodeFlags flags = ImGui_TreeNodeFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Header label arg is empty");
    }

    return ImGui::CollapsingHeader(string(label).c_str(), static_cast<ImGuiTreeNodeFlags>(flags));
}

// Draws a required nonempty-label tree node and returns whether it is open; call TreePop only when true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TreeNode([[maybe_unused]] ptr<ScriptImGui> self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Tree node label arg is empty");
    }

    return ImGui::TreeNode(string(label).c_str());
}

// Draws a required nonempty-label tree node with explicit flags and returns whether it is open; call TreePop only when true unless NoTreePushOnOpen is set.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TreeNodeEx([[maybe_unused]] ptr<ScriptImGui> self, string_view label, ImGui_TreeNodeFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Tree node label arg is empty");
    }

    return ImGui::TreeNodeEx(string(label).c_str(), static_cast<ImGuiTreeNodeFlags>(flags));
}

// Pops the tree scope pushed by an open TreeNode or TreeNodeEx call.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TreePop([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::TreePop();
}

// Draws a required nonempty-label selectable using selected as display state and returns whether it was activated; selected is not written back.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_Selectable([[maybe_unused]] ptr<ScriptImGui> self, string_view label, bool selected = false, ImGui_SelectableFlags flags = ImGui_SelectableFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Selectable label arg is empty");
    }

    return ImGui::Selectable(string(label).c_str(), selected, static_cast<ImGuiSelectableFlags>(flags));
}

// Draws a required nonempty-label radio button using active as display state and returns whether it was activated; active is not written back.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_RadioButton([[maybe_unused]] ptr<ScriptImGui> self, string_view label, bool active)
{
    if (label.empty()) {
        throw ScriptException("Radio button label arg is empty");
    }

    return ImGui::RadioButton(string(label).c_str(), active);
}

// Draws a required nonempty-label radio button, assigns buttonValue to value when activated, and returns whether the assignment occurred.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_RadioButton([[maybe_unused]] ptr<ScriptImGui> self, string_view label, int32_t& value, int32_t buttonValue)
{
    if (label.empty()) {
        throw ScriptException("Radio button label arg is empty");
    }

    return ImGui::RadioButton(string(label).c_str(), &value, buttonValue);
}

// Draws a required nonempty-label button without standard frame padding and returns whether it was activated.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SmallButton([[maybe_unused]] ptr<ScriptImGui> self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Button label arg is empty");
    }

    return ImGui::SmallButton(string(label).c_str());
}

// Draws a square arrow button with a required nonempty ID and returns whether it was activated.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ArrowButton([[maybe_unused]] ptr<ScriptImGui> self, string_view strId, ImGui_Dir dir)
{
    if (strId.empty()) {
        throw ScriptException("Arrow button id arg is empty");
    }

    return ImGui::ArrowButton(string(strId).c_str(), static_cast<ImGuiDir>(dir));
}

// Submits an invisible button with a required nonempty ID, explicit size, and button flags, and returns whether it was activated.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InvisibleButton([[maybe_unused]] ptr<ScriptImGui> self, string_view strId, fsize32 size, ImGui_ButtonFlags flags = ImGui_ButtonFlags::None)
{
    if (strId.empty()) {
        throw ScriptException("Button id arg is empty");
    }

    return ImGui::InvisibleButton(string(strId).c_str(), ToImVec2(size), static_cast<ImGuiButtonFlags>(flags));
}

// Returns whether the last submitted item is hovered under the supplied hover flags.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemHovered([[maybe_unused]] ptr<ScriptImGui> self, ImGui_HoveredFlags flags = ImGui_HoveredFlags::None)
{
    return ImGui::IsItemHovered(static_cast<ImGuiHoveredFlags>(flags));
}

// Returns whether the last submitted item was clicked with the selected mouse button this frame.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemClicked([[maybe_unused]] ptr<ScriptImGui> self, ImGui_MouseButton mouseButton = ImGui_MouseButton::Left)
{
    return ImGui::IsItemClicked(static_cast<ImGuiMouseButton>(mouseButton));
}

// Returns whether the last submitted item became active this frame.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemActivated([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::IsItemActivated();
}

// Returns whether the last submitted item stopped being active this frame.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemDeactivated([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::IsItemDeactivated();
}

// Returns whether the last submitted item stopped being active this frame after its value was edited.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemDeactivatedAfterEdit([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::IsItemDeactivatedAfterEdit();
}

// Returns whether the last submitted tree node or collapsing header toggled its open state this frame.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemToggledOpen([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::IsItemToggledOpen();
}

// Returns whether any ImGui item currently owns keyboard focus.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsAnyItemFocused([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::IsAnyItemFocused();
}

// Returns whether the current window is focused under the supplied focus flags.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsWindowFocused([[maybe_unused]] ptr<ScriptImGui> self, ImGui_FocusedFlags flags = ImGui_FocusedFlags::None)
{
    return ImGui::IsWindowFocused(static_cast<ImGuiFocusedFlags>(flags));
}

// Returns whether the current window is hovered under the supplied hover flags.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsWindowHovered([[maybe_unused]] ptr<ScriptImGui> self, ImGui_HoveredFlags flags = ImGui_HoveredFlags::None)
{
    return ImGui::IsWindowHovered(static_cast<ImGuiHoveredFlags>(flags));
}

// Returns whether the last submitted item is currently active.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemActive([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::IsItemActive();
}

// Returns whether the last submitted item's value was edited this frame.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemEdited([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::IsItemEdited();
}

// Returns whether the last submitted item overlaps the current clipping rectangle.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemVisible([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::IsItemVisible();
}

// Returns the minimum screen-space corner of the last submitted item's bounding rectangle.
///@ ExportMethod
FO_SCRIPT_API fpos32 Common_ImGui_GetItemRectMin([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ToFPos32(ImGui::GetItemRectMin());
}

// Returns the maximum screen-space corner of the last submitted item's bounding rectangle.
///@ ExportMethod
FO_SCRIPT_API fpos32 Common_ImGui_GetItemRectMax([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ToFPos32(ImGui::GetItemRectMax());
}

// Returns the size of the last submitted item's bounding rectangle.
///@ ExportMethod
FO_SCRIPT_API fsize32 Common_ImGui_GetItemRectSize([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ToFSize32(ImGui::GetItemRectSize());
}

// Returns the current ImGui mouse position in screen coordinates.
///@ ExportMethod
FO_SCRIPT_API fpos32 Common_ImGui_GetMousePos([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ToFPos32(ImGui::GetMousePos());
}

// Returns the mouse position captured when the current popup was opened.
///@ ExportMethod
FO_SCRIPT_API fpos32 Common_ImGui_GetMousePosOnOpeningCurrentPopup([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ToFPos32(ImGui::GetMousePosOnOpeningCurrentPopup());
}

// Returns whether the selected mouse button is currently held down.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsMouseDown([[maybe_unused]] ptr<ScriptImGui> self, ImGui_MouseButton mouseButton)
{
    return ImGui::IsMouseDown(static_cast<ImGuiMouseButton>(mouseButton));
}

// Returns whether the selected mouse button was clicked this frame.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsMouseClicked([[maybe_unused]] ptr<ScriptImGui> self, ImGui_MouseButton mouseButton)
{
    return ImGui::IsMouseClicked(static_cast<ImGuiMouseButton>(mouseButton));
}

// Returns whether the selected mouse button was released this frame.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsMouseReleased([[maybe_unused]] ptr<ScriptImGui> self, ImGui_MouseButton mouseButton)
{
    return ImGui::IsMouseReleased(static_cast<ImGuiMouseButton>(mouseButton));
}

// Returns whether the selected mouse button registered a double-click this frame.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsMouseDoubleClicked([[maybe_unused]] ptr<ScriptImGui> self, ImGui_MouseButton mouseButton)
{
    return ImGui::IsMouseDoubleClicked(static_cast<ImGuiMouseButton>(mouseButton));
}

// Returns whether the mouse is inside the supplied screen-space rectangle, optionally intersected with the current clip rectangle.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsMouseHoveringRect([[maybe_unused]] ptr<ScriptImGui> self, fpos32 rectMin, fpos32 rectMax, bool clip = true)
{
    return ImGui::IsMouseHoveringRect(ToImVec2(rectMin), ToImVec2(rectMax), clip);
}

// Returns whether the selected mouse button is dragging beyond the supplied threshold, or the configured threshold when negative.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsMouseDragging([[maybe_unused]] ptr<ScriptImGui> self, ImGui_MouseButton mouseButton, float32_t lockThreshold = -1.0f)
{
    return ImGui::IsMouseDragging(static_cast<ImGuiMouseButton>(mouseButton), lockThreshold);
}

// Returns mouse displacement since the selected button was clicked after crossing the supplied or configured drag threshold.
///@ ExportMethod
FO_SCRIPT_API fpos32 Common_ImGui_GetMouseDragDelta([[maybe_unused]] ptr<ScriptImGui> self, ImGui_MouseButton mouseButton = ImGui_MouseButton::Left, float32_t lockThreshold = -1.0f)
{
    return ToFPos32(ImGui::GetMouseDragDelta(static_cast<ImGuiMouseButton>(mouseButton), lockThreshold));
}

// Resets the accumulated drag origin for the selected mouse button to the current mouse position.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_ResetMouseDragDelta([[maybe_unused]] ptr<ScriptImGui> self, ImGui_MouseButton mouseButton = ImGui_MouseButton::Left)
{
    ImGui::ResetMouseDragDelta(static_cast<ImGuiMouseButton>(mouseButton));
}

// Gives the last submitted item default focus when its appearing window has no restored focus state.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetItemDefaultFocus([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::SetItemDefaultFocus();
}

// Immediately renders a tooltip containing the supplied text verbatim.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetTooltip([[maybe_unused]] ptr<ScriptImGui> self, string_view text)
{
    ImGui::BeginTooltip();
    ImGuiTextUnformatted(text);
    ImGui::EndTooltip();
}

// Begins a tooltip when the last item is hovered and returns whether it opened; call EndTooltip only when true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginItemTooltip([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::BeginItemTooltip();
}

// Shows a text tooltip for the last item when ImGui's item-tooltip hover policy is satisfied.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetItemTooltip([[maybe_unused]] ptr<ScriptImGui> self, string_view text)
{
    ImGui::SetItemTooltip("%.*s", numeric_cast<int32_t>(text.size()), string(text).c_str());
}

// Begins an unconditional tooltip window; pair it with EndTooltip.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginTooltip([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::BeginTooltip();
}

// Ends the current tooltip begun by BeginTooltip or a successful BeginItemTooltip.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndTooltip([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::EndTooltip();
}

// Marks a popup with the required nonempty ID to open on the next matching BeginPopup call.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_OpenPopup([[maybe_unused]] ptr<ScriptImGui> self, string_view strId, ImGui_PopupFlags popupFlags = ImGui_PopupFlags::None)
{
    if (strId.empty()) {
        throw ScriptException("Popup id arg is empty");
    }

    ImGui::OpenPopup(string(strId).c_str(), static_cast<ImGuiPopupFlags>(popupFlags));
}

// Begins a required nonempty-ID nonmodal popup and returns whether to submit its contents; call EndPopup only when true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginPopup([[maybe_unused]] ptr<ScriptImGui> self, string_view strId, ImGui_WindowFlags flags = ImGui_WindowFlags::None)
{
    if (strId.empty()) {
        throw ScriptException("Popup id arg is empty");
    }

    return ImGui::BeginPopup(string(strId).c_str(), static_cast<ImGuiWindowFlags>(flags));
}

// Begins a required nonempty-name modal popup and returns whether to submit its contents; call EndPopup only when true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginPopupModal([[maybe_unused]] ptr<ScriptImGui> self, string_view name)
{
    if (name.empty()) {
        throw ScriptException("Popup name arg is empty");
    }

    return ImGui::BeginPopupModal(string(name).c_str());
}

// Begins a closable required nonempty-name modal popup, updates opened from its close control, and returns whether to submit contents; EndPopup is conditional on true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginPopupModal([[maybe_unused]] ptr<ScriptImGui> self, string_view name, bool& opened, ImGui_WindowFlags flags)
{
    if (name.empty()) {
        throw ScriptException("Popup name arg is empty");
    }

    return ImGui::BeginPopupModal(string(name).c_str(), &opened, static_cast<ImGuiWindowFlags>(flags));
}

// Opens and begins a context popup for the last item under the supplied flags, using the last item's ID when strId is empty; call EndPopup only when true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginPopupContextItem([[maybe_unused]] ptr<ScriptImGui> self, string_view strId = "", ImGui_PopupFlags popupFlags = ImGui_PopupFlags::None)
{
    string str_id = string(strId);
    const char* id_ptr = str_id.empty() ? nullptr : str_id.c_str();
    return ImGui::BeginPopupContextItem(id_ptr, static_cast<ImGuiPopupFlags>(popupFlags));
}

// Opens and begins a context popup for the current window under the supplied flags, deriving an ID when strId is empty; call EndPopup only when true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginPopupContextWindow([[maybe_unused]] ptr<ScriptImGui> self, string_view strId = "", ImGui_PopupFlags popupFlags = ImGui_PopupFlags::None)
{
    string str_id = string(strId);
    const char* id_ptr = str_id.empty() ? nullptr : str_id.c_str();
    return ImGui::BeginPopupContextWindow(id_ptr, static_cast<ImGuiPopupFlags>(popupFlags));
}

// Opens and begins a context popup over empty space under the supplied flags, deriving an ID when strId is empty; call EndPopup only when true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginPopupContextVoid([[maybe_unused]] ptr<ScriptImGui> self, string_view strId = "", ImGui_PopupFlags popupFlags = ImGui_PopupFlags::MouseButtonRight)
{
    string str_id = string(strId);
    const char* id_ptr = str_id.empty() ? nullptr : str_id.c_str();
    return ImGui::BeginPopupContextVoid(id_ptr, static_cast<ImGuiPopupFlags>(popupFlags));
}

// Ends a popup begun by a successful BeginPopup, BeginPopupModal, or context-popup call.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndPopup([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::EndPopup();
}

// Requests closure of the current popup at the end of the frame.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_CloseCurrentPopup([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::CloseCurrentPopup();
}

// Returns whether a popup matching the required nonempty ID is open under the supplied query flags.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsPopupOpen([[maybe_unused]] ptr<ScriptImGui> self, string_view strId, ImGui_PopupFlags popupFlags = ImGui_PopupFlags::None)
{
    if (strId.empty()) {
        throw ScriptException("Popup id arg is empty");
    }

    return ImGui::IsPopupOpen(string(strId).c_str(), static_cast<ImGuiPopupFlags>(popupFlags));
}

// Opens a context popup when the last item is clicked under the supplied flags, using the last item's ID when strId is empty.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_OpenPopupOnItemClick([[maybe_unused]] ptr<ScriptImGui> self, string_view strId = "", ImGui_PopupFlags popupFlags = ImGui_PopupFlags::MouseButtonRight)
{
    string str_id = string(strId);
    const char* id_ptr = str_id.empty() ? nullptr : str_id.c_str();
    ImGui::OpenPopupOnItemClick(id_ptr, static_cast<ImGuiPopupFlags>(popupFlags));
}

// Begins the main viewport menu bar and returns whether contents may be submitted; call EndMainMenuBar only when true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginMainMenuBar([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::BeginMainMenuBar();
}

// Ends a main menu bar begun by a successful BeginMainMenuBar call.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndMainMenuBar([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::EndMainMenuBar();
}

// Begins the current window's menu bar and returns whether contents may be submitted; call EndMenuBar only when true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginMenuBar([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::BeginMenuBar();
}

// Ends a window menu bar begun by a successful BeginMenuBar call.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndMenuBar([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::EndMenuBar();
}

// Begins a required nonempty-label enabled or disabled menu and returns whether it is open; call EndMenu only when true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginMenu([[maybe_unused]] ptr<ScriptImGui> self, string_view label, bool enabled = true)
{
    if (label.empty()) {
        throw ScriptException("Menu label arg is empty");
    }

    return ImGui::BeginMenu(string(label).c_str(), enabled);
}

// Ends a menu begun by a successful BeginMenu call.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndMenu([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::EndMenu();
}

// Draws a required nonempty-label menu item with display-only selected state and enabled state, returning whether it was activated.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_MenuItem([[maybe_unused]] ptr<ScriptImGui> self, string_view label, bool selected = false, bool enabled = true)
{
    if (label.empty()) {
        throw ScriptException("Menu item label arg is empty");
    }

    return ImGui::MenuItem(string(label).c_str(), nullptr, selected, enabled);
}

// Begins a table with a required nonempty ID and positive column count, returning whether contents may be submitted; call EndTable only when true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginTable([[maybe_unused]] ptr<ScriptImGui> self, string_view strId, int32_t columns, ImGui_TableFlags flags = ImGui_TableFlags::None)
{
    if (strId.empty()) {
        throw ScriptException("Table id arg is empty");
    }
    if (columns <= 0) {
        throw ScriptException("Columns arg must be greater than zero");
    }

    return ImGui::BeginTable(string(strId).c_str(), columns, static_cast<ImGuiTableFlags>(flags));
}

// Ends a table begun by a successful BeginTable call.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndTable([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::EndTable();
}

// Advances the current table to a new row using the supplied row flags.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableNextRow([[maybe_unused]] ptr<ScriptImGui> self, ImGui_TableRowFlags rowFlags = ImGui_TableRowFlags::None)
{
    ImGui::TableNextRow(static_cast<ImGuiTableRowFlags>(rowFlags));
}

// Advances to the next table column, creating a row when needed, and returns whether the destination column is visible.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TableNextColumn([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::TableNextColumn();
}

// Selects a table column by zero-based index and returns whether that column is visible.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TableSetColumnIndex([[maybe_unused]] ptr<ScriptImGui> self, int32_t columnN)
{
    return ImGui::TableSetColumnIndex(columnN);
}

// Emits one table header row from columns configured by TableSetupColumn.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableHeadersRow([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::TableHeadersRow();
}

// Emits a required nonempty-label header cell for the current table column.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableHeader([[maybe_unused]] ptr<ScriptImGui> self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Table header label arg is empty");
    }

    ImGui::TableHeader(string(label).c_str());
}

// Configures the next table column with a required nonempty label and column flags before the first row is submitted.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableSetupColumn([[maybe_unused]] ptr<ScriptImGui> self, string_view label, ImGui_TableColumnFlags flags = ImGui_TableColumnFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Table column label arg is empty");
    }

    ImGui::TableSetupColumn(string(label).c_str(), static_cast<ImGuiTableColumnFlags>(flags));
}

// Freezes the requested leading table columns and rows when the table scrolls.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableSetupScrollFreeze([[maybe_unused]] ptr<ScriptImGui> self, int32_t cols, int32_t rows)
{
    ImGui::TableSetupScrollFreeze(cols, rows);
}

// Overrides a table row or cell background target with the supplied packed color, using the current column when columnN is negative.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableSetBgColor([[maybe_unused]] ptr<ScriptImGui> self, ImGui_TableBgTarget target, ucolor color, int32_t columnN = -1)
{
    ImGui::TableSetBgColor(static_cast<ImGuiTableBgTarget>(target), ToImU32(color), columnN);
}

// Returns the current table's column count, or zero outside a table.
///@ ExportMethod
FO_SCRIPT_API int32_t Common_ImGui_TableGetColumnCount([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::TableGetColumnCount();
}

// Returns the current table column index, or -1 before selecting a column or outside a table.
///@ ExportMethod
FO_SCRIPT_API int32_t Common_ImGui_TableGetColumnIndex([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::TableGetColumnIndex();
}

// Returns the current table row index, or -1 before starting a row or outside a table.
///@ ExportMethod
FO_SCRIPT_API int32_t Common_ImGui_TableGetRowIndex([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::TableGetRowIndex();
}

// Returns the requested table column's configured name, using the current column when columnN is negative.
///@ ExportMethod
FO_SCRIPT_API string Common_ImGui_TableGetColumnName([[maybe_unused]] ptr<ScriptImGui> self, int32_t columnN = -1)
{
    return string(ImGui::TableGetColumnName(columnN));
}

// Returns the requested table column's current status flags, using the current column when columnN is negative.
///@ ExportMethod
FO_SCRIPT_API ImGui_TableColumnFlags Common_ImGui_TableGetColumnFlags([[maybe_unused]] ptr<ScriptImGui> self, int32_t columnN = -1)
{
    return static_cast<ImGui_TableColumnFlags>(ImGui::TableGetColumnFlags(columnN));
}

// Enables or disables a table column by zero-based index.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableSetColumnEnabled([[maybe_unused]] ptr<ScriptImGui> self, int32_t columnN, bool enabled)
{
    ImGui::TableSetColumnEnabled(columnN, enabled);
}

// Returns the hovered table column index, with ImGui sentinel values for none or unused trailing space.
///@ ExportMethod
FO_SCRIPT_API int32_t Common_ImGui_TableGetHoveredColumn([[maybe_unused]] ptr<ScriptImGui> self)
{
    return ImGui::TableGetHoveredColumn();
}

// Emits an angled-header row for columns configured with angled-header flags.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableAngledHeadersRow([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::TableAngledHeadersRow();
}

// Begins a tab bar with a required nonempty ID and returns whether contents may be submitted; call EndTabBar only when true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginTabBar([[maybe_unused]] ptr<ScriptImGui> self, string_view strId, ImGui_TabBarFlags flags = ImGui_TabBarFlags::None)
{
    if (strId.empty()) {
        throw ScriptException("Tab bar id arg is empty");
    }

    return ImGui::BeginTabBar(string(strId).c_str(), static_cast<ImGuiTabBarFlags>(flags));
}

// Ends a tab bar begun by a successful BeginTabBar call.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndTabBar([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::EndTabBar();
}

// Begins a required nonempty-label tab item and returns whether its contents are visible; call EndTabItem only when true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginTabItem([[maybe_unused]] ptr<ScriptImGui> self, string_view label, ImGui_TabItemFlags flags = ImGui_TabItemFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Tab item label arg is empty");
    }

    return ImGui::BeginTabItem(string(label).c_str(), nullptr, static_cast<ImGuiTabItemFlags>(flags));
}

// Begins a closable required nonempty-label tab item, updates opened from its close control, and returns whether contents are visible; EndTabItem is conditional on true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginTabItem([[maybe_unused]] ptr<ScriptImGui> self, string_view label, bool& opened, ImGui_TabItemFlags flags = ImGui_TabItemFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Tab item label arg is empty");
    }

    return ImGui::BeginTabItem(string(label).c_str(), &opened, static_cast<ImGuiTabItemFlags>(flags));
}

// Ends a tab item begun by a successful BeginTabItem call.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndTabItem([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::EndTabItem();
}

// Draws a required nonempty-label tab-bar button and returns whether it was activated; it does not create a tab contents scope.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TabItemButton([[maybe_unused]] ptr<ScriptImGui> self, string_view label, ImGui_TabItemFlags flags = ImGui_TabItemFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Tab item label arg is empty");
    }

    return ImGui::TabItemButton(string(label).c_str(), static_cast<ImGuiTabItemFlags>(flags));
}

// Notifies ImGui that a required nonempty-label tab was closed externally so its tab-bar state can be updated.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetTabItemClosed([[maybe_unused]] ptr<ScriptImGui> self, string_view tabLabel)
{
    if (tabLabel.empty()) {
        throw ScriptException("Tab label arg is empty");
    }

    ImGui::SetTabItemClosed(string(tabLabel).c_str());
}

// ReSharper disable once CppInconsistentNaming
// Starts automatic ImGui text logging to the terminal, optionally auto-opening tree nodes to the supplied depth.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LogToTTY([[maybe_unused]] ptr<ScriptImGui> self, int32_t autoOpenDepth = -1)
{
    ImGui::LogToTTY(autoOpenDepth);
}

// Starts automatic ImGui text logging to the optional filename, or the configured default when empty, with optional tree auto-open depth.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LogToFile([[maybe_unused]] ptr<ScriptImGui> self, int32_t autoOpenDepth = -1, string_view filename = "")
{
    string filename_str = string(filename);
    ImGui::LogToFile(autoOpenDepth, filename_str.empty() ? nullptr : filename_str.c_str());
}

// Starts automatic ImGui text logging to the clipboard with optional tree auto-open depth.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LogToClipboard([[maybe_unused]] ptr<ScriptImGui> self, int32_t autoOpenDepth = -1)
{
    ImGui::LogToClipboard(autoOpenDepth);
}

// Finishes the active ImGui logging session and flushes its destination.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LogFinish([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::LogFinish();
}

// Draws ImGui's logging controls for starting and stopping supported log destinations.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LogButtons([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::LogButtons();
}

// Appends the supplied text verbatim to the active ImGui logging destination without rendering a widget.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LogText([[maybe_unused]] ptr<ScriptImGui> self, string_view text)
{
    ImGui::LogText("%.*s", numeric_cast<int32_t>(text.size()), string(text).c_str());
}

// Begins a required nonempty-label custom combo using previewValue and flags, returning whether popup contents may be submitted; call EndCombo only when true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginCombo([[maybe_unused]] ptr<ScriptImGui> self, string_view label, string_view previewValue, ImGui_ComboFlags flags = ImGui_ComboFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Combo label arg is empty");
    }

    return ImGui::BeginCombo(string(label).c_str(), string(previewValue).c_str(), static_cast<ImGuiComboFlags>(flags));
}

// Draws a required nonempty-label combo from a NUL-separated item string, updates currentItem on selection, and returns whether it changed.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_Combo([[maybe_unused]] ptr<ScriptImGui> self, string_view label, int32_t& currentItem, string_view itemsSeparatedByZeros, int32_t popupMaxHeightInItems = -1)
{
    if (label.empty()) {
        throw ScriptException("Combo label arg is empty");
    }

    string items = string(itemsSeparatedByZeros);
    return ImGui::Combo(string(label).c_str(), &currentItem, items.c_str(), popupMaxHeightInItems);
}

// Ends custom combo contents begun by a successful BeginCombo call.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndCombo([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::EndCombo();
}

// Begins a required nonempty-label list box of the supplied size and returns whether contents may be submitted; call EndListBox only when true.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginListBox([[maybe_unused]] ptr<ScriptImGui> self, string_view label, isize32 size = isize32 {})
{
    if (label.empty()) {
        throw ScriptException("List box label arg is empty");
    }

    return ImGui::BeginListBox(string(label).c_str(), ImVec2(numeric_cast<float32_t>(size.width), numeric_cast<float32_t>(size.height)));
}

// Ends a list box begun by a successful BeginListBox call.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndListBox([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::EndListBox();
}

// Draws a progress bar using fraction as the completion ratio and ImGui's default size and overlay.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_ProgressBar([[maybe_unused]] ptr<ScriptImGui> self, float32_t fraction)
{
    ImGui::ProgressBar(fraction);
}

// Draws a progress bar with the supplied completion ratio and size, using an explicit overlay or ImGui's percentage text when empty.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_ProgressBar([[maybe_unused]] ptr<ScriptImGui> self, float32_t fraction, isize32 size, string_view overlay = "")
{
    string overlay_text = string(overlay);
    const char* overlay_str = overlay_text.empty() ? nullptr : overlay_text.c_str();
    ImGui::ProgressBar(fraction, ToImVec2(size), overlay_str);
}

// Renders the supplied text preceded by an ImGui bullet.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_BulletText([[maybe_unused]] ptr<ScriptImGui> self, string_view text)
{
    ImGui::BulletText("%s", string(text).c_str());
}

// Draws a required nonempty-label text link and returns whether it was activated without opening a URL itself.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TextLink([[maybe_unused]] ptr<ScriptImGui> self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Link label arg is empty");
    }

    return ImGui::TextLink(string(label).c_str());
}

// ReSharper disable once CppInconsistentNaming
// Draws a required nonempty-label link, asks ImGui's platform URL handler to open the label when activated, and reports activation.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TextLinkOpenURL([[maybe_unused]] ptr<ScriptImGui> self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Link label arg is empty");
    }

    return ImGui::TextLinkOpenURL(string(label).c_str());
}

// ReSharper disable once CppInconsistentNaming
// Draws a required nonempty-label link, asks ImGui's platform URL handler to open the required URL when activated, and reports activation.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TextLinkOpenURL([[maybe_unused]] ptr<ScriptImGui> self, string_view label, string_view url)
{
    if (label.empty()) {
        throw ScriptException("Link label arg is empty");
    }
    if (url.empty()) {
        throw ScriptException("Url arg is empty");
    }

    return ImGui::TextLinkOpenURL(string(label).c_str(), string(url).c_str());
}

// Draws a required nonempty-label line plot over the supplied values with offset, optional overlay, scale bounds, and graph size.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PlotLines([[maybe_unused]] ptr<ScriptImGui> self, string_view label, readonly_vector<float32_t> values, int32_t valuesOffset = 0, string_view overlay = "", float32_t scaleMin = 3.402823466e+38f, float32_t scaleMax = 3.402823466e+38f, fsize32 graphSize = fsize32 {})
{
    if (label.empty()) {
        throw ScriptException("Plot label arg is empty");
    }

    string overlay_text = string(overlay);
    const char* overlay_str = overlay_text.empty() ? nullptr : overlay_text.c_str();
    ImGui::PlotLines(string(label).c_str(), values.empty() ? nullptr : values.data(), numeric_cast<int32_t>(values.size()), valuesOffset, overlay_str, scaleMin, scaleMax, ToImVec2(graphSize));
}

// Draws a required nonempty-label histogram over the supplied values with offset, optional overlay, scale bounds, and graph size.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PlotHistogram([[maybe_unused]] ptr<ScriptImGui> self, string_view label, readonly_vector<float32_t> values, int32_t valuesOffset = 0, string_view overlay = "", float32_t scaleMin = 3.402823466e+38f, float32_t scaleMax = 3.402823466e+38f, fsize32 graphSize = fsize32 {})
{
    if (label.empty()) {
        throw ScriptException("Plot label arg is empty");
    }

    string overlay_text = string(overlay);
    const char* overlay_str = overlay_text.empty() ? nullptr : overlay_text.c_str();
    ImGui::PlotHistogram(string(label).c_str(), values.empty() ? nullptr : values.data(), numeric_cast<int32_t>(values.size()), valuesOffset, overlay_str, scaleMin, scaleMax, ToImVec2(graphSize));
}

// Renders the supplied text with an explicit RGBA color.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TextColored([[maybe_unused]] ptr<ScriptImGui> self, string_view text, float32_t r, float32_t g, float32_t b, float32_t a)
{
    ImGui::TextColored(ImVec4(r, g, b, a), "%s", string(text).c_str());
}

// Renders a boolean value after a required nonempty prefix using ImGui's standard value formatting.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Value([[maybe_unused]] ptr<ScriptImGui> self, string_view prefix, bool value)
{
    if (prefix.empty()) {
        throw ScriptException("Value prefix arg is empty");
    }

    ImGui::Value(string(prefix).c_str(), value);
}

// Renders a signed integer value after a required nonempty prefix using ImGui's standard value formatting.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Value([[maybe_unused]] ptr<ScriptImGui> self, string_view prefix, int32_t value)
{
    if (prefix.empty()) {
        throw ScriptException("Value prefix arg is empty");
    }

    ImGui::Value(string(prefix).c_str(), value);
}

// Renders an unsigned integer value after a required nonempty prefix using ImGui's standard value formatting.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Value([[maybe_unused]] ptr<ScriptImGui> self, string_view prefix, uint32_t value)
{
    if (prefix.empty()) {
        throw ScriptException("Value prefix arg is empty");
    }

    ImGui::Value(string(prefix).c_str(), value);
}

// Renders a float value after a required nonempty prefix using ImGui's standard value formatting.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Value([[maybe_unused]] ptr<ScriptImGui> self, string_view prefix, float32_t value)
{
    if (prefix.empty()) {
        throw ScriptException("Value prefix arg is empty");
    }

    ImGui::Value(string(prefix).c_str(), value);
}

// Draws a required nonempty-label RGB editor, converts ucolor channels to floats, preserves alpha, and stores clamped 8-bit RGB only when changed.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ColorEdit3([[maybe_unused]] ptr<ScriptImGui> self, string_view label, ucolor& color, ImGui_ColorEditFlags flags = ImGui_ColorEditFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Color label arg is empty");
    }

    float32_t values[3];
    ColorToFloat3(color, values);
    bool changed = ImGui::ColorEdit3(string(label).c_str(), values, static_cast<ImGuiColorEditFlags>(flags));

    if (changed) {
        StoreColor3(color, values);
    }

    return changed;
}

// Draws a required nonempty-label RGBA editor, converts ucolor channels to floats, and stores clamped 8-bit RGBA only when changed.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ColorEdit4([[maybe_unused]] ptr<ScriptImGui> self, string_view label, ucolor& color, ImGui_ColorEditFlags flags = ImGui_ColorEditFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Color label arg is empty");
    }

    float32_t values[4];
    ColorToFloat4(color, values);
    bool changed = ImGui::ColorEdit4(string(label).c_str(), values, static_cast<ImGuiColorEditFlags>(flags));

    if (changed) {
        StoreColor4(color, values);
    }

    return changed;
}

// Draws a required nonempty-label RGB picker, converts ucolor channels to floats, preserves alpha, and stores clamped 8-bit RGB only when changed.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ColorPicker3([[maybe_unused]] ptr<ScriptImGui> self, string_view label, ucolor& color, ImGui_ColorEditFlags flags = ImGui_ColorEditFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Color label arg is empty");
    }

    float32_t values[3];
    ColorToFloat3(color, values);
    bool changed = ImGui::ColorPicker3(string(label).c_str(), values, static_cast<ImGuiColorEditFlags>(flags));

    if (changed) {
        StoreColor3(color, values);
    }

    return changed;
}

// Draws a required nonempty-label RGBA picker, converts ucolor channels to floats, and stores clamped 8-bit RGBA only when changed.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ColorPicker4([[maybe_unused]] ptr<ScriptImGui> self, string_view label, ucolor& color, ImGui_ColorEditFlags flags = ImGui_ColorEditFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Color label arg is empty");
    }

    float32_t values[4];
    ColorToFloat4(color, values);
    bool changed = ImGui::ColorPicker4(string(label).c_str(), values, static_cast<ImGuiColorEditFlags>(flags));

    if (changed) {
        StoreColor4(color, values);
    }

    return changed;
}

// Draws a color preview button with a required nonempty description ID, packed ucolor, flags, and optional size, returning whether it was activated.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ColorButton([[maybe_unused]] ptr<ScriptImGui> self, string_view descId, ucolor color, ImGui_ColorEditFlags flags = ImGui_ColorEditFlags::None, fsize32 size = fsize32 {})
{
    if (descId.empty()) {
        throw ScriptException("Color button id arg is empty");
    }

    float32_t values[4];
    ColorToFloat4(color, values);
    return ImGui::ColorButton(string(descId).c_str(), ImVec4(values[0], values[1], values[2], values[3]), static_cast<ImGuiColorEditFlags>(flags), ToImVec2(size));
}

// Sets default display, data-type, picker, and input options for subsequent color editors and pickers.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetColorEditOptions([[maybe_unused]] ptr<ScriptImGui> self, ImGui_ColorEditFlags flags)
{
    ImGuiColorEditFlags imgui_flags = static_cast<ImGuiColorEditFlags>(flags);

    if ((imgui_flags & ImGuiColorEditFlags_DisplayMask_) == 0) {
        imgui_flags |= ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_DisplayMask_;
    }
    if ((imgui_flags & ImGuiColorEditFlags_DataTypeMask_) == 0) {
        imgui_flags |= ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_DataTypeMask_;
    }
    if ((imgui_flags & ImGuiColorEditFlags_PickerMask_) == 0) {
        imgui_flags |= ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_PickerMask_;
    }
    if ((imgui_flags & ImGuiColorEditFlags_InputMask_) == 0) {
        imgui_flags |= ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_InputMask_;
    }

    ImGui::GetIO().ConfigColorEditFlags = imgui_flags;
}

// Draws a required nonempty-label float input with step controls, three-decimal formatting, and text-input flags; updates value and reports changes.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputFloat([[maybe_unused]] ptr<ScriptImGui> self, string_view label, float32_t& value, float32_t step = 0.0f, float32_t stepFast = 0.0f, ImGui_InputTextFlags flags = ImGui_InputTextFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    return ImGui::InputFloat(string(label).c_str(), &value, step, stepFast, "%.3f", static_cast<ImGuiInputTextFlags>(flags));
}

// Draws a required nonempty-label two-component float input and writes both components back only when editing changes them.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputFloat2([[maybe_unused]] ptr<ScriptImGui> self, string_view label, float32_t& valueX, float32_t& valueY, ImGui_InputTextFlags flags = ImGui_InputTextFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    float32_t values[2] {valueX, valueY};
    bool changed = ImGui::InputFloat2(string(label).c_str(), values, "%.3f", static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

// Draws a required nonempty-label three-component float input and writes all components back only when editing changes them.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputFloat3([[maybe_unused]] ptr<ScriptImGui> self, string_view label, float32_t& valueX, float32_t& valueY, float32_t& valueZ, ImGui_InputTextFlags flags = ImGui_InputTextFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    float32_t values[3] {valueX, valueY, valueZ};
    bool changed = ImGui::InputFloat3(string(label).c_str(), values, "%.3f", static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

// Draws a required nonempty-label four-component float input and writes all components back only when editing changes them.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputFloat4([[maybe_unused]] ptr<ScriptImGui> self, string_view label, float32_t& valueX, float32_t& valueY, float32_t& valueZ, float32_t& valueW, ImGui_InputTextFlags flags = ImGui_InputTextFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    float32_t values[4] {valueX, valueY, valueZ, valueW};
    bool changed = ImGui::InputFloat4(string(label).c_str(), values, "%.3f", static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

// Draws a required nonempty-label single-line text input backed by maxLength bytes, rejecting zero capacity or an oversized initial value and writing back only on change.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputText([[maybe_unused]] ptr<ScriptImGui> self, string_view label, string& value, uint32_t maxLength, ImGui_InputTextFlags flags = ImGui_InputTextFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }
    if (maxLength == 0) {
        throw ScriptException("Max length arg must be greater than zero");
    }

    auto buffer = PrepareInputBuffer(value, maxLength);
    bool changed = ImGui::InputText(string(label).c_str(), buffer.data(), buffer.size(), static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        value = buffer.data();
    }

    return changed;
}

// Draws a required nonempty-label multiline text input of the supplied size backed by maxLength bytes, rejecting invalid capacity and writing back only on change.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputTextMultiline([[maybe_unused]] ptr<ScriptImGui> self, string_view label, string& value, uint32_t maxLength, isize32 size, ImGui_InputTextFlags flags = ImGui_InputTextFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }
    if (maxLength == 0) {
        throw ScriptException("Max length arg must be greater than zero");
    }

    auto buffer = PrepareInputBuffer(value, maxLength);
    bool changed = ImGui::InputTextMultiline(string(label).c_str(), buffer.data(), buffer.size(), ImVec2(numeric_cast<float32_t>(size.width), numeric_cast<float32_t>(size.height)), static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        value = buffer.data();
    }

    return changed;
}

// Draws a required nonempty-label text input with hint backed by maxLength bytes, rejecting invalid capacity and writing value back only on change.
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputTextWithHint([[maybe_unused]] ptr<ScriptImGui> self, string_view label, string_view hint, string& value, uint32_t maxLength, ImGui_InputTextFlags flags = ImGui_InputTextFlags::None)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }
    if (maxLength == 0) {
        throw ScriptException("Max length arg must be greater than zero");
    }

    string label_text = string(label);
    string hint_text = string(hint);
    auto buffer = PrepareInputBuffer(value, maxLength);
    bool changed = ImGui::InputTextWithHint(label_text.c_str(), hint_text.c_str(), buffer.data(), buffer.size(), static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        value = buffer.data();
    }

    return changed;
}

// Begins a disabled scope when requested while still pushing a balanced scope when false; always pair with EndDisabled.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_BeginDisabled([[maybe_unused]] ptr<ScriptImGui> self, bool disabled = true)
{
    ImGui::BeginDisabled(disabled);
}

// Ends the current scope begun by BeginDisabled.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndDisabled([[maybe_unused]] ptr<ScriptImGui> self)
{
    ImGui::EndDisabled();
}

// Returns text from ImGui's platform clipboard backend, or an empty string when the backend returns null.
///@ ExportMethod
FO_SCRIPT_API string Common_ImGui_GetClipboardText([[maybe_unused]] ptr<ScriptImGui> self)
{
    auto text = make_nptr(ImGui::GetClipboardText());
    return text ? string(text.get()) : string {};
}

// Replaces text in ImGui's platform clipboard backend.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetClipboardText([[maybe_unused]] ptr<ScriptImGui> self, string_view text)
{
    ImGui::SetClipboardText(string(text).c_str());
}

// Loads ImGui window and table settings from a required nonempty filesystem path using ImGui's direct disk API.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LoadIniSettingsFromDisk([[maybe_unused]] ptr<ScriptImGui> self, string_view iniFilename)
{
    if (iniFilename.empty()) {
        throw ScriptException("Ini filename arg is empty");
    }

    ImGui::LoadIniSettingsFromDisk(string(iniFilename).c_str());
}

// Saves current ImGui window and table settings to a required nonempty filesystem path using ImGui's direct disk API.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SaveIniSettingsToDisk([[maybe_unused]] ptr<ScriptImGui> self, string_view iniFilename)
{
    if (iniFilename.empty()) {
        throw ScriptException("Ini filename arg is empty");
    }

    ImGui::SaveIniSettingsToDisk(string(iniFilename).c_str());
}

// Replaces ImGui window and table settings from the exact supplied in-memory ini bytes.
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LoadIniSettingsFromMemory([[maybe_unused]] ptr<ScriptImGui> self, string_view iniData)
{
    ImGui::LoadIniSettingsFromMemory(iniData.data(), iniData.size());
}

// Serializes current ImGui window and table settings to a string, or returns an empty string when ImGui supplies no buffer.
///@ ExportMethod
FO_SCRIPT_API string Common_ImGui_SaveIniSettingsToMemory([[maybe_unused]] ptr<ScriptImGui> self)
{
    size_t ini_size {};
    auto data = make_nptr(ImGui::SaveIniSettingsToMemory(&ini_size));
    return data ? string(data.get(), ini_size) : string {};
}

// Keep script enum bindings in sync with upstream Dear ImGui constants
static_assert(static_cast<int>(ImGui_WindowFlags::None) == ImGuiWindowFlags_None);
static_assert(static_cast<int>(ImGui_WindowFlags::NoTitleBar) == ImGuiWindowFlags_NoTitleBar);
static_assert(static_cast<int>(ImGui_WindowFlags::NoResize) == ImGuiWindowFlags_NoResize);
static_assert(static_cast<int>(ImGui_WindowFlags::NoMove) == ImGuiWindowFlags_NoMove);
static_assert(static_cast<int>(ImGui_WindowFlags::NoScrollbar) == ImGuiWindowFlags_NoScrollbar);
static_assert(static_cast<int>(ImGui_WindowFlags::NoScrollWithMouse) == ImGuiWindowFlags_NoScrollWithMouse);
static_assert(static_cast<int>(ImGui_WindowFlags::NoCollapse) == ImGuiWindowFlags_NoCollapse);
static_assert(static_cast<int>(ImGui_WindowFlags::AlwaysAutoResize) == ImGuiWindowFlags_AlwaysAutoResize);
static_assert(static_cast<int>(ImGui_WindowFlags::NoBackground) == ImGuiWindowFlags_NoBackground);
static_assert(static_cast<int>(ImGui_WindowFlags::NoSavedSettings) == ImGuiWindowFlags_NoSavedSettings);
static_assert(static_cast<int>(ImGui_WindowFlags::NoMouseInputs) == ImGuiWindowFlags_NoMouseInputs);
static_assert(static_cast<int>(ImGui_WindowFlags::MenuBar) == ImGuiWindowFlags_MenuBar);
static_assert(static_cast<int>(ImGui_WindowFlags::HorizontalScrollbar) == ImGuiWindowFlags_HorizontalScrollbar);
static_assert(static_cast<int>(ImGui_WindowFlags::NoFocusOnAppearing) == ImGuiWindowFlags_NoFocusOnAppearing);
static_assert(static_cast<int>(ImGui_WindowFlags::NoBringToFrontOnFocus) == ImGuiWindowFlags_NoBringToFrontOnFocus);
static_assert(static_cast<int>(ImGui_WindowFlags::AlwaysVerticalScrollbar) == ImGuiWindowFlags_AlwaysVerticalScrollbar);
static_assert(static_cast<int>(ImGui_WindowFlags::AlwaysHorizontalScrollbar) == ImGuiWindowFlags_AlwaysHorizontalScrollbar);
static_assert(static_cast<int>(ImGui_WindowFlags::NoNavInputs) == ImGuiWindowFlags_NoNavInputs);
static_assert(static_cast<int>(ImGui_WindowFlags::NoNavFocus) == ImGuiWindowFlags_NoNavFocus);
static_assert(static_cast<int>(ImGui_WindowFlags::UnsavedDocument) == ImGuiWindowFlags_UnsavedDocument);
static_assert(static_cast<int>(ImGui_WindowFlags::NoNav) == ImGuiWindowFlags_NoNav);
static_assert(static_cast<int>(ImGui_WindowFlags::NoDecoration) == ImGuiWindowFlags_NoDecoration);
static_assert(static_cast<int>(ImGui_WindowFlags::NoInputs) == ImGuiWindowFlags_NoInputs);
static_assert(static_cast<int>(ImGui_ChildFlags::None) == ImGuiChildFlags_None);
static_assert(static_cast<int>(ImGui_ChildFlags::Border) == ImGuiChildFlags_Borders);
static_assert(static_cast<int>(ImGui_ChildFlags::AlwaysUseWindowPadding) == ImGuiChildFlags_AlwaysUseWindowPadding);
static_assert(static_cast<int>(ImGui_ChildFlags::ResizeX) == ImGuiChildFlags_ResizeX);
static_assert(static_cast<int>(ImGui_ChildFlags::ResizeY) == ImGuiChildFlags_ResizeY);
static_assert(static_cast<int>(ImGui_ChildFlags::AutoResizeX) == ImGuiChildFlags_AutoResizeX);
static_assert(static_cast<int>(ImGui_ChildFlags::AutoResizeY) == ImGuiChildFlags_AutoResizeY);
static_assert(static_cast<int>(ImGui_ChildFlags::AlwaysAutoResize) == ImGuiChildFlags_AlwaysAutoResize);
static_assert(static_cast<int>(ImGui_ChildFlags::FrameStyle) == ImGuiChildFlags_FrameStyle);
static_assert(static_cast<int>(ImGui_ChildFlags::NavFlattened) == ImGuiChildFlags_NavFlattened);
static_assert(static_cast<int>(ImGui_Cond::None) == ImGuiCond_None);
static_assert(static_cast<int>(ImGui_Cond::Always) == ImGuiCond_Always);
static_assert(static_cast<int>(ImGui_Cond::Once) == ImGuiCond_Once);
static_assert(static_cast<int>(ImGui_Cond::FirstUseEver) == ImGuiCond_FirstUseEver);
static_assert(static_cast<int>(ImGui_Cond::Appearing) == ImGuiCond_Appearing);
static_assert(static_cast<int>(ImGui_SelectableFlags::None) == ImGuiSelectableFlags_None);
static_assert(static_cast<int>(ImGui_SelectableFlags::NoAutoClosePopups) == ImGuiSelectableFlags_NoAutoClosePopups);
static_assert(static_cast<int>(ImGui_SelectableFlags::SpanAllColumns) == ImGuiSelectableFlags_SpanAllColumns);
static_assert(static_cast<int>(ImGui_SelectableFlags::AllowDoubleClick) == ImGuiSelectableFlags_AllowDoubleClick);
static_assert(static_cast<int>(ImGui_SelectableFlags::Disabled) == ImGuiSelectableFlags_Disabled);
static_assert(static_cast<int>(ImGui_SelectableFlags::AllowOverlap) == ImGuiSelectableFlags_AllowOverlap);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::None) == ImGuiTreeNodeFlags_None);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::Selected) == ImGuiTreeNodeFlags_Selected);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::Framed) == ImGuiTreeNodeFlags_Framed);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::AllowOverlap) == ImGuiTreeNodeFlags_AllowOverlap);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::NoTreePushOnOpen) == ImGuiTreeNodeFlags_NoTreePushOnOpen);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::NoAutoOpenOnLog) == ImGuiTreeNodeFlags_NoAutoOpenOnLog);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::DefaultOpen) == ImGuiTreeNodeFlags_DefaultOpen);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::OpenOnDoubleClick) == ImGuiTreeNodeFlags_OpenOnDoubleClick);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::OpenOnArrow) == ImGuiTreeNodeFlags_OpenOnArrow);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::Leaf) == ImGuiTreeNodeFlags_Leaf);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::Bullet) == ImGuiTreeNodeFlags_Bullet);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::FramePadding) == ImGuiTreeNodeFlags_FramePadding);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::SpanAvailWidth) == ImGuiTreeNodeFlags_SpanAvailWidth);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::SpanFullWidth) == ImGuiTreeNodeFlags_SpanFullWidth);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::SpanLabelWidth) == ImGuiTreeNodeFlags_SpanLabelWidth);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::SpanAllColumns) == ImGuiTreeNodeFlags_SpanAllColumns);
static_assert(static_cast<int>(ImGui_TreeNodeFlags::CollapsingHeader) == ImGuiTreeNodeFlags_CollapsingHeader);
static_assert(static_cast<int>(ImGui_FocusedFlags::None) == ImGuiFocusedFlags_None);
static_assert(static_cast<int>(ImGui_FocusedFlags::ChildWindows) == ImGuiFocusedFlags_ChildWindows);
static_assert(static_cast<int>(ImGui_FocusedFlags::RootWindow) == ImGuiFocusedFlags_RootWindow);
static_assert(static_cast<int>(ImGui_FocusedFlags::AnyWindow) == ImGuiFocusedFlags_AnyWindow);
static_assert(static_cast<int>(ImGui_FocusedFlags::NoPopupHierarchy) == ImGuiFocusedFlags_NoPopupHierarchy);
static_assert(static_cast<int>(ImGui_FocusedFlags::RootAndChildWindows) == ImGuiFocusedFlags_RootAndChildWindows);
static_assert(static_cast<int>(ImGui_HoveredFlags::None) == ImGuiHoveredFlags_None);
static_assert(static_cast<int>(ImGui_HoveredFlags::ChildWindows) == ImGuiHoveredFlags_ChildWindows);
static_assert(static_cast<int>(ImGui_HoveredFlags::RootWindow) == ImGuiHoveredFlags_RootWindow);
static_assert(static_cast<int>(ImGui_HoveredFlags::AnyWindow) == ImGuiHoveredFlags_AnyWindow);
static_assert(static_cast<int>(ImGui_HoveredFlags::NoPopupHierarchy) == ImGuiHoveredFlags_NoPopupHierarchy);
static_assert(static_cast<int>(ImGui_HoveredFlags::AllowWhenBlockedByPopup) == ImGuiHoveredFlags_AllowWhenBlockedByPopup);
static_assert(static_cast<int>(ImGui_HoveredFlags::AllowWhenBlockedByActiveItem) == ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
static_assert(static_cast<int>(ImGui_HoveredFlags::AllowWhenOverlappedByItem) == ImGuiHoveredFlags_AllowWhenOverlappedByItem);
static_assert(static_cast<int>(ImGui_HoveredFlags::AllowWhenOverlappedByWindow) == ImGuiHoveredFlags_AllowWhenOverlappedByWindow);
static_assert(static_cast<int>(ImGui_HoveredFlags::AllowWhenDisabled) == ImGuiHoveredFlags_AllowWhenDisabled);
static_assert(static_cast<int>(ImGui_HoveredFlags::NoNavOverride) == ImGuiHoveredFlags_NoNavOverride);
static_assert(static_cast<int>(ImGui_HoveredFlags::AllowWhenOverlapped) == ImGuiHoveredFlags_AllowWhenOverlapped);
static_assert(static_cast<int>(ImGui_HoveredFlags::RectOnly) == ImGuiHoveredFlags_RectOnly);
static_assert(static_cast<int>(ImGui_HoveredFlags::ForTooltip) == ImGuiHoveredFlags_ForTooltip);
static_assert(static_cast<int>(ImGui_TableFlags::None) == ImGuiTableFlags_None);
static_assert(static_cast<int>(ImGui_TableFlags::Resizable) == ImGuiTableFlags_Resizable);
static_assert(static_cast<int>(ImGui_TableFlags::Reorderable) == ImGuiTableFlags_Reorderable);
static_assert(static_cast<int>(ImGui_TableFlags::Hideable) == ImGuiTableFlags_Hideable);
static_assert(static_cast<int>(ImGui_TableFlags::Sortable) == ImGuiTableFlags_Sortable);
static_assert(static_cast<int>(ImGui_TableFlags::NoSavedSettings) == ImGuiTableFlags_NoSavedSettings);
static_assert(static_cast<int>(ImGui_TableFlags::ContextMenuInBody) == ImGuiTableFlags_ContextMenuInBody);
static_assert(static_cast<int>(ImGui_TableFlags::RowBg) == ImGuiTableFlags_RowBg);
static_assert(static_cast<int>(ImGui_TableFlags::BordersInnerH) == ImGuiTableFlags_BordersInnerH);
static_assert(static_cast<int>(ImGui_TableFlags::BordersOuterH) == ImGuiTableFlags_BordersOuterH);
static_assert(static_cast<int>(ImGui_TableFlags::BordersInnerV) == ImGuiTableFlags_BordersInnerV);
static_assert(static_cast<int>(ImGui_TableFlags::BordersOuterV) == ImGuiTableFlags_BordersOuterV);
static_assert(static_cast<int>(ImGui_TableFlags::BordersH) == ImGuiTableFlags_BordersH);
static_assert(static_cast<int>(ImGui_TableFlags::BordersV) == ImGuiTableFlags_BordersV);
static_assert(static_cast<int>(ImGui_TableFlags::BordersInner) == ImGuiTableFlags_BordersInner);
static_assert(static_cast<int>(ImGui_TableFlags::BordersOuter) == ImGuiTableFlags_BordersOuter);
static_assert(static_cast<int>(ImGui_TableFlags::Borders) == ImGuiTableFlags_Borders);
static_assert(static_cast<int>(ImGui_TableFlags::NoBordersInBody) == ImGuiTableFlags_NoBordersInBody);
static_assert(static_cast<int>(ImGui_TableFlags::NoBordersInBodyUntilResize) == ImGuiTableFlags_NoBordersInBodyUntilResize);
static_assert(static_cast<int>(ImGui_TableFlags::SizingFixedFit) == ImGuiTableFlags_SizingFixedFit);
static_assert(static_cast<int>(ImGui_TableFlags::SizingFixedSame) == ImGuiTableFlags_SizingFixedSame);
static_assert(static_cast<int>(ImGui_TableFlags::SizingStretchProp) == ImGuiTableFlags_SizingStretchProp);
static_assert(static_cast<int>(ImGui_TableFlags::SizingStretchSame) == ImGuiTableFlags_SizingStretchSame);
static_assert(static_cast<int>(ImGui_TableFlags::NoHostExtendX) == ImGuiTableFlags_NoHostExtendX);
static_assert(static_cast<int>(ImGui_TableFlags::NoHostExtendY) == ImGuiTableFlags_NoHostExtendY);
static_assert(static_cast<int>(ImGui_TableFlags::NoKeepColumnsVisible) == ImGuiTableFlags_NoKeepColumnsVisible);
static_assert(static_cast<int>(ImGui_TableFlags::PreciseWidths) == ImGuiTableFlags_PreciseWidths);
static_assert(static_cast<int>(ImGui_TableFlags::NoClip) == ImGuiTableFlags_NoClip);
static_assert(static_cast<int>(ImGui_TableFlags::PadOuterX) == ImGuiTableFlags_PadOuterX);
static_assert(static_cast<int>(ImGui_TableFlags::NoPadOuterX) == ImGuiTableFlags_NoPadOuterX);
static_assert(static_cast<int>(ImGui_TableFlags::NoPadInnerX) == ImGuiTableFlags_NoPadInnerX);
static_assert(static_cast<int>(ImGui_TableFlags::ScrollX) == ImGuiTableFlags_ScrollX);
static_assert(static_cast<int>(ImGui_TableFlags::ScrollY) == ImGuiTableFlags_ScrollY);
static_assert(static_cast<int>(ImGui_TableFlags::SortMulti) == ImGuiTableFlags_SortMulti);
static_assert(static_cast<int>(ImGui_TableFlags::SortTristate) == ImGuiTableFlags_SortTristate);
static_assert(static_cast<int>(ImGui_TableFlags::HighlightHoveredColumn) == ImGuiTableFlags_HighlightHoveredColumn);
static_assert(static_cast<int>(ImGui_TableColumnFlags::None) == ImGuiTableColumnFlags_None);
static_assert(static_cast<int>(ImGui_TableColumnFlags::Disabled) == ImGuiTableColumnFlags_Disabled);
static_assert(static_cast<int>(ImGui_TableColumnFlags::DefaultHide) == ImGuiTableColumnFlags_DefaultHide);
static_assert(static_cast<int>(ImGui_TableColumnFlags::DefaultSort) == ImGuiTableColumnFlags_DefaultSort);
static_assert(static_cast<int>(ImGui_TableColumnFlags::WidthStretch) == ImGuiTableColumnFlags_WidthStretch);
static_assert(static_cast<int>(ImGui_TableColumnFlags::WidthFixed) == ImGuiTableColumnFlags_WidthFixed);
static_assert(static_cast<int>(ImGui_TableColumnFlags::NoResize) == ImGuiTableColumnFlags_NoResize);
static_assert(static_cast<int>(ImGui_TableColumnFlags::NoReorder) == ImGuiTableColumnFlags_NoReorder);
static_assert(static_cast<int>(ImGui_TableColumnFlags::NoHide) == ImGuiTableColumnFlags_NoHide);
static_assert(static_cast<int>(ImGui_TableColumnFlags::NoClip) == ImGuiTableColumnFlags_NoClip);
static_assert(static_cast<int>(ImGui_TableColumnFlags::NoSort) == ImGuiTableColumnFlags_NoSort);
static_assert(static_cast<int>(ImGui_TableColumnFlags::NoSortAscending) == ImGuiTableColumnFlags_NoSortAscending);
static_assert(static_cast<int>(ImGui_TableColumnFlags::NoSortDescending) == ImGuiTableColumnFlags_NoSortDescending);
static_assert(static_cast<int>(ImGui_TableColumnFlags::NoHeaderLabel) == ImGuiTableColumnFlags_NoHeaderLabel);
static_assert(static_cast<int>(ImGui_TableColumnFlags::NoHeaderWidth) == ImGuiTableColumnFlags_NoHeaderWidth);
static_assert(static_cast<int>(ImGui_TableColumnFlags::PreferSortAscending) == ImGuiTableColumnFlags_PreferSortAscending);
static_assert(static_cast<int>(ImGui_TableColumnFlags::PreferSortDescending) == ImGuiTableColumnFlags_PreferSortDescending);
static_assert(static_cast<int>(ImGui_TableColumnFlags::IndentEnable) == ImGuiTableColumnFlags_IndentEnable);
static_assert(static_cast<int>(ImGui_TableColumnFlags::IndentDisable) == ImGuiTableColumnFlags_IndentDisable);
static_assert(static_cast<int>(ImGui_TableColumnFlags::AngledHeader) == ImGuiTableColumnFlags_AngledHeader);
static_assert(static_cast<int>(ImGui_TableColumnFlags::IsEnabled) == ImGuiTableColumnFlags_IsEnabled);
static_assert(static_cast<int>(ImGui_TableColumnFlags::IsVisible) == ImGuiTableColumnFlags_IsVisible);
static_assert(static_cast<int>(ImGui_TableColumnFlags::IsSorted) == ImGuiTableColumnFlags_IsSorted);
static_assert(static_cast<int>(ImGui_TableColumnFlags::IsHovered) == ImGuiTableColumnFlags_IsHovered);
static_assert(static_cast<int>(ImGui_TableRowFlags::None) == ImGuiTableRowFlags_None);
static_assert(static_cast<int>(ImGui_TableRowFlags::Headers) == ImGuiTableRowFlags_Headers);
static_assert(static_cast<int>(ImGui_TableBgTarget::None) == ImGuiTableBgTarget_None);
static_assert(static_cast<int>(ImGui_TableBgTarget::RowBg0) == ImGuiTableBgTarget_RowBg0);
static_assert(static_cast<int>(ImGui_TableBgTarget::RowBg1) == ImGuiTableBgTarget_RowBg1);
static_assert(static_cast<int>(ImGui_TableBgTarget::CellBg) == ImGuiTableBgTarget_CellBg);
static_assert(static_cast<int>(ImGui_TabBarFlags::None) == ImGuiTabBarFlags_None);
static_assert(static_cast<int>(ImGui_TabBarFlags::Reorderable) == ImGuiTabBarFlags_Reorderable);
static_assert(static_cast<int>(ImGui_TabBarFlags::AutoSelectNewTabs) == ImGuiTabBarFlags_AutoSelectNewTabs);
static_assert(static_cast<int>(ImGui_TabBarFlags::TabListPopupButton) == ImGuiTabBarFlags_TabListPopupButton);
static_assert(static_cast<int>(ImGui_TabBarFlags::NoCloseWithMiddleMouseButton) == ImGuiTabBarFlags_NoCloseWithMiddleMouseButton);
static_assert(static_cast<int>(ImGui_TabBarFlags::NoTabListScrollingButtons) == ImGuiTabBarFlags_NoTabListScrollingButtons);
static_assert(static_cast<int>(ImGui_TabBarFlags::NoTooltip) == ImGuiTabBarFlags_NoTooltip);
static_assert(static_cast<int>(ImGui_TabBarFlags::DrawSelectedOverline) == ImGuiTabBarFlags_DrawSelectedOverline);
static_assert(static_cast<int>(ImGui_TabBarFlags::FittingPolicyMixed) == ImGuiTabBarFlags_FittingPolicyMixed);
static_assert(static_cast<int>(ImGui_TabBarFlags::FittingPolicyShrink) == ImGuiTabBarFlags_FittingPolicyShrink);
static_assert(static_cast<int>(ImGui_TabBarFlags::FittingPolicyScroll) == ImGuiTabBarFlags_FittingPolicyScroll);
static_assert(static_cast<int>(ImGui_TabItemFlags::None) == ImGuiTabItemFlags_None);
static_assert(static_cast<int>(ImGui_TabItemFlags::UnsavedDocument) == ImGuiTabItemFlags_UnsavedDocument);
static_assert(static_cast<int>(ImGui_TabItemFlags::SetSelected) == ImGuiTabItemFlags_SetSelected);
static_assert(static_cast<int>(ImGui_TabItemFlags::NoCloseWithMiddleMouseButton) == ImGuiTabItemFlags_NoCloseWithMiddleMouseButton);
static_assert(static_cast<int>(ImGui_TabItemFlags::NoPushId) == ImGuiTabItemFlags_NoPushId);
static_assert(static_cast<int>(ImGui_TabItemFlags::NoTooltip) == ImGuiTabItemFlags_NoTooltip);
static_assert(static_cast<int>(ImGui_TabItemFlags::NoReorder) == ImGuiTabItemFlags_NoReorder);
static_assert(static_cast<int>(ImGui_TabItemFlags::Leading) == ImGuiTabItemFlags_Leading);
static_assert(static_cast<int>(ImGui_TabItemFlags::Trailing) == ImGuiTabItemFlags_Trailing);
static_assert(static_cast<int>(ImGui_TabItemFlags::NoAssumedClosure) == ImGuiTabItemFlags_NoAssumedClosure);
static_assert(static_cast<int>(ImGui_ComboFlags::None) == ImGuiComboFlags_None);
static_assert(static_cast<int>(ImGui_ComboFlags::PopupAlignLeft) == ImGuiComboFlags_PopupAlignLeft);
static_assert(static_cast<int>(ImGui_ComboFlags::HeightSmall) == ImGuiComboFlags_HeightSmall);
static_assert(static_cast<int>(ImGui_ComboFlags::HeightRegular) == ImGuiComboFlags_HeightRegular);
static_assert(static_cast<int>(ImGui_ComboFlags::HeightLarge) == ImGuiComboFlags_HeightLarge);
static_assert(static_cast<int>(ImGui_ComboFlags::HeightLargest) == ImGuiComboFlags_HeightLargest);
static_assert(static_cast<int>(ImGui_ComboFlags::NoArrowButton) == ImGuiComboFlags_NoArrowButton);
static_assert(static_cast<int>(ImGui_ComboFlags::NoPreview) == ImGuiComboFlags_NoPreview);
static_assert(static_cast<int>(ImGui_ComboFlags::WidthFitPreview) == ImGuiComboFlags_WidthFitPreview);
static_assert(static_cast<int>(ImGui_InputTextFlags::None) == ImGuiInputTextFlags_None);
static_assert(static_cast<int>(ImGui_InputTextFlags::CharsDecimal) == ImGuiInputTextFlags_CharsDecimal);
static_assert(static_cast<int>(ImGui_InputTextFlags::CharsHexadecimal) == ImGuiInputTextFlags_CharsHexadecimal);
static_assert(static_cast<int>(ImGui_InputTextFlags::CharsScientific) == ImGuiInputTextFlags_CharsScientific);
static_assert(static_cast<int>(ImGui_InputTextFlags::CharsUppercase) == ImGuiInputTextFlags_CharsUppercase);
static_assert(static_cast<int>(ImGui_InputTextFlags::CharsNoBlank) == ImGuiInputTextFlags_CharsNoBlank);
static_assert(static_cast<int>(ImGui_InputTextFlags::EnterReturnsTrue) == ImGuiInputTextFlags_EnterReturnsTrue);
static_assert(static_cast<int>(ImGui_InputTextFlags::ReadOnly) == ImGuiInputTextFlags_ReadOnly);
static_assert(static_cast<int>(ImGui_InputTextFlags::AutoSelectAll) == ImGuiInputTextFlags_AutoSelectAll);
static_assert(static_cast<int>(ImGui_InputTextFlags::ParseEmptyRefVal) == ImGuiInputTextFlags_ParseEmptyRefVal);
static_assert(static_cast<int>(ImGui_InputTextFlags::DisplayEmptyRefVal) == ImGuiInputTextFlags_DisplayEmptyRefVal);
static_assert(static_cast<int>(ImGui_PopupFlags::None) == ImGuiPopupFlags_None);
static_assert(static_cast<int>(ImGui_PopupFlags::MouseButtonLeft) == ImGuiPopupFlags_MouseButtonLeft);
static_assert(static_cast<int>(ImGui_PopupFlags::MouseButtonRight) == ImGuiPopupFlags_MouseButtonRight);
static_assert(static_cast<int>(ImGui_PopupFlags::MouseButtonMiddle) == ImGuiPopupFlags_MouseButtonMiddle);
static_assert(static_cast<int>(ImGui_PopupFlags::NoReopen) == ImGuiPopupFlags_NoReopen);
static_assert(static_cast<int>(ImGui_PopupFlags::NoOpenOverExistingPopup) == ImGuiPopupFlags_NoOpenOverExistingPopup);
static_assert(static_cast<int>(ImGui_PopupFlags::NoOpenOverItems) == ImGuiPopupFlags_NoOpenOverItems);
static_assert(static_cast<int>(ImGui_PopupFlags::AnyPopupId) == ImGuiPopupFlags_AnyPopupId);
static_assert(static_cast<int>(ImGui_PopupFlags::AnyPopupLevel) == ImGuiPopupFlags_AnyPopupLevel);
static_assert(static_cast<int>(ImGui_PopupFlags::AnyPopup) == ImGuiPopupFlags_AnyPopup);
static_assert(static_cast<int>(ImGui_MouseButton::Left) == ImGuiMouseButton_Left);
static_assert(static_cast<int>(ImGui_MouseButton::Right) == ImGuiMouseButton_Right);
static_assert(static_cast<int>(ImGui_MouseButton::Middle) == ImGuiMouseButton_Middle);
static_assert(static_cast<int>(ImGui_Dir::None) == ImGuiDir_None);
static_assert(static_cast<int>(ImGui_Dir::Left) == ImGuiDir_Left);
static_assert(static_cast<int>(ImGui_Dir::Right) == ImGuiDir_Right);
static_assert(static_cast<int>(ImGui_Dir::Up) == ImGuiDir_Up);
static_assert(static_cast<int>(ImGui_Dir::Down) == ImGuiDir_Down);
static_assert(static_cast<int>(ImGui_SliderFlags::None) == ImGuiSliderFlags_None);
static_assert(static_cast<int>(ImGui_SliderFlags::Logarithmic) == ImGuiSliderFlags_Logarithmic);
static_assert(static_cast<int>(ImGui_SliderFlags::NoRoundToFormat) == ImGuiSliderFlags_NoRoundToFormat);
static_assert(static_cast<int>(ImGui_SliderFlags::NoInput) == ImGuiSliderFlags_NoInput);
static_assert(static_cast<int>(ImGui_SliderFlags::WrapAround) == ImGuiSliderFlags_WrapAround);
static_assert(static_cast<int>(ImGui_SliderFlags::ClampOnInput) == ImGuiSliderFlags_ClampOnInput);
static_assert(static_cast<int>(ImGui_SliderFlags::ClampZeroRange) == ImGuiSliderFlags_ClampZeroRange);
static_assert(static_cast<int>(ImGui_SliderFlags::NoSpeedTweaks) == ImGuiSliderFlags_NoSpeedTweaks);
static_assert(static_cast<int>(ImGui_SliderFlags::AlwaysClamp) == ImGuiSliderFlags_AlwaysClamp);
static_assert(static_cast<int>(ImGui_ButtonFlags::None) == ImGuiButtonFlags_None);
static_assert(static_cast<int>(ImGui_ButtonFlags::MouseButtonLeft) == ImGuiButtonFlags_MouseButtonLeft);
static_assert(static_cast<int>(ImGui_ButtonFlags::MouseButtonRight) == ImGuiButtonFlags_MouseButtonRight);
static_assert(static_cast<int>(ImGui_ButtonFlags::MouseButtonMiddle) == ImGuiButtonFlags_MouseButtonMiddle);
static_assert(static_cast<int>(ImGui_ButtonFlags::EnableNav) == ImGuiButtonFlags_EnableNav);
static_assert(static_cast<int>(ImGui_ColorEditFlags::None) == ImGuiColorEditFlags_None);
static_assert(static_cast<int>(ImGui_ColorEditFlags::NoAlpha) == ImGuiColorEditFlags_NoAlpha);
static_assert(static_cast<int>(ImGui_ColorEditFlags::NoPicker) == ImGuiColorEditFlags_NoPicker);
static_assert(static_cast<int>(ImGui_ColorEditFlags::NoOptions) == ImGuiColorEditFlags_NoOptions);
static_assert(static_cast<int>(ImGui_ColorEditFlags::NoSmallPreview) == ImGuiColorEditFlags_NoSmallPreview);
static_assert(static_cast<int>(ImGui_ColorEditFlags::NoInputs) == ImGuiColorEditFlags_NoInputs);
static_assert(static_cast<int>(ImGui_ColorEditFlags::NoTooltip) == ImGuiColorEditFlags_NoTooltip);
static_assert(static_cast<int>(ImGui_ColorEditFlags::NoLabel) == ImGuiColorEditFlags_NoLabel);
static_assert(static_cast<int>(ImGui_ColorEditFlags::NoSidePreview) == ImGuiColorEditFlags_NoSidePreview);
static_assert(static_cast<int>(ImGui_ColorEditFlags::NoDragDrop) == ImGuiColorEditFlags_NoDragDrop);
static_assert(static_cast<int>(ImGui_ColorEditFlags::NoBorder) == ImGuiColorEditFlags_NoBorder);
static_assert(static_cast<int>(ImGui_ColorEditFlags::AlphaOpaque) == ImGuiColorEditFlags_AlphaOpaque);
static_assert(static_cast<int>(ImGui_ColorEditFlags::AlphaNoBg) == ImGuiColorEditFlags_AlphaNoBg);
static_assert(static_cast<int>(ImGui_ColorEditFlags::AlphaPreviewHalf) == ImGuiColorEditFlags_AlphaPreviewHalf);
static_assert(static_cast<int>(ImGui_ColorEditFlags::AlphaBar) == ImGuiColorEditFlags_AlphaBar);
static_assert(static_cast<int>(ImGui_ColorEditFlags::HDR) == ImGuiColorEditFlags_HDR);
static_assert(static_cast<int>(ImGui_ColorEditFlags::DisplayRGB) == ImGuiColorEditFlags_DisplayRGB);
static_assert(static_cast<int>(ImGui_ColorEditFlags::DisplayHSV) == ImGuiColorEditFlags_DisplayHSV);
static_assert(static_cast<int>(ImGui_ColorEditFlags::DisplayHex) == ImGuiColorEditFlags_DisplayHex);
static_assert(static_cast<int>(ImGui_ColorEditFlags::Uint8) == ImGuiColorEditFlags_Uint8);
static_assert(static_cast<int>(ImGui_ColorEditFlags::Float) == ImGuiColorEditFlags_Float);
static_assert(static_cast<int>(ImGui_ColorEditFlags::PickerHueBar) == ImGuiColorEditFlags_PickerHueBar);
static_assert(static_cast<int>(ImGui_ColorEditFlags::PickerHueWheel) == ImGuiColorEditFlags_PickerHueWheel);
static_assert(static_cast<int>(ImGui_ColorEditFlags::InputRGB) == ImGuiColorEditFlags_InputRGB);
static_assert(static_cast<int>(ImGui_ColorEditFlags::InputHSV) == ImGuiColorEditFlags_InputHSV);
static_assert(static_cast<int>(ImGui_ColorEditFlags::DefaultOptions) == ImGuiColorEditFlags_DefaultOptions_);
static_assert(static_cast<int>(ImGui_Col::Text) == ImGuiCol_Text);
static_assert(static_cast<int>(ImGui_Col::TextDisabled) == ImGuiCol_TextDisabled);
static_assert(static_cast<int>(ImGui_Col::WindowBg) == ImGuiCol_WindowBg);
static_assert(static_cast<int>(ImGui_Col::ChildBg) == ImGuiCol_ChildBg);
static_assert(static_cast<int>(ImGui_Col::PopupBg) == ImGuiCol_PopupBg);
static_assert(static_cast<int>(ImGui_Col::Border) == ImGuiCol_Border);
static_assert(static_cast<int>(ImGui_Col::FrameBg) == ImGuiCol_FrameBg);
static_assert(static_cast<int>(ImGui_Col::FrameBgHovered) == ImGuiCol_FrameBgHovered);
static_assert(static_cast<int>(ImGui_Col::FrameBgActive) == ImGuiCol_FrameBgActive);
static_assert(static_cast<int>(ImGui_Col::TitleBg) == ImGuiCol_TitleBg);
static_assert(static_cast<int>(ImGui_Col::TitleBgActive) == ImGuiCol_TitleBgActive);
static_assert(static_cast<int>(ImGui_Col::MenuBarBg) == ImGuiCol_MenuBarBg);
static_assert(static_cast<int>(ImGui_Col::ScrollbarBg) == ImGuiCol_ScrollbarBg);
static_assert(static_cast<int>(ImGui_Col::ScrollbarGrab) == ImGuiCol_ScrollbarGrab);
static_assert(static_cast<int>(ImGui_Col::CheckMark) == ImGuiCol_CheckMark);
static_assert(static_cast<int>(ImGui_Col::SliderGrab) == ImGuiCol_SliderGrab);
static_assert(static_cast<int>(ImGui_Col::SliderGrabActive) == ImGuiCol_SliderGrabActive);
static_assert(static_cast<int>(ImGui_Col::Button) == ImGuiCol_Button);
static_assert(static_cast<int>(ImGui_Col::ButtonHovered) == ImGuiCol_ButtonHovered);
static_assert(static_cast<int>(ImGui_Col::ButtonActive) == ImGuiCol_ButtonActive);
static_assert(static_cast<int>(ImGui_Col::Header) == ImGuiCol_Header);
static_assert(static_cast<int>(ImGui_Col::HeaderHovered) == ImGuiCol_HeaderHovered);
static_assert(static_cast<int>(ImGui_Col::HeaderActive) == ImGuiCol_HeaderActive);
static_assert(static_cast<int>(ImGui_Col::Separator) == ImGuiCol_Separator);
static_assert(static_cast<int>(ImGui_Col::SeparatorHovered) == ImGuiCol_SeparatorHovered);
static_assert(static_cast<int>(ImGui_Col::SeparatorActive) == ImGuiCol_SeparatorActive);
static_assert(static_cast<int>(ImGui_Col::ResizeGrip) == ImGuiCol_ResizeGrip);
static_assert(static_cast<int>(ImGui_Col::ResizeGripHovered) == ImGuiCol_ResizeGripHovered);
static_assert(static_cast<int>(ImGui_Col::ResizeGripActive) == ImGuiCol_ResizeGripActive);
static_assert(static_cast<int>(ImGui_Col::Tab) == ImGuiCol_Tab);
static_assert(static_cast<int>(ImGui_Col::TabHovered) == ImGuiCol_TabHovered);
static_assert(static_cast<int>(ImGui_Col::TabSelected) == ImGuiCol_TabSelected);
static_assert(static_cast<int>(ImGui_Col::TabDimmed) == ImGuiCol_TabDimmed);
static_assert(static_cast<int>(ImGui_Col::TabDimmedSelected) == ImGuiCol_TabDimmedSelected);
static_assert(static_cast<int>(ImGui_Col::TableHeaderBg) == ImGuiCol_TableHeaderBg);
static_assert(static_cast<int>(ImGui_Col::TableRowBg) == ImGuiCol_TableRowBg);
static_assert(static_cast<int>(ImGui_Col::TableRowBgAlt) == ImGuiCol_TableRowBgAlt);
static_assert(static_cast<int>(ImGui_StyleVar::Alpha) == ImGuiStyleVar_Alpha);
static_assert(static_cast<int>(ImGui_StyleVar::DisabledAlpha) == ImGuiStyleVar_DisabledAlpha);
static_assert(static_cast<int>(ImGui_StyleVar::WindowPadding) == ImGuiStyleVar_WindowPadding);
static_assert(static_cast<int>(ImGui_StyleVar::WindowRounding) == ImGuiStyleVar_WindowRounding);
static_assert(static_cast<int>(ImGui_StyleVar::WindowBorderSize) == ImGuiStyleVar_WindowBorderSize);
static_assert(static_cast<int>(ImGui_StyleVar::FramePadding) == ImGuiStyleVar_FramePadding);
static_assert(static_cast<int>(ImGui_StyleVar::FrameRounding) == ImGuiStyleVar_FrameRounding);
static_assert(static_cast<int>(ImGui_StyleVar::FrameBorderSize) == ImGuiStyleVar_FrameBorderSize);
static_assert(static_cast<int>(ImGui_StyleVar::ItemSpacing) == ImGuiStyleVar_ItemSpacing);
static_assert(static_cast<int>(ImGui_StyleVar::ItemInnerSpacing) == ImGuiStyleVar_ItemInnerSpacing);
static_assert(static_cast<int>(ImGui_StyleVar::IndentSpacing) == ImGuiStyleVar_IndentSpacing);
static_assert(static_cast<int>(ImGui_StyleVar::CellPadding) == ImGuiStyleVar_CellPadding);
static_assert(static_cast<int>(ImGui_StyleVar::ScrollbarSize) == ImGuiStyleVar_ScrollbarSize);
static_assert(static_cast<int>(ImGui_StyleVar::ScrollbarRounding) == ImGuiStyleVar_ScrollbarRounding);
static_assert(static_cast<int>(ImGui_StyleVar::GrabMinSize) == ImGuiStyleVar_GrabMinSize);
static_assert(static_cast<int>(ImGui_StyleVar::GrabRounding) == ImGuiStyleVar_GrabRounding);
static_assert(static_cast<int>(ImGui_StyleVar::TabRounding) == ImGuiStyleVar_TabRounding);
static_assert(static_cast<int>(ImGui_StyleVar::ButtonTextAlign) == ImGuiStyleVar_ButtonTextAlign);
static_assert(static_cast<int>(ImGui_StyleVar::SelectableTextAlign) == ImGuiStyleVar_SelectableTextAlign);

FO_END_NAMESPACE
