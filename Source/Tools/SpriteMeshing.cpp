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

#include "SpriteMeshing.h"
#include "Settings.h"

#include "clipper2/clipper.h"
#include "mapbox/earcut.hpp"

FO_BEGIN_NAMESPACE

constexpr size_t SPRITE_MESH_SERIALIZED_VERTEX_LIMIT = 4096;
constexpr size_t SPRITE_MESH_CANDIDATE_BEAM_WIDTH = 8;
constexpr size_t SPRITE_MESH_CANDIDATE_REMOVAL_DEPTH = 8;
constexpr size_t SPRITE_MESH_COMPONENT_CLUSTER_LIMIT = 64;

struct SpriteContour
{
    vector<ipos32> Points {};
    int64_t DoubleArea {};
};

struct SpriteBoundaryEdge
{
    ipos32 Start {};
    uint8_t Direction {};
};

struct SpriteValidationPoint
{
    float64_t X {};
    float64_t Y {};
};

enum class SpriteMeshBuildFailure : uint8_t
{
    None,
    ContourExtraction,
    ContourOffset,
    SerializedVertexLimit,
    Triangulation,
    InvalidMeshShape,
    InvalidIndex,
    DegenerateTriangle,
    AreaOverflow,
    ValidationLimit,
    AreaMismatch,
    ClipsVisiblePixel,
    EscapesToleranceMask,
};

struct SpriteMeshBuildAttempt
{
    optional<SpriteMeshData> Mesh {};
    SpriteMeshBuildFailure Failure {SpriteMeshBuildFailure::None};
};

[[nodiscard]] static auto TryBuildSpriteMesh(const vector<uint8_t>& original_mask, const vector<uint8_t>& dilated_mask, const vector<SpriteContour>& simplified_contours, const vector<SpriteContour>& exact_contours, const vector<SpriteContour>& allowed_contours, int32_t width, int32_t height, float32_t simplify_tolerance, int32_t dilation, int64_t& mesh_double_area) -> SpriteMeshBuildAttempt;

struct SpriteMeshCandidate
{
    SpriteMeshData Mesh {};
    vector<SpriteContour> Enclosures {};
    int64_t DoubleArea {};
    bool Validated {};
    SpriteMeshCandidateSource Source {SpriteMeshCandidateSource::GreedyWhole};
    float32_t SimplifyTolerance {};
    int32_t ActualDilation {SPRITE_MESH_DILATION};
    float64_t Score {};
};

struct SpriteMeshSearchResult
{
    optional<SpriteMeshCandidate> Best {};
    optional<SpriteMeshCandidateSummary> BestRejectedCandidate {};
    SpriteMeshQuadReason QuadReason {SpriteMeshQuadReason::NoValidCandidate};
};

static auto CountSpriteMaskComponents(const_span<uint8_t> mask, int32_t width, int32_t height) -> uint32_t;
static auto DilateSpriteMask(const vector<uint8_t>& source, int32_t width, int32_t height, int32_t radius) -> vector<uint8_t>;
static auto TryBuildBestSpriteMesh(const vector<uint8_t>& original_mask, const vector<uint8_t>& dilated_mask, int32_t width, int32_t height, const SpriteMeshBakeConfig& config, int64_t reference_quad_double_area) -> SpriteMeshSearchResult;

auto ResolveSpriteMeshBakeConfig(ptr<const BakingSettings> settings) -> SpriteMeshBakeConfig
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(settings->AlphaThreshold >= 0 && settings->AlphaThreshold <= 254, "Sprite mesh alpha threshold must be in range 0..254", settings->AlphaThreshold);
    FO_VERIFY_AND_THROW(settings->MaxTriangles >= 1, "Sprite mesh maximum triangle count must be positive", settings->MaxTriangles);
    FO_VERIFY_AND_THROW(std::isfinite(settings->AreaSavingsWeight) && settings->AreaSavingsWeight >= 0.0f, "Sprite mesh area savings weight must be finite and non-negative", settings->AreaSavingsWeight);

    return SpriteMeshBakeConfig {
        .Enabled = settings->Enabled,
        .AlphaThreshold = settings->AlphaThreshold,
        .MaxTriangles = numeric_cast<size_t>(settings->MaxTriangles),
        .AreaSavingsWeight = settings->AreaSavingsWeight,
    };
}

auto BuildSpriteMesh(const vector<uint8_t>& rgba, isize32 size, const SpriteMeshBakeConfig& config, int64_t reference_quad_double_area) -> BakedSpriteMesh
{
    FO_STACK_TRACE_ENTRY();

    int32_t width = size.width;
    int32_t height = size.height;
    size_t pixel_count = numeric_cast<size_t>(size.width) * size.height;
    FO_VERIFY_AND_THROW(rgba.size() == pixel_count * 4, "Sprite mesh RGBA payload size does not match dimensions", rgba.size(), size);
    BakedSpriteMesh result;
    result.DoubleArea = numeric_cast<uint64_t>(reference_quad_double_area);

    if (width == 0 || height == 0) {
        result.QuadReason = SpriteMeshQuadReason::ZeroDimensions;
        return result;
    }

    vector<uint8_t> original_mask(pixel_count);
    size_t visible_pixels = 0;

    for (size_t i = 0; i < pixel_count; i++) {
        if (rgba[i * 4 + 3] > config.AlphaThreshold) {
            original_mask[i] = 1;
            visible_pixels++;
        }
    }

    result.VisiblePixels = numeric_cast<uint64_t>(visible_pixels);
    result.SourceComponentCount = CountSpriteMaskComponents(original_mask, width, height);

    if (!config.Enabled) {
        result.QuadReason = SpriteMeshQuadReason::Disabled;
        return result;
    }

    if (visible_pixels == 0) {
        result.Kind = SpriteMeshKind::Empty;
        result.DoubleArea = 0;
        result.DilatedComponentCount = uint32_t {0};
        return result;
    }

    vector<uint8_t> dilated_mask = DilateSpriteMask(original_mask, width, height, SPRITE_MESH_DILATION);
    size_t dilated_pixels = numeric_cast<size_t>(std::ranges::count(dilated_mask, uint8_t {1}));
    result.DilatedComponentCount = CountSpriteMaskComponents(dilated_mask, width, height);

    if (dilated_pixels == pixel_count) {
        result.QuadReason = SpriteMeshQuadReason::DilationFillsFrame;
        return result;
    }

    FO_VERIFY_AND_THROW(reference_quad_double_area > 0, "Sprite mesh reference quad area must be positive", reference_quad_double_area);

    SpriteMeshSearchResult search_result = TryBuildBestSpriteMesh(original_mask, dilated_mask, width, height, config, reference_quad_double_area);
    if (!search_result.Best.has_value()) {
        result.QuadReason = search_result.QuadReason;
        result.BestRejectedCandidate = std::move(search_result.BestRejectedCandidate);
        return result;
    }

    result.Kind = SpriteMeshKind::Mesh;
    result.Data = std::move(search_result.Best->Mesh);
    result.Data.SourceSize = size;
    result.Data.SourceOffset = {};
    result.Source = search_result.Best->Source;
    result.DoubleArea = numeric_cast<uint64_t>(search_result.Best->DoubleArea);
    result.SimplifyTolerance = search_result.Best->SimplifyTolerance;
    result.ActualDilation = search_result.Best->ActualDilation;
    result.SelectionScore = search_result.Best->Score;
    return result;
}

auto SpriteMeshCandidateSourceName(SpriteMeshCandidateSource source) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    switch (source) {
    case SpriteMeshCandidateSource::None:
        return "none";
    case SpriteMeshCandidateSource::GreedyWhole:
        return "greedy_whole";
    case SpriteMeshCandidateSource::GreedyComponents:
        return "greedy_components";
    case SpriteMeshCandidateSource::ClusteredComponents:
        return "clustered_components";
    case SpriteMeshCandidateSource::EnclosingTriangle:
        return "enclosing_triangle";
    case SpriteMeshCandidateSource::EnclosingQuad:
        return "enclosing_quad";
    case SpriteMeshCandidateSource::DetailedConstrained:
        return "detailed_constrained";
    case SpriteMeshCandidateSource::DetailedSimplified:
        return "detailed_simplified";
    case SpriteMeshCandidateSource::DetailedExpanded:
        return "detailed_expanded";
    default:
        FO_UNREACHABLE_PLACE();
    }
}

auto SpriteMeshQuadReasonName(SpriteMeshQuadReason reason) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    switch (reason) {
    case SpriteMeshQuadReason::None:
        return "none";
    case SpriteMeshQuadReason::Disabled:
        return "disabled";
    case SpriteMeshQuadReason::ZeroDimensions:
        return "zero_dimensions";
    case SpriteMeshQuadReason::DilationFillsFrame:
        return "dilation_fills_frame";
    case SpriteMeshQuadReason::ContourExtractionFailed:
        return "contour_extraction_failed";
    case SpriteMeshQuadReason::NoValidCandidate:
        return "no_valid_candidate";
    case SpriteMeshQuadReason::ScorePreferredQuad:
        return "score_preferred_quad";
    default:
        FO_UNREACHABLE_PLACE();
    }
}

static auto CountSpriteMaskComponents(const_span<uint8_t> mask, int32_t width, int32_t height) -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(width >= 0 && height >= 0, "Sprite mask dimensions must be non-negative", width, height);
    FO_VERIFY_AND_THROW(mask.size() == numeric_cast<size_t>(width) * height, "Sprite mask size does not match dimensions", mask.size(), width, height);

    if (mask.empty()) {
        return 0;
    }

    vector<uint8_t> visited(mask.size());
    vector<size_t> pending;
    size_t row_width = numeric_cast<size_t>(width);
    uint32_t components = 0;

    for (size_t start = 0; start < mask.size(); start++) {
        if (mask[start] == 0 || visited[start] != 0) {
            continue;
        }

        FO_VERIFY_AND_THROW(components != std::numeric_limits<uint32_t>::max(), "Sprite mask component count exceeds report range", mask.size());
        components++;
        visited[start] = 1;
        pending.emplace_back(start);

        while (!pending.empty()) {
            size_t current = pending.back();
            pending.pop_back();
            size_t x = current % row_width;

            auto add_neighbor = [&](size_t neighbor) {
                if (mask[neighbor] != 0 && visited[neighbor] == 0) {
                    visited[neighbor] = 1;
                    pending.emplace_back(neighbor);
                }
            };

            if (x != 0) {
                add_neighbor(current - 1);
            }
            if (x + 1 < row_width) {
                add_neighbor(current + 1);
            }
            if (current >= row_width) {
                add_neighbor(current - row_width);
            }
            if (current + row_width < mask.size()) {
                add_neighbor(current + row_width);
            }
        }
    }

    return components;
}

static auto DilateSpriteMask(const vector<uint8_t>& source, int32_t width, int32_t height, int32_t radius) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    if (radius == 0 || source.empty()) {
        return source;
    }

    vector<uint8_t> horizontal(source.size());
    vector<uint8_t> result(source.size());

    // A separable square dilation keeps the configured pixel padding conservative while staying linear in image area.
    for (int32_t y = 0; y < height; y++) {
        size_t visible_count = 0;
        int32_t initial_right = std::min(width - 1, radius);

        for (int32_t x = 0; x <= initial_right; x++) {
            visible_count += source[numeric_cast<size_t>(y) * width + x] != 0 ? 1u : 0u;
        }

        for (int32_t x = 0; x < width; x++) {
            horizontal[numeric_cast<size_t>(y) * width + x] = visible_count != 0 ? uint8_t {1} : uint8_t {0};

            int32_t remove_x = x - radius;
            int32_t add_x = x + radius + 1;

            if (remove_x >= 0) {
                visible_count -= source[numeric_cast<size_t>(y) * width + remove_x] != 0 ? 1u : 0u;
            }
            if (add_x < width) {
                visible_count += source[numeric_cast<size_t>(y) * width + add_x] != 0 ? 1u : 0u;
            }
        }
    }

    for (int32_t x = 0; x < width; x++) {
        size_t visible_count = 0;
        int32_t initial_bottom = std::min(height - 1, radius);

        for (int32_t y = 0; y <= initial_bottom; y++) {
            visible_count += horizontal[numeric_cast<size_t>(y) * width + x] != 0 ? 1u : 0u;
        }

        for (int32_t y = 0; y < height; y++) {
            result[numeric_cast<size_t>(y) * width + x] = visible_count != 0 ? uint8_t {1} : uint8_t {0};

            int32_t remove_y = y - radius;
            int32_t add_y = y + radius + 1;

            if (remove_y >= 0) {
                visible_count -= horizontal[numeric_cast<size_t>(remove_y) * width + x] != 0 ? 1u : 0u;
            }
            if (add_y < height) {
                visible_count += horizontal[numeric_cast<size_t>(add_y) * width + x] != 0 ? 1u : 0u;
            }
        }
    }

    return result;
}

static auto SpriteContourDoubleArea(const vector<ipos32>& points) noexcept -> int64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    int64_t double_area = 0;

    for (size_t i = 0; i < points.size(); i++) {
        ipos32 current = points[i];
        ipos32 next = points[(i + 1) % points.size()];
        double_area += numeric_cast<int64_t>(current.x) * next.y - numeric_cast<int64_t>(next.x) * current.y;
    }

    return double_area;
}

static auto SpritePointCross(ipos32 a, ipos32 b, ipos32 c) noexcept -> int64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<int64_t>(b.x - a.x) * (c.y - a.y) - numeric_cast<int64_t>(b.y - a.y) * (c.x - a.x);
}

