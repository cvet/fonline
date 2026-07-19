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

#include "BakingReport.h"

#include "Settings.h"

FO_DISABLE_WARNINGS_PUSH()
#include <json.hpp>
FO_DISABLE_WARNINGS_POP()

FO_BEGIN_NAMESPACE

constexpr size_t BAKING_REPORT_TOP_ENTRY_COUNT = 25;

static void KeepLargestMissedSpriteFrames(vector<SpriteMeshBakingFrameReport>& frames, const SpriteMeshBakingFrameReport& frame);
static void KeepLargestRejectedSpriteFrames(vector<SpriteMeshBakingFrameReport>& frames, const SpriteMeshBakingFrameReport& frame);
static void KeepMostComplexSpriteFrames(vector<SpriteMeshBakingFrameReport>& frames, const SpriteMeshBakingFrameReport& frame);
static void KeepLargestCroppedSpriteFrames(vector<SpriteMeshBakingFrameReport>& frames, const SpriteMeshBakingFrameReport& frame);
static void KeepLargestPaddingSpriteFrames(vector<SpriteMeshBakingFrameReport>& frames, const SpriteMeshBakingFrameReport& frame);

BakingReport::BakingReport(ptr<const BakingSettings> settings) :
    _bakeOutput {settings->BakeOutput},
    _forceRequested {settings->ForceBaking},
    _singleThread {settings->SingleThreadBaking}
{
    FO_STACK_TRACE_ENTRY();
}

void BakingReport::SetRebuildMode(bool effective, string_view reason)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_locker};
    _fullRebuild = effective;
    _rebuildReason = reason;
}

void BakingReport::RecordPackInput(string_view pack_name, string_view path, size_t size)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_locker};
    _packs[string(pack_name)].InputPathSizes[string(path)] = numeric_cast<uint64_t>(size);
}

void BakingReport::RecordBakerRegistration(string_view pack_name, string_view baker_name, int32_t order)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_locker};
    GetPackBaker(pack_name, baker_name).Order = order;
    GetAggregateBaker(baker_name).Order = order;
}

void BakingReport::RecordBakerInvocation(string_view pack_name, string_view baker_name, int32_t order, size_t input_files, uint64_t input_bytes, int64_t duration_ms, bool success, string_view failure_message)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_locker};
    const auto update = [&](BakingReportBakerStats& stats, string_view stored_failure_message) {
        stats.Order = order;
        stats.Invocations++;
        stats.SuccessfulInvocations += success ? 1 : 0;
        stats.FailedInvocations += success ? 0 : 1;
        stats.AvailableInputFiles += numeric_cast<uint64_t>(input_files);
        stats.AvailableInputBytes += input_bytes;
        stats.DurationMs += duration_ms;
        if (!success && !stored_failure_message.empty()) {
            stats.FailureMessages.emplace_back(stored_failure_message);
        }
    };
    update(GetPackBaker(pack_name, baker_name), failure_message);
    update(GetAggregateBaker(baker_name), failure_message.empty() ? string {} : strex("{}: {}", pack_name, failure_message).str());
}

void BakingReport::RecordOutputCheck(string_view pack_name, string_view baker_name, string_view path, bool scheduled)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_locker};
    const string aggregate_path = strex(pack_name).combine_path(path).str();
    const auto update = [&](BakingReportBakerStats& stats, string_view stored_path) {
        stats.Outputs.CheckCalls++;
        stats.Outputs.ScheduledCheckCalls += scheduled ? 1 : 0;
        stats.Outputs.UpToDateCheckCalls += scheduled ? 0 : 1;
        stats.Outputs.CheckedPaths.emplace(stored_path);
        (scheduled ? stats.Outputs.ScheduledPaths : stats.Outputs.UpToDatePaths).emplace(stored_path);
    };
    update(GetPackBaker(pack_name, baker_name), path);
    update(GetAggregateBaker(baker_name), aggregate_path);
}

void BakingReport::RecordOutputSubmission(string_view pack_name, string_view baker_name, string_view path, size_t size, BakingWriteResult result)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_locker};
    const uint64_t output_size = numeric_cast<uint64_t>(size);
    const string aggregate_path = strex(pack_name).combine_path(path).str();
    const auto update = [&](BakingReportBakerStats& stats, string_view stored_path) {
        stats.Outputs.SubmitCalls++;
        stats.Outputs.SubmittedBytes += output_size;
        stats.Outputs.SubmittedPathSizes[string(stored_path)] = output_size;
        if (result == BakingWriteResult::Changed) {
            stats.Outputs.ChangedPathSizes[string(stored_path)] = output_size;
            stats.Outputs.UnchangedPathSizes.erase(string(stored_path));
        }
        else if (!stats.Outputs.ChangedPathSizes.contains(string(stored_path))) {
            stats.Outputs.UnchangedPathSizes[string(stored_path)] = output_size;
        }
    };
    update(GetPackBaker(pack_name, baker_name), path);
    update(GetAggregateBaker(baker_name), aggregate_path);

    BakingReportPackStats& pack = _packs[string(pack_name)];
    if (result == BakingWriteResult::Changed) {
        pack.ChangedPathSizes[string(path)] = output_size;
        pack.UnchangedPathSizes.erase(string(path));
    }
    else if (!pack.ChangedPathSizes.contains(string(path))) {
        pack.UnchangedPathSizes[string(path)] = output_size;
    }
}

void BakingReport::RecordPackDuration(string_view pack_name, int64_t duration_ms)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_locker};
    _packs[string(pack_name)].DurationMs = duration_ms;
}

void BakingReport::RecordOutdatedFile(string_view path)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_locker};
    _outdatedFilesDeleted++;

    const string normalized_path = strex(path).normalize_path_slashes().str();
    const string_view pack_name = strvex(normalized_path).substring_until('/');
    if (const auto it = _packs.find(pack_name); it != _packs.end()) {
        it->second.OutdatedFilesDeleted++;
    }
}

