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

#if FO_SPARK_PARTICLES

#include "FileSystem.h"
#include "ParticleEditor.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

class MapperEngine;

[[nodiscard]] auto CreateSparkParticleSubEditor(ptr<MapperEngine> mapper) -> unique_ptr<ParticleSubEditor>;

class SparkParticleEditor final
{
public:
    SparkParticleEditor(string_view asset_path, ptr<GlobalSettings> settings, ptr<FileSystem> raw_resources, ptr<FileSystem> baked_resources, function<void(string_view)> on_saved);
    SparkParticleEditor(const SparkParticleEditor&) = delete;
    SparkParticleEditor(SparkParticleEditor&&) noexcept;
    auto operator=(const SparkParticleEditor&) = delete;
    auto operator=(SparkParticleEditor&&) noexcept = delete;
    ~SparkParticleEditor();

    [[nodiscard]] auto GetAssetPath() const noexcept -> string_view { return _assetPath; }
    [[nodiscard]] auto IsChanged() const noexcept -> bool { return _changed; }

    void BringToFront() noexcept;
    void Hide() noexcept { _visible = false; }
    auto Draw() -> bool;

private:
    void DrawContent();
    void DiscardChanges();
    auto SaveChanges() -> bool;

    struct Impl;
    unique_ptr<Impl> _impl;
    string _assetPath {};
    string _windowTitle {};
    string _closePopupTitle {};
    string _saveError {};
    function<void(string_view)> _onSaved {};
    float32_t _dirAngle {};
    bool _autoReplay {};
    bool _changed {};
    bool _bringToFront {};
    bool _visible {true};
};

FO_END_NAMESPACE

#endif
