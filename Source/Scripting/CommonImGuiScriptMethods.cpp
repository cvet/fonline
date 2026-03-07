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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

static auto PrepareInputBuffer(string_view text, uint32 max_length) -> vector<char>
{
    FO_STACK_TRACE_ENTRY();

    if (text.size() > numeric_cast<size_t>(max_length)) {
        throw ScriptException("Text arg length must be less or equal to maxLength arg");
    }

    const auto initial_len = std::max(numeric_cast<size_t>(max_length), text.size());
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

    return {numeric_cast<float32>(size.width), numeric_cast<float32>(size.height)};
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

static auto ToImU32(ucolor color) -> ImU32
{
    FO_STACK_TRACE_ENTRY();

    return IM_COL32(color.comp.r, color.comp.g, color.comp.b, color.comp.a);
}

static auto ToColorComp(float32 value) -> uint8
{
    FO_STACK_TRACE_ENTRY();

    return numeric_cast<uint8>(std::clamp(iround<int32>(value * 255.0f), 0, 255));
}

static void ColorToFloat3(ucolor color, float32 (&values)[3])
{
    FO_STACK_TRACE_ENTRY();

    values[0] = numeric_cast<float32>(color.comp.r) / 255.0f;
    values[1] = numeric_cast<float32>(color.comp.g) / 255.0f;
    values[2] = numeric_cast<float32>(color.comp.b) / 255.0f;
}

static void ColorToFloat4(ucolor color, float32 (&values)[4])
{
    FO_STACK_TRACE_ENTRY();

    values[0] = numeric_cast<float32>(color.comp.r) / 255.0f;
    values[1] = numeric_cast<float32>(color.comp.g) / 255.0f;
    values[2] = numeric_cast<float32>(color.comp.b) / 255.0f;
    values[3] = numeric_cast<float32>(color.comp.a) / 255.0f;
}

static void StoreColor3(ucolor& color, const float32 (&values)[3])
{
    FO_STACK_TRACE_ENTRY();

    color = ucolor(ToColorComp(values[0]), ToColorComp(values[1]), ToColorComp(values[2]), color.comp.a);
}

static void StoreColor4(ucolor& color, const float32 (&values)[4])
{
    FO_STACK_TRACE_ENTRY();

    color = ucolor(ToColorComp(values[0]), ToColorComp(values[1]), ToColorComp(values[2]), ToColorComp(values[3]));
}

///@ ExportMethod GlobalGetter
FO_SCRIPT_API ScriptImGui* Common_Game_ImGui(BaseEngine* engine)
{
    if (ImGui::GetCurrentContext() == nullptr) {
        throw ScriptException("ImGui context is not available");
    }
    if (!GImGui->WithinFrameScope) {
        throw ScriptException("You can use this function only in active ImGui frame");
    }

    FO_RUNTIME_ASSERT(engine);
    auto* imgui = engine->GetImGui();
    FO_RUNTIME_ASSERT(imgui);
    return imgui;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_Begin([[maybe_unused]] ScriptImGui* self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Window label arg is empty");
    }

    return ImGui::Begin(string(label).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_Begin([[maybe_unused]] ScriptImGui* self, string_view label, bool& opened)
{
    if (label.empty()) {
        throw ScriptException("Window label arg is empty");
    }

    return ImGui::Begin(string(label).c_str(), &opened);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_Begin([[maybe_unused]] ScriptImGui* self, string_view label, ImGui_WindowFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Window label arg is empty");
    }

    return ImGui::Begin(string(label).c_str(), nullptr, static_cast<ImGuiWindowFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_Begin([[maybe_unused]] ScriptImGui* self, string_view label, bool& opened, ImGui_WindowFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Window label arg is empty");
    }

    return ImGui::Begin(string(label).c_str(), &opened, static_cast<ImGuiWindowFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_End([[maybe_unused]] ScriptImGui* self)
{
    ImGui::End();
}

// ReSharper disable once CppInconsistentNaming
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PushID([[maybe_unused]] ScriptImGui* self, string_view strId)
{
    if (strId.empty()) {
        throw ScriptException("Id arg is empty");
    }

    ImGui::PushID(string(strId).c_str());
}

// ReSharper disable once CppInconsistentNaming
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PushID([[maybe_unused]] ScriptImGui* self, int32 intId)
{
    ImGui::PushID(intId);
}

// ReSharper disable once CppInconsistentNaming
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PopID([[maybe_unused]] ScriptImGui* self)
{
    ImGui::PopID();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PushStyleColor([[maybe_unused]] ScriptImGui* self, ImGui_Col colorId, float32 r, float32 g, float32 b, float32 a)
{
    ImGui::PushStyleColor(static_cast<ImGuiCol>(colorId), ImVec4(r, g, b, a));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PopStyleColor([[maybe_unused]] ScriptImGui* self)
{
    ImGui::PopStyleColor();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PopStyleColor([[maybe_unused]] ScriptImGui* self, int32 count)
{
    ImGui::PopStyleColor(count);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PushStyleVar([[maybe_unused]] ScriptImGui* self, ImGui_StyleVar styleVar, float32 value)
{
    ImGui::PushStyleVar(static_cast<ImGuiStyleVar>(styleVar), value);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PushStyleVarVec2([[maybe_unused]] ScriptImGui* self, ImGui_StyleVar styleVar, float32 x, float32 y)
{
    ImGui::PushStyleVar(static_cast<ImGuiStyleVar>(styleVar), ImVec2(x, y));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PopStyleVar([[maybe_unused]] ScriptImGui* self)
{
    ImGui::PopStyleVar();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PopStyleVar([[maybe_unused]] ScriptImGui* self, int32 count)
{
    ImGui::PopStyleVar(count);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowPos([[maybe_unused]] ScriptImGui* self, ipos32 pos)
{
    ImGui::SetNextWindowPos(ImVec2(numeric_cast<float32>(pos.x), numeric_cast<float32>(pos.y)));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowPos([[maybe_unused]] ScriptImGui* self, ipos32 pos, ImGui_Cond cond)
{
    ImGui::SetNextWindowPos(ImVec2(numeric_cast<float32>(pos.x), numeric_cast<float32>(pos.y)), static_cast<ImGuiCond>(cond));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowSize([[maybe_unused]] ScriptImGui* self, isize32 size)
{
    ImGui::SetNextWindowSize(ImVec2(numeric_cast<float32>(size.width), numeric_cast<float32>(size.height)));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowSize([[maybe_unused]] ScriptImGui* self, isize32 size, ImGui_Cond cond)
{
    ImGui::SetNextWindowSize(ImVec2(numeric_cast<float32>(size.width), numeric_cast<float32>(size.height)), static_cast<ImGuiCond>(cond));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowCollapsed([[maybe_unused]] ScriptImGui* self, bool collapsed)
{
    ImGui::SetNextWindowCollapsed(collapsed);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowCollapsed([[maybe_unused]] ScriptImGui* self, bool collapsed, ImGui_Cond cond)
{
    ImGui::SetNextWindowCollapsed(collapsed, static_cast<ImGuiCond>(cond));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowSizeConstraints([[maybe_unused]] ScriptImGui* self, isize32 sizeMin, isize32 sizeMax)
{
    ImGui::SetNextWindowSizeConstraints(ImVec2(numeric_cast<float32>(sizeMin.width), numeric_cast<float32>(sizeMin.height)), ImVec2(numeric_cast<float32>(sizeMax.width), numeric_cast<float32>(sizeMax.height)));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowContentSize([[maybe_unused]] ScriptImGui* self, isize32 size)
{
    ImGui::SetNextWindowContentSize(ImVec2(numeric_cast<float32>(size.width), numeric_cast<float32>(size.height)));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowFocus([[maybe_unused]] ScriptImGui* self)
{
    ImGui::SetNextWindowFocus();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowScroll([[maybe_unused]] ScriptImGui* self, fpos32 scroll)
{
    ImGui::SetNextWindowScroll(ToImVec2(scroll));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextWindowBgAlpha([[maybe_unused]] ScriptImGui* self, float32 alpha)
{
    ImGui::SetNextWindowBgAlpha(alpha);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Text([[maybe_unused]] ScriptImGui* self, string_view text)
{
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TextDisabled([[maybe_unused]] ScriptImGui* self, string_view text)
{
    ImGui::TextDisabled("%s", string(text).c_str());
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TextWrapped([[maybe_unused]] ScriptImGui* self, string_view text)
{
    ImGui::TextWrapped("%s", string(text).c_str());
}

///@ ExportMethod
FO_SCRIPT_API fsize32 Common_ImGui_CalcTextSize([[maybe_unused]] ScriptImGui* self, string_view text)
{
    const auto source = string(text);
    return ToFSize32(ImGui::CalcTextSize(source.c_str()));
}

///@ ExportMethod
FO_SCRIPT_API fsize32 Common_ImGui_CalcTextSize([[maybe_unused]] ScriptImGui* self, string_view text, bool hideTextAfterDoubleHash)
{
    const auto source = string(text);
    return ToFSize32(ImGui::CalcTextSize(source.c_str(), nullptr, hideTextAfterDoubleHash));
}

///@ ExportMethod
FO_SCRIPT_API fsize32 Common_ImGui_CalcTextSize([[maybe_unused]] ScriptImGui* self, string_view text, bool hideTextAfterDoubleHash, float32 wrapWidth)
{
    const auto source = string(text);
    return ToFSize32(ImGui::CalcTextSize(source.c_str(), nullptr, hideTextAfterDoubleHash, wrapWidth));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_AlignTextToFramePadding([[maybe_unused]] ScriptImGui* self)
{
    ImGui::AlignTextToFramePadding();
}

///@ ExportMethod
FO_SCRIPT_API float64 Common_ImGui_GetTime([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::GetTime();
}

///@ ExportMethod
FO_SCRIPT_API int32 Common_ImGui_GetFrameCount([[maybe_unused]] ScriptImGui* self)
{
    return numeric_cast<int32>(ImGui::GetFrameCount());
}

///@ ExportMethod
FO_SCRIPT_API float32 Common_ImGui_GetTextLineHeight([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::GetTextLineHeight();
}

///@ ExportMethod
FO_SCRIPT_API float32 Common_ImGui_GetTextLineHeightWithSpacing([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::GetTextLineHeightWithSpacing();
}

///@ ExportMethod
FO_SCRIPT_API float32 Common_ImGui_GetFrameHeight([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::GetFrameHeight();
}

///@ ExportMethod
FO_SCRIPT_API float32 Common_ImGui_GetFrameHeightWithSpacing([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::GetFrameHeightWithSpacing();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsWindowAppearing([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::IsWindowAppearing();
}

///@ ExportMethod
FO_SCRIPT_API fpos32 Common_ImGui_GetWindowPos([[maybe_unused]] ScriptImGui* self)
{
    return ToFPos32(ImGui::GetWindowPos());
}

///@ ExportMethod
FO_SCRIPT_API fsize32 Common_ImGui_GetWindowSize([[maybe_unused]] ScriptImGui* self)
{
    return ToFSize32(ImGui::GetWindowSize());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsRectVisible([[maybe_unused]] ScriptImGui* self, fsize32 size)
{
    return ImGui::IsRectVisible(ToImVec2(size));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsRectVisible([[maybe_unused]] ScriptImGui* self, fpos32 rectMin, fpos32 rectMax)
{
    return ImGui::IsRectVisible(ToImVec2(rectMin), ToImVec2(rectMax));
}

///@ ExportMethod
FO_SCRIPT_API float32 Common_ImGui_GetWindowWidth([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::GetWindowWidth();
}

///@ ExportMethod
FO_SCRIPT_API float32 Common_ImGui_GetWindowHeight([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::GetWindowHeight();
}

///@ ExportMethod
FO_SCRIPT_API float32 Common_ImGui_GetScrollX([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::GetScrollX();
}

///@ ExportMethod
FO_SCRIPT_API float32 Common_ImGui_GetScrollY([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::GetScrollY();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetScrollX([[maybe_unused]] ScriptImGui* self, float32 scrollX)
{
    ImGui::SetScrollX(scrollX);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetScrollY([[maybe_unused]] ScriptImGui* self, float32 scrollY)
{
    ImGui::SetScrollY(scrollY);
}

///@ ExportMethod
FO_SCRIPT_API float32 Common_ImGui_GetScrollMaxX([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::GetScrollMaxX();
}

///@ ExportMethod
FO_SCRIPT_API float32 Common_ImGui_GetScrollMaxY([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::GetScrollMaxY();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetScrollHereX([[maybe_unused]] ScriptImGui* self)
{
    ImGui::SetScrollHereX();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetScrollHereX([[maybe_unused]] ScriptImGui* self, float32 centerXRatio)
{
    ImGui::SetScrollHereX(centerXRatio);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetScrollHereY([[maybe_unused]] ScriptImGui* self)
{
    ImGui::SetScrollHereY();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetScrollHereY([[maybe_unused]] ScriptImGui* self, float32 centerYRatio)
{
    ImGui::SetScrollHereY(centerYRatio);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetScrollFromPosX([[maybe_unused]] ScriptImGui* self, float32 localX)
{
    ImGui::SetScrollFromPosX(localX);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetScrollFromPosX([[maybe_unused]] ScriptImGui* self, float32 localX, float32 centerXRatio)
{
    ImGui::SetScrollFromPosX(localX, centerXRatio);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetScrollFromPosY([[maybe_unused]] ScriptImGui* self, float32 localY)
{
    ImGui::SetScrollFromPosY(localY);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetScrollFromPosY([[maybe_unused]] ScriptImGui* self, float32 localY, float32 centerYRatio)
{
    ImGui::SetScrollFromPosY(localY, centerYRatio);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsAnyItemHovered([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::IsAnyItemHovered();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsAnyItemActive([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::IsAnyItemActive();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Bullet([[maybe_unused]] ScriptImGui* self)
{
    ImGui::Bullet();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SeparatorText([[maybe_unused]] ScriptImGui* self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Separator label arg is empty");
    }

    ImGui::SeparatorText(string(label).c_str());
}

///@ ExportMethod
FO_SCRIPT_API float32 Common_ImGui_GetTreeNodeToLabelSpacing([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::GetTreeNodeToLabelSpacing();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextItemOpen([[maybe_unused]] ScriptImGui* self, bool isOpen)
{
    ImGui::SetNextItemOpen(isOpen);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextItemOpen([[maybe_unused]] ScriptImGui* self, bool isOpen, ImGui_Cond cond)
{
    ImGui::SetNextItemOpen(isOpen, static_cast<ImGuiCond>(cond));
}

///@ ExportMethod
FO_SCRIPT_API float32 Common_ImGui_GetContentRegionAvailX([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::GetContentRegionAvail().x;
}

///@ ExportMethod
FO_SCRIPT_API float32 Common_ImGui_GetContentRegionAvailY([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::GetContentRegionAvail().y;
}

///@ ExportMethod
FO_SCRIPT_API float32 Common_ImGui_GetCursorPosX([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::GetCursorPosX();
}

///@ ExportMethod
FO_SCRIPT_API float32 Common_ImGui_GetCursorPosY([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::GetCursorPosY();
}

///@ ExportMethod
FO_SCRIPT_API fpos32 Common_ImGui_GetCursorScreenPos([[maybe_unused]] ScriptImGui* self)
{
    return ToFPos32(ImGui::GetCursorScreenPos());
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetCursorPos([[maybe_unused]] ScriptImGui* self, float32 x, float32 y)
{
    ImGui::SetCursorPos(ImVec2(x, y));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetCursorScreenPos([[maybe_unused]] ScriptImGui* self, fpos32 pos)
{
    ImGui::SetCursorScreenPos(ToImVec2(pos));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PushTextWrapPos([[maybe_unused]] ScriptImGui* self)
{
    ImGui::PushTextWrapPos();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PushTextWrapPos([[maybe_unused]] ScriptImGui* self, float32 wrapLocalPosX)
{
    ImGui::PushTextWrapPos(wrapLocalPosX);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PopTextWrapPos([[maybe_unused]] ScriptImGui* self)
{
    ImGui::PopTextWrapPos();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetKeyboardFocusHere([[maybe_unused]] ScriptImGui* self)
{
    ImGui::SetKeyboardFocusHere();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetKeyboardFocusHere([[maybe_unused]] ScriptImGui* self, int32 offset)
{
    ImGui::SetKeyboardFocusHere(offset);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_Button([[maybe_unused]] ScriptImGui* self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Button label arg is empty");
    }

    return ImGui::Button(string(label).c_str());
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SameLine([[maybe_unused]] ScriptImGui* self)
{
    ImGui::SameLine();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SameLine([[maybe_unused]] ScriptImGui* self, float32 offsetFromStartX, float32 spacing)
{
    ImGui::SameLine(offsetFromStartX, spacing);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetNextItemWidth([[maybe_unused]] ScriptImGui* self, float32 itemWidth)
{
    ImGui::SetNextItemWidth(itemWidth);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PushItemWidth([[maybe_unused]] ScriptImGui* self, float32 itemWidth)
{
    ImGui::PushItemWidth(itemWidth);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PopItemWidth([[maybe_unused]] ScriptImGui* self)
{
    ImGui::PopItemWidth();
}

///@ ExportMethod
FO_SCRIPT_API float32 Common_ImGui_CalcItemWidth([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::CalcItemWidth();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Dummy([[maybe_unused]] ScriptImGui* self, isize32 size)
{
    ImGui::Dummy(ImVec2(numeric_cast<float32>(size.width), numeric_cast<float32>(size.height)));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Indent([[maybe_unused]] ScriptImGui* self)
{
    ImGui::Indent();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Indent([[maybe_unused]] ScriptImGui* self, float32 indentW)
{
    ImGui::Indent(indentW);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Unindent([[maybe_unused]] ScriptImGui* self)
{
    ImGui::Unindent();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Unindent([[maybe_unused]] ScriptImGui* self, float32 indentW)
{
    ImGui::Unindent(indentW);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_BeginGroup([[maybe_unused]] ScriptImGui* self)
{
    ImGui::BeginGroup();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndGroup([[maybe_unused]] ScriptImGui* self)
{
    ImGui::EndGroup();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_Checkbox([[maybe_unused]] ScriptImGui* self, string_view label, bool& value)
{
    if (label.empty()) {
        throw ScriptException("Checkbox label arg is empty");
    }

    return ImGui::Checkbox(string(label).c_str(), &value);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_CheckboxFlags([[maybe_unused]] ScriptImGui* self, string_view label, int32& flags, int32 flagsValue)
{
    if (label.empty()) {
        throw ScriptException("Checkbox label arg is empty");
    }

    return ImGui::CheckboxFlags(string(label).c_str(), &flags, flagsValue);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_CheckboxFlags([[maybe_unused]] ScriptImGui* self, string_view label, uint32& flags, uint32 flagsValue)
{
    if (label.empty()) {
        throw ScriptException("Checkbox label arg is empty");
    }

    return ImGui::CheckboxFlags(string(label).c_str(), &flags, flagsValue);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputInt([[maybe_unused]] ScriptImGui* self, string_view label, int32& value)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    return ImGui::InputInt(string(label).c_str(), &value);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputInt([[maybe_unused]] ScriptImGui* self, string_view label, int32& value, int32 step, int32 stepFast)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    return ImGui::InputInt(string(label).c_str(), &value, step, stepFast);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputInt([[maybe_unused]] ScriptImGui* self, string_view label, int32& value, int32 step, int32 stepFast, ImGui_InputTextFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    return ImGui::InputInt(string(label).c_str(), &value, step, stepFast, static_cast<ImGuiInputTextFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputInt2([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    int values[2] {valueX, valueY};
    const auto changed = ImGui::InputInt2(string(label).c_str(), values);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputInt2([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, ImGui_InputTextFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    int values[2] {valueX, valueY};
    const auto changed = ImGui::InputInt2(string(label).c_str(), values, static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputInt3([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, int32& valueZ)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    int values[3] {valueX, valueY, valueZ};
    const auto changed = ImGui::InputInt3(string(label).c_str(), values);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputInt3([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, int32& valueZ, ImGui_InputTextFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    int values[3] {valueX, valueY, valueZ};
    const auto changed = ImGui::InputInt3(string(label).c_str(), values, static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputInt4([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, int32& valueZ, int32& valueW)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    int values[4] {valueX, valueY, valueZ, valueW};
    const auto changed = ImGui::InputInt4(string(label).c_str(), values);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputInt4([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, int32& valueZ, int32& valueW, ImGui_InputTextFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    int values[4] {valueX, valueY, valueZ, valueW};
    const auto changed = ImGui::InputInt4(string(label).c_str(), values, static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragFloat([[maybe_unused]] ScriptImGui* self, string_view label, float32& value, float32 speed, float32 minValue, float32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    return ImGui::DragFloat(string(label).c_str(), &value, speed, minValue, maxValue);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragFloat([[maybe_unused]] ScriptImGui* self, string_view label, float32& value, float32 speed, float32 minValue, float32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    return ImGui::DragFloat(string(label).c_str(), &value, speed, minValue, maxValue, "%.3f", static_cast<ImGuiSliderFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragInt([[maybe_unused]] ScriptImGui* self, string_view label, int32& value, float32 speed, int32 minValue, int32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    return ImGui::DragInt(string(label).c_str(), &value, speed, minValue, maxValue);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragInt([[maybe_unused]] ScriptImGui* self, string_view label, int32& value, float32 speed, int32 minValue, int32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    return ImGui::DragInt(string(label).c_str(), &value, speed, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragFloat2([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, float32 speed, float32 minValue, float32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    float32 values[2] {valueX, valueY};
    const auto changed = ImGui::DragFloat2(string(label).c_str(), values, speed, minValue, maxValue);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragFloat2([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, float32 speed, float32 minValue, float32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    float32 values[2] {valueX, valueY};
    const auto changed = ImGui::DragFloat2(string(label).c_str(), values, speed, minValue, maxValue, "%.3f", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragFloat3([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, float32& valueZ, float32 speed, float32 minValue, float32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    float32 values[3] {valueX, valueY, valueZ};
    const auto changed = ImGui::DragFloat3(string(label).c_str(), values, speed, minValue, maxValue);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragFloat3([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, float32& valueZ, float32 speed, float32 minValue, float32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    float32 values[3] {valueX, valueY, valueZ};
    const auto changed = ImGui::DragFloat3(string(label).c_str(), values, speed, minValue, maxValue, "%.3f", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragFloat4([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, float32& valueZ, float32& valueW, float32 speed, float32 minValue, float32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    float32 values[4] {valueX, valueY, valueZ, valueW};
    const auto changed = ImGui::DragFloat4(string(label).c_str(), values, speed, minValue, maxValue);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragFloat4([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, float32& valueZ, float32& valueW, float32 speed, float32 minValue, float32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    float32 values[4] {valueX, valueY, valueZ, valueW};
    const auto changed = ImGui::DragFloat4(string(label).c_str(), values, speed, minValue, maxValue, "%.3f", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragInt2([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, float32 speed, int32 minValue, int32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    int values[2] {valueX, valueY};
    const auto changed = ImGui::DragInt2(string(label).c_str(), values, speed, minValue, maxValue);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragInt2([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, float32 speed, int32 minValue, int32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    int values[2] {valueX, valueY};
    const auto changed = ImGui::DragInt2(string(label).c_str(), values, speed, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragInt3([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, int32& valueZ, float32 speed, int32 minValue, int32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    int values[3] {valueX, valueY, valueZ};
    const auto changed = ImGui::DragInt3(string(label).c_str(), values, speed, minValue, maxValue);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragInt3([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, int32& valueZ, float32 speed, int32 minValue, int32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    int values[3] {valueX, valueY, valueZ};
    const auto changed = ImGui::DragInt3(string(label).c_str(), values, speed, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragInt4([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, int32& valueZ, int32& valueW, float32 speed, int32 minValue, int32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    int values[4] {valueX, valueY, valueZ, valueW};
    const auto changed = ImGui::DragInt4(string(label).c_str(), values, speed, minValue, maxValue);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_DragInt4([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, int32& valueZ, int32& valueW, float32 speed, int32 minValue, int32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Drag label arg is empty");
    }

    int values[4] {valueX, valueY, valueZ, valueW};
    const auto changed = ImGui::DragInt4(string(label).c_str(), values, speed, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderFloat([[maybe_unused]] ScriptImGui* self, string_view label, float32& value, float32 minValue, float32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    return ImGui::SliderFloat(string(label).c_str(), &value, minValue, maxValue);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderInt([[maybe_unused]] ScriptImGui* self, string_view label, int32& value, int32 minValue, int32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    return ImGui::SliderInt(string(label).c_str(), &value, minValue, maxValue);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderInt([[maybe_unused]] ScriptImGui* self, string_view label, int32& value, int32 minValue, int32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    return ImGui::SliderInt(string(label).c_str(), &value, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderFloat2([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, float32 minValue, float32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    float32 values[2] {valueX, valueY};
    const auto changed = ImGui::SliderFloat2(string(label).c_str(), values, minValue, maxValue);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderFloat2([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, float32 minValue, float32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    float32 values[2] {valueX, valueY};
    const auto changed = ImGui::SliderFloat2(string(label).c_str(), values, minValue, maxValue, "%.3f", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderFloat3([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, float32& valueZ, float32 minValue, float32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    float32 values[3] {valueX, valueY, valueZ};
    const auto changed = ImGui::SliderFloat3(string(label).c_str(), values, minValue, maxValue);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderFloat3([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, float32& valueZ, float32 minValue, float32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    float32 values[3] {valueX, valueY, valueZ};
    const auto changed = ImGui::SliderFloat3(string(label).c_str(), values, minValue, maxValue, "%.3f", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderFloat4([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, float32& valueZ, float32& valueW, float32 minValue, float32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    float32 values[4] {valueX, valueY, valueZ, valueW};
    const auto changed = ImGui::SliderFloat4(string(label).c_str(), values, minValue, maxValue);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderFloat4([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, float32& valueZ, float32& valueW, float32 minValue, float32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    float32 values[4] {valueX, valueY, valueZ, valueW};
    const auto changed = ImGui::SliderFloat4(string(label).c_str(), values, minValue, maxValue, "%.3f", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderInt2([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, int32 minValue, int32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    int values[2] {valueX, valueY};
    const auto changed = ImGui::SliderInt2(string(label).c_str(), values, minValue, maxValue);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderInt2([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, int32 minValue, int32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    int values[2] {valueX, valueY};
    const auto changed = ImGui::SliderInt2(string(label).c_str(), values, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderInt3([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, int32& valueZ, int32 minValue, int32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    int values[3] {valueX, valueY, valueZ};
    const auto changed = ImGui::SliderInt3(string(label).c_str(), values, minValue, maxValue);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderInt3([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, int32& valueZ, int32 minValue, int32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    int values[3] {valueX, valueY, valueZ};
    const auto changed = ImGui::SliderInt3(string(label).c_str(), values, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderInt4([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, int32& valueZ, int32& valueW, int32 minValue, int32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    int values[4] {valueX, valueY, valueZ, valueW};
    const auto changed = ImGui::SliderInt4(string(label).c_str(), values, minValue, maxValue);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SliderInt4([[maybe_unused]] ScriptImGui* self, string_view label, int32& valueX, int32& valueY, int32& valueZ, int32& valueW, int32 minValue, int32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    int values[4] {valueX, valueY, valueZ, valueW};
    const auto changed = ImGui::SliderInt4(string(label).c_str(), values, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_VSliderFloat([[maybe_unused]] ScriptImGui* self, string_view label, fsize32 size, float32& value, float32 minValue, float32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    return ImGui::VSliderFloat(string(label).c_str(), ToImVec2(size), &value, minValue, maxValue);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_VSliderFloat([[maybe_unused]] ScriptImGui* self, string_view label, fsize32 size, float32& value, float32 minValue, float32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    return ImGui::VSliderFloat(string(label).c_str(), ToImVec2(size), &value, minValue, maxValue, "%.3f", static_cast<ImGuiSliderFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_VSliderInt([[maybe_unused]] ScriptImGui* self, string_view label, fsize32 size, int32& value, int32 minValue, int32 maxValue)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    return ImGui::VSliderInt(string(label).c_str(), ToImVec2(size), &value, minValue, maxValue);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_VSliderInt([[maybe_unused]] ScriptImGui* self, string_view label, fsize32 size, int32& value, int32 minValue, int32 maxValue, ImGui_SliderFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Slider label arg is empty");
    }

    return ImGui::VSliderInt(string(label).c_str(), ToImVec2(size), &value, minValue, maxValue, "%d", static_cast<ImGuiSliderFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Separator([[maybe_unused]] ScriptImGui* self)
{
    ImGui::Separator();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Spacing([[maybe_unused]] ScriptImGui* self)
{
    ImGui::Spacing();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_NewLine([[maybe_unused]] ScriptImGui* self)
{
    ImGui::NewLine();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginChild([[maybe_unused]] ScriptImGui* self, string_view strId, isize32 size, bool border)
{
    if (strId.empty()) {
        throw ScriptException("Child id arg is empty");
    }

    return ImGui::BeginChild(string(strId).c_str(), ImVec2(numeric_cast<float32>(size.width), numeric_cast<float32>(size.height)), border ? ImGuiChildFlags_Borders : ImGuiChildFlags_None);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginChild([[maybe_unused]] ScriptImGui* self, string_view strId, isize32 size, ImGui_ChildFlags childFlags, ImGui_WindowFlags windowFlags)
{
    if (strId.empty()) {
        throw ScriptException("Child id arg is empty");
    }

    return ImGui::BeginChild(string(strId).c_str(), ImVec2(numeric_cast<float32>(size.width), numeric_cast<float32>(size.height)), static_cast<ImGuiChildFlags>(childFlags), static_cast<ImGuiWindowFlags>(windowFlags));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndChild([[maybe_unused]] ScriptImGui* self)
{
    ImGui::EndChild();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_CollapsingHeader([[maybe_unused]] ScriptImGui* self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Header label arg is empty");
    }

    return ImGui::CollapsingHeader(string(label).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_CollapsingHeader([[maybe_unused]] ScriptImGui* self, string_view label, ImGui_TreeNodeFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Header label arg is empty");
    }

    return ImGui::CollapsingHeader(string(label).c_str(), static_cast<ImGuiTreeNodeFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TreeNode([[maybe_unused]] ScriptImGui* self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Tree node label arg is empty");
    }

    return ImGui::TreeNode(string(label).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TreeNodeEx([[maybe_unused]] ScriptImGui* self, string_view label, ImGui_TreeNodeFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Tree node label arg is empty");
    }

    return ImGui::TreeNodeEx(string(label).c_str(), static_cast<ImGuiTreeNodeFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TreePop([[maybe_unused]] ScriptImGui* self)
{
    ImGui::TreePop();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_Selectable([[maybe_unused]] ScriptImGui* self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Selectable label arg is empty");
    }

    return ImGui::Selectable(string(label).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_Selectable([[maybe_unused]] ScriptImGui* self, string_view label, bool selected, ImGui_SelectableFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Selectable label arg is empty");
    }

    return ImGui::Selectable(string(label).c_str(), selected, static_cast<ImGuiSelectableFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_RadioButton([[maybe_unused]] ScriptImGui* self, string_view label, bool active)
{
    if (label.empty()) {
        throw ScriptException("Radio button label arg is empty");
    }

    return ImGui::RadioButton(string(label).c_str(), active);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_RadioButton([[maybe_unused]] ScriptImGui* self, string_view label, int32& value, int32 buttonValue)
{
    if (label.empty()) {
        throw ScriptException("Radio button label arg is empty");
    }

    return ImGui::RadioButton(string(label).c_str(), &value, buttonValue);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_SmallButton([[maybe_unused]] ScriptImGui* self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Button label arg is empty");
    }

    return ImGui::SmallButton(string(label).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ArrowButton([[maybe_unused]] ScriptImGui* self, string_view strId, ImGui_Dir dir)
{
    if (strId.empty()) {
        throw ScriptException("Arrow button id arg is empty");
    }

    return ImGui::ArrowButton(string(strId).c_str(), static_cast<ImGuiDir>(dir));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InvisibleButton([[maybe_unused]] ScriptImGui* self, string_view strId, fsize32 size)
{
    if (strId.empty()) {
        throw ScriptException("Button id arg is empty");
    }

    return ImGui::InvisibleButton(string(strId).c_str(), ToImVec2(size));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InvisibleButton([[maybe_unused]] ScriptImGui* self, string_view strId, fsize32 size, ImGui_ButtonFlags flags)
{
    if (strId.empty()) {
        throw ScriptException("Button id arg is empty");
    }

    return ImGui::InvisibleButton(string(strId).c_str(), ToImVec2(size), static_cast<ImGuiButtonFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemHovered([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::IsItemHovered();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemHovered([[maybe_unused]] ScriptImGui* self, ImGui_HoveredFlags flags)
{
    return ImGui::IsItemHovered(static_cast<ImGuiHoveredFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemClicked([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::IsItemClicked();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemClicked([[maybe_unused]] ScriptImGui* self, ImGui_MouseButton mouseButton)
{
    return ImGui::IsItemClicked(static_cast<ImGuiMouseButton>(mouseButton));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemActivated([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::IsItemActivated();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemDeactivated([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::IsItemDeactivated();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemDeactivatedAfterEdit([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::IsItemDeactivatedAfterEdit();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemToggledOpen([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::IsItemToggledOpen();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsWindowFocused([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::IsWindowFocused();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsAnyItemFocused([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::IsAnyItemFocused();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsWindowFocused([[maybe_unused]] ScriptImGui* self, ImGui_FocusedFlags flags)
{
    return ImGui::IsWindowFocused(static_cast<ImGuiFocusedFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsWindowHovered([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::IsWindowHovered();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsWindowHovered([[maybe_unused]] ScriptImGui* self, ImGui_HoveredFlags flags)
{
    return ImGui::IsWindowHovered(static_cast<ImGuiHoveredFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemActive([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::IsItemActive();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemEdited([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::IsItemEdited();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsItemVisible([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::IsItemVisible();
}

///@ ExportMethod
FO_SCRIPT_API fpos32 Common_ImGui_GetItemRectMin([[maybe_unused]] ScriptImGui* self)
{
    return ToFPos32(ImGui::GetItemRectMin());
}

///@ ExportMethod
FO_SCRIPT_API fpos32 Common_ImGui_GetItemRectMax([[maybe_unused]] ScriptImGui* self)
{
    return ToFPos32(ImGui::GetItemRectMax());
}

///@ ExportMethod
FO_SCRIPT_API fsize32 Common_ImGui_GetItemRectSize([[maybe_unused]] ScriptImGui* self)
{
    return ToFSize32(ImGui::GetItemRectSize());
}

///@ ExportMethod
FO_SCRIPT_API fpos32 Common_ImGui_GetMousePos([[maybe_unused]] ScriptImGui* self)
{
    return ToFPos32(ImGui::GetMousePos());
}

///@ ExportMethod
FO_SCRIPT_API fpos32 Common_ImGui_GetMousePosOnOpeningCurrentPopup([[maybe_unused]] ScriptImGui* self)
{
    return ToFPos32(ImGui::GetMousePosOnOpeningCurrentPopup());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsMouseDown([[maybe_unused]] ScriptImGui* self, ImGui_MouseButton mouseButton)
{
    return ImGui::IsMouseDown(static_cast<ImGuiMouseButton>(mouseButton));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsMouseClicked([[maybe_unused]] ScriptImGui* self, ImGui_MouseButton mouseButton)
{
    return ImGui::IsMouseClicked(static_cast<ImGuiMouseButton>(mouseButton));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsMouseReleased([[maybe_unused]] ScriptImGui* self, ImGui_MouseButton mouseButton)
{
    return ImGui::IsMouseReleased(static_cast<ImGuiMouseButton>(mouseButton));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsMouseDoubleClicked([[maybe_unused]] ScriptImGui* self, ImGui_MouseButton mouseButton)
{
    return ImGui::IsMouseDoubleClicked(static_cast<ImGuiMouseButton>(mouseButton));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsMouseHoveringRect([[maybe_unused]] ScriptImGui* self, fpos32 rectMin, fpos32 rectMax)
{
    return ImGui::IsMouseHoveringRect(ToImVec2(rectMin), ToImVec2(rectMax));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsMouseHoveringRect([[maybe_unused]] ScriptImGui* self, fpos32 rectMin, fpos32 rectMax, bool clip)
{
    return ImGui::IsMouseHoveringRect(ToImVec2(rectMin), ToImVec2(rectMax), clip);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsMouseDragging([[maybe_unused]] ScriptImGui* self, ImGui_MouseButton mouseButton)
{
    return ImGui::IsMouseDragging(static_cast<ImGuiMouseButton>(mouseButton));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsMouseDragging([[maybe_unused]] ScriptImGui* self, ImGui_MouseButton mouseButton, float32 lockThreshold)
{
    return ImGui::IsMouseDragging(static_cast<ImGuiMouseButton>(mouseButton), lockThreshold);
}

///@ ExportMethod
FO_SCRIPT_API fpos32 Common_ImGui_GetMouseDragDelta([[maybe_unused]] ScriptImGui* self)
{
    return ToFPos32(ImGui::GetMouseDragDelta());
}

///@ ExportMethod
FO_SCRIPT_API fpos32 Common_ImGui_GetMouseDragDelta([[maybe_unused]] ScriptImGui* self, ImGui_MouseButton mouseButton)
{
    return ToFPos32(ImGui::GetMouseDragDelta(static_cast<ImGuiMouseButton>(mouseButton)));
}

///@ ExportMethod
FO_SCRIPT_API fpos32 Common_ImGui_GetMouseDragDelta([[maybe_unused]] ScriptImGui* self, ImGui_MouseButton mouseButton, float32 lockThreshold)
{
    return ToFPos32(ImGui::GetMouseDragDelta(static_cast<ImGuiMouseButton>(mouseButton), lockThreshold));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_ResetMouseDragDelta([[maybe_unused]] ScriptImGui* self)
{
    ImGui::ResetMouseDragDelta();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_ResetMouseDragDelta([[maybe_unused]] ScriptImGui* self, ImGui_MouseButton mouseButton)
{
    ImGui::ResetMouseDragDelta(static_cast<ImGuiMouseButton>(mouseButton));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetItemDefaultFocus([[maybe_unused]] ScriptImGui* self)
{
    ImGui::SetItemDefaultFocus();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetTooltip([[maybe_unused]] ScriptImGui* self, string_view text)
{
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
    ImGui::EndTooltip();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginItemTooltip([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::BeginItemTooltip();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetItemTooltip([[maybe_unused]] ScriptImGui* self, string_view text)
{
    ImGui::SetItemTooltip("%.*s", numeric_cast<int32>(text.size()), string(text).c_str());
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_BeginTooltip([[maybe_unused]] ScriptImGui* self)
{
    ImGui::BeginTooltip();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndTooltip([[maybe_unused]] ScriptImGui* self)
{
    ImGui::EndTooltip();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_OpenPopup([[maybe_unused]] ScriptImGui* self, string_view strId)
{
    if (strId.empty()) {
        throw ScriptException("Popup id arg is empty");
    }

    ImGui::OpenPopup(string(strId).c_str());
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_OpenPopup([[maybe_unused]] ScriptImGui* self, string_view strId, ImGui_PopupFlags popupFlags)
{
    if (strId.empty()) {
        throw ScriptException("Popup id arg is empty");
    }

    ImGui::OpenPopup(string(strId).c_str(), static_cast<ImGuiPopupFlags>(popupFlags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginPopup([[maybe_unused]] ScriptImGui* self, string_view strId)
{
    if (strId.empty()) {
        throw ScriptException("Popup id arg is empty");
    }

    return ImGui::BeginPopup(string(strId).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginPopup([[maybe_unused]] ScriptImGui* self, string_view strId, ImGui_WindowFlags flags)
{
    if (strId.empty()) {
        throw ScriptException("Popup id arg is empty");
    }

    return ImGui::BeginPopup(string(strId).c_str(), static_cast<ImGuiWindowFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginPopupModal([[maybe_unused]] ScriptImGui* self, string_view name)
{
    if (name.empty()) {
        throw ScriptException("Popup name arg is empty");
    }

    return ImGui::BeginPopupModal(string(name).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginPopupModal([[maybe_unused]] ScriptImGui* self, string_view name, bool& opened, ImGui_WindowFlags flags)
{
    if (name.empty()) {
        throw ScriptException("Popup name arg is empty");
    }

    return ImGui::BeginPopupModal(string(name).c_str(), &opened, static_cast<ImGuiWindowFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginPopupContextItem([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonDefault_);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginPopupContextItem([[maybe_unused]] ScriptImGui* self, string_view strId, ImGui_PopupFlags popupFlags)
{
    const auto str_id = string(strId);
    const char* id_ptr = str_id.empty() ? nullptr : str_id.c_str();
    return ImGui::BeginPopupContextItem(id_ptr, static_cast<ImGuiPopupFlags>(popupFlags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginPopupContextWindow([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonDefault_);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginPopupContextWindow([[maybe_unused]] ScriptImGui* self, string_view strId, ImGui_PopupFlags popupFlags)
{
    const auto str_id = string(strId);
    const char* id_ptr = str_id.empty() ? nullptr : str_id.c_str();
    return ImGui::BeginPopupContextWindow(id_ptr, static_cast<ImGuiPopupFlags>(popupFlags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginPopupContextVoid([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::BeginPopupContextVoid();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginPopupContextVoid([[maybe_unused]] ScriptImGui* self, string_view strId, ImGui_PopupFlags popupFlags)
{
    const auto str_id = string(strId);
    const char* id_ptr = str_id.empty() ? nullptr : str_id.c_str();
    return ImGui::BeginPopupContextVoid(id_ptr, static_cast<ImGuiPopupFlags>(popupFlags));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndPopup([[maybe_unused]] ScriptImGui* self)
{
    ImGui::EndPopup();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_CloseCurrentPopup([[maybe_unused]] ScriptImGui* self)
{
    ImGui::CloseCurrentPopup();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsPopupOpen([[maybe_unused]] ScriptImGui* self, string_view strId)
{
    if (strId.empty()) {
        throw ScriptException("Popup id arg is empty");
    }

    return ImGui::IsPopupOpen(string(strId).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_IsPopupOpen([[maybe_unused]] ScriptImGui* self, string_view strId, ImGui_PopupFlags popupFlags)
{
    if (strId.empty()) {
        throw ScriptException("Popup id arg is empty");
    }

    return ImGui::IsPopupOpen(string(strId).c_str(), static_cast<ImGuiPopupFlags>(popupFlags));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_OpenPopupOnItemClick([[maybe_unused]] ScriptImGui* self)
{
    ImGui::OpenPopupOnItemClick();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_OpenPopupOnItemClick([[maybe_unused]] ScriptImGui* self, string_view strId, ImGui_PopupFlags popupFlags)
{
    const auto str_id = string(strId);
    const char* id_ptr = str_id.empty() ? nullptr : str_id.c_str();
    ImGui::OpenPopupOnItemClick(id_ptr, static_cast<ImGuiPopupFlags>(popupFlags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginMainMenuBar([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::BeginMainMenuBar();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndMainMenuBar([[maybe_unused]] ScriptImGui* self)
{
    ImGui::EndMainMenuBar();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginMenuBar([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::BeginMenuBar();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndMenuBar([[maybe_unused]] ScriptImGui* self)
{
    ImGui::EndMenuBar();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginMenu([[maybe_unused]] ScriptImGui* self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Menu label arg is empty");
    }

    return ImGui::BeginMenu(string(label).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginMenu([[maybe_unused]] ScriptImGui* self, string_view label, bool enabled)
{
    if (label.empty()) {
        throw ScriptException("Menu label arg is empty");
    }

    return ImGui::BeginMenu(string(label).c_str(), enabled);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndMenu([[maybe_unused]] ScriptImGui* self)
{
    ImGui::EndMenu();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_MenuItem([[maybe_unused]] ScriptImGui* self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Menu item label arg is empty");
    }

    return ImGui::MenuItem(string(label).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_MenuItem([[maybe_unused]] ScriptImGui* self, string_view label, bool selected, bool enabled)
{
    if (label.empty()) {
        throw ScriptException("Menu item label arg is empty");
    }

    return ImGui::MenuItem(string(label).c_str(), nullptr, selected, enabled);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginTable([[maybe_unused]] ScriptImGui* self, string_view strId, int32 columns)
{
    if (strId.empty()) {
        throw ScriptException("Table id arg is empty");
    }
    if (columns <= 0) {
        throw ScriptException("Columns arg must be greater than zero");
    }

    return ImGui::BeginTable(string(strId).c_str(), columns);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginTable([[maybe_unused]] ScriptImGui* self, string_view strId, int32 columns, ImGui_TableFlags flags)
{
    if (strId.empty()) {
        throw ScriptException("Table id arg is empty");
    }
    if (columns <= 0) {
        throw ScriptException("Columns arg must be greater than zero");
    }

    return ImGui::BeginTable(string(strId).c_str(), columns, static_cast<ImGuiTableFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndTable([[maybe_unused]] ScriptImGui* self)
{
    ImGui::EndTable();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableNextRow([[maybe_unused]] ScriptImGui* self)
{
    ImGui::TableNextRow();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableNextRow([[maybe_unused]] ScriptImGui* self, ImGui_TableRowFlags rowFlags)
{
    ImGui::TableNextRow(static_cast<ImGuiTableRowFlags>(rowFlags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TableNextColumn([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::TableNextColumn();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TableSetColumnIndex([[maybe_unused]] ScriptImGui* self, int32 columnN)
{
    return ImGui::TableSetColumnIndex(columnN);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableHeadersRow([[maybe_unused]] ScriptImGui* self)
{
    ImGui::TableHeadersRow();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableHeader([[maybe_unused]] ScriptImGui* self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Table header label arg is empty");
    }

    ImGui::TableHeader(string(label).c_str());
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableSetupColumn([[maybe_unused]] ScriptImGui* self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Table column label arg is empty");
    }

    ImGui::TableSetupColumn(string(label).c_str());
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableSetupColumn([[maybe_unused]] ScriptImGui* self, string_view label, ImGui_TableColumnFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Table column label arg is empty");
    }

    ImGui::TableSetupColumn(string(label).c_str(), static_cast<ImGuiTableColumnFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableSetupScrollFreeze([[maybe_unused]] ScriptImGui* self, int32 cols, int32 rows)
{
    ImGui::TableSetupScrollFreeze(cols, rows);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableSetBgColor([[maybe_unused]] ScriptImGui* self, ImGui_TableBgTarget target, ucolor color)
{
    ImGui::TableSetBgColor(static_cast<ImGuiTableBgTarget>(target), ToImU32(color));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableSetBgColor([[maybe_unused]] ScriptImGui* self, ImGui_TableBgTarget target, ucolor color, int32 columnN)
{
    ImGui::TableSetBgColor(static_cast<ImGuiTableBgTarget>(target), ToImU32(color), columnN);
}

///@ ExportMethod
FO_SCRIPT_API int32 Common_ImGui_TableGetColumnCount([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::TableGetColumnCount();
}

///@ ExportMethod
FO_SCRIPT_API int32 Common_ImGui_TableGetColumnIndex([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::TableGetColumnIndex();
}

///@ ExportMethod
FO_SCRIPT_API int32 Common_ImGui_TableGetRowIndex([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::TableGetRowIndex();
}

///@ ExportMethod
FO_SCRIPT_API string Common_ImGui_TableGetColumnName([[maybe_unused]] ScriptImGui* self)
{
    return string(ImGui::TableGetColumnName());
}

///@ ExportMethod
FO_SCRIPT_API string Common_ImGui_TableGetColumnName([[maybe_unused]] ScriptImGui* self, int32 columnN)
{
    return string(ImGui::TableGetColumnName(columnN));
}

///@ ExportMethod
FO_SCRIPT_API ImGui_TableColumnFlags Common_ImGui_TableGetColumnFlags([[maybe_unused]] ScriptImGui* self)
{
    return static_cast<ImGui_TableColumnFlags>(ImGui::TableGetColumnFlags());
}

///@ ExportMethod
FO_SCRIPT_API ImGui_TableColumnFlags Common_ImGui_TableGetColumnFlags([[maybe_unused]] ScriptImGui* self, int32 columnN)
{
    return static_cast<ImGui_TableColumnFlags>(ImGui::TableGetColumnFlags(columnN));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableSetColumnEnabled([[maybe_unused]] ScriptImGui* self, int32 columnN, bool enabled)
{
    ImGui::TableSetColumnEnabled(columnN, enabled);
}

///@ ExportMethod
FO_SCRIPT_API int32 Common_ImGui_TableGetHoveredColumn([[maybe_unused]] ScriptImGui* self)
{
    return ImGui::TableGetHoveredColumn();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TableAngledHeadersRow([[maybe_unused]] ScriptImGui* self)
{
    ImGui::TableAngledHeadersRow();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginTabBar([[maybe_unused]] ScriptImGui* self, string_view strId)
{
    if (strId.empty()) {
        throw ScriptException("Tab bar id arg is empty");
    }

    return ImGui::BeginTabBar(string(strId).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginTabBar([[maybe_unused]] ScriptImGui* self, string_view strId, ImGui_TabBarFlags flags)
{
    if (strId.empty()) {
        throw ScriptException("Tab bar id arg is empty");
    }

    return ImGui::BeginTabBar(string(strId).c_str(), static_cast<ImGuiTabBarFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndTabBar([[maybe_unused]] ScriptImGui* self)
{
    ImGui::EndTabBar();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginTabItem([[maybe_unused]] ScriptImGui* self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Tab item label arg is empty");
    }

    return ImGui::BeginTabItem(string(label).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginTabItem([[maybe_unused]] ScriptImGui* self, string_view label, ImGui_TabItemFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Tab item label arg is empty");
    }

    return ImGui::BeginTabItem(string(label).c_str(), nullptr, static_cast<ImGuiTabItemFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginTabItem([[maybe_unused]] ScriptImGui* self, string_view label, bool& opened)
{
    if (label.empty()) {
        throw ScriptException("Tab item label arg is empty");
    }

    return ImGui::BeginTabItem(string(label).c_str(), &opened);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginTabItem([[maybe_unused]] ScriptImGui* self, string_view label, bool& opened, ImGui_TabItemFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Tab item label arg is empty");
    }

    return ImGui::BeginTabItem(string(label).c_str(), &opened, static_cast<ImGuiTabItemFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndTabItem([[maybe_unused]] ScriptImGui* self)
{
    ImGui::EndTabItem();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TabItemButton([[maybe_unused]] ScriptImGui* self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Tab item label arg is empty");
    }

    return ImGui::TabItemButton(string(label).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TabItemButton([[maybe_unused]] ScriptImGui* self, string_view label, ImGui_TabItemFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Tab item label arg is empty");
    }

    return ImGui::TabItemButton(string(label).c_str(), static_cast<ImGuiTabItemFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetTabItemClosed([[maybe_unused]] ScriptImGui* self, string_view tabLabel)
{
    if (tabLabel.empty()) {
        throw ScriptException("Tab label arg is empty");
    }

    ImGui::SetTabItemClosed(string(tabLabel).c_str());
}

// ReSharper disable once CppInconsistentNaming
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LogToTTY([[maybe_unused]] ScriptImGui* self)
{
    ImGui::LogToTTY();
}

// ReSharper disable once CppInconsistentNaming
///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LogToTTY([[maybe_unused]] ScriptImGui* self, int32 autoOpenDepth)
{
    ImGui::LogToTTY(autoOpenDepth);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LogToFile([[maybe_unused]] ScriptImGui* self)
{
    ImGui::LogToFile();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LogToFile([[maybe_unused]] ScriptImGui* self, int32 autoOpenDepth, string_view filename)
{
    const auto filename_str = string(filename);
    ImGui::LogToFile(autoOpenDepth, filename_str.empty() ? nullptr : filename_str.c_str());
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LogToClipboard([[maybe_unused]] ScriptImGui* self)
{
    ImGui::LogToClipboard();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LogToClipboard([[maybe_unused]] ScriptImGui* self, int32 autoOpenDepth)
{
    ImGui::LogToClipboard(autoOpenDepth);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LogFinish([[maybe_unused]] ScriptImGui* self)
{
    ImGui::LogFinish();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LogButtons([[maybe_unused]] ScriptImGui* self)
{
    ImGui::LogButtons();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LogText([[maybe_unused]] ScriptImGui* self, string_view text)
{
    ImGui::LogText("%.*s", numeric_cast<int32>(text.size()), string(text).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginCombo([[maybe_unused]] ScriptImGui* self, string_view label, string_view previewValue)
{
    if (label.empty()) {
        throw ScriptException("Combo label arg is empty");
    }

    return ImGui::BeginCombo(string(label).c_str(), string(previewValue).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginCombo([[maybe_unused]] ScriptImGui* self, string_view label, string_view previewValue, ImGui_ComboFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Combo label arg is empty");
    }

    return ImGui::BeginCombo(string(label).c_str(), string(previewValue).c_str(), static_cast<ImGuiComboFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_Combo([[maybe_unused]] ScriptImGui* self, string_view label, int32& currentItem, string_view itemsSeparatedByZeros)
{
    if (label.empty()) {
        throw ScriptException("Combo label arg is empty");
    }

    const auto items = string(itemsSeparatedByZeros);
    return ImGui::Combo(string(label).c_str(), &currentItem, items.c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_Combo([[maybe_unused]] ScriptImGui* self, string_view label, int32& currentItem, string_view itemsSeparatedByZeros, int32 popupMaxHeightInItems)
{
    if (label.empty()) {
        throw ScriptException("Combo label arg is empty");
    }

    const auto items = string(itemsSeparatedByZeros);
    return ImGui::Combo(string(label).c_str(), &currentItem, items.c_str(), popupMaxHeightInItems);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndCombo([[maybe_unused]] ScriptImGui* self)
{
    ImGui::EndCombo();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginListBox([[maybe_unused]] ScriptImGui* self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("List box label arg is empty");
    }

    return ImGui::BeginListBox(string(label).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_BeginListBox([[maybe_unused]] ScriptImGui* self, string_view label, isize32 size)
{
    if (label.empty()) {
        throw ScriptException("List box label arg is empty");
    }

    return ImGui::BeginListBox(string(label).c_str(), ImVec2(numeric_cast<float32>(size.width), numeric_cast<float32>(size.height)));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndListBox([[maybe_unused]] ScriptImGui* self)
{
    ImGui::EndListBox();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_ProgressBar([[maybe_unused]] ScriptImGui* self, float32 fraction)
{
    ImGui::ProgressBar(fraction);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_ProgressBar([[maybe_unused]] ScriptImGui* self, float32 fraction, isize32 size)
{
    ImGui::ProgressBar(fraction, ToImVec2(size));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_ProgressBar([[maybe_unused]] ScriptImGui* self, float32 fraction, isize32 size, string_view overlay)
{
    const auto overlay_text = string(overlay);
    const char* overlay_str = overlay_text.empty() ? nullptr : overlay_text.c_str();
    ImGui::ProgressBar(fraction, ToImVec2(size), overlay_str);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_BulletText([[maybe_unused]] ScriptImGui* self, string_view text)
{
    ImGui::BulletText("%s", string(text).c_str());
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TextLink([[maybe_unused]] ScriptImGui* self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Link label arg is empty");
    }

    return ImGui::TextLink(string(label).c_str());
}

// ReSharper disable once CppInconsistentNaming
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TextLinkOpenURL([[maybe_unused]] ScriptImGui* self, string_view label)
{
    if (label.empty()) {
        throw ScriptException("Link label arg is empty");
    }

    return ImGui::TextLinkOpenURL(string(label).c_str());
}

// ReSharper disable once CppInconsistentNaming
///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_TextLinkOpenURL([[maybe_unused]] ScriptImGui* self, string_view label, string_view url)
{
    if (label.empty()) {
        throw ScriptException("Link label arg is empty");
    }
    if (url.empty()) {
        throw ScriptException("Url arg is empty");
    }

    return ImGui::TextLinkOpenURL(string(label).c_str(), string(url).c_str());
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PlotLines([[maybe_unused]] ScriptImGui* self, string_view label, readonly_vector<float32> values)
{
    if (label.empty()) {
        throw ScriptException("Plot label arg is empty");
    }

    ImGui::PlotLines(string(label).c_str(), values.empty() ? nullptr : values.data(), numeric_cast<int32>(values.size()));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PlotLines([[maybe_unused]] ScriptImGui* self, string_view label, readonly_vector<float32> values, int32 valuesOffset, string_view overlay, float32 scaleMin, float32 scaleMax, fsize32 graphSize)
{
    if (label.empty()) {
        throw ScriptException("Plot label arg is empty");
    }

    const auto overlay_text = string(overlay);
    const char* overlay_str = overlay_text.empty() ? nullptr : overlay_text.c_str();
    ImGui::PlotLines(string(label).c_str(), values.empty() ? nullptr : values.data(), numeric_cast<int32>(values.size()), valuesOffset, overlay_str, scaleMin, scaleMax, ToImVec2(graphSize));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PlotHistogram([[maybe_unused]] ScriptImGui* self, string_view label, readonly_vector<float32> values)
{
    if (label.empty()) {
        throw ScriptException("Plot label arg is empty");
    }

    ImGui::PlotHistogram(string(label).c_str(), values.empty() ? nullptr : values.data(), numeric_cast<int32>(values.size()));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_PlotHistogram([[maybe_unused]] ScriptImGui* self, string_view label, readonly_vector<float32> values, int32 valuesOffset, string_view overlay, float32 scaleMin, float32 scaleMax, fsize32 graphSize)
{
    if (label.empty()) {
        throw ScriptException("Plot label arg is empty");
    }

    const auto overlay_text = string(overlay);
    const char* overlay_str = overlay_text.empty() ? nullptr : overlay_text.c_str();
    ImGui::PlotHistogram(string(label).c_str(), values.empty() ? nullptr : values.data(), numeric_cast<int32>(values.size()), valuesOffset, overlay_str, scaleMin, scaleMax, ToImVec2(graphSize));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_TextColored([[maybe_unused]] ScriptImGui* self, string_view text, float32 r, float32 g, float32 b, float32 a)
{
    ImGui::TextColored(ImVec4(r, g, b, a), "%s", string(text).c_str());
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Value([[maybe_unused]] ScriptImGui* self, string_view prefix, bool value)
{
    if (prefix.empty()) {
        throw ScriptException("Value prefix arg is empty");
    }

    ImGui::Value(string(prefix).c_str(), value);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Value([[maybe_unused]] ScriptImGui* self, string_view prefix, int32 value)
{
    if (prefix.empty()) {
        throw ScriptException("Value prefix arg is empty");
    }

    ImGui::Value(string(prefix).c_str(), value);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Value([[maybe_unused]] ScriptImGui* self, string_view prefix, uint32 value)
{
    if (prefix.empty()) {
        throw ScriptException("Value prefix arg is empty");
    }

    ImGui::Value(string(prefix).c_str(), value);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_Value([[maybe_unused]] ScriptImGui* self, string_view prefix, float32 value)
{
    if (prefix.empty()) {
        throw ScriptException("Value prefix arg is empty");
    }

    ImGui::Value(string(prefix).c_str(), value);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ColorEdit3([[maybe_unused]] ScriptImGui* self, string_view label, ucolor& color)
{
    if (label.empty()) {
        throw ScriptException("Color label arg is empty");
    }

    float32 values[3];
    ColorToFloat3(color, values);
    const auto changed = ImGui::ColorEdit3(string(label).c_str(), values);

    if (changed) {
        StoreColor3(color, values);
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ColorEdit3([[maybe_unused]] ScriptImGui* self, string_view label, ucolor& color, ImGui_ColorEditFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Color label arg is empty");
    }

    float32 values[3];
    ColorToFloat3(color, values);
    const auto changed = ImGui::ColorEdit3(string(label).c_str(), values, static_cast<ImGuiColorEditFlags>(flags));

    if (changed) {
        StoreColor3(color, values);
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ColorEdit4([[maybe_unused]] ScriptImGui* self, string_view label, ucolor& color)
{
    if (label.empty()) {
        throw ScriptException("Color label arg is empty");
    }

    float32 values[4];
    ColorToFloat4(color, values);
    const auto changed = ImGui::ColorEdit4(string(label).c_str(), values);

    if (changed) {
        StoreColor4(color, values);
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ColorEdit4([[maybe_unused]] ScriptImGui* self, string_view label, ucolor& color, ImGui_ColorEditFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Color label arg is empty");
    }

    float32 values[4];
    ColorToFloat4(color, values);
    const auto changed = ImGui::ColorEdit4(string(label).c_str(), values, static_cast<ImGuiColorEditFlags>(flags));

    if (changed) {
        StoreColor4(color, values);
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ColorPicker3([[maybe_unused]] ScriptImGui* self, string_view label, ucolor& color)
{
    if (label.empty()) {
        throw ScriptException("Color label arg is empty");
    }

    float32 values[3];
    ColorToFloat3(color, values);
    const auto changed = ImGui::ColorPicker3(string(label).c_str(), values);

    if (changed) {
        StoreColor3(color, values);
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ColorPicker3([[maybe_unused]] ScriptImGui* self, string_view label, ucolor& color, ImGui_ColorEditFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Color label arg is empty");
    }

    float32 values[3];
    ColorToFloat3(color, values);
    const auto changed = ImGui::ColorPicker3(string(label).c_str(), values, static_cast<ImGuiColorEditFlags>(flags));

    if (changed) {
        StoreColor3(color, values);
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ColorPicker4([[maybe_unused]] ScriptImGui* self, string_view label, ucolor& color)
{
    if (label.empty()) {
        throw ScriptException("Color label arg is empty");
    }

    float32 values[4];
    ColorToFloat4(color, values);
    const auto changed = ImGui::ColorPicker4(string(label).c_str(), values);

    if (changed) {
        StoreColor4(color, values);
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ColorPicker4([[maybe_unused]] ScriptImGui* self, string_view label, ucolor& color, ImGui_ColorEditFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Color label arg is empty");
    }

    float32 values[4];
    ColorToFloat4(color, values);
    const auto changed = ImGui::ColorPicker4(string(label).c_str(), values, static_cast<ImGuiColorEditFlags>(flags));

    if (changed) {
        StoreColor4(color, values);
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ColorButton([[maybe_unused]] ScriptImGui* self, string_view descId, ucolor color)
{
    if (descId.empty()) {
        throw ScriptException("Color button id arg is empty");
    }

    float32 values[4];
    ColorToFloat4(color, values);
    return ImGui::ColorButton(string(descId).c_str(), ImVec4(values[0], values[1], values[2], values[3]));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ColorButton([[maybe_unused]] ScriptImGui* self, string_view descId, ucolor color, ImGui_ColorEditFlags flags)
{
    if (descId.empty()) {
        throw ScriptException("Color button id arg is empty");
    }

    float32 values[4];
    ColorToFloat4(color, values);
    return ImGui::ColorButton(string(descId).c_str(), ImVec4(values[0], values[1], values[2], values[3]), static_cast<ImGuiColorEditFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_ColorButton([[maybe_unused]] ScriptImGui* self, string_view descId, ucolor color, ImGui_ColorEditFlags flags, fsize32 size)
{
    if (descId.empty()) {
        throw ScriptException("Color button id arg is empty");
    }

    float32 values[4];
    ColorToFloat4(color, values);
    return ImGui::ColorButton(string(descId).c_str(), ImVec4(values[0], values[1], values[2], values[3]), static_cast<ImGuiColorEditFlags>(flags), ToImVec2(size));
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetColorEditOptions([[maybe_unused]] ScriptImGui* self, ImGui_ColorEditFlags flags)
{
    ImGui::SetColorEditOptions(static_cast<ImGuiColorEditFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputFloat([[maybe_unused]] ScriptImGui* self, string_view label, float32& value)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    return ImGui::InputFloat(string(label).c_str(), &value);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputFloat([[maybe_unused]] ScriptImGui* self, string_view label, float32& value, float32 step, float32 stepFast)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    return ImGui::InputFloat(string(label).c_str(), &value, step, stepFast);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputFloat([[maybe_unused]] ScriptImGui* self, string_view label, float32& value, float32 step, float32 stepFast, ImGui_InputTextFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    return ImGui::InputFloat(string(label).c_str(), &value, step, stepFast, "%.3f", static_cast<ImGuiInputTextFlags>(flags));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputFloat2([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    float32 values[2] {valueX, valueY};
    const auto changed = ImGui::InputFloat2(string(label).c_str(), values);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputFloat2([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, ImGui_InputTextFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    float32 values[2] {valueX, valueY};
    const auto changed = ImGui::InputFloat2(string(label).c_str(), values, "%.3f", static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputFloat3([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, float32& valueZ)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    float32 values[3] {valueX, valueY, valueZ};
    const auto changed = ImGui::InputFloat3(string(label).c_str(), values);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputFloat3([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, float32& valueZ, ImGui_InputTextFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    float32 values[3] {valueX, valueY, valueZ};
    const auto changed = ImGui::InputFloat3(string(label).c_str(), values, "%.3f", static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputFloat4([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, float32& valueZ, float32& valueW)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    float32 values[4] {valueX, valueY, valueZ, valueW};
    const auto changed = ImGui::InputFloat4(string(label).c_str(), values);

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputFloat4([[maybe_unused]] ScriptImGui* self, string_view label, float32& valueX, float32& valueY, float32& valueZ, float32& valueW, ImGui_InputTextFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }

    float32 values[4] {valueX, valueY, valueZ, valueW};
    const auto changed = ImGui::InputFloat4(string(label).c_str(), values, "%.3f", static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        valueX = values[0];
        valueY = values[1];
        valueZ = values[2];
        valueW = values[3];
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputText([[maybe_unused]] ScriptImGui* self, string_view label, string& value, uint32 maxLength)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }
    if (maxLength == 0) {
        throw ScriptException("Max length arg must be greater than zero");
    }

    auto buffer = PrepareInputBuffer(value, maxLength);
    const auto changed = ImGui::InputText(string(label).c_str(), buffer.data(), buffer.size());

    if (changed) {
        value = buffer.data();
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputText([[maybe_unused]] ScriptImGui* self, string_view label, string& value, uint32 maxLength, ImGui_InputTextFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }
    if (maxLength == 0) {
        throw ScriptException("Max length arg must be greater than zero");
    }

    auto buffer = PrepareInputBuffer(value, maxLength);
    const auto changed = ImGui::InputText(string(label).c_str(), buffer.data(), buffer.size(), static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        value = buffer.data();
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputTextMultiline([[maybe_unused]] ScriptImGui* self, string_view label, string& value, uint32 maxLength, isize32 size)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }
    if (maxLength == 0) {
        throw ScriptException("Max length arg must be greater than zero");
    }

    auto buffer = PrepareInputBuffer(value, maxLength);
    const auto changed = ImGui::InputTextMultiline(string(label).c_str(), buffer.data(), buffer.size(), ImVec2(numeric_cast<float32>(size.width), numeric_cast<float32>(size.height)));

    if (changed) {
        value = buffer.data();
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputTextMultiline([[maybe_unused]] ScriptImGui* self, string_view label, string& value, uint32 maxLength, isize32 size, ImGui_InputTextFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }
    if (maxLength == 0) {
        throw ScriptException("Max length arg must be greater than zero");
    }

    auto buffer = PrepareInputBuffer(value, maxLength);
    const auto changed = ImGui::InputTextMultiline(string(label).c_str(), buffer.data(), buffer.size(), ImVec2(numeric_cast<float32>(size.width), numeric_cast<float32>(size.height)), static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        value = buffer.data();
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputTextWithHint([[maybe_unused]] ScriptImGui* self, string_view label, string_view hint, string& value, uint32 maxLength)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }
    if (maxLength == 0) {
        throw ScriptException("Max length arg must be greater than zero");
    }

    const auto label_text = string(label);
    const auto hint_text = string(hint);
    auto buffer = PrepareInputBuffer(value, maxLength);
    const auto changed = ImGui::InputTextWithHint(label_text.c_str(), hint_text.c_str(), buffer.data(), buffer.size());

    if (changed) {
        value = buffer.data();
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API bool Common_ImGui_InputTextWithHint([[maybe_unused]] ScriptImGui* self, string_view label, string_view hint, string& value, uint32 maxLength, ImGui_InputTextFlags flags)
{
    if (label.empty()) {
        throw ScriptException("Input label arg is empty");
    }
    if (maxLength == 0) {
        throw ScriptException("Max length arg must be greater than zero");
    }

    const auto label_text = string(label);
    const auto hint_text = string(hint);
    auto buffer = PrepareInputBuffer(value, maxLength);
    const auto changed = ImGui::InputTextWithHint(label_text.c_str(), hint_text.c_str(), buffer.data(), buffer.size(), static_cast<ImGuiInputTextFlags>(flags));

    if (changed) {
        value = buffer.data();
    }

    return changed;
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_BeginDisabled([[maybe_unused]] ScriptImGui* self)
{
    ImGui::BeginDisabled();
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_BeginDisabled([[maybe_unused]] ScriptImGui* self, bool disabled)
{
    ImGui::BeginDisabled(disabled);
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_EndDisabled([[maybe_unused]] ScriptImGui* self)
{
    ImGui::EndDisabled();
}

///@ ExportMethod
FO_SCRIPT_API string Common_ImGui_GetClipboardText([[maybe_unused]] ScriptImGui* self)
{
    const auto* text = ImGui::GetClipboardText();
    return text != nullptr ? string(text) : string {};
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SetClipboardText([[maybe_unused]] ScriptImGui* self, string_view text)
{
    ImGui::SetClipboardText(string(text).c_str());
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LoadIniSettingsFromDisk([[maybe_unused]] ScriptImGui* self, string_view iniFilename)
{
    if (iniFilename.empty()) {
        throw ScriptException("Ini filename arg is empty");
    }

    ImGui::LoadIniSettingsFromDisk(string(iniFilename).c_str());
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_SaveIniSettingsToDisk([[maybe_unused]] ScriptImGui* self, string_view iniFilename)
{
    if (iniFilename.empty()) {
        throw ScriptException("Ini filename arg is empty");
    }

    ImGui::SaveIniSettingsToDisk(string(iniFilename).c_str());
}

///@ ExportMethod
FO_SCRIPT_API void Common_ImGui_LoadIniSettingsFromMemory([[maybe_unused]] ScriptImGui* self, string_view iniData)
{
    ImGui::LoadIniSettingsFromMemory(iniData.data(), iniData.size());
}

///@ ExportMethod
FO_SCRIPT_API string Common_ImGui_SaveIniSettingsToMemory([[maybe_unused]] ScriptImGui* self)
{
    size_t ini_size {};
    const auto* data = ImGui::SaveIniSettingsToMemory(&ini_size);
    return data != nullptr ? string(data, ini_size) : string {};
}

// Keep script enum bindings in sync with upstream Dear ImGui constants.
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