void BakingReport::AddCounter(string_view pack_name, string_view baker_name, string_view name, uint64_t value)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_locker};
    GetPackBaker(pack_name, baker_name).Counters[string(name)] += value;
    GetAggregateBaker(baker_name).Counters[string(name)] += value;
}

void BakingReport::AddHistogramValue(string_view pack_name, string_view baker_name, string_view name, string_view value, uint64_t count)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_locker};
    GetPackBaker(pack_name, baker_name).Histograms[string(name)][string(value)] += count;
    GetAggregateBaker(baker_name).Histograms[string(name)][string(value)] += count;
}

void BakingReport::RecordSpriteMeshSettings(string_view pack_name, string_view baker_name, const SpriteMeshBakingReportSettings& settings)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_locker};
    GetPackBaker(pack_name, baker_name).SpriteMesh.Settings = settings;
    GetAggregateBaker(baker_name).SpriteMesh.Settings = settings;
}

static void UpdateBakingReportScoreStats(BakingReportScoreStats& stats, float64_t score)
{
    FO_STACK_TRACE_ENTRY();

    stats.Count++;
    stats.Sum += score;
    stats.Minimum = !stats.Minimum.has_value() ? score : std::min(*stats.Minimum, score);
    stats.Maximum = !stats.Maximum.has_value() ? score : std::max(*stats.Maximum, score);
}

void BakingReport::RecordSpriteMeshFrame(string_view pack_name, string_view baker_name, const SpriteMeshBakingFrameReport& frame)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_locker};

    const auto update = [&](BakingReportSpriteMeshStats& stats, const SpriteMeshBakingFrameReport& stored_frame) {
        stats.Forms[stored_frame.Form]++;
        stats.PaddingHistogram[stored_frame.Padding]++;
        stats.SourceFramePixels += stored_frame.SourceFramePixels;
        stats.BakedCanvasPixels += stored_frame.BakedCanvasPixels;
        if (stored_frame.BakedCanvasPixels < stored_frame.SourceFramePixels) {
            stats.CroppedFrames++;
            stats.CroppedTexturePixels += stored_frame.SourceFramePixels - stored_frame.BakedCanvasPixels;
        }
        else if (stored_frame.BakedCanvasPixels > stored_frame.SourceFramePixels) {
            stats.ExpandedFrames++;
            stats.ExpandedTexturePixels += stored_frame.BakedCanvasPixels - stored_frame.SourceFramePixels;
        }
        stats.VisiblePixels += stored_frame.VisiblePixels;
        stats.SubmittedGeometryDoubleArea += stored_frame.SubmittedGeometryDoubleArea;
        stats.SourceComponentHistogram[stored_frame.SourceComponentCount]++;
        if (stored_frame.DilatedComponentCount.has_value()) {
            stats.DilatedComponentHistogram[*stored_frame.DilatedComponentCount]++;
        }

        BakingReportSpriteResourceStats& resource = stats.Resources[stored_frame.OutputPath];
        resource.MeshFrames += stored_frame.Form == "mesh" ? 1 : 0;
        resource.QuadFrames += stored_frame.Form == "quad" ? 1 : 0;
        resource.EmptyFrames += stored_frame.Form == "empty" ? 1 : 0;

        if (stored_frame.Form == "mesh") {
            stats.SelectionOrigins[stored_frame.SelectionOrigin]++;
            stats.TriangleHistogram[stored_frame.TriangleCount]++;
            stats.VertexHistogram[stored_frame.VertexCount]++;
            stats.SimplifyToleranceHistogram[stored_frame.SimplifyTolerance]++;
            stats.ActualDilationHistogram[stored_frame.ActualDilation]++;
            stats.Vertices += stored_frame.VertexCount;
            stats.Triangles += stored_frame.TriangleCount;
            KeepMostComplexSpriteFrames(stats.MostComplexMeshes, stored_frame);

            if (stored_frame.SelectionScore.has_value()) {
                UpdateBakingReportScoreStats(stats.SelectedScores, *stored_frame.SelectionScore);
            }
        }
        else if (stored_frame.Form == "quad") {
            stats.QuadReasons[stored_frame.QuadReason]++;
            KeepLargestMissedSpriteFrames(stats.LargestMissedSavings, stored_frame);
        }

        if (stored_frame.BestRejectedCandidate.has_value()) {
            const SpriteMeshBakingCandidateReport& candidate = *stored_frame.BestRejectedCandidate;
            stats.RejectedSelectionOrigins[candidate.SelectionOrigin]++;
            stats.RejectedTriangleHistogram[candidate.TriangleCount]++;
            stats.RejectedSimplifyToleranceHistogram[candidate.SimplifyTolerance]++;
            stats.RejectedActualDilationHistogram[candidate.ActualDilation]++;
            stats.RejectedCandidateReferenceDoubleArea += stored_frame.SourceFramePixels * 2;
            stats.RejectedCandidateDoubleArea += candidate.SubmittedGeometryDoubleArea;
            stats.RejectedCandidateTriangles += candidate.TriangleCount;
            UpdateBakingReportScoreStats(stats.RejectedScores, candidate.Score);
            KeepLargestRejectedSpriteFrames(stats.LargestRejectedCandidateSavings, stored_frame);
        }

        KeepLargestCroppedSpriteFrames(stats.LargestCroppingSavings, stored_frame);
        KeepLargestPaddingSpriteFrames(stats.LargestPaddingOverhead, stored_frame);
    };

    update(GetPackBaker(pack_name, baker_name).SpriteMesh, frame);

    SpriteMeshBakingFrameReport aggregate_frame = frame;
    aggregate_frame.SourcePath = strex(pack_name).combine_path(frame.SourcePath).str();
    aggregate_frame.OutputPath = strex(pack_name).combine_path(frame.OutputPath).str();
    update(GetAggregateBaker(baker_name).SpriteMesh, aggregate_frame);
}

void BakingReport::RecordSharedSpriteMeshFrames(string_view pack_name, string_view baker_name, uint64_t count)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_locker};
    GetPackBaker(pack_name, baker_name).SpriteMesh.SharedFrameReferences += count;
    GetAggregateBaker(baker_name).SpriteMesh.SharedFrameReferences += count;
}

