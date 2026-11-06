// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "SpriteResource.h"

FO_BEGIN_NAMESPACE

struct BakingSettings;

constexpr int32_t SPRITE_MESH_DILATION {1};
constexpr int32_t SPRITE_MESH_MAXIMUM_PADDING {20};

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
