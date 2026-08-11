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

#pragma once

#include "Common.h"

#include "imgui.h"
#include "imgui_internal.h"

FO_BEGIN_NAMESPACE

// Drives ImGui panels that normally wait for a user: a headless frame draws every widget but nothing is
// ever pressed, so the code behind a button, a list entry or a folded section stays unreachable. These
// helpers address widgets by the id they are built with and let a test take those branches
namespace ImGuiTestHarness
{
    // Windows are addressed by their visible name; child windows carry the "Parent/Child" form that ImGui
    // itself builds, so the caller spells out the same path it sees in the ImGui window list
    inline auto FindWindow(string_view window_name) -> nptr<ImGuiWindow>
    {
        string name {window_name};
        return ImGui::FindWindowByName(name.c_str());
    }

    // ImGui derives a widget id from its label and the id stack of the window that draws it, so an id
    // rebuilt against that window matches the widget without the test knowing where it landed on screen
    inline auto GetItemId(ptr<ImGuiWindow> window, string_view item_label) -> ImGuiID
    {
        return window->GetID(item_label.data(), item_label.data() + item_label.size());
    }

    // Activation is queued and consumed when the widget is met again, so the caller must draw one more
    // frame before the pressed branch runs. Returns false when the window has not been drawn yet
    inline auto ActivateItem(string_view window_name, string_view item_label) -> bool
    {
        nptr<ImGuiWindow> window = FindWindow(window_name);

        if (!window) {
            return false;
        }

        // A code activation leaves the widget owning the active id with no input source that would ever
        // release it, so the previous press is retired here instead of blocking every later one
        ImGui::ClearActiveID();
        ImGui::ActivateItemByID(GetItemId(window.as_ptr(), item_label));
        return true;
    }

    // ImGui names a child window "<parent>/<label>_<id>", so a chain of labels walks down to a nested one
    inline auto FindChildWindow(string_view window_name, std::initializer_list<string_view> child_labels) -> nptr<ImGuiWindow>
    {
        nptr<ImGuiWindow> window = FindWindow(window_name);

        for (string_view child_label : child_labels) {
            if (!window) {
                return nullptr;
            }

            // The id in that name depends on whatever id stack was pushed where the child was opened - a
            // table or a PushID scope changes it - so the child is matched by the prefix instead
            string child_prefix = strex("{}/{}_", window->Name, child_label).str();
            nptr<ImGuiWindow> found;

            for (ptr<ImGuiWindow> candidate : ImGui::GetCurrentContext()->Windows) {
                if (string_view {candidate->Name}.starts_with(child_prefix)) {
                    found = candidate;
                    break;
                }
            }

            window = found;
        }

        return window;
    }

    // Most panels lay their controls out inside child windows, and a control there belongs to the child's
    // id stack rather than the panel's
    inline auto ActivateChildItem(string_view window_name, std::initializer_list<string_view> child_labels, string_view item_label) -> bool
    {
        nptr<ImGuiWindow> window = FindChildWindow(window_name, child_labels);

        if (!window) {
            return false;
        }

        ImGui::ClearActiveID();
        ImGui::ActivateItemByID(GetItemId(window.as_ptr(), item_label));
        return true;
    }

    // A collapsing header, a tree node and a menu keep their open/closed flag in the window state store
    // rather than in a widget, and they all start closed - so everything they wrap never draws
    inline auto SetItemOpen(string_view window_name, string_view item_label, bool open = true) -> bool
    {
        nptr<ImGuiWindow> window = FindWindow(window_name);

        if (!window) {
            return false;
        }

        window->StateStorage.SetInt(GetItemId(window.as_ptr(), item_label), open ? 1 : 0);
        return true;
    }

    // The window itself can be folded too, which skips its whole body
    inline auto SetWindowCollapsed(string_view window_name, bool collapsed) -> bool
    {
        nptr<ImGuiWindow> window = FindWindow(window_name);

        if (!window) {
            return false;
        }

        window->Collapsed = collapsed;
        return true;
    }
}

FO_END_NAMESPACE