void BakingReport::Complete(bool success, string_view failure_message)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_locker};
    _status = success ? "success" : "failed";
    _failureMessage = failure_message;
    _completedDurationMs = _duration.GetDuration().milliseconds();
}

auto BakingReport::IsFullRebuild() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_locker};
    return _fullRebuild;
}

auto BakingReport::GetPackBaker(string_view pack_name, string_view baker_name) -> BakingReportBakerStats&
{
    FO_STACK_TRACE_ENTRY();

    return _packs[string(pack_name)].Bakers[string(baker_name)];
}

auto BakingReport::GetAggregateBaker(string_view baker_name) -> BakingReportBakerStats&
{
    FO_STACK_TRACE_ENTRY();

    return _aggregateBakers[string(baker_name)];
}

auto GetBakingReportPath(string_view bake_output) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (bake_output.empty()) {
        return {};
    }

    const string normalized_output = strex(bake_output).normalize_path_slashes().rtrim("/").str();
    return strex(normalized_output).combine_path("Baking.report.json").str();
}

auto GetFullBakingReportPath(string_view bake_output) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (bake_output.empty()) {
        return {};
    }

    const string normalized_output = strex(bake_output).normalize_path_slashes().rtrim("/").str();
    return strex(normalized_output).combine_path("Baking.full.report.json").str();
}

static auto BakingReportPercent(uint64_t part, uint64_t total) noexcept -> float64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return total != 0 ? numeric_cast<float64_t>(part) * 100.0 / numeric_cast<float64_t>(total) : 0.0;
}

static auto IsBakingReportSpriteFrameIdentityLess(const SpriteMeshBakingFrameReport& left, const SpriteMeshBakingFrameReport& right) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (left.OutputPath != right.OutputPath) {
        return left.OutputPath < right.OutputPath;
    }
    if (left.SourcePath != right.SourcePath) {
        return left.SourcePath < right.SourcePath;
    }
    if (left.Direction != right.Direction) {
        return left.Direction < right.Direction;
    }
    return left.FrameIndex < right.FrameIndex;
}

static void KeepLargestMissedSpriteFrames(vector<SpriteMeshBakingFrameReport>& frames, const SpriteMeshBakingFrameReport& frame)
{
    FO_STACK_TRACE_ENTRY();

    if (frame.Form != "quad" || frame.SourceFramePixels <= frame.VisiblePixels) {
        return;
    }

    frames.emplace_back(frame);
    std::ranges::sort(frames, [](const SpriteMeshBakingFrameReport& left, const SpriteMeshBakingFrameReport& right) {
        const uint64_t left_savings = left.SourceFramePixels - left.VisiblePixels;
        const uint64_t right_savings = right.SourceFramePixels - right.VisiblePixels;
        return left_savings != right_savings ? left_savings > right_savings : IsBakingReportSpriteFrameIdentityLess(left, right);
    });
    if (frames.size() > BAKING_REPORT_TOP_ENTRY_COUNT) {
        frames.resize(BAKING_REPORT_TOP_ENTRY_COUNT);
    }
}

static void KeepLargestRejectedSpriteFrames(vector<SpriteMeshBakingFrameReport>& frames, const SpriteMeshBakingFrameReport& frame)
{
    FO_STACK_TRACE_ENTRY();

    if (!frame.BestRejectedCandidate.has_value() || frame.SourceFramePixels * 2 <= frame.BestRejectedCandidate->SubmittedGeometryDoubleArea) {
        return;
    }

    frames.emplace_back(frame);
    std::ranges::sort(frames, [](const SpriteMeshBakingFrameReport& left, const SpriteMeshBakingFrameReport& right) {
        const uint64_t left_savings = left.SourceFramePixels * 2 - left.BestRejectedCandidate->SubmittedGeometryDoubleArea;
        const uint64_t right_savings = right.SourceFramePixels * 2 - right.BestRejectedCandidate->SubmittedGeometryDoubleArea;
        return left_savings != right_savings ? left_savings > right_savings : IsBakingReportSpriteFrameIdentityLess(left, right);
    });
    if (frames.size() > BAKING_REPORT_TOP_ENTRY_COUNT) {
        frames.resize(BAKING_REPORT_TOP_ENTRY_COUNT);
    }
}

static void KeepMostComplexSpriteFrames(vector<SpriteMeshBakingFrameReport>& frames, const SpriteMeshBakingFrameReport& frame)
{
    FO_STACK_TRACE_ENTRY();

    if (frame.Form != "mesh") {
        return;
    }

    frames.emplace_back(frame);
    std::ranges::sort(frames, [](const SpriteMeshBakingFrameReport& left, const SpriteMeshBakingFrameReport& right) {
        if (left.TriangleCount != right.TriangleCount) {
            return left.TriangleCount > right.TriangleCount;
        }
        if (left.VertexCount != right.VertexCount) {
            return left.VertexCount > right.VertexCount;
        }
        return IsBakingReportSpriteFrameIdentityLess(left, right);
    });
    if (frames.size() > BAKING_REPORT_TOP_ENTRY_COUNT) {
        frames.resize(BAKING_REPORT_TOP_ENTRY_COUNT);
    }
}

static void KeepLargestPaddingSpriteFrames(vector<SpriteMeshBakingFrameReport>& frames, const SpriteMeshBakingFrameReport& frame)
{
    FO_STACK_TRACE_ENTRY();

    if (frame.BakedCanvasPixels <= frame.SourceFramePixels) {
        return;
    }

    frames.emplace_back(frame);
    std::ranges::sort(frames, [](const SpriteMeshBakingFrameReport& left, const SpriteMeshBakingFrameReport& right) {
        const uint64_t left_overhead = left.BakedCanvasPixels - left.SourceFramePixels;
        const uint64_t right_overhead = right.BakedCanvasPixels - right.SourceFramePixels;
        return left_overhead != right_overhead ? left_overhead > right_overhead : IsBakingReportSpriteFrameIdentityLess(left, right);
    });
    if (frames.size() > BAKING_REPORT_TOP_ENTRY_COUNT) {
        frames.resize(BAKING_REPORT_TOP_ENTRY_COUNT);
    }
}