static auto IsSpritePointBetween(ipos32 a, ipos32 point, ipos32 b) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return point.x >= std::min(a.x, b.x) && point.x <= std::max(a.x, b.x) && point.y >= std::min(a.y, b.y) && point.y <= std::max(a.y, b.y);
}

static auto RoundSpriteIntersectionPoint(float64_t x, float64_t y) -> optional<ipos32>
{
    FO_STACK_TRACE_ENTRY();

    // Nearly parallel support lines intersect arbitrarily far from the frame, and iround throws instead of
    // saturating, so unrepresentable coordinates must be rejected before conversion. The bound sits far outside
    // any frame yet well inside the int32 range, so the callers keep owning the actual frame range checks.
    // The negated form also rejects the non-finite coordinates that iround refuses.
    constexpr float64_t coordinate_bound = 1.0e9;

    if (!(std::abs(x) <= coordinate_bound && std::abs(y) <= coordinate_bound)) {
        return std::nullopt;
    }

    return ipos32 {iround<int32_t>(x), iround<int32_t>(y)};
}

static void NormalizeSpriteContour(vector<ipos32>& points)
{
    FO_STACK_TRACE_ENTRY();

    if (points.empty()) {
        return;
    }

    vector<ipos32> unique_points;
    unique_points.reserve(points.size());

    for (ipos32 point : points) {
        if (unique_points.empty() || unique_points.back() != point) {
            unique_points.emplace_back(point);
        }
    }
    if (unique_points.size() > 1 && unique_points.front() == unique_points.back()) {
        unique_points.pop_back();
    }

    bool changed = true;

    while (changed && unique_points.size() >= 3) {
        changed = false;
        vector<ipos32> normalized;
        normalized.reserve(unique_points.size());

        for (size_t i = 0; i < unique_points.size(); i++) {
            ipos32 previous = unique_points[(i + unique_points.size() - 1) % unique_points.size()];
            ipos32 current = unique_points[i];
            ipos32 next = unique_points[(i + 1) % unique_points.size()];

            if (SpritePointCross(previous, current, next) == 0 && IsSpritePointBetween(previous, current, next)) {
                changed = true;
                continue;
            }

            normalized.emplace_back(current);
        }

        unique_points = std::move(normalized);
    }

    points = std::move(unique_points);
}

static auto SpritePointSegmentDistanceSquared(ipos32 point, ipos32 segment_start, ipos32 segment_end) noexcept -> float64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    float64_t dx = numeric_cast<float64_t>(segment_end.x - segment_start.x);
    float64_t dy = numeric_cast<float64_t>(segment_end.y - segment_start.y);

    if (is_float_equal(dx, 0.0) && is_float_equal(dy, 0.0)) {
        float64_t point_dx = numeric_cast<float64_t>(point.x - segment_start.x);
        float64_t point_dy = numeric_cast<float64_t>(point.y - segment_start.y);
        return point_dx * point_dx + point_dy * point_dy;
    }

    float64_t projection = std::clamp((numeric_cast<float64_t>(point.x - segment_start.x) * dx + numeric_cast<float64_t>(point.y - segment_start.y) * dy) / (dx * dx + dy * dy), 0.0, 1.0);
    float64_t projected_x = numeric_cast<float64_t>(segment_start.x) + projection * dx;
    float64_t projected_y = numeric_cast<float64_t>(segment_start.y) + projection * dy;
    float64_t point_dx = numeric_cast<float64_t>(point.x) - projected_x;
    float64_t point_dy = numeric_cast<float64_t>(point.y) - projected_y;
    return point_dx * point_dx + point_dy * point_dy;
}

static auto SimplifyOpenSpriteContour(const vector<ipos32>& points, float64_t tolerance_squared) -> vector<ipos32>
{
    FO_STACK_TRACE_ENTRY();

    if (points.size() <= 2) {
        return points;
    }

    vector<uint8_t> keep(points.size());
    keep.front() = 1;
    keep.back() = 1;

    vector<pair<size_t, size_t>> ranges;
    ranges.emplace_back(0, points.size() - 1);

    while (!ranges.empty()) {
        auto range = ranges.back();
        ranges.pop_back();

        float64_t farthest_distance = tolerance_squared;
        optional<size_t> farthest_index;

        for (size_t i = range.first + 1; i < range.second; i++) {
            float64_t distance = SpritePointSegmentDistanceSquared(points[i], points[range.first], points[range.second]);

            if (distance > farthest_distance) {
                farthest_distance = distance;
                farthest_index = i;
            }
        }

        if (farthest_index.has_value()) {
            keep[*farthest_index] = 1;
            ranges.emplace_back(*farthest_index, range.second);
            ranges.emplace_back(range.first, *farthest_index);
        }
    }

    vector<ipos32> result;
    result.reserve(points.size());

    for (size_t i = 0; i < points.size(); i++) {
        if (keep[i] != 0) {
            result.emplace_back(points[i]);
        }
    }

    return result;
}

static void SimplifyClosedSpriteContour(vector<ipos32>& points, float32_t tolerance)
{
    FO_STACK_TRACE_ENTRY();

    if (tolerance <= 0.0f || points.size() <= 3) {
        return;
    }

    size_t split_index = 1;
    int64_t split_distance = -1;

    for (size_t i = 1; i < points.size(); i++) {
        int64_t dx = numeric_cast<int64_t>(points[i].x) - points[0].x;
        int64_t dy = numeric_cast<int64_t>(points[i].y) - points[0].y;
        int64_t distance = dx * dx + dy * dy;

        if (distance > split_distance) {
            split_distance = distance;
            split_index = i;
        }
    }

    vector<ipos32> first_half(points.begin(), points.begin() + numeric_cast<ptrdiff_t>(split_index + 1));
    vector<ipos32> second_half(points.begin() + numeric_cast<ptrdiff_t>(split_index), points.end());
    second_half.emplace_back(points.front());

    float64_t tolerance_squared = numeric_cast<float64_t>(tolerance) * tolerance;
    first_half = SimplifyOpenSpriteContour(first_half, tolerance_squared);
    second_half = SimplifyOpenSpriteContour(second_half, tolerance_squared);

    vector<ipos32> simplified;
    simplified.reserve(first_half.size() + second_half.size());
    simplified.insert(simplified.end(), first_half.begin(), first_half.end());

    if (second_half.size() > 2) {
        simplified.insert(simplified.end(), second_half.begin() + 1, second_half.end() - 1);
    }

    NormalizeSpriteContour(simplified);

    if (simplified.size() >= 3) {
        points = std::move(simplified);
    }
}

static void SimplifyEnclosingClosedSpriteContour(vector<ipos32>& points, float32_t tolerance)
{
    FO_STACK_TRACE_ENTRY();

    if (tolerance <= 0.0f || points.size() <= 3) {
        return;
    }

    vector<ipos32> source = points;
    vector<ipos32> simplified = points;
    SimplifyClosedSpriteContour(simplified, tolerance);

    if (simplified.size() >= source.size()) {
        return;
    }

    vector<size_t> source_indices;
    source_indices.reserve(simplified.size());
    size_t search_from = 0;

    for (ipos32 point : simplified) {
        optional<size_t> found;

        for (size_t offset = 0; offset < source.size(); offset++) {
            size_t index = (search_from + offset) % source.size();
            if (source[index] == point) {
                found = index;
                break;
            }
        }

        if (!found.has_value()) {
            return;
        }

        source_indices.emplace_back(*found);
        search_from = (*found + 1) % source.size();
    }

    struct SupportLine
    {
        int64_t NormalX {};
        int64_t NormalY {};
        float64_t Limit {};
    };

    vector<SupportLine> lines;
    lines.reserve(simplified.size());
    constexpr float64_t quantization_margin = 0.5;

    for (size_t i = 0; i < simplified.size(); i++) {
        ipos32 a = simplified[i];
        ipos32 b = simplified[(i + 1) % simplified.size()];
        int64_t direction_x = numeric_cast<int64_t>(b.x) - a.x;
        int64_t direction_y = numeric_cast<int64_t>(b.y) - a.y;

        if (direction_x == 0 && direction_y == 0) {
            return;
        }

        int64_t normal_x = -direction_y;
        int64_t normal_y = direction_x;
        int64_t limit = std::numeric_limits<int64_t>::max();
        size_t source_index = source_indices[i];
        size_t source_end = source_indices[(i + 1) % simplified.size()];

        while (true) {
            ipos32 point = source[source_index];
            limit = std::min(limit, normal_x * point.x + normal_y * point.y);

            if (source_index == source_end) {
                break;
            }

            source_index = (source_index + 1) % source.size();
        }

        float64_t margin = std::hypot(numeric_cast<float64_t>(normal_x), numeric_cast<float64_t>(normal_y)) * quantization_margin;
        lines.emplace_back(SupportLine {.NormalX = normal_x, .NormalY = normal_y, .Limit = numeric_cast<float64_t>(limit) - margin});
    }

    vector<ipos32> enclosing;
    enclosing.reserve(lines.size());

    for (size_t i = 0; i < lines.size(); i++) {
        const SupportLine& previous = lines[(i + lines.size() - 1) % lines.size()];
        const SupportLine& current = lines[i];
        int64_t integer_determinant = previous.NormalX * current.NormalY - previous.NormalY * current.NormalX;

        if (integer_determinant == 0) {
            return;
        }

        float64_t determinant = numeric_cast<float64_t>(integer_determinant);
        float64_t x = (previous.Limit * numeric_cast<float64_t>(current.NormalY) - numeric_cast<float64_t>(previous.NormalY) * current.Limit) / determinant;
        float64_t y = (numeric_cast<float64_t>(previous.NormalX) * current.Limit - previous.Limit * numeric_cast<float64_t>(current.NormalX)) / determinant;
        optional<ipos32> point = RoundSpriteIntersectionPoint(x, y);

        if (!point.has_value()) {
            return;
        }

        enclosing.emplace_back(point.value());
    }

    NormalizeSpriteContour(enclosing);
    if (enclosing.size() >= 3 && SpriteContourDoubleArea(enclosing) > 0) {
        points = std::move(enclosing);
    }
}

static auto RemoveEnclosingSpriteContourPoint(vector<ipos32>& points, size_t point_index, int64_t contour_double_area) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (points.size() <= 3 || point_index >= points.size() || contour_double_area == 0) {
        return false;
    }

    size_t previous_index = (point_index + points.size() - 1) % points.size();
    size_t next_index = (point_index + 1) % points.size();
    ipos32 previous = points[previous_index];
    ipos32 removed = points[point_index];
    ipos32 next = points[next_index];
    int64_t removed_side = SpritePointCross(previous, next, removed);
    bool chord_clips_contour = contour_double_area > 0 ? removed_side < 0 : removed_side > 0;

    if (!chord_clips_contour) {
        points.erase(points.begin() + numeric_cast<ptrdiff_t>(point_index));
        NormalizeSpriteContour(points);
        return points.size() >= 3;
    }

    ipos32 before_previous = points[(previous_index + points.size() - 1) % points.size()];
    ipos32 after_next = points[(next_index + 1) % points.size()];
    float64_t replacement_direction_x = numeric_cast<float64_t>(next.x - previous.x);
    float64_t replacement_direction_y = numeric_cast<float64_t>(next.y - previous.y);
    float64_t replacement_length = std::hypot(replacement_direction_x, replacement_direction_y);

    if (replacement_length == 0.0) {
        return false;
    }

    // Preserve the removed exterior support point by moving the replacement chord through it;
    // intersecting the adjacent edge lines keeps the vertex count one lower without clipping the contour.
    float64_t replacement_x = removed.x;
    float64_t replacement_y = removed.y;

    auto intersect_lines = [](float64_t first_x, float64_t first_y, float64_t first_direction_x, float64_t first_direction_y, float64_t second_x, float64_t second_y, float64_t second_direction_x, float64_t second_direction_y) -> optional<pair<float64_t, float64_t>> {
        float64_t determinant = first_direction_x * second_direction_y - first_direction_y * second_direction_x;

        if (std::abs(determinant) <= std::numeric_limits<float64_t>::epsilon()) {
            return std::nullopt;
        }

        float64_t offset_x = second_x - first_x;
        float64_t offset_y = second_y - first_y;
        float64_t factor = (offset_x * second_direction_y - offset_y * second_direction_x) / determinant;
        return pair<float64_t, float64_t> {first_x + first_direction_x * factor, first_y + first_direction_y * factor};
    };

    optional<pair<float64_t, float64_t>> new_previous = intersect_lines(before_previous.x, before_previous.y, previous.x - before_previous.x, previous.y - before_previous.y, replacement_x, replacement_y, replacement_direction_x, replacement_direction_y);
    optional<pair<float64_t, float64_t>> new_next = intersect_lines(replacement_x, replacement_y, replacement_direction_x, replacement_direction_y, next.x, next.y, after_next.x - next.x, after_next.y - next.y);
    if (!new_previous.has_value() || !new_next.has_value()) {
        return false;
    }

    optional<ipos32> new_previous_point = RoundSpriteIntersectionPoint(new_previous->first, new_previous->second);
    optional<ipos32> new_next_point = RoundSpriteIntersectionPoint(new_next->first, new_next->second);

    if (!new_previous_point.has_value() || !new_next_point.has_value()) {
        return false;
    }

    vector<ipos32> reduced;
    reduced.reserve(points.size() - 1);

    for (size_t i = 0; i < points.size(); i++) {
        if (i == point_index) {
            continue;
        }
        if (i == previous_index) {
            reduced.emplace_back(new_previous_point.value());
        }
        else if (i == next_index) {
            reduced.emplace_back(new_next_point.value());
        }
        else {
            reduced.emplace_back(points[i]);
        }
    }

    NormalizeSpriteContour(reduced);
    int64_t reduced_double_area = SpriteContourDoubleArea(reduced);

    if (reduced.size() < 3 || reduced_double_area == 0 || (reduced_double_area > 0) != (contour_double_area > 0)) {
        return false;
    }

    points = std::move(reduced);
    return true;
}

