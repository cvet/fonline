//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
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

#pragma once

#include "Common.h"

#include "BakingReport.h"
#include "ResourceUsage.h"

FO_BEGIN_NAMESPACE

struct BakingSettings;
class FileSystem;
struct ImageDependencyGraph;

using BakeCheckerCallback = copyable_function<bool(string_view, uint64_t)>;
using AsyncWriteDataCallback = copyable_function<BakingWriteResult(string_view, const_span<uint8_t>)>;

// Pure input configuration of one baking run; the baker name is identity of a concrete
// BaseBaker instance and therefore belongs to BakingContext itself, not to the caller
struct BakingContextOptions
{
    nptr<const BakingSettings> Settings {};
    string PackName {};
    BakeCheckerCallback BakeChecker {};
    AsyncWriteDataCallback WriteData {};
    nptr<const FileSystem> BakedFiles {};
    nptr<const FileSystem> PackBakedFiles {};
    optional<bool> ForceSyncMode {};
    shared_ptr<BakingReport> Report {};
    bool OutputDiscovery {};
    shared_ptr<const ImageDependencyGraph> ImageDependencies {};
    shared_ptr<ResourceUsage> Usage {};
};

class BakingContext final
{
public:
    explicit BakingContext(BakingContextOptions options);

    // Per-baker identity copy: shares the borrowed settings/files and the shared
    // report/graph owners with the source context, but carries its own baker name
    BakingContext(const BakingContext& context, string_view baker_name, BakingOutputKind output_kind = BakingOutputKind::Required);
    BakingContext(const BakingContext&) = delete;
    BakingContext(BakingContext&&) noexcept = delete;
    auto operator=(const BakingContext&) = delete;
    auto operator=(BakingContext&&) noexcept = delete;
    ~BakingContext() = default;

    [[nodiscard]] auto GetSettings() const -> nptr<const BakingSettings>;
    [[nodiscard]] auto GetPackName() const -> const string&;
    [[nodiscard]] auto GetBakedFiles() const -> nptr<const FileSystem>;
    [[nodiscard]] auto GetPackBakedFiles() const -> nptr<const FileSystem>;
    [[nodiscard]] auto GetForceSyncMode() const -> optional<bool>;
    [[nodiscard]] auto HasBakeChecker() const -> bool;
    [[nodiscard]] auto IsOutputDiscovery() const -> bool;
    [[nodiscard]] auto IsBakingReportEnabled() const noexcept -> bool;
    [[nodiscard]] auto GetImageDependencies() const -> shared_ptr<const ImageDependencyGraph>;
    [[nodiscard]] auto GetAsyncMode() const -> async_launch_mode;
    [[nodiscard]] auto IsImageOutputOwned(string_view path) const -> bool;

    // The original checker runs at most once even when required overrides the answer,
    // and the report always receives exactly the final decision under this pack/baker
    auto CheckOutput(string_view path, uint64_t write_time, optional<bool> required = {}) const -> bool;
    auto WriteData(string_view path, const_span<uint8_t> data) const -> BakingWriteResult;
    void AddBakingReportCounter(string_view name, uint64_t value = 1) const;
    void AddBakingReportHistogramValue(string_view name, string_view value, uint64_t count = 1) const;
    void RecordSpriteMeshBakingSettings(const SpriteMeshBakingReportSettings& settings) const;
    void RecordSpriteMeshBakingFrame(const SpriteMeshBakingFrameReport& frame) const;
    void RecordSharedSpriteMeshBakingFrames(uint64_t count) const;

    void KeepOutput(string_view path) const;

    void SetForceSyncMode(optional<bool> force_sync_mode);
    void SetOutputDiscovery(bool output_discovery);
    void SetImageDependencies(shared_ptr<const ImageDependencyGraph> graph);

private:
    nptr<const BakingSettings> _settings {};
    string _packName {};
    BakeCheckerCallback _bakeChecker {};
    AsyncWriteDataCallback _writeData {};
    nptr<const FileSystem> _bakedFiles {};
    nptr<const FileSystem> _packBakedFiles {};
    optional<bool> _forceSyncMode {};
    shared_ptr<BakingReport> _report {};
    string _bakerName {};
    bool _outputDiscovery {};
    shared_ptr<const ImageDependencyGraph> _imageDependencies {};
    shared_ptr<ResourceUsage> _usage {};
    BakingOutputKind _outputKind {};
};

FO_END_NAMESPACE