static void KeepLargestCroppedSpriteFrames(vector<SpriteMeshBakingFrameReport>& frames, const SpriteMeshBakingFrameReport& frame)
{
    FO_STACK_TRACE_ENTRY();

    if (frame.BakedCanvasPixels >= frame.SourceFramePixels) {
        return;
    }

    frames.emplace_back(frame);
    std::ranges::sort(frames, [](const SpriteMeshBakingFrameReport& left, const SpriteMeshBakingFrameReport& right) {
        const uint64_t left_savings = left.SourceFramePixels - left.BakedCanvasPixels;
        const uint64_t right_savings = right.SourceFramePixels - right.BakedCanvasPixels;
        return left_savings != right_savings ? left_savings > right_savings : IsBakingReportSpriteFrameIdentityLess(left, right);
    });
    if (frames.size() > BAKING_REPORT_TOP_ENTRY_COUNT) {
        frames.resize(BAKING_REPORT_TOP_ENTRY_COUNT);
    }
}

using BakingReportJson = nlohmann::ordered_json;

static auto SumBakingReportPathSizes(const map<string, uint64_t>& path_sizes) noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    uint64_t total = 0;
    for (const auto& [path, size] : path_sizes) {
        ignore_unused(path);
        total += size;
    }
    return total;
}

static auto MakeBakingReportPathCountJson(const set<string>& paths) -> BakingReportJson
{
    FO_STACK_TRACE_ENTRY();

    map<string, uint64_t> extensions;
    for (const string& path : paths) {
        string extension = strex(path).get_file_extension();
        extensions[extension.empty() ? "<none>" : extension]++;
    }

    BakingReportJson result;
    result["count"] = paths.size();
    result["byExtension"] = BakingReportJson::array();
    for (const auto& [extension, count] : extensions) {
        result["byExtension"].push_back({{"extension", extension}, {"count", count}, {"percent", BakingReportPercent(count, numeric_cast<uint64_t>(paths.size()))}});
    }
    return result;
}

static auto MakeBakingReportPathSizeJson(const map<string, uint64_t>& path_sizes) -> BakingReportJson
{
    FO_STACK_TRACE_ENTRY();

    struct ExtensionStats
    {
        uint64_t Count {};
        uint64_t Bytes {};
    };

    map<string, ExtensionStats> extensions;
    vector<pair<string, uint64_t>> largest;
    largest.reserve(path_sizes.size());
    const uint64_t total_bytes = SumBakingReportPathSizes(path_sizes);

    for (const auto& [path, size] : path_sizes) {
        string extension = strex(path).get_file_extension();
        ExtensionStats& extension_stats = extensions[extension.empty() ? "<none>" : extension];
        extension_stats.Count++;
        extension_stats.Bytes += size;
        largest.emplace_back(path, size);
    }

    std::ranges::sort(largest, [](const auto& left, const auto& right) { return left.second != right.second ? left.second > right.second : left.first < right.first; });
    if (largest.size() > BAKING_REPORT_TOP_ENTRY_COUNT) {
        largest.resize(BAKING_REPORT_TOP_ENTRY_COUNT);
    }

    BakingReportJson result;
    result["count"] = path_sizes.size();
    result["bytes"] = total_bytes;
    result["byExtension"] = BakingReportJson::array();
    for (const auto& [extension, stats] : extensions) {
        result["byExtension"].push_back({{"extension", extension}, {"count", stats.Count}, {"countPercent", BakingReportPercent(stats.Count, numeric_cast<uint64_t>(path_sizes.size()))}, {"bytes", stats.Bytes}, {"bytesPercent", BakingReportPercent(stats.Bytes, total_bytes)}});
    }
    result["largest"] = BakingReportJson::array();
    for (const auto& [path, size] : largest) {
        result["largest"].push_back({{"path", path}, {"bytes", size}});
    }
    return result;
}

static auto MakeBakingReportStringDistribution(const map<string, uint64_t>& distribution, uint64_t denominator, string_view value_name) -> BakingReportJson
{
    FO_STACK_TRACE_ENTRY();

    BakingReportJson result = BakingReportJson::array();
    for (const auto& [value, count] : distribution) {
        result.push_back({{string(value_name), value}, {"count", count}, {"percent", BakingReportPercent(count, denominator)}});
    }
    return result;
}

template<typename T>
static auto MakeBakingReportNumericDistribution(const map<T, uint64_t>& distribution, uint64_t denominator, string_view value_name) -> BakingReportJson
{
    FO_STACK_TRACE_ENTRY();

    BakingReportJson result = BakingReportJson::array();
    for (const auto& [value, count] : distribution) {
        result.push_back({{string(value_name), value}, {"count", count}, {"percent", BakingReportPercent(count, denominator)}});
    }
    return result;
}

