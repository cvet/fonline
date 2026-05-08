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

#include "SpriteMesher.h"

#include <mapbox/earcut.hpp>

FO_BEGIN_NAMESPACE

struct PreviewMaskPoint final
{
    int32 X {};
    int32 Y {};

    [[nodiscard]] auto operator==(const PreviewMaskPoint&) const noexcept -> bool = default;
};

struct PreviewMaskPointHasher final
{
    [[nodiscard]] auto operator()(const PreviewMaskPoint& point) const noexcept -> size_t
    {
        const size_t hx = numeric_cast<size_t>(static_cast<uint32>(point.X)) * 73856093U;
        const size_t hy = numeric_cast<size_t>(static_cast<uint32>(point.Y)) * 19349663U;
        return hx ^ hy;
    }
};

struct PreviewPolygonGroup final
{
    vector<array<float64, 2>> Outer {};
    vector<vector<array<float64, 2>>> Holes {};
};

static auto EncodePreviewCellKey(int32 x, int32 y) -> uint64
{
    FO_STACK_TRACE_ENTRY();

    return (numeric_cast<uint64>(static_cast<uint32>(x)) << 32U) | numeric_cast<uint64>(static_cast<uint32>(y));
}

static auto ComputePreviewRingSignedArea(const vector<array<float64, 2>>& ring) -> float64
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(ring.size() >= 3);

    float64 area = 0.0;

    for (size_t i = 0, j = ring.size(); i < j; i++) {
        const auto& p0 = ring[i];
        const auto& p1 = ring[(i + 1) % j];
        area += p0[0] * p1[1] - p1[0] * p0[1];
    }

    return area * 0.5;
}

static auto IsPreviewPointInsidePolygon(const array<float64, 2>& point, const vector<array<float64, 2>>& polygon) -> bool
{
    FO_STACK_TRACE_ENTRY();

    bool inside = false;

    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const auto& pi = polygon[i];
        const auto& pj = polygon[j];
        const bool intersect = ((pi[1] > point[1]) != (pj[1] > point[1])) &&
            (point[0] < (pj[0] - pi[0]) * (point[1] - pi[1]) / (pj[1] - pi[1]) + pi[0]);

        if (intersect) {
            inside = !inside;
        }
    }

    return inside;
}

static auto RemovePreviewCollinearPoints(const vector<PreviewMaskPoint>& ring) -> vector<PreviewMaskPoint>
{
    FO_STACK_TRACE_ENTRY();

    if (ring.size() <= 3) {
        return ring;
    }

    vector<PreviewMaskPoint> result;
    result.reserve(ring.size());

    for (size_t i = 0, j = ring.size(); i < j; i++) {
        const auto& prev = ring[(i + j - 1) % j];
        const auto& cur = ring[i];
        const auto& next = ring[(i + 1) % j];

        const int64 dx1 = numeric_cast<int64>(cur.X) - prev.X;
        const int64 dy1 = numeric_cast<int64>(cur.Y) - prev.Y;
        const int64 dx2 = numeric_cast<int64>(next.X) - cur.X;
        const int64 dy2 = numeric_cast<int64>(next.Y) - cur.Y;
        const int64 cross = dx1 * dy2 - dy1 * dx2;

        if (cross != 0) {
            result.emplace_back(cur);
        }
    }

    return result.size() >= 3 ? result : ring;
}

static auto ComputePreviewPointToSegmentDistanceSquared(const array<float64, 2>& point, const array<float64, 2>& start, const array<float64, 2>& end) -> float64
{
    FO_STACK_TRACE_ENTRY();

    const float64 dx = end[0] - start[0];
    const float64 dy = end[1] - start[1];

    if (dx == 0.0 && dy == 0.0) {
        const float64 dist_x = point[0] - start[0];
        const float64 dist_y = point[1] - start[1];
        return dist_x * dist_x + dist_y * dist_y;
    }

    const float64 projection = ((point[0] - start[0]) * dx + (point[1] - start[1]) * dy) / (dx * dx + dy * dy);
    const float64 t = std::clamp(projection, 0.0, 1.0);
    const float64 closest_x = start[0] + dx * t;
    const float64 closest_y = start[1] + dy * t;
    const float64 dist_x = point[0] - closest_x;
    const float64 dist_y = point[1] - closest_y;
    return dist_x * dist_x + dist_y * dist_y;
}

