// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_SPARK_PARTICLES

#include "FileSystem.h"
#include "ParticleEditor.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

class MapperEngine;

auto CreateSparkParticleSubEditor(ptr<MapperEngine> mapper) -> unique_ptr<ParticleSubEditor>;

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
    class Impl;

    void DrawContent();
    void DiscardChanges();
    auto SaveChanges() -> bool;

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