static auto MakeBakingReportSpriteFrameJson(const SpriteMeshBakingFrameReport& frame) -> BakingReportJson
{
    FO_STACK_TRACE_ENTRY();

    BakingReportJson result {
        {"sourcePath", frame.SourcePath},
        {"outputPath", frame.OutputPath},
        {"direction", frame.Direction},
        {"frame", frame.FrameIndex},
        {"form", frame.Form},
        {"selectionOrigin", frame.SelectionOrigin},
        {"quadReason", frame.QuadReason},
        {"triangles", frame.TriangleCount},
        {"vertices", frame.VertexCount},
        {"sourceComponents", frame.SourceComponentCount},
        {"padding", frame.Padding},
        {"simplifyTolerance", frame.SimplifyTolerance},
        {"actualDilation", frame.ActualDilation},
        {"sourceFramePixels", frame.SourceFramePixels},
        {"bakedCanvasPixels", frame.BakedCanvasPixels},
        {"visiblePixels", frame.VisiblePixels},
        {"submittedGeometryDoubleArea", frame.SubmittedGeometryDoubleArea},
        {"submittedGeometryAreaPixels", numeric_cast<float64_t>(frame.SubmittedGeometryDoubleArea) / 2.0},
        {"potentialTransparentPixels", frame.SourceFramePixels >= frame.VisiblePixels ? frame.SourceFramePixels - frame.VisiblePixels : 0},
        {"paddingAddedPixels", frame.BakedCanvasPixels >= frame.SourceFramePixels ? frame.BakedCanvasPixels - frame.SourceFramePixels : 0},
        {"croppingSavedPixels", frame.SourceFramePixels >= frame.BakedCanvasPixels ? frame.SourceFramePixels - frame.BakedCanvasPixels : 0},
    };
    result["dilatedComponents"] = nullptr;
    if (frame.DilatedComponentCount.has_value()) {
        result["dilatedComponents"] = *frame.DilatedComponentCount;
    }
    result["selectionScore"] = nullptr;
    if (frame.SelectionScore.has_value()) {
        result["selectionScore"] = *frame.SelectionScore;
    }
    if (frame.BestRejectedCandidate.has_value()) {
        const SpriteMeshBakingCandidateReport& candidate = *frame.BestRejectedCandidate;
        const uint64_t reference_double_area = frame.SourceFramePixels * 2;
        const uint64_t saved_double_area = reference_double_area >= candidate.SubmittedGeometryDoubleArea ? reference_double_area - candidate.SubmittedGeometryDoubleArea : 0;
        const int64_t additional_triangles = numeric_cast<int64_t>(candidate.TriangleCount) - 2;
        result["bestRejectedCandidate"] = {
            {"selectionOrigin", candidate.SelectionOrigin},
            {"triangles", candidate.TriangleCount},
            {"vertices", candidate.VertexCount},
            {"submittedGeometryDoubleArea", candidate.SubmittedGeometryDoubleArea},
            {"submittedGeometryAreaPixels", numeric_cast<float64_t>(candidate.SubmittedGeometryDoubleArea) / 2.0},
            {"savedDoubleArea", saved_double_area},
            {"savedFrameAreaPercent", BakingReportPercent(saved_double_area, reference_double_area)},
            {"additionalTrianglesVsQuad", additional_triangles},
            {"score", candidate.Score},
            {"simplifyTolerance", candidate.SimplifyTolerance},
            {"actualDilation", candidate.ActualDilation},
        };
        result["bestRejectedCandidate"]["breakEvenAreaSavingsWeight"] = nullptr;
        if (saved_double_area != 0) {
            const float64_t saved_area_ratio = numeric_cast<float64_t>(saved_double_area) / numeric_cast<float64_t>(reference_double_area);
            result["bestRejectedCandidate"]["breakEvenAreaSavingsWeight"] = std::max(0.0, numeric_cast<float64_t>(additional_triangles) / saved_area_ratio);
        }
    }
    else {
        result["bestRejectedCandidate"] = nullptr;
    }
    return result;
}

static auto MakeBakingReportScoreJson(const BakingReportScoreStats& stats) -> BakingReportJson
{
    FO_STACK_TRACE_ENTRY();

    BakingReportJson result {
        {"count", stats.Count},
        {"average", stats.Count != 0 ? stats.Sum / numeric_cast<float64_t>(stats.Count) : 0.0},
    };
    result["minimum"] = nullptr;
    if (stats.Minimum.has_value()) {
        result["minimum"] = *stats.Minimum;
    }
    result["maximum"] = nullptr;
    if (stats.Maximum.has_value()) {
        result["maximum"] = *stats.Maximum;
    }
    return result;
}