static auto BuildSpriteConvexHull(const vector<SpriteContour>& contours) -> vector<ipos32>
{
    FO_STACK_TRACE_ENTRY();

    vector<ipos32> points;

    for (const SpriteContour& contour : contours) {
        points.insert(points.end(), contour.Points.begin(), contour.Points.end());
    }

    std::ranges::sort(points, [](const ipos32 left, const ipos32 right) { return left.x != right.x ? left.x < right.x : left.y < right.y; });
    points.erase(std::unique(points.begin(), points.end()), points.end());

    if (points.size() <= 3) {
        return points;
    }

    vector<ipos32> hull;
    hull.reserve(points.size() * 2);

    for (ipos32 point : points) {
        while (hull.size() >= 2 && SpritePointCross(hull[hull.size() - 2], hull.back(), point) <= 0) {
            hull.pop_back();
        }

        hull.emplace_back(point);
    }

    size_t lower_size = hull.size();

    for (size_t i = points.size() - 1; i > 0; i--) {
        ipos32 point = points[i - 1];

        while (hull.size() > lower_size && SpritePointCross(hull[hull.size() - 2], hull.back(), point) <= 0) {
            hull.pop_back();
        }

        hull.emplace_back(point);
    }

    hull.pop_back();
    NormalizeSpriteContour(hull);

    if (SpriteContourDoubleArea(hull) < 0) {
        std::ranges::reverse(hull);
    }

    return hull;
}

static auto DoesSpriteConvexPolygonCoverHull(const vector<ipos32>& polygon, const vector<ipos32>& hull) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (polygon.size() < 3 || hull.empty()) {
        return false;
    }

    return std::ranges::all_of(hull, [&polygon](const ipos32 hull_point) {
        bool has_positive_cross = false;
        bool has_negative_cross = false;

        for (size_t i = 0; i < polygon.size(); i++) {
            int64_t cross = SpritePointCross(polygon[i], polygon[(i + 1) % polygon.size()], hull_point);
            has_positive_cross = has_positive_cross || cross > 0;
            has_negative_cross = has_negative_cross || cross < 0;
        }

        return !(has_positive_cross && has_negative_cross);
    });
}

static auto SpriteGridPointKey(ipos32 point) noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return (numeric_cast<uint64_t>(numeric_cast<uint32_t>(point.y)) << 32u) | numeric_cast<uint32_t>(point.x);
}

static auto AdvanceSpriteBoundaryPoint(ipos32 point, uint8_t direction) noexcept -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (direction) {
    case 0:
        point.x++;
        break;
    case 1:
        point.y++;
        break;
    case 2:
        point.x--;
        break;
    case 3:
        point.y--;
        break;
    default:
        FO_STRONG_ASSERT(false, "Sprite boundary direction is invalid");
    }

    return point;
}

static auto ExtractSpriteContours(const vector<uint8_t>& mask, int32_t width, int32_t height, float32_t simplify_tolerance) -> optional<vector<SpriteContour>>
{
    FO_STACK_TRACE_ENTRY();

    vector<SpriteBoundaryEdge> edges;
    unordered_map<uint64_t, uint8_t> outgoing_edges;

    auto add_edge = [&edges, &outgoing_edges](ipos32 start, uint8_t direction) {
        edges.emplace_back(SpriteBoundaryEdge {.Start = start, .Direction = direction});
        outgoing_edges[SpriteGridPointKey(start)] |= numeric_cast<uint8_t>(1u << direction);
    };
    auto is_visible = [&mask, width, height](int32_t x, int32_t y) -> bool { return x >= 0 && x < width && y >= 0 && y < height && mask[numeric_cast<size_t>(y) * width + x] != 0; };

    for (int32_t y = 0; y < height; y++) {
        for (int32_t x = 0; x < width; x++) {
            if (!is_visible(x, y)) {
                continue;
            }

            if (!is_visible(x, y - 1)) {
                // Boundary edges are clockwise in screen coordinates, with opaque pixels on their right side.
                add_edge({x, y}, 0);
            }
            if (!is_visible(x + 1, y)) {
                add_edge({x + 1, y}, 1);
            }
            if (!is_visible(x, y + 1)) {
                add_edge({x + 1, y + 1}, 2);
            }
            if (!is_visible(x - 1, y)) {
                add_edge({x, y + 1}, 3);
            }
        }
    }

    vector<SpriteContour> contours;

    for (const SpriteBoundaryEdge& first_edge : edges) {
        uint64_t first_key = SpriteGridPointKey(first_edge.Start);
        auto first_outgoing = outgoing_edges.find(first_key);
        uint8_t first_bit = numeric_cast<uint8_t>(1u << first_edge.Direction);

        if (first_outgoing == outgoing_edges.end() || (first_outgoing->second & first_bit) == 0) {
            continue;
        }

        first_outgoing->second &= numeric_cast<uint8_t>(0xFFu ^ first_bit);

        vector<ipos32> points;
        points.emplace_back(first_edge.Start);

        ipos32 current = AdvanceSpriteBoundaryPoint(first_edge.Start, first_edge.Direction);
        uint8_t incoming_direction = first_edge.Direction;
        size_t traversed_edges = 1;

        while (current != first_edge.Start) {
            points.emplace_back(current);

            auto outgoing = outgoing_edges.find(SpriteGridPointKey(current));
            if (outgoing == outgoing_edges.end()) {
                return std::nullopt;
            }

            const uint8_t candidate_directions[] = {
                // Prefer keeping the same opaque component on the right. This also separates diagonal pixel islands.
                numeric_cast<uint8_t>((incoming_direction + 1) % 4),
                incoming_direction,
                numeric_cast<uint8_t>((incoming_direction + 3) % 4),
                numeric_cast<uint8_t>((incoming_direction + 2) % 4),
            };
            optional<uint8_t> next_direction;

            for (uint8_t direction : candidate_directions) {
                uint8_t bit = numeric_cast<uint8_t>(1u << direction);

                if ((outgoing->second & bit) != 0) {
                    next_direction = direction;
                    outgoing->second &= numeric_cast<uint8_t>(0xFFu ^ bit);
                    break;
                }
            }

            if (!next_direction.has_value()) {
                return std::nullopt;
            }

            incoming_direction = *next_direction;
            current = AdvanceSpriteBoundaryPoint(current, incoming_direction);
            traversed_edges++;

            if (traversed_edges > edges.size()) {
                return std::nullopt;
            }
        }

        NormalizeSpriteContour(points);

        SimplifyClosedSpriteContour(points, simplify_tolerance);

        if (points.size() < 3) {
            return std::nullopt;
        }

        int64_t double_area = SpriteContourDoubleArea(points);

        if (double_area == 0) {
            return std::nullopt;
        }

        contours.emplace_back(SpriteContour {.Points = std::move(points), .DoubleArea = double_area});
    }

    for (const auto& [point, directions] : outgoing_edges) {
        ignore_unused(point);

        if (directions != 0) {
            return std::nullopt;
        }
    }

    return contours;
}

static auto OffsetSpriteContours(const vector<SpriteContour>& contours, const vector<SpriteContour>& exact_contours, const vector<SpriteContour>& allowed_contours, int32_t width, int32_t height, float32_t simplify_tolerance, int32_t dilation) -> optional<vector<SpriteContour>>
{
    FO_STACK_TRACE_ENTRY();

    if (dilation <= 0) {
        return contours;
    }

    constexpr int64_t coordinate_scale = 256;
    auto make_paths = [](const vector<SpriteContour>& source_contours) -> Clipper2Lib::Paths64 {
        Clipper2Lib::Paths64 paths;
        paths.reserve(source_contours.size());

        for (const SpriteContour& contour : source_contours) {
            Clipper2Lib::Path64& path = paths.emplace_back();
            path.reserve(contour.Points.size());

            for (ipos32 point : contour.Points) {
                path.emplace_back(numeric_cast<int64_t>(point.x) * coordinate_scale, numeric_cast<int64_t>(point.y) * coordinate_scale);
            }
        }

        return paths;
    };

    Clipper2Lib::Paths64 source_paths = make_paths(contours);
    source_paths = Clipper2Lib::Union(source_paths, Clipper2Lib::FillRule::NonZero);

    if (source_paths.empty()) {
        return std::nullopt;
    }

    Clipper2Lib::Paths64 offset_paths = Clipper2Lib::InflatePaths(source_paths, numeric_cast<float64_t>(dilation) * coordinate_scale, Clipper2Lib::JoinType::Miter, Clipper2Lib::EndType::Polygon);

    if (offset_paths.empty()) {
        return std::nullopt;
    }

    Clipper2Lib::Paths64 exact_paths = make_paths(exact_contours);
    offset_paths.insert(offset_paths.end(), exact_paths.begin(), exact_paths.end());
    offset_paths = Clipper2Lib::Union(offset_paths, Clipper2Lib::FillRule::NonZero);

    Clipper2Lib::Paths64 allowed_paths = make_paths(allowed_contours);
    allowed_paths = Clipper2Lib::Union(allowed_paths, Clipper2Lib::FillRule::NonZero);
    offset_paths = Clipper2Lib::Intersect(offset_paths, allowed_paths, Clipper2Lib::FillRule::NonZero);

    if (simplify_tolerance > 0.0f) {
        offset_paths = Clipper2Lib::SimplifyPaths(offset_paths, numeric_cast<float64_t>(simplify_tolerance) * coordinate_scale);
        offset_paths.insert(offset_paths.end(), exact_paths.begin(), exact_paths.end());
        offset_paths = Clipper2Lib::Union(offset_paths, Clipper2Lib::FillRule::NonZero);
        offset_paths = Clipper2Lib::Intersect(offset_paths, allowed_paths, Clipper2Lib::FillRule::NonZero);
    }

    vector<SpriteContour> result;
    result.reserve(offset_paths.size());

    for (const Clipper2Lib::Path64& offset_path : offset_paths) {
        vector<ipos32> points;
        points.reserve(offset_path.size());

        for (const Clipper2Lib::Point64& point : offset_path) {
            int32_t x = std::clamp(iround<int32_t>(numeric_cast<float64_t>(point.x) / coordinate_scale), 0, width);
            int32_t y = std::clamp(iround<int32_t>(numeric_cast<float64_t>(point.y) / coordinate_scale), 0, height);
            points.emplace_back(x, y);
        }

        NormalizeSpriteContour(points);

        if (points.size() < 3) {
            continue;
        }

        int64_t double_area = SpriteContourDoubleArea(points);

        if (double_area != 0) {
            result.emplace_back(SpriteContour {.Points = std::move(points), .DoubleArea = double_area});
        }
    }

    if (result.empty()) {
        return std::nullopt;
    }

    return result;
}

static auto InflateSimplifiedSpriteContours(const vector<SpriteContour>& contours, int32_t dilation) -> optional<vector<SpriteContour>>
{
    FO_STACK_TRACE_ENTRY();

    if (dilation <= 0) {
        return contours;
    }

    constexpr int64_t coordinate_scale = 256;
    Clipper2Lib::Paths64 paths;
    paths.reserve(contours.size());

    for (const SpriteContour& contour : contours) {
        Clipper2Lib::Path64& path = paths.emplace_back();
        path.reserve(contour.Points.size());

        for (ipos32 point : contour.Points) {
            path.emplace_back(numeric_cast<int64_t>(point.x) * coordinate_scale, numeric_cast<int64_t>(point.y) * coordinate_scale);
        }
    }

    paths = Clipper2Lib::Union(paths, Clipper2Lib::FillRule::NonZero);
    paths = Clipper2Lib::InflatePaths(paths, numeric_cast<float64_t>(dilation) * coordinate_scale, Clipper2Lib::JoinType::Miter, Clipper2Lib::EndType::Polygon);

    if (paths.empty()) {
        return std::nullopt;
    }

    vector<SpriteContour> result;
    result.reserve(paths.size());

    for (const Clipper2Lib::Path64& path : paths) {
        vector<ipos32> points;
        points.reserve(path.size());

        for (Clipper2Lib::Point64 point : path) {
            points.emplace_back(iround<int32_t>(numeric_cast<float64_t>(point.x) / coordinate_scale), iround<int32_t>(numeric_cast<float64_t>(point.y) / coordinate_scale));
        }

        NormalizeSpriteContour(points);

        if (points.size() < 3) {
            continue;
        }

        int64_t double_area = SpriteContourDoubleArea(points);

        if (double_area != 0) {
            result.emplace_back(SpriteContour {.Points = std::move(points), .DoubleArea = double_area});
        }
    }

    return result.empty() ? std::nullopt : optional<vector<SpriteContour>> {std::move(result)};
}

static auto IsPointOnSpriteContour(ipos32 point, const vector<ipos32>& contour) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < contour.size(); i++) {
        ipos32 a = contour[i];
        ipos32 b = contour[(i + 1) % contour.size()];

        if (SpritePointCross(a, b, point) == 0 && IsSpritePointBetween(a, point, b)) {
            return true;
        }
    }

    return false;
}

