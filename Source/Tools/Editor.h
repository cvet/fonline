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

#pragma once

#include "Common.h"

#include "Baker.h"
#include "FileSystem.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE();

class FOEditor;

class EditorView
{
    friend class FOEditor;

public:
    EditorView(string_view view_name, FOEditor& editor);
    EditorView(const EditorView&) = delete;
    EditorView(EditorView&&) noexcept = default;
    auto operator=(const EditorView&) = delete;
    auto operator=(EditorView&&) noexcept = delete;
    virtual ~EditorView() = default;

    [[nodiscard]] auto GetViewName() const -> const string&;

    void Draw();
    void BringToFront();
    void Close();

protected:
    virtual void OnPreDraw() { }
    virtual void OnPostDraw() { }
    virtual void OnDraw() { }

    string _viewName;
    raw_ptr<FOEditor> _editor;

private:
    bool _bringToFront {};
    bool _requestClose {};
};

class EditorAssetView : public EditorView
{
public:
    EditorAssetView(string_view view_name, FOEditor& data, string_view asset_path);
    EditorAssetView(const EditorAssetView&) = delete;
    EditorAssetView(EditorAssetView&&) noexcept = default;
    auto operator=(const EditorAssetView&) = delete;
    auto operator=(EditorAssetView&&) noexcept = delete;

    [[nodiscard]] auto GetAssetPath() const -> const string&;
    [[nodiscard]] auto IsChanged() const -> bool;

protected:
    void OnPreDraw() override;
    void OnDraw() override;

    string _assetPath;
    bool _changed {};
};

class FOEditor final
{
public:
    explicit FOEditor(GlobalSettings& settings);

    FOEditor(const FOEditor&) = delete;
    FOEditor(FOEditor&&) noexcept = delete;
    auto operator=(const FOEditor&) = delete;
    auto operator=(FOEditor&&) noexcept = delete;

    [[nodiscard]] auto GetAssetViews() -> vector<EditorAssetView*>;

    void OpenAsset(string_view path);
    void MainLoop();

    GlobalSettings& Settings;
    FileSystem RawResources {};
    FileSystem BakedResources {};

private:
    vector<unique_ptr<EditorView>> _newViews {};
    vector<unique_ptr<EditorView>> _views {};
};

FO_END_NAMESPACE();
