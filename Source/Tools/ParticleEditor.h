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

FO_BEGIN_NAMESPACE

class MapperEngine;
class MapView;

class ParticleSubEditor
{
public:
    ParticleSubEditor() = default;
    ParticleSubEditor(const ParticleSubEditor&) = delete;
    ParticleSubEditor(ParticleSubEditor&&) noexcept = delete;
    auto operator=(const ParticleSubEditor&) -> ParticleSubEditor& = delete;
    auto operator=(ParticleSubEditor&&) noexcept -> ParticleSubEditor& = delete;
    virtual ~ParticleSubEditor() = default;

    virtual void Initialize() { }
    virtual void Shutdown() { }
    virtual void ResetLayout() { }
    virtual void OnFocusGained() { }
    virtual void OnCurrentMapChanging(nptr<MapView> next_map) { ignore_unused(next_map); }
    virtual void OnMapUnloading(ptr<MapView> map) { ignore_unused(map); }
    virtual void DrawMenuItem() = 0;
    virtual void DrawWindows() = 0;
};

class ParticleEditorManager final
{
public:
    explicit ParticleEditorManager(ptr<MapperEngine> mapper);
    ParticleEditorManager(const ParticleEditorManager&) = delete;
    ParticleEditorManager(ParticleEditorManager&&) noexcept = delete;
    auto operator=(const ParticleEditorManager&) -> ParticleEditorManager& = delete;
    auto operator=(ParticleEditorManager&&) noexcept -> ParticleEditorManager& = delete;
    ~ParticleEditorManager();

    void Initialize();
    void Shutdown();
    void ResetLayout();
    void OnFocusGained();
    void OnCurrentMapChanging(nptr<MapView> next_map);
    void OnMapUnloading(ptr<MapView> map);
    void DrawMenuItems();
    void DrawWindows();

private:
    vector<unique_ptr<ParticleSubEditor>> _subEditors {};
};

FO_END_NAMESPACE