static auto IsPointStrictlyInsideSpriteContour(ipos32 point, const vector<ipos32>& contour) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (IsPointOnSpriteContour(point, contour)) {
        return false;
    }

    bool inside = false;

    for (size_t i = 0, j = contour.size() - 1; i < contour.size(); j = i++) {
        ipos32 a = contour[i];
        ipos32 b = contour[j];
        bool crosses_y = (a.y > point.y) != (b.y > point.y);

        if (crosses_y) {
            float64_t crossing_x = numeric_cast<float64_t>(b.x - a.x) * numeric_cast<float64_t>(point.y - a.y) / numeric_cast<float64_t>(b.y - a.y) + a.x;

            if (numeric_cast<float64_t>(point.x) < crossing_x) {
                inside = !inside;
            }
        }
    }

    return inside;
}

using EarcutPoint = array<float64_t, 2>;
using EarcutPolygon = vector<vector<EarcutPoint>>;

static auto TriangulateSpriteContours(const vector<SpriteContour>& contours, int32_t width, int32_t height) -> optional<SpriteMeshData>
{
    FO_STACK_TRACE_ENTRY();

    size_t total_vertices = 0;
    vector<size_t> outer_indices;
    vector<size_t> hole_indices;
    unordered_map<uint64_t, size_t> contour_vertex_owners;

    for (size_t i = 0; i < contours.size(); i++) {
        const SpriteContour& contour = contours[i];

        if (contour.Points.size() < 3 || contour.DoubleArea == 0) {
            return std::nullopt;
        }
        if (contour.Points.size() > SPRITE_MESH_SERIALIZED_VERTEX_LIMIT - std::min(SPRITE_MESH_SERIALIZED_VERTEX_LIMIT, total_vertices)) {
            return std::nullopt;
        }

        total_vertices += contour.Points.size();

        if (total_vertices > SPRITE_MESH_SERIALIZED_VERTEX_LIMIT || total_vertices > std::numeric_limits<uint16_t>::max()) {
            return std::nullopt;
        }

        for (ipos32 point : contour.Points) {
            if (point.x < 0 || point.x > width || point.y < 0 || point.y > height) {
                return std::nullopt;
            }

            const auto [owner, inserted] = contour_vertex_owners.emplace(SpriteGridPointKey(point), i);

            if (!inserted) {
                ignore_unused(owner);
                return std::nullopt;
            }
        }

        if (contour.DoubleArea > 0) {
            outer_indices.emplace_back(i);
        }
        else {
            hole_indices.emplace_back(i);
        }
    }

    if (outer_indices.empty()) {
        return std::nullopt;
    }

    vector<vector<size_t>> holes_by_outer(outer_indices.size());

    for (size_t hole_index : hole_indices) {
        ipos32 probe = contours[hole_index].Points.front();
        optional<size_t> owner;
        int64_t owner_area = std::numeric_limits<int64_t>::max();

        for (size_t outer_pos = 0; outer_pos < outer_indices.size(); outer_pos++) {
            const SpriteContour& outer = contours[outer_indices[outer_pos]];

            if (outer.DoubleArea < owner_area && IsPointStrictlyInsideSpriteContour(probe, outer.Points)) {
                owner = outer_pos;
                owner_area = outer.DoubleArea;
            }
        }

        if (!owner.has_value()) {
            return std::nullopt;
        }

        holes_by_outer[*owner].emplace_back(hole_index);
    }

    SpriteMeshData mesh;
    mesh.Vertices.reserve(total_vertices);

    for (size_t outer_pos = 0; outer_pos < outer_indices.size(); outer_pos++) {
        EarcutPolygon polygon;
        vector<size_t> polygon_contours {outer_indices[outer_pos]};
        polygon_contours.insert(polygon_contours.end(), holes_by_outer[outer_pos].begin(), holes_by_outer[outer_pos].end());

        size_t polygon_vertex_count = 0;

        for (size_t contour_index : polygon_contours) {
            const vector<ipos32>& contour_points = contours[contour_index].Points;
            vector<EarcutPoint>& ring = polygon.emplace_back();
            ring.reserve(contour_points.size());

            for (ipos32 point : contour_points) {
                ring.push_back(EarcutPoint {numeric_cast<float64_t>(point.x), numeric_cast<float64_t>(point.y)});
                mesh.Vertices.emplace_back(point);
            }

            polygon_vertex_count += contour_points.size();
        }

        size_t vertex_base = mesh.Vertices.size() - polygon_vertex_count;
        std::vector<uint32_t> local_indices = mapbox::earcut<uint32_t>(polygon);

        if (local_indices.empty() || local_indices.size() % 3 != 0) {
            return std::nullopt;
        }
        if (local_indices.size() > std::numeric_limits<uint32_t>::max() - mesh.Indices.size()) {
            return std::nullopt;
        }

        mesh.Indices.reserve(mesh.Indices.size() + local_indices.size());

        for (uint32_t local_index : local_indices) {
            if (local_index >= polygon_vertex_count) {
                return std::nullopt;
            }

            size_t global_index = vertex_base + local_index;

            if (global_index > std::numeric_limits<uint16_t>::max()) {
                return std::nullopt;
            }

            mesh.Indices.emplace_back(numeric_cast<uint16_t>(global_index));
        }
    }

    if (mesh.Vertices.size() != total_vertices || mesh.Indices.empty() || mesh.Indices.size() > std::numeric_limits<uint32_t>::max()) {
        return std::nullopt;
    }

    return mesh;
}

static auto SpriteTriangleDoubleArea(ipos32 a, ipos32 b, ipos32 c) noexcept -> int64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::abs(SpritePointCross(a, b, c));
}

static auto ClipSpriteValidationPolygon(const array<SpriteValidationPoint, 8>& source, size_t source_count, array<SpriteValidationPoint, 8>& destination, bool x_axis, float64_t boundary, bool keep_greater) noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (source_count == 0) {
        return 0;
    }

    auto get_coordinate = [x_axis](const SpriteValidationPoint& point) noexcept -> float64_t { return x_axis ? point.X : point.Y; };
    auto is_inside = [keep_greater, boundary](float64_t coordinate) noexcept -> bool { return keep_greater ? coordinate >= boundary : coordinate <= boundary; };

    size_t destination_count = 0;
    SpriteValidationPoint previous = source[source_count - 1];
    float64_t previous_coordinate = get_coordinate(previous);
    bool previous_inside = is_inside(previous_coordinate);

    for (size_t i = 0; i < source_count; i++) {
        SpriteValidationPoint current = source[i];
        float64_t current_coordinate = get_coordinate(current);
        bool current_inside = is_inside(current_coordinate);

        if (previous_inside != current_inside) {
            float64_t factor = (boundary - previous_coordinate) / (current_coordinate - previous_coordinate);
            FO_STRONG_ASSERT(destination_count < destination.size(), "Sprite validation polygon exceeds clipping capacity");
            destination[destination_count++] = SpriteValidationPoint {
                .X = previous.X + (current.X - previous.X) * factor,
                .Y = previous.Y + (current.Y - previous.Y) * factor,
            };
        }

        if (current_inside) {
            FO_STRONG_ASSERT(destination_count < destination.size(), "Sprite validation polygon exceeds clipping capacity");
            destination[destination_count++] = current;
        }

        previous = current;
        previous_coordinate = current_coordinate;
        previous_inside = current_inside;
    }

    return destination_count;
}

static auto SpriteTrianglePixelCoveredDoubleArea(ipos32 a, ipos32 b, ipos32 c, int32_t pixel_x, int32_t pixel_y) noexcept -> float64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    array<SpriteValidationPoint, 8> first {};
    array<SpriteValidationPoint, 8> second {};
    first[0] = SpriteValidationPoint {.X = numeric_cast<float64_t>(a.x), .Y = numeric_cast<float64_t>(a.y)};
    first[1] = SpriteValidationPoint {.X = numeric_cast<float64_t>(b.x), .Y = numeric_cast<float64_t>(b.y)};
    first[2] = SpriteValidationPoint {.X = numeric_cast<float64_t>(c.x), .Y = numeric_cast<float64_t>(c.y)};

    float64_t min_x = numeric_cast<float64_t>(pixel_x);
    float64_t min_y = numeric_cast<float64_t>(pixel_y);
    float64_t max_x = min_x + 1.0;
    float64_t max_y = min_y + 1.0;
    size_t point_count = ClipSpriteValidationPolygon(first, 3, second, true, min_x, true);
    point_count = ClipSpriteValidationPolygon(second, point_count, first, true, max_x, false);
    point_count = ClipSpriteValidationPolygon(first, point_count, second, false, min_y, true);
    point_count = ClipSpriteValidationPolygon(second, point_count, first, false, max_y, false);

    if (point_count < 3) {
        return 0.0;
    }

    float64_t double_area = 0.0;

    for (size_t i = 0; i < point_count; i++) {
        const SpriteValidationPoint& current = first[i];
        const SpriteValidationPoint& next = first[(i + 1) % point_count];
        double_area += current.X * next.Y - current.Y * next.X;
    }

    return std::abs(double_area);
}

static auto ValidateSpriteMesh(const SpriteMeshData& mesh, const vector<SpriteContour>& contours, const_span<uint8_t> original_mask, const_span<uint8_t> tolerance_mask, int32_t width, int32_t height, int64_t& mesh_double_area) -> SpriteMeshBuildFailure
{
    FO_STACK_TRACE_ENTRY();

    if (mesh.Vertices.size() < 3 || mesh.Vertices.size() > std::numeric_limits<uint16_t>::max() || mesh.Indices.empty() || mesh.Indices.size() % 3 != 0) {
        return SpriteMeshBuildFailure::InvalidMeshShape;
    }

    int64_t expected_double_area = 0;

    for (const SpriteContour& contour : contours) {
        expected_double_area += contour.DoubleArea;
    }
    if (expected_double_area <= 0) {
        return SpriteMeshBuildFailure::InvalidMeshShape;
    }

    mesh_double_area = 0;
    size_t validation_cells = 0;
    size_t pixel_count = original_mask.size();

    if (!tolerance_mask.empty() && tolerance_mask.size() != pixel_count) {
        return SpriteMeshBuildFailure::InvalidMeshShape;
    }

    size_t validation_limit = pixel_count > (std::numeric_limits<size_t>::max() - 1'000'000u) / 32u ? std::numeric_limits<size_t>::max() : pixel_count * 32u + 1'000'000u;

    struct TriangleBounds
    {
        ipos32 A {};
        ipos32 B {};
        ipos32 C {};
        int32_t MinX {};
        int32_t MinY {};
        int32_t MaxX {};
        int32_t MaxY {};
    };

    vector<TriangleBounds> triangles;
    triangles.reserve(mesh.Indices.size() / 3);

    for (size_t i = 0; i < mesh.Indices.size(); i += 3) {
        uint16_t index_a = mesh.Indices[i];
        uint16_t index_b = mesh.Indices[i + 1];
        uint16_t index_c = mesh.Indices[i + 2];

        if (index_a >= mesh.Vertices.size() || index_b >= mesh.Vertices.size() || index_c >= mesh.Vertices.size()) {
            return SpriteMeshBuildFailure::InvalidIndex;
        }

        ipos32 a = mesh.Vertices[index_a];
        ipos32 b = mesh.Vertices[index_b];
        ipos32 c = mesh.Vertices[index_c];
        int64_t triangle_double_area = SpriteTriangleDoubleArea(a, b, c);

        if (triangle_double_area == 0) {
            return SpriteMeshBuildFailure::DegenerateTriangle;
        }
        if (mesh_double_area > std::numeric_limits<int64_t>::max() - triangle_double_area) {
            return SpriteMeshBuildFailure::AreaOverflow;
        }

        mesh_double_area += triangle_double_area;

        int32_t min_x = std::clamp(std::min({a.x, b.x, c.x}), 0, width - 1);
        int32_t min_y = std::clamp(std::min({a.y, b.y, c.y}), 0, height - 1);
        int32_t max_x = std::clamp(std::max({a.x, b.x, c.x}), 0, width - 1);
        int32_t max_y = std::clamp(std::max({a.y, b.y, c.y}), 0, height - 1);
        size_t bbox_cells = numeric_cast<size_t>(max_x - min_x + 1) * numeric_cast<size_t>(max_y - min_y + 1);

        if (bbox_cells > validation_limit - std::min(validation_limit, validation_cells)) {
            return SpriteMeshBuildFailure::ValidationLimit;
        }

        validation_cells += bbox_cells;
        triangles.emplace_back(TriangleBounds {.A = a, .B = b, .C = c, .MinX = min_x, .MinY = min_y, .MaxX = max_x, .MaxY = max_y});
    }

    if (mesh_double_area != expected_double_area) {
        return SpriteMeshBuildFailure::AreaMismatch;
    }

    vector<float64_t> covered_double_area(pixel_count);

    for (const TriangleBounds& triangle : triangles) {
        for (int32_t y = triangle.MinY; y <= triangle.MaxY; y++) {
            for (int32_t x = triangle.MinX; x <= triangle.MaxX; x++) {
                covered_double_area[numeric_cast<size_t>(y) * width + x] += SpriteTrianglePixelCoveredDoubleArea(triangle.A, triangle.B, triangle.C, x, y);
            }
        }
    }

    constexpr float64_t pixel_double_area = 2.0;
    constexpr float64_t area_epsilon = 0.000001;

    for (size_t i = 0; i < pixel_count; i++) {
        if (covered_double_area[i] > pixel_double_area + area_epsilon) {
            return SpriteMeshBuildFailure::InvalidMeshShape;
        }
        if (original_mask[i] != 0 && covered_double_area[i] < pixel_double_area - area_epsilon) {
            return SpriteMeshBuildFailure::ClipsVisiblePixel;
        }
        if (!tolerance_mask.empty() && tolerance_mask[i] == 0 && covered_double_area[i] > area_epsilon) {
            return SpriteMeshBuildFailure::EscapesToleranceMask;
        }
    }

    return SpriteMeshBuildFailure::None;
}

