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

#include "BakingContext.h"

#include "DataSource.h"
#include "ImageDependencyGraph.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

BakingContext::BakingContext(BakingContextOptions options) :
    _settings {options.Settings},
    _packName {std::move(options.PackName)},
    _bakeChecker {std::move(options.BakeChecker)},
    _writeData {std::move(options.WriteData)},
    _bakedFiles {options.BakedFiles},
    _packBakedFiles {options.PackBakedFiles},
    _forceSyncMode {options.ForceSyncMode},
    _report {std::move(options.Report)},
    _outputDiscovery {options.OutputDiscovery},
    _imageDependencies {std::move(options.ImageDependencies)},
    _usage {std::move(options.Usage)}
{
}

BakingContext::BakingContext(const BakingContext& context, string_view baker_name, BakingOutputKind output_kind) :
    _settings {context._settings},
    _packName {context._packName},
    _bakeChecker {context._bakeChecker},
    _writeData {context._writeData},
    _bakedFiles {context._bakedFiles},
    _packBakedFiles {context._packBakedFiles},
    _forceSyncMode {context._forceSyncMode},
    _report {context._report},
    _bakerName {baker_name},
    _outputDiscovery {context._outputDiscovery},
    _imageDependencies {context._imageDependencies},
    _usage {context._usage},
    _outputKind {context._packName == EMBEDDED_PACK_NAME ? BakingOutputKind::Required : output_kind}
{
    FO_VERIFY_AND_THROW(_writeData, "Baker context has no output writer");
    FO_VERIFY_AND_THROW(!_bakerName.empty(), "Baker name is empty");
}

auto BakingContext::GetSettings() const -> nptr<const BakingSettings>
{
    return _settings;
}

auto BakingContext::GetPackName() const -> const string&
{
    return _packName;
}

auto BakingContext::GetBakedFiles() const -> nptr<const FileSystem>
{
    return _bakedFiles;
}

auto BakingContext::GetPackBakedFiles() const -> nptr<const FileSystem>
{
    return _packBakedFiles;
}

auto BakingContext::GetForceSyncMode() const -> optional<bool>
{
    return _forceSyncMode;
}

auto BakingContext::HasBakeChecker() const -> bool
{
    return !!_bakeChecker;
}

auto BakingContext::IsOutputDiscovery() const -> bool
{
    return _outputDiscovery;
}

auto BakingContext::IsBakingReportEnabled() const noexcept -> bool
{
    return !!_report;
}

auto BakingContext::GetImageDependencies() const -> shared_ptr<const ImageDependencyGraph>
{
    return _imageDependencies;
}

auto BakingContext::GetAsyncMode() const -> async_launch_mode
{
    return _forceSyncMode.value_or(_settings->Baking.SingleThreadBaking) ? launch_deferred_only : launch_async_and_deferred;
}

auto BakingContext::IsImageOutputOwned(string_view path) const -> bool
{
    if (!_imageDependencies) {
        return false;
    }

    if (_imageDependencies->Consumed.contains(string {path})) {
        return true;
    }

    const auto& image_outputs = GetRootOutputs(_imageDependencies->Outputs, _packName);

    return std::ranges::any_of(image_outputs, [path](const ImageOutputCandidate& output) { return output.ResourcePath == path; });
}

auto BakingContext::CheckOutput(string_view path, uint64_t write_time, optional<bool> required) const -> bool
{
    if (_usage) {
        shared_ptr<ResourceUsage> usage = _usage;
        usage->RegisterOutput(_packName, path, _bakerName, _outputKind);
    }

    bool scheduled = !_bakeChecker || _bakeChecker(path, write_time);

    if (required.has_value()) {
        scheduled = *required;
    }

    if (_report) {
        shared_ptr<BakingReport> report = _report;
        report->RecordOutputCheck(_packName, _bakerName, path, scheduled);
    }

    return scheduled;
}

auto BakingContext::WriteData(string_view path, const_span<uint8_t> data) const -> BakingWriteResult
{
    BakingWriteResult result = _writeData(path, data);

    if (_usage) {
        shared_ptr<ResourceUsage> usage = _usage;
        usage->RegisterOutput(_packName, path, _bakerName, _outputKind);
    }

    if (_report) {
        shared_ptr<BakingReport> report = _report;
        report->RecordOutputSubmission(_packName, _bakerName, path, data.size(), result);
    }

    return result;
}

void BakingContext::AddBakingReportCounter(string_view name, uint64_t value) const
{
    if (_report) {
        shared_ptr<BakingReport> report = _report;
        report->AddCounter(_packName, _bakerName, name, value);
    }
}

void BakingContext::AddBakingReportHistogramValue(string_view name, string_view value, uint64_t count) const
{
    if (_report) {
        shared_ptr<BakingReport> report = _report;
        report->AddHistogramValue(_packName, _bakerName, name, value, count);
    }
}

void BakingContext::RecordSpriteMeshBakingSettings(const SpriteMeshBakingReportSettings& settings) const
{
    if (_report) {
        shared_ptr<BakingReport> report = _report;
        report->RecordSpriteMeshSettings(_packName, _bakerName, settings);
    }
}

void BakingContext::RecordSpriteMeshBakingFrame(const SpriteMeshBakingFrameReport& frame) const
{
    if (_report) {
        shared_ptr<BakingReport> report = _report;
        report->RecordSpriteMeshFrame(_packName, _bakerName, frame);
    }
}

void BakingContext::RecordSharedSpriteMeshBakingFrames(uint64_t count) const
{
    if (_report) {
        shared_ptr<BakingReport> report = _report;
        report->RecordSharedSpriteMeshFrames(_packName, _bakerName, count);
    }
}

void BakingContext::KeepOutput(string_view path) const
{
    if (_usage) {
        shared_ptr<ResourceUsage> usage = _usage;
        usage->RegisterOutput(_packName, path, _bakerName, BakingOutputKind::Required);
    }
}

void BakingContext::SetForceSyncMode(optional<bool> force_sync_mode)
{
    _forceSyncMode = force_sync_mode;
}

void BakingContext::SetOutputDiscovery(bool output_discovery)
{
    _outputDiscovery = output_discovery;
}

void BakingContext::SetImageDependencies(shared_ptr<const ImageDependencyGraph> graph)
{
    _imageDependencies = std::move(graph);
}

FO_END_NAMESPACE