static auto MakeBakingReportSpriteMeshJson(const BakingReportSpriteMeshStats& stats, bool complete_corpus) -> BakingReportJson
{
    FO_STACK_TRACE_ENTRY();

    const uint64_t unique_frames = [&] {
        uint64_t total = 0;
        for (const auto& [form, count] : stats.Forms) {
            ignore_unused(form);
            total += count;
        }
        return total;
    }();
    const uint64_t mesh_frames = stats.Forms.contains("mesh") ? stats.Forms.at("mesh") : 0;
    const uint64_t quad_frames = stats.Forms.contains("quad") ? stats.Forms.at("quad") : 0;
    const uint64_t empty_frames = stats.Forms.contains("empty") ? stats.Forms.at("empty") : 0;

    BakingReportJson result;
    result["measurementScope"] = {
        {"frames", "rebuilt_this_run"},
        {"completeCorpus", complete_corpus},
    };
    if (stats.Settings.has_value()) {
        result["settings"] = {
            {"enabled", stats.Settings->Enabled},
            {"alphaThreshold", stats.Settings->AlphaThreshold},
            {"maxTriangles", stats.Settings->MaxTriangles},
            {"areaSavingsWeight", stats.Settings->AreaSavingsWeight},
            {"baseDilation", stats.Settings->BaseDilation},
            {"maximumPadding", stats.Settings->MaximumPadding},
        };
    }

    result["frames"] = {
        {"slots", unique_frames + stats.SharedFrameReferences},
        {"unique", unique_frames},
        {"sharedReferences", stats.SharedFrameReferences},
        {"mesh", {{"count", mesh_frames}, {"percent", BakingReportPercent(mesh_frames, unique_frames)}}},
        {"quad", {{"count", quad_frames}, {"percent", BakingReportPercent(quad_frames, unique_frames)}}},
        {"empty", {{"count", empty_frames}, {"percent", BakingReportPercent(empty_frames, unique_frames)}}},
    };

    result["triangleHistogram"] = BakingReportJson::array();
    for (const auto& [triangles, count] : stats.TriangleHistogram) {
        result["triangleHistogram"].push_back({{"triangles", triangles}, {"count", count}, {"percentOfMeshes", BakingReportPercent(count, mesh_frames)}, {"percentOfUniqueFrames", BakingReportPercent(count, unique_frames)}});
    }
    result["vertexHistogram"] = MakeBakingReportNumericDistribution(stats.VertexHistogram, mesh_frames, "vertices");
    result["sourceComponentHistogram"] = MakeBakingReportNumericDistribution(stats.SourceComponentHistogram, unique_frames, "components");
    uint64_t dilated_component_frames = 0;
    for (const auto& [components, count] : stats.DilatedComponentHistogram) {
        ignore_unused(components);
        dilated_component_frames += count;
    }
    result["dilatedComponentHistogram"] = MakeBakingReportNumericDistribution(stats.DilatedComponentHistogram, dilated_component_frames, "components");
    result["selectedSimplifyToleranceHistogram"] = MakeBakingReportNumericDistribution(stats.SimplifyToleranceHistogram, mesh_frames, "tolerance");
    result["selectedActualDilationHistogram"] = MakeBakingReportNumericDistribution(stats.ActualDilationHistogram, mesh_frames, "dilation");
    result["selectionOrigins"] = MakeBakingReportStringDistribution(stats.SelectionOrigins, mesh_frames, "origin");
    result["quadReasons"] = MakeBakingReportStringDistribution(stats.QuadReasons, quad_frames, "reason");

    const uint64_t rejected_candidates = stats.RejectedScores.Count;
    const uint64_t rejected_saved_double_area = stats.RejectedCandidateReferenceDoubleArea >= stats.RejectedCandidateDoubleArea ? stats.RejectedCandidateReferenceDoubleArea - stats.RejectedCandidateDoubleArea : 0;
    const int64_t rejected_additional_triangles = numeric_cast<int64_t>(stats.RejectedCandidateTriangles) - numeric_cast<int64_t>(rejected_candidates * 2);
    result["bestRejectedCandidates"] = {
        {"count", rejected_candidates},
        {"selectionOrigins", MakeBakingReportStringDistribution(stats.RejectedSelectionOrigins, rejected_candidates, "origin")},
        {"triangleHistogram", MakeBakingReportNumericDistribution(stats.RejectedTriangleHistogram, rejected_candidates, "triangles")},
        {"simplifyToleranceHistogram", MakeBakingReportNumericDistribution(stats.RejectedSimplifyToleranceHistogram, rejected_candidates, "tolerance")},
        {"actualDilationHistogram", MakeBakingReportNumericDistribution(stats.RejectedActualDilationHistogram, rejected_candidates, "dilation")},
        {"score", MakeBakingReportScoreJson(stats.RejectedScores)},
        {"geometry",
            {
                {"candidateTriangles", stats.RejectedCandidateTriangles},
                {"quadTriangles", rejected_candidates * 2},
                {"additionalTriangles", rejected_additional_triangles},
            }},
        {"area",
            {
                {"referenceQuadDoubleArea", stats.RejectedCandidateReferenceDoubleArea},
                {"candidateDoubleArea", stats.RejectedCandidateDoubleArea},
                {"savedDoubleArea", rejected_saved_double_area},
                {"savedFrameAreaPercent", BakingReportPercent(rejected_saved_double_area, stats.RejectedCandidateReferenceDoubleArea)},
                {"savedAreaPixelsPerAdditionalTriangle", rejected_additional_triangles > 0 ? numeric_cast<float64_t>(rejected_saved_double_area) / 2.0 / numeric_cast<float64_t>(rejected_additional_triangles) : 0.0},
            }},
    };

    const uint64_t baseline_triangles = unique_frames * 2;
    const uint64_t submitted_triangles = stats.Triangles + quad_frames * 2;
    const uint64_t submitted_vertices = stats.Vertices + quad_frames * 4;
    result["geometry"] = {
        {"submittedVertices", submitted_vertices},
        {"meshVertices", stats.Vertices},
        {"submittedTriangles", submitted_triangles},
        {"meshTriangles", stats.Triangles},
        {"allQuadBaselineVertices", unique_frames * 4},
        {"allQuadBaselineTriangles", baseline_triangles},
        {"triangleDelta", numeric_cast<int64_t>(submitted_triangles) - numeric_cast<int64_t>(baseline_triangles)},
        {"averageVerticesPerMesh", mesh_frames != 0 ? numeric_cast<float64_t>(stats.Vertices) / numeric_cast<float64_t>(mesh_frames) : 0.0},
        {"averageTrianglesPerMesh", mesh_frames != 0 ? numeric_cast<float64_t>(stats.Triangles) / numeric_cast<float64_t>(mesh_frames) : 0.0},
    };

    const uint64_t baseline_double_area = stats.SourceFramePixels * 2;
    const uint64_t visible_double_area = stats.VisiblePixels * 2;
    const int64_t saved_double_area = numeric_cast<int64_t>(baseline_double_area) - numeric_cast<int64_t>(stats.SubmittedGeometryDoubleArea);
    const int64_t baseline_transparent_double_area = numeric_cast<int64_t>(baseline_double_area) - numeric_cast<int64_t>(visible_double_area);
    const int64_t remaining_transparent_double_area = numeric_cast<int64_t>(stats.SubmittedGeometryDoubleArea) - numeric_cast<int64_t>(visible_double_area);
    result["area"] = {
        {"baselineQuadDoubleArea", baseline_double_area},
        {"submittedGeometryDoubleArea", stats.SubmittedGeometryDoubleArea},
        {"visibleDoubleArea", visible_double_area},
        {"baselineQuadAreaPixels", numeric_cast<float64_t>(baseline_double_area) / 2.0},
        {"submittedGeometryAreaPixels", numeric_cast<float64_t>(stats.SubmittedGeometryDoubleArea) / 2.0},
        {"visiblePixels", stats.VisiblePixels},
        {"savedDoubleArea", saved_double_area},
        {"savedFrameAreaPercent", baseline_double_area != 0 ? numeric_cast<float64_t>(saved_double_area) * 100.0 / numeric_cast<float64_t>(baseline_double_area) : 0.0},
        {"baselineTransparentDoubleArea", baseline_transparent_double_area},
        {"remainingTransparentDoubleArea", remaining_transparent_double_area},
        {"savedTransparentDoubleArea", baseline_transparent_double_area - remaining_transparent_double_area},
        {"savedTransparentPercent", baseline_transparent_double_area > 0 ? numeric_cast<float64_t>(baseline_transparent_double_area - remaining_transparent_double_area) * 100.0 / numeric_cast<float64_t>(baseline_transparent_double_area) : 0.0},
    };

    const uint64_t zero_padding_frames = stats.PaddingHistogram.contains(0) ? stats.PaddingHistogram.at(0) : 0;
    int32_t maximum_padding = 0;
    if (!stats.PaddingHistogram.empty()) {
        maximum_padding = stats.PaddingHistogram.rbegin()->first;
    }
    result["padding"] = {
        {"framesPadded", unique_frames >= zero_padding_frames ? unique_frames - zero_padding_frames : 0},
        {"maximum", maximum_padding},
        {"serializedTexturePixels", stats.BakedCanvasPixels},
        {"framesExpanded", stats.ExpandedFrames},
        {"addedTexturePixels", stats.ExpandedTexturePixels},
        {"addedTextureBytesRgba", stats.ExpandedTexturePixels * 4},
        {"histogram", MakeBakingReportNumericDistribution(stats.PaddingHistogram, unique_frames, "padding")},
    };

    result["cropping"] = {
        {"framesCropped", stats.CroppedFrames},
        {"savedTexturePixels", stats.CroppedTexturePixels},
        {"savedTextureBytesRgba", stats.CroppedTexturePixels * 4},
    };

    result["selectionScore"] = MakeBakingReportScoreJson(stats.SelectedScores);
    result["selectionScore"]["quadScore"] = -2.0;

    map<string, uint64_t> resource_forms;
    for (const auto& [path, resource] : stats.Resources) {
        ignore_unused(path);
        const uint32_t populated_forms = (resource.MeshFrames != 0 ? 1 : 0) + (resource.QuadFrames != 0 ? 1 : 0) + (resource.EmptyFrames != 0 ? 1 : 0);
        if (populated_forms > 1) {
            resource_forms["mixed"]++;
        }
        else if (resource.MeshFrames != 0) {
            resource_forms["mesh_only"]++;
        }
        else if (resource.QuadFrames != 0) {
            resource_forms["quad_only"]++;
        }
        else {
            resource_forms["empty_only"]++;
        }
    }
    result["resources"] = {
        {"count", stats.Resources.size()},
        {"classifications", MakeBakingReportStringDistribution(resource_forms, numeric_cast<uint64_t>(stats.Resources.size()), "classification")},
    };

    result["largestMissedSavings"] = BakingReportJson::array();
    for (const SpriteMeshBakingFrameReport& frame : stats.LargestMissedSavings) {
        result["largestMissedSavings"].push_back(MakeBakingReportSpriteFrameJson(frame));
    }
    result["largestRejectedCandidateSavings"] = BakingReportJson::array();
    for (const SpriteMeshBakingFrameReport& frame : stats.LargestRejectedCandidateSavings) {
        result["largestRejectedCandidateSavings"].push_back(MakeBakingReportSpriteFrameJson(frame));
    }
    result["mostComplexMeshes"] = BakingReportJson::array();
    for (const SpriteMeshBakingFrameReport& frame : stats.MostComplexMeshes) {
        result["mostComplexMeshes"].push_back(MakeBakingReportSpriteFrameJson(frame));
    }
    result["largestCroppingSavings"] = BakingReportJson::array();
    for (const SpriteMeshBakingFrameReport& frame : stats.LargestCroppingSavings) {
        result["largestCroppingSavings"].push_back(MakeBakingReportSpriteFrameJson(frame));
    }
    result["largestPaddingOverhead"] = BakingReportJson::array();
    for (const SpriteMeshBakingFrameReport& frame : stats.LargestPaddingOverhead) {
        result["largestPaddingOverhead"].push_back(MakeBakingReportSpriteFrameJson(frame));
    }

    return result;
}