static void SimplifyPreviewPolylineSection(const vector<array<float64, 2>>& points, size_t begin, size_t end, float64 epsilon_squared, vector<bool>& keep_points)
{
    FO_STACK_TRACE_ENTRY();

    float64 max_distance = 0.0;
    size_t farthest_index = 0;

    for (size_t i = begin + 1; i < end; i++) {
        const float64 distance = ComputePreviewPointToSegmentDistanceSquared(points[i], points[begin], points[end]);

        if (distance > max_distance) {
            max_distance = distance;
            farthest_index = i;
        }
    }

    if (max_distance > epsilon_squared) {
        keep_points[farthest_index] = true;
        SimplifyPreviewPolylineSection(points, begin, farthest_index, epsilon_squared, keep_points);
        SimplifyPreviewPolylineSection(points, farthest_index, end, epsilon_squared, keep_points);
    }
}

static auto SimplifyPreviewRing(const vector<PreviewMaskPoint>& ring, float64 epsilon) -> vector<array<float64, 2>>
{
    FO_STACK_TRACE_ENTRY();

    vector<array<float64, 2>> original_points;
    original_points.reserve(ring.size());

    for (const auto& point : ring) {
        original_points.emplace_back(array<float64, 2> {numeric_cast<float64>(point.X), numeric_cast<float64>(point.Y)});
    }

    vector<array<float64, 2>> points = original_points;
    points.emplace_back(points.front());

    vector<bool> keep_points(points.size());
    keep_points.front() = true;
    keep_points.back() = true;
    SimplifyPreviewPolylineSection(points, 0, points.size() - 1, epsilon * epsilon, keep_points);

    vector<array<float64, 2>> simplified;
    simplified.reserve(points.size());

    for (size_t i = 0, j = points.size(); i < j; i++) {
        if (keep_points[i]) {
            simplified.emplace_back(points[i]);
        }
    }

    if (!simplified.empty()) {
        simplified.pop_back();
    }

    return simplified.size() >= 3 ? simplified : original_points;
}

static auto ExtractPreviewComponents(isize32 size, const ucolor* data) -> vector<vector<PreviewMaskPoint>>
{
    FO_STACK_TRACE_ENTRY();

    const size_t pixels_count = numeric_cast<size_t>(size.width) * size.height;
    vector<bool> opaque_mask(pixels_count);

    for (size_t i = 0; i < pixels_count; i++) {
        opaque_mask[i] = data[i].comp.a != 0;
    }

    vector<bool> visited(pixels_count);
    vector<vector<PreviewMaskPoint>> components;
    vector<PreviewMaskPoint> stack;

    for (int32 y = 0; y < size.height; y++) {
        for (int32 x = 0; x < size.width; x++) {
            const size_t index = numeric_cast<size_t>(y) * size.width + x;

            if (!opaque_mask[index] || visited[index]) {
                continue;
            }

            auto& component = components.emplace_back();
            stack.clear();
            stack.emplace_back(PreviewMaskPoint {x, y});
            visited[index] = true;

            while (!stack.empty()) {
                const auto point = stack.back();
                stack.pop_back();
                component.emplace_back(point);

                static constexpr int32 OFFS_X[4] = {0, 1, 0, -1};
                static constexpr int32 OFFS_Y[4] = {-1, 0, 1, 0};

                for (const auto dir : iterate_range(4)) {
                    const int32 nx = point.X + OFFS_X[dir];
                    const int32 ny = point.Y + OFFS_Y[dir];

                    if (nx < 0 || nx >= size.width || ny < 0 || ny >= size.height) {
                        continue;
                    }

                    const size_t next_index = numeric_cast<size_t>(ny) * size.width + nx;

                    if (!opaque_mask[next_index] || visited[next_index]) {
                        continue;
                    }

                    visited[next_index] = true;
                    stack.emplace_back(PreviewMaskPoint {nx, ny});
                }
            }
        }
    }

    return components;
}

