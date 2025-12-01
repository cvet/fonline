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

#include "Editor.h"
#include "Application.h"
#include "AssetExplorer.h"
#include "ImGuiStuff.h"
#include "ParticleEditor.h"

FO_BEGIN_NAMESPACE();

EditorView::EditorView(string_view view_name, FOEditor& editor) :
    _viewName {view_name},
    _editor {&editor}
{
    FO_STACK_TRACE_ENTRY();
}

auto EditorView::GetViewName() const -> const string&
{
    FO_STACK_TRACE_ENTRY();

    return _viewName;
}

void EditorView::Draw()
{
    FO_STACK_TRACE_ENTRY();

    OnPreDraw();

    if (_bringToFront) {
        ImGui::SetNextWindowFocus();
        _bringToFront = false;
    }

    bool opened = true;
    if (ImGui::Begin(GetViewName().c_str(), &opened, ImGuiWindowFlags_NoCollapse)) {
        OnDraw();
    }
    ImGui::End();

    if (!opened) {
        _requestClose = true;
    }

    OnPostDraw();
}

void EditorView::BringToFront()
{
    FO_STACK_TRACE_ENTRY();

    _bringToFront = true;
}

void EditorView::Close()
{
    FO_STACK_TRACE_ENTRY();

    _requestClose = true;
}

EditorAssetView::EditorAssetView(string_view view_name, FOEditor& data, string_view asset_path) :
    EditorView(asset_path, data),
    _assetPath {asset_path}
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(view_name);
}

auto EditorAssetView::GetAssetPath() const -> const string&
{
    FO_STACK_TRACE_ENTRY();

    return _assetPath;
}

auto EditorAssetView::IsChanged() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _changed;
}

void EditorAssetView::OnPreDraw()
{
    FO_STACK_TRACE_ENTRY();

    ImGui::SetNextWindowPos({300.0f, 0.0f}, ImGuiCond_Once);
    ImGui::SetNextWindowSize({500.0f, numeric_cast<float32>(App->MainWindow.GetSize().height)}, ImGuiCond_Once);
}

void EditorAssetView::OnDraw()
{
    FO_STACK_TRACE_ENTRY();

    /*if (ImGui::TreeNode("Dependencies")) {
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Usages")) {
        ImGui::TreePop();
    }*/
}

FOEditor::FOEditor(GlobalSettings& settings) :
    Settings {settings}
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& res_pack : Settings.GetResourcePacks()) {
        for (const auto& dir : res_pack.InputDir) {
            RawResources.AddDirSource(dir, res_pack.RecursiveInput, true);
        }
    }

    BakedResources.AddCustomSource(SafeAlloc::MakeUnique<BakerDataSource>(Settings));

    App->LoadImGuiEffect(BakedResources);

    _newViews.emplace_back(SafeAlloc::MakeUnique<AssetExplorer>(*this));
}

auto FOEditor::GetAssetViews() -> vector<EditorAssetView*>
{
    FO_STACK_TRACE_ENTRY();

    vector<EditorAssetView*> result;

    for (auto& view : _views) {
        if (auto* asset_view = dynamic_cast<EditorAssetView*>(view.get()); asset_view != nullptr) {
            result.emplace_back(asset_view);
        }
    }

    return result;
}

void FOEditor::OpenAsset(string_view path)
{
    FO_STACK_TRACE_ENTRY();

    for (auto& view : _views) {
        if (auto* asset_view = dynamic_cast<EditorAssetView*>(view.get()); asset_view != nullptr && asset_view->GetAssetPath() == path) {
            asset_view->BringToFront();
            return;
        }
    }

    const string ext = strex(path).get_file_extension();

    if (ext == "fopts") {
        _newViews.emplace_back(SafeAlloc::MakeUnique<ParticleEditor>(path, *this));
    }
}

void FOEditor::MainLoop()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& view : _newViews) {
        _views.emplace_back(std::move(view));
    }
    _newViews.clear();

    for (auto it = _views.begin(); it != _views.end();) {
        if (const auto* asset_view = dynamic_cast<EditorAssetView*>(it->get()); asset_view != nullptr && asset_view->IsChanged()) {
            (*it)->_requestClose = false;
        }

        if ((*it)->_requestClose) {
            it = _views.erase(it); // Free view resources
        }
        else {
            ++it;
        }
    }

    for (auto& view : _views) {
        try {
            view->Draw();
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }
}

FO_END_NAMESPACE();