static auto TryBuildEnclosingSpriteTriangle(const vector<uint8_t>& mask, int32_t width, int32_t height, int64_t& mesh_double_area) -> SpriteMeshBuildAttempt
{
    FO_STACK_TRACE_ENTRY();

    optional<vector<SpriteContour>> exact_contours = ExtractSpriteContours(mask, width, height, 0.0f);

    if (!exact_contours.has_value() || exact_contours->empty()) {
        return SpriteMeshBuildAttempt {.Failure = SpriteMeshBuildFailure::ContourExtraction};
    }

    vector<ipos32> hull = BuildSpriteConvexHull(*exact_contours);

    if (hull.size() < 3) {
        return SpriteMeshBuildAttempt {.Failure = SpriteMeshBuildFailure::InvalidMeshShape};
    }

    struct SpriteHullLine
    {
        int64_t NormalX {};
        int64_t NormalY {};
        float64_t Limit {};
    };

    vector<SpriteHullLine> lines;
    lines.reserve(hull.size());
    constexpr float64_t quantization_margin = 0.25;

    for (size_t i = 0; i < hull.size(); i++) {
        ipos32 current = hull[i];
        ipos32 next = hull[(i + 1) % hull.size()];
        int64_t direction_x = numeric_cast<int64_t>(next.x) - current.x;
        int64_t direction_y = numeric_cast<int64_t>(next.y) - current.y;
        int64_t divisor = std::gcd(std::abs(direction_x), std::abs(direction_y));

        if (divisor == 0) {
            continue;
        }

        direction_x /= divisor;
        direction_y /= divisor;

        int64_t normal_x = direction_y;
        int64_t normal_y = -direction_x;
        bool duplicate = std::ranges::any_of(lines, [normal_x, normal_y](const SpriteHullLine& line) { return line.NormalX == normal_x && line.NormalY == normal_y; });

        if (duplicate) {
            continue;
        }

        float64_t support = numeric_cast<float64_t>(normal_x * current.x + normal_y * current.y);
        float64_t margin = std::hypot(numeric_cast<float64_t>(normal_x), numeric_cast<float64_t>(normal_y)) * quantization_margin;
        lines.emplace_back(SpriteHullLine {.NormalX = normal_x, .NormalY = normal_y, .Limit = support + margin});
    }

    constexpr size_t maximum_triangle_support_directions = 48;

    if (lines.size() < 3 || lines.size() > maximum_triangle_support_directions) {
        return SpriteMeshBuildAttempt {.Failure = SpriteMeshBuildFailure::ContourExtraction};
    }

    optional<vector<ipos32>> best_points;
    int64_t best_double_area = std::numeric_limits<int64_t>::max();

    for (size_t first = 0; first + 2 < lines.size(); first++) {
        for (size_t second = first + 1; second + 1 < lines.size(); second++) {
            for (size_t third = second + 1; third < lines.size(); third++) {
                array<const SpriteHullLine*, 3> triangle_lines {&lines[first], &lines[second], &lines[third]};
                vector<ipos32> points;
                points.reserve(3);
                bool valid_intersections = true;

                for (size_t i = 0; i < triangle_lines.size(); i++) {
                    const SpriteHullLine& a = *triangle_lines[i];
                    const SpriteHullLine& b = *triangle_lines[(i + 1) % triangle_lines.size()];
                    int64_t integer_determinant = a.NormalX * b.NormalY - a.NormalY * b.NormalX;

                    if (integer_determinant == 0) {
                        valid_intersections = false;
                        break;
                    }

                    float64_t determinant = numeric_cast<float64_t>(integer_determinant);
                    float64_t x = (a.Limit * numeric_cast<float64_t>(b.NormalY) - numeric_cast<float64_t>(a.NormalY) * b.Limit) / determinant;
                    float64_t y = (numeric_cast<float64_t>(a.NormalX) * b.Limit - a.Limit * numeric_cast<float64_t>(b.NormalX)) / determinant;
                    optional<ipos32> point = RoundSpriteIntersectionPoint(x, y);

                    if (!point.has_value()) {
                        valid_intersections = false;
                        break;
                    }

                    if (point->x < 0 || point->x > width || point->y < 0 || point->y > height) {
                        valid_intersections = false;
                        break;
                    }

                    points.emplace_back(point.value());
                }

                if (!valid_intersections) {
                    continue;
                }

                NormalizeSpriteContour(points);

                if (points.size() != 3) {
                    continue;
                }

                int64_t candidate_double_area = std::abs(SpriteContourDoubleArea(points));

                if (candidate_double_area == 0 || candidate_double_area >= best_double_area) {
                    continue;
                }
                if (!DoesSpriteConvexPolygonCoverHull(points, hull)) {
                    continue;
                }

                best_double_area = candidate_double_area;
                best_points = std::move(points);
            }
        }
    }

    if (!best_points.has_value()) {
        return SpriteMeshBuildAttempt {.Failure = SpriteMeshBuildFailure::ContourOffset};
    }

    vector<ipos32> points = std::move(*best_points);
    int64_t double_area = SpriteContourDoubleArea(points);

    if (double_area < 0) {
        std::ranges::reverse(points);
        double_area = -double_area;
    }

    vector<SpriteContour> contours {SpriteContour {.Points = std::move(points), .DoubleArea = double_area}};
    optional<SpriteMeshData> mesh = TriangulateSpriteContours(contours, width, height);

    if (!mesh.has_value()) {
        return SpriteMeshBuildAttempt {.Failure = SpriteMeshBuildFailure::Triangulation};
    }

    SpriteMeshBuildFailure validation_failure = ValidateSpriteMesh(*mesh, contours, mask, {}, width, height, mesh_double_area);

    if (validation_failure != SpriteMeshBuildFailure::None) {
        return SpriteMeshBuildAttempt {.Failure = validation_failure};
    }

    return SpriteMeshBuildAttempt {.Mesh = std::move(mesh)};
}

static auto TryBuildEnclosingSpriteQuad(const vector<uint8_t>& mask, int32_t width, int32_t height, int64_t& mesh_double_area) -> SpriteMeshBuildAttempt
{
    FO_STACK_TRACE_ENTRY();

    optional<vector<SpriteContour>> exact_contours = ExtractSpriteContours(mask, width, height, 0.0f);

    if (!exact_contours.has_value() || exact_contours->empty()) {
        return SpriteMeshBuildAttempt {.Failure = SpriteMeshBuildFailure::ContourExtraction};
    }

    vector<ipos32> hull = BuildSpriteConvexHull(*exact_contours);

    if (hull.size() < 3) {
        return SpriteMeshBuildAttempt {.Failure = SpriteMeshBuildFailure::InvalidMeshShape};
    }

    struct SpriteHullSupport
    {
        int64_t NormalX {};
        int64_t NormalY {};
        int64_t Minimum {};
        int64_t Maximum {};
    };

    vector<SpriteHullSupport> supports;
    supports.reserve(hull.size());

    auto add_support = [&supports, &hull](int64_t normal_x, int64_t normal_y) {
        bool duplicate = std::ranges::any_of(supports, [normal_x, normal_y](const SpriteHullSupport& support) { return support.NormalX == normal_x && support.NormalY == normal_y; });

        if (duplicate) {
            return;
        }

        int64_t minimum = std::numeric_limits<int64_t>::max();
        int64_t maximum = std::numeric_limits<int64_t>::min();

        for (ipos32 point : hull) {
            int64_t projection = normal_x * point.x + normal_y * point.y;
            minimum = std::min(minimum, projection);
            maximum = std::max(maximum, projection);
        }

        supports.emplace_back(SpriteHullSupport {.NormalX = normal_x, .NormalY = normal_y, .Minimum = minimum, .Maximum = maximum});
    };

    for (size_t i = 0; i < hull.size(); i++) {
        ipos32 current = hull[i];
        ipos32 next = hull[(i + 1) % hull.size()];
        int64_t direction_x = numeric_cast<int64_t>(next.x) - current.x;
        int64_t direction_y = numeric_cast<int64_t>(next.y) - current.y;
        int64_t divisor = std::gcd(std::abs(direction_x), std::abs(direction_y));

        if (divisor == 0) {
            continue;
        }

        direction_x /= divisor;
        direction_y /= divisor;

        if (direction_x < 0 || (direction_x == 0 && direction_y < 0)) {
            direction_x = -direction_x;
            direction_y = -direction_y;
        }

        int64_t normal_x = -direction_y;
        int64_t normal_y = direction_x;
        add_support(normal_x, normal_y);
    }

    add_support(1, 0);
    add_support(0, 1);

    constexpr size_t maximum_support_directions = 128;

    if (supports.size() < 2 || supports.size() > maximum_support_directions) {
        return SpriteMeshBuildAttempt {.Failure = SpriteMeshBuildFailure::ContourExtraction};
    }

    optional<vector<ipos32>> best_points;
    int64_t best_double_area = std::numeric_limits<int64_t>::max();
    constexpr float64_t quantization_margin = 0.25;

    for (size_t first = 0; first + 1 < supports.size(); first++) {
        for (size_t second = first + 1; second < supports.size(); second++) {
            const SpriteHullSupport& a = supports[first];
            const SpriteHullSupport& b = supports[second];
            int64_t integer_determinant = a.NormalX * b.NormalY - a.NormalY * b.NormalX;

            if (integer_determinant == 0) {
                continue;
            }

            float64_t determinant = numeric_cast<float64_t>(integer_determinant);
            float64_t margin_a = std::hypot(numeric_cast<float64_t>(a.NormalX), numeric_cast<float64_t>(a.NormalY)) * quantization_margin;
            float64_t margin_b = std::hypot(numeric_cast<float64_t>(b.NormalX), numeric_cast<float64_t>(b.NormalY)) * quantization_margin;
            float64_t a_minimum = numeric_cast<float64_t>(a.Minimum) - margin_a;
            float64_t a_maximum = numeric_cast<float64_t>(a.Maximum) + margin_a;
            float64_t b_minimum = numeric_cast<float64_t>(b.Minimum) - margin_b;
            float64_t b_maximum = numeric_cast<float64_t>(b.Maximum) + margin_b;
            array<pair<float64_t, float64_t>, 4> support_values {{{a_minimum, b_minimum}, {a_maximum, b_minimum}, {a_maximum, b_maximum}, {a_minimum, b_maximum}}};
            vector<ipos32> points;
            points.reserve(4);
            bool points_fit_frame = true;

            for (const auto& [support_a, support_b] : support_values) {
                float64_t x = (support_a * numeric_cast<float64_t>(b.NormalY) - numeric_cast<float64_t>(a.NormalY) * support_b) / determinant;
                float64_t y = (numeric_cast<float64_t>(a.NormalX) * support_b - support_a * numeric_cast<float64_t>(b.NormalX)) / determinant;
                optional<ipos32> point = RoundSpriteIntersectionPoint(x, y);

                if (!point.has_value()) {
                    points_fit_frame = false;
                    break;
                }

                if (point->x < 0 || point->x > width || point->y < 0 || point->y > height) {
                    points_fit_frame = false;
                    break;
                }

                points.emplace_back(point.value());
            }

            if (!points_fit_frame) {
                continue;
            }

            NormalizeSpriteContour(points);

            if (points.size() != 4) {
                continue;
            }

            if (!DoesSpriteConvexPolygonCoverHull(points, hull)) {
                continue;
            }

            int64_t candidate_double_area = std::abs(SpriteContourDoubleArea(points));

            if (candidate_double_area > 0 && candidate_double_area < best_double_area) {
                best_double_area = candidate_double_area;
                best_points = std::move(points);
            }
        }
    }

    if (!best_points.has_value()) {
        return SpriteMeshBuildAttempt {.Failure = SpriteMeshBuildFailure::ContourOffset};
    }

    vector<ipos32> points = std::move(*best_points);
    int64_t double_area = SpriteContourDoubleArea(points);

    if (double_area < 0) {
        std::ranges::reverse(points);
        double_area = -double_area;
    }
    if (double_area == 0) {
        return SpriteMeshBuildAttempt {.Failure = SpriteMeshBuildFailure::InvalidMeshShape};
    }

    vector<SpriteContour> contours {SpriteContour {.Points = std::move(points), .DoubleArea = double_area}};
    optional<SpriteMeshData> mesh = TriangulateSpriteContours(contours, width, height);

    if (!mesh.has_value()) {
        return SpriteMeshBuildAttempt {.Failure = SpriteMeshBuildFailure::Triangulation};
    }

    SpriteMeshBuildFailure validation_failure = ValidateSpriteMesh(*mesh, contours, mask, {}, width, height, mesh_double_area);

    if (validation_failure != SpriteMeshBuildFailure::None) {
        return SpriteMeshBuildAttempt {.Failure = validation_failure};
    }

    return SpriteMeshBuildAttempt {.Mesh = std::move(mesh)};
}