static auto ExtractPreviewComponentRings(const vector<PreviewMaskPoint>& component_pixels, isize32 size) -> vector<vector<PreviewMaskPoint>>
{
    FO_STACK_TRACE_ENTRY();

    unordered_set<uint64> pixel_set;
    pixel_set.reserve(component_pixels.size());

    for (const auto& point : component_pixels) {
        pixel_set.emplace(EncodePreviewCellKey(point.X, point.Y));
    }

    unordered_map<PreviewMaskPoint, vector<PreviewMaskPoint>, PreviewMaskPointHasher> outgoing_edges;

    const auto has_pixel = [&pixel_set](int32 x, int32 y) -> bool {
        return pixel_set.contains(EncodePreviewCellKey(x, y));
    };

    const auto add_edge = [&outgoing_edges](PreviewMaskPoint from, PreviewMaskPoint to) {
        outgoing_edges[from].emplace_back(to);
    };

    for (const auto& point : component_pixels) {
        const int32 x = point.X;
        const int32 y = point.Y;

        if (y == 0 || !has_pixel(x, y - 1)) {
            add_edge({x, y}, {x + 1, y});
        }
        if (x == size.width - 1 || !has_pixel(x + 1, y)) {
            add_edge({x + 1, y}, {x + 1, y + 1});
        }
        if (y == size.height - 1 || !has_pixel(x, y + 1)) {
            add_edge({x + 1, y + 1}, {x, y + 1});
        }
        if (x == 0 || !has_pixel(x - 1, y)) {
            add_edge({x, y + 1}, {x, y});
        }
    }

    vector<vector<PreviewMaskPoint>> rings;

    while (!outgoing_edges.empty()) {
        auto start_it = outgoing_edges.begin();
        FO_RUNTIME_ASSERT(!start_it->second.empty());

        vector<PreviewMaskPoint> ring;
        ring.reserve(start_it->second.size() + 8);

        const auto start = start_it->first;
        auto current = start;
        auto next = start_it->second.back();
        start_it->second.pop_back();

        if (start_it->second.empty()) {
            outgoing_edges.erase(start_it);
        }

        ring.emplace_back(start);

        size_t guard = 0;

        while (true) {
            ring.emplace_back(next);
            current = next;

            if (current == start) {
                break;
            }

            auto next_it = outgoing_edges.find(current);
            FO_RUNTIME_ASSERT(next_it != outgoing_edges.end());
            FO_RUNTIME_ASSERT(next_it->second.size() == 1);

            next = next_it->second.back();
            next_it->second.pop_back();

            if (next_it->second.empty()) {
                outgoing_edges.erase(next_it);
            }

            guard++;
            FO_RUNTIME_ASSERT(guard <= component_pixels.size() * 8 + 8);
        }

        FO_RUNTIME_ASSERT(ring.size() >= 4);
        ring.pop_back();
        rings.emplace_back(RemovePreviewCollinearPoints(ring));
    }

    return rings;
}

static auto BuildPreviewPolygonGroups(const vector<vector<PreviewMaskPoint>>& rings, isize32 size) -> vector<PreviewPolygonGroup>
{
    FO_STACK_TRACE_ENTRY();

    struct RingData final
    {
        vector<array<float64, 2>> Points {};
        float64 Area {};
    };

    vector<RingData> ring_data;
    const float64 epsilon = std::max(0.75, numeric_cast<float64>(std::max(size.width, size.height)) / 48.0);

    for (const auto& ring : rings) {
        const auto simplified = SimplifyPreviewRing(ring, epsilon);

        if (simplified.size() < 3) {
            continue;
        }

        const float64 area = ComputePreviewRingSignedArea(simplified);

        if (std::abs(area) < 0.5) {
            continue;
        }

        ring_data.emplace_back(RingData {simplified, area});
    }

    std::sort(ring_data.begin(), ring_data.end(), [](const RingData& left, const RingData& right) {
        return std::abs(left.Area) > std::abs(right.Area);
    });

    vector<PreviewPolygonGroup> groups;

    for (const auto& ring : ring_data) {
        bool assigned_as_hole = false;

        for (auto& group : groups) {
            if (IsPreviewPointInsidePolygon(ring.Points.front(), group.Outer)) {
                const float64 group_area = ComputePreviewRingSignedArea(group.Outer);

                if ((group_area > 0.0) != (ring.Area > 0.0)) {
                    group.Holes.emplace_back(ring.Points);
                    assigned_as_hole = true;
                    break;
                }
            }
        }

        if (!assigned_as_hole) {
            auto& group = groups.emplace_back();
            group.Outer = ring.Points;
        }
    }

    return groups;
}

static auto IsPreviewPointInsideTriangle(float64 px, float64 py, const SpritePreviewTriangle& triangle) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto edge_sign = [](const SpritePreviewPoint& from, const SpritePreviewPoint& to, float64 x, float64 y) -> float64 {
        return (x - from.X) * numeric_cast<float64>(to.Y - from.Y) - (y - from.Y) * numeric_cast<float64>(to.X - from.X);
    };

    static constexpr float64 EPSILON = 0.0001;

    const float64 d0 = edge_sign(triangle.Vertices[0], triangle.Vertices[1], px, py);
    const float64 d1 = edge_sign(triangle.Vertices[1], triangle.Vertices[2], px, py);
    const float64 d2 = edge_sign(triangle.Vertices[2], triangle.Vertices[0], px, py);
    const bool has_negative = d0 < -EPSILON || d1 < -EPSILON || d2 < -EPSILON;
    const bool has_positive = d0 > EPSILON || d1 > EPSILON || d2 > EPSILON;
    return !(has_negative && has_positive);
}

