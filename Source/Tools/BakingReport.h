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

struct BakingSettings;

enum class BakingWriteResult : uint8_t
{
    Changed,
    Unchanged,
};

struct SpriteMeshBakingReportSettings
{
    bool Enabled {};
    int32_t AlphaThreshold {};
    size_t MaxTriangles {};
    float32_t AreaSavingsWeight {};
    int32_t BaseDilation {};
    int32_t MaximumPadding {};
};

struct SpriteMeshBakingCandidateReport
{
    string SelectionOrigin {};
    uint32_t TriangleCount {};
    uint32_t VertexCount {};
    uint64_t SubmittedGeometryDoubleArea {};
    float64_t Score {};
    float32_t SimplifyTolerance {};
    int32_t ActualDilation {};
};

struct SpriteMeshBakingFrameReport
{
    string SourcePath {};
    string OutputPath {};
    uint32_t Direction {};
    uint32_t FrameIndex {};
    string Form {};
    string SelectionOrigin {};
    string QuadReason {};
    uint32_t TriangleCount {};
    uint32_t VertexCount {};
    uint32_t SourceComponentCount {};
    optional<uint32_t> DilatedComponentCount {};
    int32_t Padding {};
    float32_t SimplifyTolerance {};
    int32_t ActualDilation {};
    uint64_t SourceFramePixels {};
    uint64_t BakedCanvasPixels {};
    uint64_t VisiblePixels {};
    uint64_t SubmittedGeometryDoubleArea {};
    optional<float64_t> SelectionScore {};
    optional<SpriteMeshBakingCandidateReport> BestRejectedCandidate {};
};

struct BakingReportOutputStats
{
    uint64_t CheckCalls {};
    uint64_t ScheduledCheckCalls {};
    uint64_t UpToDateCheckCalls {};
    uint64_t SubmitCalls {};
    uint64_t SubmittedBytes {};
    set<string> CheckedPaths {};
    set<string> ScheduledPaths {};
    set<string> UpToDatePaths {};
    map<string, uint64_t> SubmittedPathSizes {};
    map<string, uint64_t> ChangedPathSizes {};
    map<string, uint64_t> UnchangedPathSizes {};
};

struct BakingReportScoreStats
{
    uint64_t Count {};
    float64_t Sum {};
    optional<float64_t> Minimum {};
    optional<float64_t> Maximum {};
};

struct BakingReportSpriteResourceStats
{
    uint64_t MeshFrames {};
    uint64_t QuadFrames {};
    uint64_t EmptyFrames {};
};

struct BakingReportSpriteMeshStats
{
    optional<SpriteMeshBakingReportSettings> Settings {};
    uint64_t SharedFrameReferences {};
    map<string, uint64_t> Forms {};
    map<string, uint64_t> SelectionOrigins {};
    map<string, uint64_t> QuadReasons {};
    map<uint32_t, uint64_t> TriangleHistogram {};
    map<uint32_t, uint64_t> VertexHistogram {};
    map<uint32_t, uint64_t> SourceComponentHistogram {};
    map<uint32_t, uint64_t> DilatedComponentHistogram {};
    map<float32_t, uint64_t> SimplifyToleranceHistogram {};
    map<int32_t, uint64_t> ActualDilationHistogram {};
    map<string, uint64_t> RejectedSelectionOrigins {};
    map<uint32_t, uint64_t> RejectedTriangleHistogram {};
    map<float32_t, uint64_t> RejectedSimplifyToleranceHistogram {};
    map<int32_t, uint64_t> RejectedActualDilationHistogram {};
    map<int32_t, uint64_t> PaddingHistogram {};
    map<string, BakingReportSpriteResourceStats> Resources {};
    uint64_t SourceFramePixels {};
    uint64_t BakedCanvasPixels {};
    uint64_t CroppedFrames {};
    uint64_t CroppedTexturePixels {};
    uint64_t ExpandedFrames {};
    uint64_t ExpandedTexturePixels {};
    uint64_t VisiblePixels {};
    uint64_t SubmittedGeometryDoubleArea {};
    uint64_t Vertices {};
    uint64_t Triangles {};
    uint64_t RejectedCandidateReferenceDoubleArea {};
    uint64_t RejectedCandidateDoubleArea {};
    uint64_t RejectedCandidateTriangles {};
    BakingReportScoreStats SelectedScores {};
    BakingReportScoreStats RejectedScores {};
    vector<SpriteMeshBakingFrameReport> LargestMissedSavings {};
    vector<SpriteMeshBakingFrameReport> LargestRejectedCandidateSavings {};
    vector<SpriteMeshBakingFrameReport> MostComplexMeshes {};
    vector<SpriteMeshBakingFrameReport> LargestCroppingSavings {};
    vector<SpriteMeshBakingFrameReport> LargestPaddingOverhead {};
};