static auto MergeSpriteMeshCandidates(const SpriteMeshCandidate& left, const SpriteMeshCandidate& right) -> optional<SpriteMeshCandidate>
{
    FO_STACK_TRACE_ENTRY();

    if (right.Mesh.Vertices.size() > SPRITE_MESH_SERIALIZED_VERTEX_LIMIT - std::min(SPRITE_MESH_SERIALIZED_VERTEX_LIMIT, left.Mesh.Vertices.size())) {
        return std::nullopt;
    }

    size_t vertex_offset = left.Mesh.Vertices.size();

    if (vertex_offset + right.Mesh.Vertices.size() > std::numeric_limits<uint16_t>::max()) {
        return std::nullopt;
    }

    SpriteMeshCandidate merged;
    merged.Mesh.Vertices.reserve(vertex_offset + right.Mesh.Vertices.size());
    merged.Mesh.Vertices.insert(merged.Mesh.Vertices.end(), left.Mesh.Vertices.begin(), left.Mesh.Vertices.end());
    merged.Mesh.Vertices.insert(merged.Mesh.Vertices.end(), right.Mesh.Vertices.begin(), right.Mesh.Vertices.end());
    merged.Mesh.Indices.reserve(left.Mesh.Indices.size() + right.Mesh.Indices.size());
    merged.Mesh.Indices.insert(merged.Mesh.Indices.end(), left.Mesh.Indices.begin(), left.Mesh.Indices.end());

    for (uint16_t index : right.Mesh.Indices) {
        merged.Mesh.Indices.emplace_back(numeric_cast<uint16_t>(vertex_offset + index));
    }

    merged.Enclosures.reserve(left.Enclosures.size() + right.Enclosures.size());
    merged.Enclosures.insert(merged.Enclosures.end(), left.Enclosures.begin(), left.Enclosures.end());
    merged.Enclosures.insert(merged.Enclosures.end(), right.Enclosures.begin(), right.Enclosures.end());
    merged.DoubleArea = left.DoubleArea + right.DoubleArea;
    merged.Source = left.Mesh.Indices.empty() ? right.Source : left.Source;
    return merged;
}

static auto BuildGreedyEnclosingSpriteCandidates(const vector<SpriteContour>& source_contours, int32_t width, int32_t height, size_t max_triangles) -> vector<SpriteMeshCandidate>
{
    FO_STACK_TRACE_ENTRY();

    vector<ipos32> hull = BuildSpriteConvexHull(source_contours);

    if (hull.size() < 3 || max_triangles == 0) {
        return {};
    }

    struct SpriteHullLine
    {
        int64_t NormalX {};
        int64_t NormalY {};
        float64_t Limit {};
    };

    vector<SpriteHullLine> lines;
    lines.reserve(hull.size());
    constexpr float64_t quantization_margin = 1.0;

    for (size_t i = 0; i < hull.size(); i++) {
        ipos32 current = hull[i];
        ipos32 next = hull[(i + 1) % hull.size()];
        int64_t direction_x = numeric_cast<int64_t>(next.x) - current.x;
        int64_t direction_y = numeric_cast<int64_t>(next.y) - current.y;
        int64_t divisor = std::gcd(std::abs(direction_x), std::abs(direction_y));

        if (divisor == 0) {
            continue;
        }

        direction_x /= divisor;
        direction_y /= divisor;
        int64_t normal_x = direction_y;
        int64_t normal_y = -direction_x;

        if (std::ranges::any_of(lines, [normal_x, normal_y](const SpriteHullLine& line) { return line.NormalX == normal_x && line.NormalY == normal_y; })) {
            continue;
        }

        float64_t support = numeric_cast<float64_t>(normal_x * current.x + normal_y * current.y);
        float64_t margin = std::hypot(numeric_cast<float64_t>(normal_x), numeric_cast<float64_t>(normal_y)) * quantization_margin;
        lines.emplace_back(SpriteHullLine {.NormalX = normal_x, .NormalY = normal_y, .Limit = support + margin});
    }

    constexpr size_t maximum_support_directions = 128;

    if (lines.size() < 3 || lines.size() > maximum_support_directions) {
        return {};
    }

    auto intersect_lines = [&lines](size_t first, size_t second) -> optional<pair<float64_t, float64_t>> {
        const SpriteHullLine& a = lines[first];
        const SpriteHullLine& b = lines[second];
        int64_t integer_determinant = a.NormalX * b.NormalY - a.NormalY * b.NormalX;

        if (integer_determinant == 0) {
            return std::nullopt;
        }

        float64_t determinant = numeric_cast<float64_t>(integer_determinant);
        float64_t x = (a.Limit * numeric_cast<float64_t>(b.NormalY) - numeric_cast<float64_t>(a.NormalY) * b.Limit) / determinant;
        float64_t y = (numeric_cast<float64_t>(a.NormalX) * b.Limit - a.Limit * numeric_cast<float64_t>(b.NormalX)) / determinant;
        return pair<float64_t, float64_t> {x, y};
    };

    struct ActiveHullLine
    {
        size_t Previous {};
        size_t Next {};
        size_t Revision {};
        bool Active {true};
    };

    struct HullLineRemoval
    {
        float64_t Cost {};
        size_t Index {};
        size_t Revision {};
    };

    vector<ActiveHullLine> active_lines(lines.size());

    for (size_t i = 0; i < active_lines.size(); i++) {
        active_lines[i].Previous = (i + active_lines.size() - 1) % active_lines.size();
        active_lines[i].Next = (i + 1) % active_lines.size();
    }

    auto compare_removals = [](const HullLineRemoval& left, const HullLineRemoval& right) { return left.Cost != right.Cost ? left.Cost > right.Cost : left.Index > right.Index; };
    std::priority_queue<HullLineRemoval, vector<HullLineRemoval>, decltype(compare_removals)> removals {compare_removals};

    auto queue_removal = [&active_lines, &intersect_lines, &removals](size_t index) {
        const ActiveHullLine& current = active_lines[index];

        if (!current.Active) {
            return;
        }

        optional<pair<float64_t, float64_t>> previous_intersection = intersect_lines(current.Previous, index);
        optional<pair<float64_t, float64_t>> next_intersection = intersect_lines(index, current.Next);
        optional<pair<float64_t, float64_t>> replacement_intersection = intersect_lines(current.Previous, current.Next);

        if (!previous_intersection.has_value() || !next_intersection.has_value() || !replacement_intersection.has_value()) {
            return;
        }

        float64_t ab_x = next_intersection->first - previous_intersection->first;
        float64_t ab_y = next_intersection->second - previous_intersection->second;
        float64_t ac_x = replacement_intersection->first - previous_intersection->first;
        float64_t ac_y = replacement_intersection->second - previous_intersection->second;
        float64_t cost = std::abs(ab_x * ac_y - ab_y * ac_x);
        removals.emplace(HullLineRemoval {.Cost = cost, .Index = index, .Revision = current.Revision});
    };

    for (size_t i = 0; i < active_lines.size(); i++) {
        queue_removal(i);
    }

    vector<optional<SpriteMeshCandidate>> candidates_by_triangle(max_triangles + 1);
    size_t active_count = active_lines.size();

    auto collect_active_indices = [&active_lines](size_t expected_count) -> optional<vector<size_t>> {
        size_t first = 0;

        while (first < active_lines.size() && !active_lines[first].Active) {
            first++;
        }
        if (first == active_lines.size()) {
            return std::nullopt;
        }

        vector<size_t> active_indices;
        active_indices.reserve(expected_count);
        size_t current = first;

        do {
            active_indices.emplace_back(current);
            current = active_lines[current].Next;
        } while (current != first && active_indices.size() <= expected_count);

        if (active_indices.size() != expected_count) {
            return std::nullopt;
        }

        return active_indices;
    };

    auto build_candidate = [&intersect_lines, &hull, width, height](const vector<size_t>& active_indices) -> optional<SpriteMeshCandidate> {
        if (active_indices.size() < 3) {
            return std::nullopt;
        }

        vector<ipos32> points;
        points.reserve(active_indices.size());
        for (size_t i = 0; i < active_indices.size(); i++) {
            optional<pair<float64_t, float64_t>> intersection = intersect_lines(active_indices[i], active_indices[(i + 1) % active_indices.size()]);

            if (!intersection.has_value()) {
                return std::nullopt;
            }

            optional<ipos32> point = RoundSpriteIntersectionPoint(intersection->first, intersection->second);

            if (!point.has_value()) {
                return std::nullopt;
            }

            if (point->x < 0 || point->x > width || point->y < 0 || point->y > height) {
                return std::nullopt;
            }

            points.emplace_back(point.value());
        }

        NormalizeSpriteContour(points);

        if (points.size() < 3 || !DoesSpriteConvexPolygonCoverHull(points, hull)) {
            return std::nullopt;
        }

        int64_t double_area = SpriteContourDoubleArea(points);

        if (double_area < 0) {
            std::ranges::reverse(points);
            double_area = -double_area;
        }
        if (double_area == 0) {
            return std::nullopt;
        }

        vector<SpriteContour> enclosures {SpriteContour {.Points = std::move(points), .DoubleArea = double_area}};
        optional<SpriteMeshData> mesh = TriangulateSpriteContours(enclosures, width, height);

        if (!mesh.has_value() || mesh->Indices.empty()) {
            return std::nullopt;
        }

        return SpriteMeshCandidate {.Mesh = std::move(*mesh), .Enclosures = std::move(enclosures), .DoubleArea = double_area};
    };

    auto store_candidate = [&candidates_by_triangle, max_triangles](SpriteMeshCandidate incoming) {
        size_t triangle_count = incoming.Mesh.Indices.size() / 3;

        if (triangle_count == 0 || triangle_count > max_triangles) {
            return;
        }

        optional<SpriteMeshCandidate>& stored = candidates_by_triangle[triangle_count];

        if (!stored.has_value() || incoming.DoubleArea < stored->DoubleArea) {
            stored = std::move(incoming);
        }
    };

    auto capture_candidate = [&]() {
        if (active_count < 3 || active_count > max_triangles + 2) {
            return;
        }

        optional<vector<size_t>> active_indices = collect_active_indices(active_count);

        if (!active_indices.has_value()) {
            return;
        }

        optional<SpriteMeshCandidate> candidate = build_candidate(*active_indices);

        if (candidate.has_value()) {
            store_candidate(std::move(*candidate));
        }
    };

    size_t beam_start_count = std::min(lines.size(), max_triangles + 2 + SPRITE_MESH_CANDIDATE_REMOVAL_DEPTH);
    optional<vector<size_t>> beam_seed;

    while (active_count >= 3) {
        if (!beam_seed.has_value() && active_count == beam_start_count) {
            beam_seed = collect_active_indices(active_count);
        }

        capture_candidate();

        if (active_count == 3) {
            break;
        }

        optional<size_t> removal_index;

        while (!removals.empty()) {
            HullLineRemoval removal = removals.top();
            removals.pop();

            if (active_lines[removal.Index].Active && active_lines[removal.Index].Revision == removal.Revision) {
                removal_index = removal.Index;
                break;
            }
        }
        if (!removal_index.has_value()) {
            break;
        }

        ActiveHullLine& removed = active_lines[*removal_index];
        size_t previous = removed.Previous;
        size_t next = removed.Next;
        removed.Active = false;
        active_lines[previous].Next = next;
        active_lines[next].Previous = previous;
        active_lines[previous].Revision++;
        active_lines[next].Revision++;
        active_count--;
        queue_removal(previous);
        queue_removal(next);
    }

    if (beam_seed.has_value()) {
        struct GreedyBeamState
        {
            vector<size_t> ActiveIndices {};
            SpriteMeshCandidate Candidate {};
        };

        optional<SpriteMeshCandidate> seed_candidate = build_candidate(*beam_seed);
        vector<GreedyBeamState> states;

        if (seed_candidate.has_value()) {
            store_candidate(*seed_candidate);
            states.emplace_back(GreedyBeamState {.ActiveIndices = std::move(*beam_seed), .Candidate = std::move(*seed_candidate)});
        }

        while (!states.empty() && states.front().ActiveIndices.size() > 3) {
            vector<GreedyBeamState> next_states;

            for (const GreedyBeamState& state : states) {
                for (size_t remove_index = 0; remove_index < state.ActiveIndices.size(); remove_index++) {
                    vector<size_t> reduced_indices = state.ActiveIndices;
                    reduced_indices.erase(reduced_indices.begin() + numeric_cast<ptrdiff_t>(remove_index));
                    optional<SpriteMeshCandidate> candidate = build_candidate(reduced_indices);

                    if (!candidate.has_value()) {
                        continue;
                    }

                    bool duplicate = std::ranges::any_of(next_states, [&reduced_indices](const GreedyBeamState& existing) { return existing.ActiveIndices == reduced_indices; });

                    if (!duplicate) {
                        next_states.emplace_back(GreedyBeamState {.ActiveIndices = std::move(reduced_indices), .Candidate = std::move(*candidate)});
                    }
                }
            }

            std::ranges::stable_sort(next_states, [](const GreedyBeamState& left, const GreedyBeamState& right) { return left.Candidate.DoubleArea < right.Candidate.DoubleArea; });

            if (next_states.size() > SPRITE_MESH_CANDIDATE_BEAM_WIDTH) {
                next_states.resize(SPRITE_MESH_CANDIDATE_BEAM_WIDTH);
            }

            for (const GreedyBeamState& state : next_states) {
                store_candidate(state.Candidate);
            }

            states = std::move(next_states);
        }
    }

    vector<SpriteMeshCandidate> candidates;

    for (optional<SpriteMeshCandidate>& candidate : candidates_by_triangle) {
        if (candidate.has_value()) {
            candidates.emplace_back(std::move(*candidate));
        }
    }
    return candidates;
}

