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

#pragma once

#include "Common.h"

#include "SpriteResource.h"

FO_BEGIN_NAMESPACE

struct BakingSettings;

constexpr int32_t SPRITE_MESH_DILATION = 1;
constexpr int32_t SPRITE_MESH_MAXIMUM_PADDING = 20;

struct SpriteMeshBakeConfig
{
    bool Enabled {};
    int32_t AlphaThreshold {};
    size_t MaxTriangles {};
    float32_t AreaSavingsWeight {};
};

enum class SpriteMeshCandidateSource : uint8_t
{
    None,
    GreedyWhole,
    GreedyComponents,
    ClusteredComponents,
    EnclosingTriangle,
    EnclosingQuad,
    DetailedConstrained,
    DetailedSimplified,
    DetailedExpanded,
};

enum class SpriteMeshQuadReason : uint8_t
{
    None,
    Disabled,
    ZeroDimensions,
    DilationFillsFrame,
    ContourExtractionFailed,
    NoValidCandidate,
    ScorePreferredQuad,
};

struct SpriteMeshCandidateSummary
{
    SpriteMeshCandidateSource Source {SpriteMeshCandidateSource::None};
    uint32_t TriangleCount {};
    uint32_t VertexCount {};
    uint64_t DoubleArea {};
    float64_t Score {};
    float32_t SimplifyTolerance {};
    int32_t ActualDilation {};
};

struct BakedSpriteMesh
{
    SpriteMeshKind Kind {SpriteMeshKind::Quad};
    SpriteMeshData Data {};
    SpriteMeshCandidateSource Source {SpriteMeshCandidateSource::None};
    SpriteMeshQuadReason QuadReason {SpriteMeshQuadReason::None};
    uint64_t VisiblePixels {};
    uint64_t DoubleArea {};
    uint32_t SourceComponentCount {};
    optional<uint32_t> DilatedComponentCount {};
    float32_t SimplifyTolerance {};
    int32_t ActualDilation {};
    optional<float64_t> SelectionScore {};
    optional<SpriteMeshCandidateSummary> BestRejectedCandidate {};
};

auto ResolveSpriteMeshBakeConfig(ptr<const BakingSettings> settings) -> SpriteMeshBakeConfig;
auto BuildSpriteMesh(const vector<uint8_t>& rgba, isize32 size, const SpriteMeshBakeConfig& config, int64_t reference_quad_double_area) -> BakedSpriteMesh;
auto SpriteMeshCandidateSourceName(SpriteMeshCandidateSource source) -> string_view;
auto SpriteMeshQuadReasonName(SpriteMeshQuadReason reason) -> string_view;

FO_END_NAMESPACE
