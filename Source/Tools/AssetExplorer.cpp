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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "AssetExplorer.h"
#include "Application.h"
#include "ImGuiStuff.h"

FO_BEGIN_NAMESPACE();

AssetExplorer::AssetExplorer(FOEditor& editor) :
    EditorView("Asset Explorer", editor)
{
    FO_STACK_TRACE_ENTRY();
}

void AssetExplorer::OnPreDraw()
{
    FO_STACK_TRACE_ENTRY();

    ImGui::SetNextWindowPos({0.0f, 0.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({300.0f, numeric_cast<float32>(App->MainWindow.GetSize().height)}, ImGuiCond_Always);
}

void AssetExplorer::OnDraw()
{
    FO_STACK_TRACE_ENTRY();

    if (ImGui::TreeNodeEx("Opened", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed)) {
        for (const auto* asset_view : _editor->GetAssetViews()) {
            ImGui::SetNextItemOpen(false);
            if (ImGui::TreeNodeEx(asset_view->GetAssetPath().c_str(), ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                _editor->OpenAsset(asset_view->GetAssetPath());
                ImGui::TreePop();
            }
        }

        ImGui::TreePop();
    }

    DrawSection("Configs", "focfg");
    DrawSection("Locations", "foloc");
    DrawSection("Maps", "fomap");
    DrawSection("Critters", "focr");
    DrawSection("Items", "foitem");
    DrawSection("Dialogs", "fodlg");
    DrawSection("Interface", "fogui");
    DrawSection("Models", "fo3d");
    DrawSection("Texts", "fotxt");

    DrawSection("Particles", "fopts");
    DrawSection("Effects", "fofx");
    // DrawSection("Sprites", "...");
}

void AssetExplorer::DrawSection(const string& section_name, string_view file_ext)
{
    FO_STACK_TRACE_ENTRY();

    if (ImGui::TreeNodeEx(section_name.c_str(), ImGuiTreeNodeFlags_Framed)) {
        auto files = _editor->RawResources.FilterFiles(file_ext);

        for (const auto& file_header : files) {
            ImGui::SetNextItemOpen(false);
            if (ImGui::TreeNodeEx(file_header.GetPath().c_str(), ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                _editor->OpenAsset(file_header.GetPath());
                ImGui::TreePop();
            }
        }

        ImGui::TreePop();
    }
}

FO_END_NAMESPACE();