static auto BuildSimplifiedSpriteCandidates(const vector<SpriteContour>& simplified_contours, int32_t width, int32_t height, int32_t dilation, size_t max_triangles) -> vector<SpriteMeshCandidate>
{
    FO_STACK_TRACE_ENTRY();

    optional<vector<SpriteContour>> contours = InflateSimplifiedSpriteContours(simplified_contours, dilation);

    if (!contours.has_value() || contours->empty()) {
        return {};
    }

    auto build_candidate = [&](const vector<SpriteContour>& candidate_contours) -> optional<SpriteMeshCandidate> {
        for (const SpriteContour& contour : candidate_contours) {
            if (contour.Points.size() < 3 || contour.DoubleArea == 0) {
                return std::nullopt;
            }
        }

        optional<SpriteMeshData> mesh = TriangulateSpriteContours(candidate_contours, width, height);

        if (!mesh.has_value()) {
            return std::nullopt;
        }

        size_t triangle_count = mesh->Indices.size() / 3;

        if (triangle_count == 0) {
            return std::nullopt;
        }

        int64_t candidate_double_area = 0;

        for (const SpriteContour& contour : candidate_contours) {
            candidate_double_area += contour.DoubleArea;
        }
        if (candidate_double_area <= 0) {
            return std::nullopt;
        }

        return SpriteMeshCandidate {.Mesh = std::move(*mesh), .Enclosures = candidate_contours, .DoubleArea = candidate_double_area};
    };

    optional<SpriteMeshCandidate> base_candidate = build_candidate(*contours);

    if (!base_candidate.has_value()) {
        return {};
    }

    size_t base_triangle_count = base_candidate->Mesh.Indices.size() / 3;

    if (base_triangle_count > max_triangles + SPRITE_MESH_CANDIDATE_REMOVAL_DEPTH) {
        return {};
    }

    vector<vector<SpriteMeshCandidate>> candidates_by_triangle(max_triangles + 1);

    auto store_candidate = [&candidates_by_triangle, max_triangles](const SpriteMeshCandidate& candidate) {
        size_t triangle_count = candidate.Mesh.Indices.size() / 3;

        if (triangle_count == 0 || triangle_count > max_triangles) {
            return;
        }

        vector<SpriteMeshCandidate>& stored = candidates_by_triangle[triangle_count];
        bool duplicate = std::ranges::any_of(stored, [&candidate](const SpriteMeshCandidate& existing) { return existing.Mesh.Vertices == candidate.Mesh.Vertices && existing.Mesh.Indices == candidate.Mesh.Indices; });

        if (!duplicate) {
            stored.emplace_back(candidate);
            std::ranges::stable_sort(stored, [](const SpriteMeshCandidate& left, const SpriteMeshCandidate& right) { return left.DoubleArea < right.DoubleArea; });

            if (stored.size() > SPRITE_MESH_CANDIDATE_BEAM_WIDTH) {
                stored.resize(SPRITE_MESH_CANDIDATE_BEAM_WIDTH);
            }
        }
    };

    store_candidate(*base_candidate);

    struct RemovalOption
    {
        vector<SpriteContour> Contours {};
        int64_t DoubleArea {};
    };

    struct RemovalState
    {
        vector<SpriteContour> Contours {};
        SpriteMeshCandidate Candidate {};
    };

    vector<RemovalState> states;
    states.emplace_back(RemovalState {.Contours = std::move(*contours), .Candidate = std::move(*base_candidate)});

    for (size_t depth = 0; depth < SPRITE_MESH_CANDIDATE_REMOVAL_DEPTH && !states.empty(); depth++) {
        vector<RemovalOption> removal_options;

        for (const RemovalState& state : states) {
            for (size_t contour_index = 0; contour_index < state.Contours.size(); contour_index++) {
                const SpriteContour& source_contour = state.Contours[contour_index];

                if (source_contour.Points.size() <= 3) {
                    continue;
                }

                for (size_t point_index = 0; point_index < source_contour.Points.size(); point_index++) {
                    vector<SpriteContour> reduced_contours = state.Contours;
                    SpriteContour& reduced_contour = reduced_contours[contour_index];

                    if (!RemoveEnclosingSpriteContourPoint(reduced_contour.Points, point_index, reduced_contour.DoubleArea)) {
                        continue;
                    }

                    reduced_contour.DoubleArea = SpriteContourDoubleArea(reduced_contour.Points);

                    if (reduced_contour.Points.size() < 3 || reduced_contour.DoubleArea == 0 || (reduced_contour.DoubleArea > 0) != (source_contour.DoubleArea > 0)) {
                        continue;
                    }

                    int64_t total_double_area = 0;

                    for (const SpriteContour& contour : reduced_contours) {
                        total_double_area += contour.DoubleArea;
                    }
                    if (total_double_area > 0) {
                        removal_options.emplace_back(RemovalOption {.Contours = std::move(reduced_contours), .DoubleArea = total_double_area});
                    }
                }
            }
        }

        std::ranges::stable_sort(removal_options, [](const RemovalOption& left, const RemovalOption& right) { return left.DoubleArea < right.DoubleArea; });

        vector<RemovalState> next_states;
        next_states.reserve(SPRITE_MESH_CANDIDATE_BEAM_WIDTH);

        for (RemovalOption& option : removal_options) {
            optional<SpriteMeshCandidate> candidate = build_candidate(option.Contours);

            if (!candidate.has_value()) {
                continue;
            }

            bool duplicate = std::ranges::any_of(next_states, [&candidate](const RemovalState& state) { return state.Candidate.Mesh.Vertices == candidate->Mesh.Vertices && state.Candidate.Mesh.Indices == candidate->Mesh.Indices; });

            if (duplicate) {
                continue;
            }

            store_candidate(*candidate);
            next_states.emplace_back(RemovalState {.Contours = std::move(option.Contours), .Candidate = std::move(*candidate)});

            if (next_states.size() >= SPRITE_MESH_CANDIDATE_BEAM_WIDTH) {
                break;
            }
        }

        states = std::move(next_states);
    }

    vector<SpriteMeshCandidate> candidates;

    for (vector<SpriteMeshCandidate>& triangle_candidates : candidates_by_triangle) {
        for (SpriteMeshCandidate& candidate : triangle_candidates) {
            candidates.emplace_back(std::move(candidate));
        }
    }

    return candidates;
}

struct SpriteContourCluster
{
    vector<SpriteContour> Contours {};
    int32_t MinX {};
    int32_t MinY {};
    int32_t MaxX {};
    int32_t MaxY {};
};

static auto BuildSpriteContourCluster(const SpriteContour& contour) -> SpriteContourCluster
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!contour.Points.empty(), "Sprite contour cluster source must not be empty");

    SpriteContourCluster cluster {
        .Contours = {contour},
        .MinX = contour.Points.front().x,
        .MinY = contour.Points.front().y,
        .MaxX = contour.Points.front().x,
        .MaxY = contour.Points.front().y,
    };

    for (ipos32 point : contour.Points) {
        cluster.MinX = std::min(cluster.MinX, point.x);
        cluster.MinY = std::min(cluster.MinY, point.y);
        cluster.MaxX = std::max(cluster.MaxX, point.x);
        cluster.MaxY = std::max(cluster.MaxY, point.y);
    }

    return cluster;
}

static auto MergeSpriteContourClusters(SpriteContourCluster left, SpriteContourCluster right) -> SpriteContourCluster
{
    FO_STACK_TRACE_ENTRY();

    left.Contours.reserve(left.Contours.size() + right.Contours.size());
    left.Contours.insert(left.Contours.end(), std::make_move_iterator(right.Contours.begin()), std::make_move_iterator(right.Contours.end()));
    left.MinX = std::min(left.MinX, right.MinX);
    left.MinY = std::min(left.MinY, right.MinY);
    left.MaxX = std::max(left.MaxX, right.MaxX);
    left.MaxY = std::max(left.MaxY, right.MaxY);
    return left;
}

static auto BuildClusteredSpriteCandidates(const vector<SpriteContour>& outer_contours, int32_t width, int32_t height, size_t max_triangles) -> vector<SpriteMeshCandidate>
{
    FO_STACK_TRACE_ENTRY();

    if (outer_contours.size() <= 1 || outer_contours.size() > SPRITE_MESH_COMPONENT_CLUSTER_LIMIT || max_triangles == 0) {
        return {};
    }

    auto append_partition_candidates = [width, height, max_triangles](const vector<SpriteContourCluster>& clusters, vector<SpriteMeshCandidate>& result) {
        if (clusters.size() <= 1 || clusters.size() > max_triangles) {
            return;
        }

        bool contains_clustered_components = std::ranges::any_of(clusters, [](const SpriteContourCluster& cluster) { return cluster.Contours.size() > 1; });
        vector<vector<SpriteMeshCandidate>> states(max_triangles + 1);
        states[0].emplace_back(SpriteMeshCandidate {});

        for (const SpriteContourCluster& cluster : clusters) {
            vector<SpriteMeshCandidate> cluster_candidates = BuildGreedyEnclosingSpriteCandidates(cluster.Contours, width, height, max_triangles);

            if (cluster_candidates.empty()) {
                return;
            }

            vector<vector<SpriteMeshCandidate>> next_states(max_triangles + 1);

            for (size_t used = 0; used < states.size(); used++) {
                for (const SpriteMeshCandidate& state : states[used]) {
                    for (const SpriteMeshCandidate& component : cluster_candidates) {
                        size_t component_triangles = component.Mesh.Indices.size() / 3;

                        if (component_triangles == 0 || component_triangles > max_triangles - used) {
                            continue;
                        }

                        optional<SpriteMeshCandidate> merged = MergeSpriteMeshCandidates(state, component);

                        if (!merged.has_value()) {
                            continue;
                        }

                        vector<SpriteMeshCandidate>& destination = next_states[used + component_triangles];
                        bool duplicate = std::ranges::any_of(destination, [&merged](const SpriteMeshCandidate& candidate) { return candidate.Mesh.Vertices == merged->Mesh.Vertices && candidate.Mesh.Indices == merged->Mesh.Indices; });

                        if (duplicate) {
                            continue;
                        }

                        destination.emplace_back(std::move(*merged));
                        std::ranges::stable_sort(destination, [](const SpriteMeshCandidate& left, const SpriteMeshCandidate& right) { return left.DoubleArea < right.DoubleArea; });

                        if (destination.size() > SPRITE_MESH_CANDIDATE_BEAM_WIDTH) {
                            destination.resize(SPRITE_MESH_CANDIDATE_BEAM_WIDTH);
                        }
                    }
                }
            }

            states = std::move(next_states);
        }

        for (size_t triangles = 1; triangles < states.size(); triangles++) {
            for (SpriteMeshCandidate& candidate : states[triangles]) {
                candidate.Source = contains_clustered_components ? SpriteMeshCandidateSource::ClusteredComponents : SpriteMeshCandidateSource::GreedyComponents;
                result.emplace_back(std::move(candidate));
            }
        }
    };

    vector<SpriteContourCluster> clusters;
    clusters.reserve(outer_contours.size());

    for (const SpriteContour& contour : outer_contours) {
        clusters.emplace_back(BuildSpriteContourCluster(contour));
    }

    vector<SpriteMeshCandidate> candidates;

    while (clusters.size() > 1) {
        append_partition_candidates(clusters, candidates);

        size_t best_left = 0;
        size_t best_right = 1;
        int64_t best_distance = std::numeric_limits<int64_t>::max();
        int64_t best_combined_area = std::numeric_limits<int64_t>::max();

        for (size_t left = 0; left + 1 < clusters.size(); left++) {
            for (size_t right = left + 1; right < clusters.size(); right++) {
                const SpriteContourCluster& a = clusters[left];
                const SpriteContourCluster& b = clusters[right];
                int64_t gap_x = std::max<int64_t>({0, numeric_cast<int64_t>(a.MinX) - b.MaxX, numeric_cast<int64_t>(b.MinX) - a.MaxX});
                int64_t gap_y = std::max<int64_t>({0, numeric_cast<int64_t>(a.MinY) - b.MaxY, numeric_cast<int64_t>(b.MinY) - a.MaxY});
                int64_t distance = gap_x * gap_x + gap_y * gap_y;
                int64_t combined_width = numeric_cast<int64_t>(std::max(a.MaxX, b.MaxX)) - std::min(a.MinX, b.MinX);
                int64_t combined_height = numeric_cast<int64_t>(std::max(a.MaxY, b.MaxY)) - std::min(a.MinY, b.MinY);
                int64_t combined_area = combined_width * combined_height;

                if (distance < best_distance || (distance == best_distance && combined_area < best_combined_area)) {
                    best_left = left;
                    best_right = right;
                    best_distance = distance;
                    best_combined_area = combined_area;
                }
            }
        }

        clusters[best_left] = MergeSpriteContourClusters(std::move(clusters[best_left]), std::move(clusters[best_right]));
        clusters.erase(clusters.begin() + numeric_cast<ptrdiff_t>(best_right));
    }

    return candidates;
}