static auto EvaluatePreviewTriangleColor(const SpritePreviewTriangle& triangle, const ucolor* data, isize32 size) -> ucolor
{
    FO_STACK_TRACE_ENTRY();

    float32 min_x = triangle.Vertices[0].X;
    float32 max_x = triangle.Vertices[0].X;
    float32 min_y = triangle.Vertices[0].Y;
    float32 max_y = triangle.Vertices[0].Y;

    for (const auto& vertex : triangle.Vertices) {
        min_x = std::min(min_x, vertex.X);
        max_x = std::max(max_x, vertex.X);
        min_y = std::min(min_y, vertex.Y);
        max_y = std::max(max_y, vertex.Y);
    }

    const int32 left = std::clamp(ifloor<int32>(min_x), 0, size.width - 1);
    const int32 right = std::clamp(iceil<int32>(max_x) - 1, 0, size.width - 1);
    const int32 top = std::clamp(ifloor<int32>(min_y), 0, size.height - 1);
    const int32 bottom = std::clamp(iceil<int32>(max_y) - 1, 0, size.height - 1);

    uint64 accum_r = 0;
    uint64 accum_g = 0;
    uint64 accum_b = 0;
    uint64 accum_a = 0;
    uint64 accum_weight = 0;
    uint32 sample_count = 0;

    for (int32 y = top; y <= bottom; y++) {
        for (int32 x = left; x <= right; x++) {
            if (!IsPreviewPointInsideTriangle(numeric_cast<float64>(x) + 0.5, numeric_cast<float64>(y) + 0.5, triangle)) {
                continue;
            }

            const auto& pixel = data[numeric_cast<size_t>(y) * size.width + x];
            const uint32 alpha = pixel.comp.a;

            if (alpha == 0) {
                continue;
            }

            accum_r += numeric_cast<uint64>(pixel.comp.r) * alpha;
            accum_g += numeric_cast<uint64>(pixel.comp.g) * alpha;
            accum_b += numeric_cast<uint64>(pixel.comp.b) * alpha;
            accum_a += alpha;
            accum_weight += alpha;
            sample_count++;
        }
    }

    if (accum_weight != 0 && sample_count != 0) {
        return {
            numeric_cast<uint8>(accum_r / accum_weight),
            numeric_cast<uint8>(accum_g / accum_weight),
            numeric_cast<uint8>(accum_b / accum_weight),
            numeric_cast<uint8>(accum_a / sample_count)};
    }

    for (int32 y = top; y <= bottom; y++) {
        for (int32 x = left; x <= right; x++) {
            const auto& pixel = data[numeric_cast<size_t>(y) * size.width + x];

            if (pixel.comp.a != 0) {
                return pixel;
            }
        }
    }

    return ucolor::clear;
}

auto BuildSpritePreviewMesh(isize32 size, const_span<ucolor> data) -> vector<SpritePreviewTriangle>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(size.width > 0);
    FO_RUNTIME_ASSERT(size.height > 0);
    FO_RUNTIME_ASSERT(data.size() == numeric_cast<size_t>(size.width) * size.height);

    vector<SpritePreviewTriangle> triangles;

    for (const auto& component : ExtractPreviewComponents(size, data.data())) {
        const auto rings = ExtractPreviewComponentRings(component, size);

        for (const auto& group : BuildPreviewPolygonGroups(rings, size)) {
            vector<vector<array<float64, 2>>> polygon;
            vector<array<float64, 2>> flat_points;

            polygon.emplace_back(group.Outer);
            flat_points.insert(flat_points.end(), group.Outer.begin(), group.Outer.end());

            for (const auto& hole : group.Holes) {
                polygon.emplace_back(hole);
                flat_points.insert(flat_points.end(), hole.begin(), hole.end());
            }

            const auto indices = mapbox::earcut<uint32>(polygon);
            FO_RUNTIME_ASSERT(!indices.empty());
            FO_RUNTIME_ASSERT(indices.size() % 3 == 0);

            for (size_t i = 0, j = indices.size(); i < j; i += 3) {
                const auto& p0 = flat_points[indices[i]];
                const auto& p1 = flat_points[indices[i + 1]];
                const auto& p2 = flat_points[indices[i + 2]];
                SpritePreviewTriangle triangle;
                triangle.Vertices[0] = {numeric_cast<float32>(p0[0]), numeric_cast<float32>(p0[1])};
                triangle.Vertices[1] = {numeric_cast<float32>(p1[0]), numeric_cast<float32>(p1[1])};
                triangle.Vertices[2] = {numeric_cast<float32>(p2[0]), numeric_cast<float32>(p2[1])};
                triangle.Color = EvaluatePreviewTriangleColor(triangle, data.data(), size);
                triangles.emplace_back(triangle);
            }
        }
    }

    return triangles;
}

FO_END_NAMESPACE