static auto MakeBakingReportBakerJson(string_view name, const BakingReportBakerStats& stats, bool complete_corpus) -> BakingReportJson
{
    FO_STACK_TRACE_ENTRY();

    BakingReportJson result {
        {"name", name},
        {"order", stats.Order},
        {"status",
            stats.FailedInvocations != 0 ? "failed" :
                stats.Invocations != 0   ? "success" :
                                           "not_run"},
        {"invocations", stats.Invocations},
        {"successfulInvocations", stats.SuccessfulInvocations},
        {"failedInvocations", stats.FailedInvocations},
        {"failureMessages", stats.FailureMessages},
        {"durationMs", stats.DurationMs},
        {"availableInputFiles", stats.AvailableInputFiles},
        {"availableInputBytes", stats.AvailableInputBytes},
        {"measurementScope",
            {
                {"availableInputs", "complete_corpus"},
                {"outputsAndDetails", "this_run"},
                {"completeCorpusDetails", complete_corpus},
            }},
    };

    result["outputs"] = {
        {"checkCalls", stats.Outputs.CheckCalls},
        {"scheduledCheckCalls", stats.Outputs.ScheduledCheckCalls},
        {"upToDateCheckCalls", stats.Outputs.UpToDateCheckCalls},
        {"checked", MakeBakingReportPathCountJson(stats.Outputs.CheckedPaths)},
        {"scheduled", MakeBakingReportPathCountJson(stats.Outputs.ScheduledPaths)},
        {"upToDate", MakeBakingReportPathCountJson(stats.Outputs.UpToDatePaths)},
        {"cacheHitPercent", BakingReportPercent(stats.Outputs.UpToDateCheckCalls, stats.Outputs.CheckCalls)},
        {"submitCalls", stats.Outputs.SubmitCalls},
        {"submittedBytesAcrossCalls", stats.Outputs.SubmittedBytes},
        {"submitted", MakeBakingReportPathSizeJson(stats.Outputs.SubmittedPathSizes)},
        {"changed", MakeBakingReportPathSizeJson(stats.Outputs.ChangedPathSizes)},
        {"unchanged", MakeBakingReportPathSizeJson(stats.Outputs.UnchangedPathSizes)},
    };

    result["details"]["counters"] = BakingReportJson::object();
    for (const auto& [counter, value] : stats.Counters) {
        result["details"]["counters"][std::string {counter.begin(), counter.end()}] = value;
    }
    result["details"]["histograms"] = BakingReportJson::object();
    for (const auto& [histogram, values] : stats.Histograms) {
        uint64_t total = 0;
        for (const auto& [value, count] : values) {
            ignore_unused(value);
            total += count;
        }
        result["details"]["histograms"][std::string {histogram.begin(), histogram.end()}] = MakeBakingReportStringDistribution(values, total, "value");
    }
    if (stats.SpriteMesh.Settings.has_value() || !stats.SpriteMesh.Forms.empty() || stats.SpriteMesh.SharedFrameReferences != 0) {
        result["details"]["spriteMesh"] = MakeBakingReportSpriteMeshJson(stats.SpriteMesh, complete_corpus);
    }
    return result;
}