static auto TryBuildBestSpriteMesh(const vector<uint8_t>& original_mask, const vector<uint8_t>& dilated_mask, int32_t width, int32_t height, const SpriteMeshBakeConfig& config, int64_t reference_quad_double_area) -> SpriteMeshSearchResult
{
    FO_STACK_TRACE_ENTRY();

    if (config.MaxTriangles == 0) {
        return SpriteMeshSearchResult {.QuadReason = SpriteMeshQuadReason::NoValidCandidate};
    }

    optional<vector<SpriteContour>> exact_contours = ExtractSpriteContours(dilated_mask, width, height, 0.0f);

    if (!exact_contours.has_value() || exact_contours->empty()) {
        return SpriteMeshSearchResult {.QuadReason = SpriteMeshQuadReason::ContourExtractionFailed};
    }

    vector<SpriteMeshCandidate> candidates = BuildGreedyEnclosingSpriteCandidates(*exact_contours, width, height, config.MaxTriangles);

    vector<SpriteContour> outer_contours;

    for (const SpriteContour& contour : *exact_contours) {
        if (contour.DoubleArea > 0) {
            outer_contours.emplace_back(contour);
        }
    }

    vector<SpriteMeshCandidate> clustered_candidates = BuildClusteredSpriteCandidates(outer_contours, width, height, config.MaxTriangles);
    candidates.insert(candidates.end(), std::make_move_iterator(clustered_candidates.begin()), std::make_move_iterator(clustered_candidates.end()));

    if (config.MaxTriangles >= 1) {
        int64_t triangle_double_area = 0;
        SpriteMeshBuildAttempt triangle_attempt = TryBuildEnclosingSpriteTriangle(dilated_mask, width, height, triangle_double_area);

        if (triangle_attempt.Mesh.has_value()) {
            candidates.emplace_back(SpriteMeshCandidate {.Mesh = std::move(*triangle_attempt.Mesh), .DoubleArea = triangle_double_area, .Validated = true, .Source = SpriteMeshCandidateSource::EnclosingTriangle});
        }
    }
    if (config.MaxTriangles >= 2) {
        int64_t quad_double_area = 0;
        SpriteMeshBuildAttempt quad_attempt = TryBuildEnclosingSpriteQuad(dilated_mask, width, height, quad_double_area);

        if (quad_attempt.Mesh.has_value()) {
            candidates.emplace_back(SpriteMeshCandidate {.Mesh = std::move(*quad_attempt.Mesh), .DoubleArea = quad_double_area, .Validated = true, .Source = SpriteMeshCandidateSource::EnclosingQuad});
        }
    }

    size_t visible_pixels = numeric_cast<size_t>(std::ranges::count(original_mask, uint8_t {1}));
    optional<SpriteMeshCandidate> best;
    float64_t best_score = -2.0;
    optional<SpriteMeshCandidateSummary> best_valid_candidate;
    constexpr float64_t score_epsilon = 0.000001;

    auto consider_candidate = [&](SpriteMeshCandidate candidate) {
        size_t triangle_count = candidate.Mesh.Indices.size() / 3;

        if (triangle_count == 0 || triangle_count > config.MaxTriangles || candidate.DoubleArea <= 0 || candidate.DoubleArea > reference_quad_double_area) {
            return;
        }

        if (!candidate.Validated) {
            int64_t validated_double_area = 0;
            SpriteMeshBuildFailure failure = ValidateSpriteMesh(candidate.Mesh, candidate.Enclosures, dilated_mask, {}, width, height, validated_double_area);

            if (failure != SpriteMeshBuildFailure::None) {
                return;
            }

            candidate.DoubleArea = validated_double_area;
        }

        float64_t saved_area_ratio = 1.0 - numeric_cast<float64_t>(candidate.DoubleArea) / numeric_cast<float64_t>(reference_quad_double_area);
        float64_t score = saved_area_ratio * config.AreaSavingsWeight - numeric_cast<float64_t>(triangle_count);
        candidate.Score = score;

        uint64_t candidate_double_area = numeric_cast<uint64_t>(candidate.DoubleArea);
        bool better_valid_score = !best_valid_candidate.has_value() || score > best_valid_candidate->Score + score_epsilon;
        bool equal_valid_score_with_smaller_area = best_valid_candidate.has_value() && std::abs(score - best_valid_candidate->Score) <= score_epsilon && candidate_double_area < best_valid_candidate->DoubleArea;

        if (better_valid_score || equal_valid_score_with_smaller_area) {
            best_valid_candidate = SpriteMeshCandidateSummary {
                .Source = candidate.Source,
                .TriangleCount = numeric_cast<uint32_t>(triangle_count),
                .VertexCount = numeric_cast<uint32_t>(candidate.Mesh.Vertices.size()),
                .DoubleArea = candidate_double_area,
                .Score = score,
                .SimplifyTolerance = candidate.SimplifyTolerance,
                .ActualDilation = candidate.ActualDilation,
            };
        }

        bool better_score = score > best_score + score_epsilon;
        bool equal_score_with_smaller_area = best.has_value() && std::abs(score - best_score) <= score_epsilon && candidate.DoubleArea < best->DoubleArea;

        if (better_score || equal_score_with_smaller_area) {
            best = std::move(candidate);
            best_score = score;
        }
    };

    for (SpriteMeshCandidate& candidate : candidates) {
        consider_candidate(std::move(candidate));
    }

    int64_t minimum_detailed_double_area = numeric_cast<int64_t>(visible_pixels) * 2;
    float64_t maximum_detailed_saved_ratio = 1.0 - numeric_cast<float64_t>(minimum_detailed_double_area) / numeric_cast<float64_t>(reference_quad_double_area);
    float64_t maximum_detailed_score = maximum_detailed_saved_ratio * config.AreaSavingsWeight - 1.0;

    if (maximum_detailed_score > best_score + score_epsilon) {
        optional<vector<SpriteContour>> original_exact_contours = ExtractSpriteContours(original_mask, width, height, 0.0f);

        if (original_exact_contours.has_value() && !original_exact_contours->empty()) {
            float32_t maximum_tolerance = numeric_cast<float32_t>(std::max(width, height));
            vector<uint8_t> detailed_tolerance_mask = DilateSpriteMask(dilated_mask, width, height, 1);
            constexpr size_t maximum_detailed_attempts = 10;
            auto advance_tolerance = [](float32_t& tolerance) {
                if (tolerance == 0.5f) {
                    tolerance = 1.0f;
                }
                else if (tolerance == 4.0f) {
                    tolerance = 5.0f;
                }
                else if (tolerance == 5.0f) {
                    tolerance = 8.0f;
                }
                else {
                    tolerance *= 2.0f;
                }
            };

            float32_t tolerance = 0.5f;

            for (size_t attempt_index = 0; attempt_index < maximum_detailed_attempts && tolerance <= maximum_tolerance; attempt_index++) {
                optional<vector<SpriteContour>> simplified_contours = ExtractSpriteContours(original_mask, width, height, tolerance);

                if (simplified_contours.has_value() && !simplified_contours->empty()) {
                    auto submit_detailed_attempt = [&](SpriteMeshBuildAttempt attempt, int64_t attempt_double_area) {
                        if (attempt.Mesh.has_value() && !attempt.Mesh->Indices.empty() && attempt.Mesh->Indices.size() / 3 <= config.MaxTriangles && attempt_double_area > 0 && attempt_double_area <= reference_quad_double_area) {
                            consider_candidate(SpriteMeshCandidate {
                                .Mesh = std::move(*attempt.Mesh),
                                .DoubleArea = attempt_double_area,
                                .Validated = true,
                                .Source = SpriteMeshCandidateSource::DetailedConstrained,
                                .SimplifyTolerance = tolerance,
                                .ActualDilation = SPRITE_MESH_DILATION,
                            });
                        }
                    };

                    int64_t constrained_double_area = 0;
                    SpriteMeshBuildAttempt constrained_attempt = TryBuildSpriteMesh(original_mask, dilated_mask, *simplified_contours, *original_exact_contours, *exact_contours, width, height, tolerance, SPRITE_MESH_DILATION, constrained_double_area);
                    submit_detailed_attempt(std::move(constrained_attempt), constrained_double_area);

                    auto validate_detailed_candidates = [&](vector<SpriteMeshCandidate>& detailed_candidates, const_span<uint8_t> validation_mask, SpriteMeshCandidateSource source, int32_t actual_dilation) {
                        vector<bool> accepted_triangle_counts(config.MaxTriangles + 1);

                        for (SpriteMeshCandidate& candidate : detailed_candidates) {
                            size_t triangle_count = candidate.Mesh.Indices.size() / 3;

                            if (triangle_count == 0 || triangle_count > config.MaxTriangles || accepted_triangle_counts[triangle_count] || candidate.DoubleArea <= 0 || candidate.DoubleArea > reference_quad_double_area) {
                                continue;
                            }

                            float64_t saved_area_ratio = 1.0 - numeric_cast<float64_t>(candidate.DoubleArea) / numeric_cast<float64_t>(reference_quad_double_area);
                            float64_t score = saved_area_ratio * config.AreaSavingsWeight - numeric_cast<float64_t>(triangle_count);
                            uint64_t candidate_double_area = numeric_cast<uint64_t>(candidate.DoubleArea);
                            bool can_improve_selected = score > best_score + score_epsilon || (best.has_value() && std::abs(score - best_score) <= score_epsilon && candidate.DoubleArea < best->DoubleArea);
                            bool can_improve_report = !best_valid_candidate.has_value() || score > best_valid_candidate->Score + score_epsilon || (std::abs(score - best_valid_candidate->Score) <= score_epsilon && candidate_double_area < best_valid_candidate->DoubleArea);

                            if (!can_improve_selected && !can_improve_report) {
                                accepted_triangle_counts[triangle_count] = true;
                                continue;
                            }

                            int64_t validated_double_area = 0;
                            SpriteMeshBuildFailure failure = ValidateSpriteMesh(candidate.Mesh, candidate.Enclosures, original_mask, validation_mask, width, height, validated_double_area);

                            if (failure != SpriteMeshBuildFailure::None) {
                                continue;
                            }

                            candidate.DoubleArea = validated_double_area;
                            candidate.Validated = true;
                            candidate.Source = source;
                            candidate.SimplifyTolerance = tolerance;
                            candidate.ActualDilation = actual_dilation;
                            consider_candidate(std::move(candidate));
                            accepted_triangle_counts[triangle_count] = true;
                        }
                    };

                    vector<SpriteMeshCandidate> simplified_candidates = BuildSimplifiedSpriteCandidates(*simplified_contours, width, height, SPRITE_MESH_DILATION, config.MaxTriangles);
                    validate_detailed_candidates(simplified_candidates, detailed_tolerance_mask, SpriteMeshCandidateSource::DetailedSimplified, SPRITE_MESH_DILATION);

                    // Douglas-Peucker bounds discarded points by the tolerance. Expanding by
                    // that distance turns a simplified contour into a cover candidate; any
                    // resulting transparent overdraw is compared by the normal score.
                    int32_t enclosing_dilation = std::min(SPRITE_MESH_MAXIMUM_PADDING, SPRITE_MESH_DILATION + iround<int32_t>(std::ceil(tolerance)));
                    vector<SpriteMeshCandidate> expanded_candidates = BuildSimplifiedSpriteCandidates(*simplified_contours, width, height, enclosing_dilation, config.MaxTriangles);
                    validate_detailed_candidates(expanded_candidates, {}, SpriteMeshCandidateSource::DetailedExpanded, enclosing_dilation);
                }

                advance_tolerance(tolerance);
            }
        }
    }

    if (!best.has_value()) {
        SpriteMeshQuadReason quad_reason = best_valid_candidate.has_value() ? SpriteMeshQuadReason::ScorePreferredQuad : SpriteMeshQuadReason::NoValidCandidate;
        return SpriteMeshSearchResult {.BestRejectedCandidate = std::move(best_valid_candidate), .QuadReason = quad_reason};
    }

    return SpriteMeshSearchResult {.Best = std::move(best), .QuadReason = SpriteMeshQuadReason::None};
}

static auto TryBuildSpriteMesh(const vector<uint8_t>& original_mask, const vector<uint8_t>& dilated_mask, const vector<SpriteContour>& simplified_contours, const vector<SpriteContour>& exact_contours, const vector<SpriteContour>& allowed_contours, int32_t width, int32_t height, float32_t simplify_tolerance, int32_t dilation, int64_t& mesh_double_area) -> SpriteMeshBuildAttempt
{
    FO_STACK_TRACE_ENTRY();

    optional<vector<SpriteContour>> contours = OffsetSpriteContours(simplified_contours, exact_contours, allowed_contours, width, height, simplify_tolerance, dilation);

    if (!contours.has_value() || contours->empty()) {
        return SpriteMeshBuildAttempt {.Failure = SpriteMeshBuildFailure::ContourOffset};
    }

    bool cleanup_offset_contours = dilation > 0 && simplify_tolerance > 0.0f;

    if (cleanup_offset_contours) {
        for (SpriteContour& contour : *contours) {
            if (contour.DoubleArea > 0) {
                SimplifyEnclosingClosedSpriteContour(contour.Points, simplify_tolerance);
            }
            else {
                SimplifyClosedSpriteContour(contour.Points, simplify_tolerance);
            }
            contour.DoubleArea = SpriteContourDoubleArea(contour.Points);
        }
    }

    size_t contour_vertices = 0;

    for (const SpriteContour& contour : *contours) {
        if (contour.Points.size() > SPRITE_MESH_SERIALIZED_VERTEX_LIMIT - std::min(SPRITE_MESH_SERIALIZED_VERTEX_LIMIT, contour_vertices)) {
            return SpriteMeshBuildAttempt {.Failure = SpriteMeshBuildFailure::SerializedVertexLimit};
        }

        contour_vertices += contour.Points.size();
    }

    optional<SpriteMeshData> mesh = TriangulateSpriteContours(*contours, width, height);

    if (!mesh.has_value()) {
        return SpriteMeshBuildAttempt {.Failure = SpriteMeshBuildFailure::Triangulation};
    }

    vector<uint8_t> cleanup_tolerance_mask;
    const_span<uint8_t> tolerance_mask = dilated_mask;

    if (cleanup_offset_contours) {
        cleanup_tolerance_mask = DilateSpriteMask(dilated_mask, width, height, 1);
        tolerance_mask = cleanup_tolerance_mask;
    }

    SpriteMeshBuildFailure validation_failure = ValidateSpriteMesh(*mesh, *contours, original_mask, tolerance_mask, width, height, mesh_double_area);

    if (validation_failure != SpriteMeshBuildFailure::None) {
        return SpriteMeshBuildAttempt {.Failure = validation_failure};
    }

    return SpriteMeshBuildAttempt {.Mesh = std::move(mesh)};
}

FO_END_NAMESPACE
