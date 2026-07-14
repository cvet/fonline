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

#include "Client.h"
#include "ImGuiStuff.h"

FO_BEGIN_NAMESPACE

static constexpr float32_t IMGUI_UV_EPSILON = 0.001f;

static void CheckImageSize(fsize32 image_size)
{
    FO_STACK_TRACE_ENTRY();

    if (image_size.width <= 0.0f || image_size.height <= 0.0f) {
        throw ScriptException("Image size args must be positive");
    }
}

static void CheckUvRange(fpos32 uv0, fpos32 uv1)
{
    FO_STACK_TRACE_ENTRY();

    const auto in_range = [](float32_t value) noexcept -> bool { return value >= -IMGUI_UV_EPSILON && value <= 1.0f + IMGUI_UV_EPSILON; };

    if (!in_range(uv0.x) || !in_range(uv0.y) || !in_range(uv1.x) || !in_range(uv1.y)) {
        throw ScriptException("Image UV args must be in [0, 1] range");
    }
}

static auto IsFullUvRange(fpos32 uv0, fpos32 uv1) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return std::abs(uv0.x) <= IMGUI_UV_EPSILON && std::abs(uv0.y) <= IMGUI_UV_EPSILON && std::abs(uv1.x - 1.0f) <= IMGUI_UV_EPSILON && std::abs(uv1.y - 1.0f) <= IMGUI_UV_EPSILON;
}

static auto ResolveTintColor(ucolor tint_color) noexcept -> ucolor
{
    FO_STACK_TRACE_ENTRY();

    return tint_color == ucolor::clear ? Color::Neutral : tint_color;
}

static auto MakeItemRect(const ImVec2& min_pos, const ImVec2& max_pos) -> irect32
{
    FO_STACK_TRACE_ENTRY();

    const auto left = iround<int32_t>(min_pos.x);
    const auto top = iround<int32_t>(min_pos.y);
    const auto right = std::max(left, iround<int32_t>(max_pos.x));
    const auto bottom = std::max(top, iround<int32_t>(max_pos.y));
    return {left, top, right - left, bottom - top};
}

static void RenderImageButtonFrame(ucolor bg_color)
{
    FO_STACK_TRACE_ENTRY();

    const auto item_min = ImGui::GetItemRectMin();
    const auto item_max = ImGui::GetItemRectMax();
    const auto& style = ImGui::GetStyle();

    const auto frame_color = ImGui::GetColorU32(ImGui::IsItemActive() ? ImGuiCol_ButtonActive : ImGui::IsItemHovered() ? ImGuiCol_ButtonHovered : ImGuiCol_Button);

    ImGui::RenderFrame(item_min, item_max, frame_color, true, style.FrameRounding);

    if (bg_color != ucolor::clear) {
        auto draw_list = make_ptr(ImGui::GetWindowDrawList());
        draw_list->AddRectFilled(item_min, item_max, ToImU32(bg_color), style.FrameRounding);
    }
}

static void DrawItemSprite(ptr<ClientEngine> client_ptr, uint32_t spr_id, fsize32 image_size, ucolor tint_color)
{
    FO_STACK_TRACE_ENTRY();

    auto sprite = client_ptr->AnimGetSpr(spr_id);

    if (!sprite) {
        return;
    }

    const auto min_pos = ImGui::GetItemRectMin();
    const auto max_pos = ImGui::GetItemRectMax();
    const auto item_rect = MakeItemRect(min_pos, max_pos);

    client_ptr->SprMngr.PushScissor(item_rect);
    auto scissor_guard = scope_fail([&]() noexcept { safe_call([&] { client_ptr->SprMngr.PopScissor(); }); });
    client_ptr->SprMngr.DrawSpriteSizeExt(sprite, {min_pos.x, min_pos.y}, image_size, false, false, true, ResolveTintColor(tint_color));
    scissor_guard.release();
    client_ptr->SprMngr.PopScissor();
}

static void DrawItemSprite(ptr<ClientEngine> client_ptr, uint32_t spr_id, fsize32 image_size, fpos32 uv0, fpos32 uv1, ucolor tint_color)
{
    FO_STACK_TRACE_ENTRY();

    if (IsFullUvRange(uv0, uv1)) {
        DrawItemSprite(client_ptr, spr_id, image_size, tint_color);
        return;
    }

    auto sprite = client_ptr->AnimGetSpr(spr_id);

    if (!sprite) {
        return;
    }

    const auto min_pos = ImGui::GetItemRectMin();
    const auto max_pos = ImGui::GetItemRectMax();
    const auto item_rect = MakeItemRect(min_pos, max_pos);

    client_ptr->SprMngr.PushScissor(item_rect);
    auto scissor_guard = scope_fail([&]() noexcept { safe_call([&] { client_ptr->SprMngr.PopScissor(); }); });
    const auto drawn = client_ptr->SprMngr.DrawSpriteRegion(sprite, uv0, uv1, {min_pos.x, min_pos.y}, image_size, ResolveTintColor(tint_color));
    scissor_guard.release();
    client_ptr->SprMngr.PopScissor();

    if (!drawn) {
        throw ScriptException("Partial UV is supported only for atlas-based sprites");
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_ImGui_Image([[maybe_unused]] ptr<ScriptImGui> self, uint32_t sprId, fsize32 imageSize, fpos32 uv0 = fpos32 {0.0f, 0.0f}, fpos32 uv1 = fpos32 {1.0f, 1.0f})
{
    CheckImageSize(imageSize);
    CheckUvRange(uv0, uv1);

    auto client = self->GetEngine().dyn_cast<ClientEngine>();
    FO_VERIFY_AND_THROW(client, "Missing client instance");

    ImGui::Dummy({imageSize.width, imageSize.height});
    DrawItemSprite(client, sprId, imageSize, uv0, uv1, ucolor::clear);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_ImGui_ImageButton([[maybe_unused]] ptr<ScriptImGui> self, string_view strId, uint32_t sprId, fsize32 imageSize, fpos32 uv0 = fpos32 {0.0f, 0.0f}, fpos32 uv1 = fpos32 {1.0f, 1.0f}, ucolor bgColor = ucolor {}, ucolor tintColor = ucolor {})
{
    CheckImageSize(imageSize);
    CheckUvRange(uv0, uv1);

    if (strId.empty()) {
        throw ScriptException("Image button id arg is empty");
    }

    auto client = self->GetEngine().dyn_cast<ClientEngine>();
    FO_VERIFY_AND_THROW(client, "Missing client instance");

    const auto button_id = string(strId);
    const auto pressed = ImGui::InvisibleButton(button_id.c_str(), {imageSize.width, imageSize.height});

    RenderImageButtonFrame(bgColor);
    DrawItemSprite(client, sprId, imageSize, uv0, uv1, tintColor);
    return pressed;
}

FO_END_NAMESPACE