struct BakingReportBakerStats
{
    int32_t Order {};
    uint64_t Invocations {};
    uint64_t SuccessfulInvocations {};
    uint64_t FailedInvocations {};
    uint64_t AvailableInputFiles {};
    uint64_t AvailableInputBytes {};
    int64_t DurationMs {};
    vector<string> FailureMessages {};
    BakingReportOutputStats Outputs {};
    map<string, uint64_t> Counters {};
    map<string, map<string, uint64_t>> Histograms {};
    BakingReportSpriteMeshStats SpriteMesh {};
};

struct BakingReportPackStats
{
    map<string, uint64_t> InputPathSizes {};
    map<string, BakingReportBakerStats> Bakers {};
    map<string, uint64_t> ChangedPathSizes {};
    map<string, uint64_t> UnchangedPathSizes {};
    uint64_t OutdatedFilesDeleted {};
    int64_t DurationMs {};
};

class BakingReport final
{
public:
    explicit BakingReport(ptr<const BakingSettings> settings);
    BakingReport(const BakingReport&) = delete;
    BakingReport(BakingReport&&) noexcept = delete;
    auto operator=(const BakingReport&) = delete;
    auto operator=(BakingReport&&) noexcept = delete;
    ~BakingReport() = default;

    void SetRebuildMode(bool effective, string_view reason);
    void RecordPackInput(string_view pack_name, string_view path, size_t size);
    void RecordBakerRegistration(string_view pack_name, string_view baker_name, int32_t order);
    void RecordBakerInvocation(string_view pack_name, string_view baker_name, int32_t order, size_t input_files, uint64_t input_bytes, int64_t duration_ms, bool success, string_view failure_message);
    void RecordOutputCheck(string_view pack_name, string_view baker_name, string_view path, bool scheduled);
    void RecordOutputSubmission(string_view pack_name, string_view baker_name, string_view path, size_t size, BakingWriteResult result);
    void RecordPackDuration(string_view pack_name, int64_t duration_ms);
    void RecordOutdatedFile(string_view path);
    void AddCounter(string_view pack_name, string_view baker_name, string_view name, uint64_t value);
    void AddHistogramValue(string_view pack_name, string_view baker_name, string_view name, string_view value, uint64_t count);
    void RecordSpriteMeshSettings(string_view pack_name, string_view baker_name, const SpriteMeshBakingReportSettings& settings);
    void RecordSpriteMeshFrame(string_view pack_name, string_view baker_name, const SpriteMeshBakingFrameReport& frame);
    void RecordSharedSpriteMeshFrames(string_view pack_name, string_view baker_name, uint64_t count);
    void Complete(bool success, string_view failure_message);
    [[nodiscard]] auto IsFullRebuild() const -> bool;
    [[nodiscard]] auto Serialize() const -> string;

private:
    [[nodiscard]] auto GetPackBaker(string_view pack_name, string_view baker_name) -> BakingReportBakerStats&;
    [[nodiscard]] auto GetAggregateBaker(string_view baker_name) -> BakingReportBakerStats&;

    mutable mutex _locker {};
    string _bakeOutput {};
    bool _forceRequested {};
    bool _singleThread {};
    bool _fullRebuild {};
    string _rebuildReason {"incremental"};
    string _status {"running"};
    string _failureMessage {};
    TimeMeter _duration {};
    int64_t _completedDurationMs {};
    uint64_t _outdatedFilesDeleted {};
    map<string, BakingReportPackStats> _packs {};
    map<string, BakingReportBakerStats> _aggregateBakers {};
};

[[nodiscard]] auto GetBakingReportPath(string_view bake_output) -> string;
[[nodiscard]] auto GetFullBakingReportPath(string_view bake_output) -> string;

FO_END_NAMESPACE