auto BakingReport::Serialize() const -> string
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_locker};

    uint64_t total_input_files = 0;
    uint64_t total_input_bytes = 0;
    uint64_t total_changed_files = 0;
    uint64_t total_changed_bytes = 0;
    uint64_t total_unchanged_files = 0;
    uint64_t total_unchanged_bytes = 0;
    uint64_t total_baker_runs = 0;
    uint64_t total_output_checks = 0;
    uint64_t total_scheduled_output_checks = 0;
    uint64_t total_up_to_date_output_checks = 0;
    uint64_t total_outputs_scheduled = 0;
    uint64_t total_outputs_up_to_date = 0;
    uint64_t total_outputs_submitted = 0;
    uint64_t total_submitted_bytes = 0;

    for (const auto& [pack_name, pack] : _packs) {
        ignore_unused(pack_name);
        total_input_files += numeric_cast<uint64_t>(pack.InputPathSizes.size());
        total_input_bytes += SumBakingReportPathSizes(pack.InputPathSizes);
        total_changed_files += numeric_cast<uint64_t>(pack.ChangedPathSizes.size());
        total_changed_bytes += SumBakingReportPathSizes(pack.ChangedPathSizes);
        total_unchanged_files += numeric_cast<uint64_t>(pack.UnchangedPathSizes.size());
        total_unchanged_bytes += SumBakingReportPathSizes(pack.UnchangedPathSizes);
    }
    for (const auto& [baker_name, baker] : _aggregateBakers) {
        ignore_unused(baker_name);
        total_baker_runs += baker.Invocations;
        total_output_checks += baker.Outputs.CheckCalls;
        total_scheduled_output_checks += baker.Outputs.ScheduledCheckCalls;
        total_up_to_date_output_checks += baker.Outputs.UpToDateCheckCalls;
        total_outputs_scheduled += numeric_cast<uint64_t>(baker.Outputs.ScheduledPaths.size());
        total_outputs_up_to_date += numeric_cast<uint64_t>(baker.Outputs.UpToDatePaths.size());
        total_outputs_submitted += numeric_cast<uint64_t>(baker.Outputs.SubmittedPathSizes.size());
        total_submitted_bytes += SumBakingReportPathSizes(baker.Outputs.SubmittedPathSizes);
    }

    BakingReportJson report {
        {"schemaVersion", 1},
        {"status", _status},
        {"failureMessage", _failureMessage},
        {"buildHash", FO_BUILD_HASH},
        {"bakeOutput", _bakeOutput},
        {"durationMs", _status == "running" ? _duration.GetDuration().milliseconds() : _completedDurationMs},
        {"mode", {{"forceRequested", _forceRequested}, {"fullRebuild", _fullRebuild}, {"rebuildReason", _rebuildReason}, {"singleThread", _singleThread}}},
        {"measurementScope",
            {
                {"availableInputs", "complete_corpus"},
                {"outputActivity", "this_run"},
                {"bakerDetails", "rebuilt_this_run"},
                {"completeCorpusDetails", _fullRebuild},
            }},
        {"totals",
            {
                {"packs", _packs.size()},
                {"bakers", _aggregateBakers.size()},
                {"bakerRuns", total_baker_runs},
                {"inputFiles", total_input_files},
                {"inputBytes", total_input_bytes},
                {"outputCheckCalls", total_output_checks},
                {"scheduledOutputCheckCalls", total_scheduled_output_checks},
                {"upToDateOutputCheckCalls", total_up_to_date_output_checks},
                {"outputsScheduled", total_outputs_scheduled},
                {"outputsUpToDate", total_outputs_up_to_date},
                {"outputsSubmitted", total_outputs_submitted},
                {"submittedBytes", total_submitted_bytes},
                {"filesChanged", total_changed_files},
                {"changedBytes", total_changed_bytes},
                {"filesUnchanged", total_unchanged_files},
                {"unchangedBytes", total_unchanged_bytes},
                {"outdatedFilesDeleted", _outdatedFilesDeleted},
            }},
    };

    report["bakers"] = BakingReportJson::array();
    for (const auto& [name, baker] : _aggregateBakers) {
        report["bakers"].push_back(MakeBakingReportBakerJson(name, baker, _fullRebuild));
    }

    report["packs"] = BakingReportJson::array();
    for (const auto& [name, pack] : _packs) {
        BakingReportJson pack_json {
            {"name", name},
            {"durationMs", pack.DurationMs},
            {"inputs", MakeBakingReportPathSizeJson(pack.InputPathSizes)},
            {"outputs",
                {
                    {"changed", MakeBakingReportPathSizeJson(pack.ChangedPathSizes)},
                    {"unchanged", MakeBakingReportPathSizeJson(pack.UnchangedPathSizes)},
                    {"outdatedFilesDeleted", pack.OutdatedFilesDeleted},
                }},
        };
        pack_json["bakers"] = BakingReportJson::array();
        for (const auto& [baker_name, baker] : pack.Bakers) {
            pack_json["bakers"].push_back(MakeBakingReportBakerJson(baker_name, baker, _fullRebuild));
        }
        report["packs"].push_back(std::move(pack_json));
    }

    const std::string serialized_report = report.dump(2);
    string result {serialized_report.begin(), serialized_report.end()};
    result += '\n';
    return result;
}

FO_END_NAMESPACE
