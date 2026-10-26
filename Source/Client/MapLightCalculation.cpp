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

#include "MapLightCalculation.h"

FO_BEGIN_NAMESPACE

static constexpr int32_t LIGHT_INTENSITY_MAX = 100;
static constexpr int32_t LIGHT_CAPACITY_MAX = 100;
static constexpr int32_t LIGHT_RAW_INTENSITY_MAX = 10000;
static constexpr int32_t LIGHT_HEX_COLOR_MAX = 200;
static constexpr int32_t LIGHT_COLOR_CHANNEL_MAX = 255;

template<typename Reader, typename Sink>
class LightCalculation final
{
public:
    LightCalculation(const MapLightCalculationInput& input, const Reader& read, const Sink& mark, MapLightCalculationResult& result) :
        _input(&input),
        _read(&read),
        _mark(&mark),
        _result(&result)
    {
    }

    void Calculate();

private:
    void TraceLightLine(mpos from_hex, mpos& to_hex, int32_t distance, int32_t raw_intensity);
    void MarkLightStep(mpos from_hex, mpos to_hex, int32_t raw_intensity);
    void MarkLightEnd(mpos from_hex, mpos to_hex, int32_t raw_intensity);
    void MarkLightEndNeighbor(mpos hex, bool north_south, int32_t raw_intensity);
    void MarkLight(mpos hex, int32_t raw_intensity);

    ptr<const MapLightCalculationInput> _input;
    ptr<const Reader> _read;
    ptr<const Sink> _mark;
    ptr<MapLightCalculationResult> _result;
};

static auto ReadLightObstacle(const MapLightObstacleView& view, mpos hex) -> MapLightObstacle;

auto CalculateMapLight(const MapLightCalculationInput& input, const MapLightObstacleView& obstacles) -> MapLightCalculationResult
{
    FO_TRACE_ZONE(Map);

    MapLightCalculationResult result;
    auto read = [&obstacles](mpos hex) { return ReadLightObstacle(obstacles, hex); };
    auto mark = [&result](mpos hex, ucolor color) { result.Marks.emplace_back(hex, color); };
    LightCalculation calculation(input, read, mark, result);
    calculation.Calculate();
    return result;
}

auto CalculateMapLightDirect(const MapLightCalculationInput& input, const function<MapLightObstacle(mpos)>& read_obstacle, const function<void(mpos, ucolor)>& mark) -> MapLightCalculationResult
{
    FO_TRACE_ZONE(Map);

    MapLightCalculationResult result;
    LightCalculation calculation(input, read_obstacle, mark, result);
    calculation.Calculate();
    return result;
}

static auto ReadLightObstacle(const MapLightObstacleView& view, mpos hex) -> MapLightObstacle
{
    int32_t x = hex.x - view.Origin.x;
    int32_t y = hex.y - view.Origin.y;
    FO_VERIFY_AND_THROW(x >= 0 && y >= 0 && x < view.Size.width && y < view.Size.height, "Light obstacle lookup is outside its snapshot", hex, view.Origin, view.Size);
    size_t index = numeric_cast<size_t>(y) * numeric_cast<size_t>(view.Size.width) + numeric_cast<size_t>(x);
    FO_VERIFY_AND_THROW(index < view.Cells.size(), "Light obstacle snapshot is incomplete", index, view.Cells.size());
    return view.Cells[index];
}

template<typename Reader, typename Sink>
void LightCalculation<Reader, Sink>::Calculate()
{
    mpos center_hex = _input->Hex;
    int32_t distance = std::max(1, _input->Distance);

    _result->FanHexes.clear();
    _result->FanHexes.reserve(numeric_cast<size_t>(distance) * GameSettings::MAP_DIR_COUNT);

    if (_input->Global) {
        _result->Capacity = _input->GlobalCapacity;
    }
    else if (_input->Intensity >= 0) {
        _result->Capacity = _input->MapCapacity;
    }
    else {
        _result->Capacity = LIGHT_CAPACITY_MAX;
    }

    if (_input->Inverse) {
        _result->Capacity = LIGHT_CAPACITY_MAX - _result->Capacity;
    }

    int32_t raw_intensity = std::clamp(_input->CurIntensity, 0, LIGHT_INTENSITY_MAX) * LIGHT_RAW_INTENSITY_MAX / LIGHT_INTENSITY_MAX;
    int32_t clamped_capacity = std::clamp(_result->Capacity, 0, LIGHT_CAPACITY_MAX);
    int64_t scaled_center_alpha = numeric_cast<int64_t>(raw_intensity) * LIGHT_COLOR_CHANNEL_MAX * clamped_capacity;
    uint8_t center_alpha = numeric_cast<uint8_t>(scaled_center_alpha / LIGHT_RAW_INTENSITY_MAX / LIGHT_CAPACITY_MAX);

    _result->CenterColor = ucolor {_input->Color, center_alpha};

    MarkLight(center_hex, raw_intensity);

    ipos32 raw_traced_hex = {center_hex.x, center_hex.y};
    bool seek_start = true;
    optional<mpos> last_traced_hex;

    // Preserve the perimeter order and one stop bit per direction
    for (int32_t i = 0, ii = GameSettings::MAP_DIR_COUNT; i < ii; i++) {
        mdir dir = hdir((i + 2) % GameSettings::MAP_DIR_COUNT);

        for (int32_t j = 0; j < distance; j++) {
            if (seek_start) {
                for (int32_t l = 0; l < distance; l++) {
#if FO_GEOMETRY == 1
                    GeometryHelper::MoveHexByDirUnsafe(raw_traced_hex, hdir::NorthEast);
#elif FO_GEOMETRY == 2
                    GeometryHelper::MoveHexByDirUnsafe(raw_traced_hex, hdir::North);
#endif
                }

                seek_start = false;
                j = -1;
            }
            else {
                GeometryHelper::MoveHexByDirUnsafe(raw_traced_hex, dir);
            }

            mpos traced_hex = _input->MapSize.clamp_pos(raw_traced_hex);

            bool dir_disabled = (_input->StopDirections & (uint16_t {1} << i)) != 0;

            if (dir_disabled) {
                traced_hex = center_hex;
            }
            else {
                TraceLightLine(center_hex, traced_hex, distance, raw_intensity);
            }

            if (!last_traced_hex.has_value() || traced_hex != last_traced_hex.value()) {
                uint8_t traced_alpha;
                bool use_offsets = false;

                if (ipos32 {traced_hex.x, traced_hex.y} != raw_traced_hex) {
                    traced_alpha = numeric_cast<uint8_t>(lerp(numeric_cast<int32_t>(center_alpha), 0, numeric_cast<float32_t>(GeometryHelper::GetDistance(center_hex, traced_hex)) / numeric_cast<float32_t>(distance)));

                    if (traced_hex == center_hex) {
                        use_offsets = true;
                    }
                }
                else {
                    traced_alpha = 0;
                    use_offsets = true;
                }

                _result->FanHexes.emplace_back(traced_hex, traced_alpha, use_offsets);
                last_traced_hex = traced_hex;
            }
        }
    }
}

template<typename Reader, typename Sink>
void LightCalculation<Reader, Sink>::TraceLightLine(mpos from_hex, mpos& to_hex, int32_t distance, int32_t raw_intensity)
{
    const auto [base_sx, base_sy] = GeometryHelper::GetStepsCoords({from_hex.x, from_hex.y}, {to_hex.x, to_hex.y});
    float32_t sx1_f = base_sx;
    float32_t sy1_f = base_sy;

    float32_t curx1_f = numeric_cast<float32_t>(from_hex.x);
    float32_t cury1_f = numeric_cast<float32_t>(from_hex.y);
    int32_t curx1_i = numeric_cast<int32_t>(from_hex.x);
    int32_t cury1_i = numeric_cast<int32_t>(from_hex.y);

    int32_t cur_raw_intensity = raw_intensity;
    int32_t raw_intensity_sub = raw_intensity / distance;

    auto resolve_hex = [this](int32_t hx, int32_t hy) -> mpos { return _input->MapSize.from_raw_pos(hx, hy); };

    while (true) {
        cur_raw_intensity -= raw_intensity_sub;
        curx1_f += sx1_f;
        cury1_f += sy1_f;

        int32_t old_curx1_i = curx1_i;
        int32_t old_cury1_i = cury1_i;

        // Casts
        curx1_i = iround<int32_t>(curx1_f);

        if (curx1_f - numeric_cast<float32_t>(curx1_i) >= 0.5f) {
            curx1_i++;
        }

        cury1_i = iround<int32_t>(cury1_f);

        if (cury1_f - numeric_cast<float32_t>(cury1_i) >= 0.5f) {
            cury1_i++;
        }

        // Left&Right trace
        int32_t ox = 0;
        int32_t oy = 0;

        if ((old_curx1_i % 2) != 0) {
            if (old_curx1_i + 1 == curx1_i && old_cury1_i + 1 == cury1_i) {
                ox = 1;
                oy = 1;
            }

            if (old_curx1_i - 1 == curx1_i && old_cury1_i + 1 == cury1_i) {
                ox = -1;
                oy = 1;
            }
        }
        else {
            if (old_curx1_i - 1 == curx1_i && old_cury1_i - 1 == cury1_i) {
                ox = -1;
                oy = -1;
            }

            if (old_curx1_i + 1 == curx1_i && old_cury1_i - 1 == cury1_i) {
                ox = 1;
                oy = -1;
            }
        }

        int32_t map_width = numeric_cast<int32_t>(_input->MapSize.width);
        int32_t map_height = numeric_cast<int32_t>(_input->MapSize.height);

        if (ox != 0) {
            // Left side
            ox = old_curx1_i + ox;

            if (ox < 0 || ox >= map_width || (*_read)(resolve_hex(ox, old_cury1_i)).LightBlocked) {
                to_hex = resolve_hex(ox < 0 || ox >= map_width ? old_curx1_i : ox, old_cury1_i);
                MarkLightEnd(resolve_hex(old_curx1_i, old_cury1_i), to_hex, cur_raw_intensity);
                break;
            }

            MarkLightStep(resolve_hex(old_curx1_i, old_cury1_i), resolve_hex(ox, old_cury1_i), cur_raw_intensity);

            // Right side
            oy = old_cury1_i + oy;

            if (oy < 0 || oy >= map_height || (*_read)(resolve_hex(old_curx1_i, oy)).LightBlocked) {
                to_hex = resolve_hex(old_curx1_i, oy < 0 || oy >= map_height ? old_cury1_i : oy);
                MarkLightEnd(resolve_hex(old_curx1_i, old_cury1_i), to_hex, cur_raw_intensity);
                break;
            }

            MarkLightStep(resolve_hex(old_curx1_i, old_cury1_i), resolve_hex(old_curx1_i, oy), cur_raw_intensity);
        }

        // Main trace
        if (curx1_i < 0 || curx1_i >= map_width || cury1_i < 0 || cury1_i >= map_height || (*_read)(resolve_hex(curx1_i, cury1_i)).LightBlocked) {
            to_hex = resolve_hex(curx1_i < 0 || curx1_i >= map_width ? old_curx1_i : curx1_i, cury1_i < 0 || cury1_i >= map_height ? old_cury1_i : cury1_i);
            MarkLightEnd(resolve_hex(old_curx1_i, old_cury1_i), to_hex, cur_raw_intensity);
            break;
        }

        MarkLightEnd(resolve_hex(old_curx1_i, old_cury1_i), resolve_hex(curx1_i, cury1_i), cur_raw_intensity);

        if (curx1_i == numeric_cast<int32_t>(to_hex.x) && cury1_i == numeric_cast<int32_t>(to_hex.y)) {
            break;
        }
    }
}

template<typename Reader, typename Sink>
void LightCalculation<Reader, Sink>::MarkLightStep(mpos from_hex, mpos to_hex, int32_t raw_intensity)
{
    const auto& field = (*_read)(to_hex);

    if (field.HasTransparentWall) {
        bool north_south = field.Corner == CornerType::NorthSouth || field.Corner == CornerType::North || field.Corner == CornerType::West;
        hdir dir = GeometryHelper::GetHexDir(from_hex, to_hex);

        if (dir == hdir::NorthEast || (north_south && dir == hdir::East) || (!north_south && (dir == hdir::West || dir == hdir::NorthWest))) {
            MarkLight(to_hex, raw_intensity);
        }
    }
    else {
        MarkLight(to_hex, raw_intensity);
    }
}

template<typename Reader, typename Sink>
void LightCalculation<Reader, Sink>::MarkLightEnd(mpos from_hex, mpos to_hex, int32_t raw_intensity)
{
    bool is_wall = false;
    bool north_south = false;
    const auto& field = (*_read)(to_hex);

    if (field.HasWall) {
        is_wall = true;

        if (field.Corner == CornerType::NorthSouth || field.Corner == CornerType::North || field.Corner == CornerType::West) {
            north_south = true;
        }
    }

    hdir dir = GeometryHelper::GetHexDir(from_hex, to_hex);

    if (dir == hdir::NorthEast || (north_south && dir == hdir::East) || (!north_south && (dir == hdir::West || dir == hdir::NorthWest))) {
        MarkLight(to_hex, raw_intensity);

        if (is_wall) {
            if (north_south) {
                if (to_hex.y > 0) {
                    MarkLightEndNeighbor(_input->MapSize.from_raw_pos(to_hex.x, to_hex.y - 1), true, raw_intensity);
                }
                if (to_hex.y < _input->MapSize.height - 1) {
                    MarkLightEndNeighbor(_input->MapSize.from_raw_pos(to_hex.x, to_hex.y + 1), true, raw_intensity);
                }
            }
            else {
                if (to_hex.x > 0) {
                    MarkLightEndNeighbor(_input->MapSize.from_raw_pos(to_hex.x - 1, to_hex.y), false, raw_intensity);

                    if (to_hex.y > 0) {
                        MarkLightEndNeighbor(_input->MapSize.from_raw_pos(to_hex.x - 1, to_hex.y - 1), false, raw_intensity);
                    }
                    if (to_hex.y < _input->MapSize.height - 1) {
                        MarkLightEndNeighbor(_input->MapSize.from_raw_pos(to_hex.x - 1, to_hex.y + 1), false, raw_intensity);
                    }
                }
                if (to_hex.x < _input->MapSize.width - 1) {
                    MarkLightEndNeighbor(_input->MapSize.from_raw_pos(to_hex.x + 1, to_hex.y), false, raw_intensity);

                    if (to_hex.y > 0) {
                        MarkLightEndNeighbor(_input->MapSize.from_raw_pos(to_hex.x + 1, to_hex.y - 1), false, raw_intensity);
                    }
                    if (to_hex.y < _input->MapSize.height - 1) {
                        MarkLightEndNeighbor(_input->MapSize.from_raw_pos(to_hex.x + 1, to_hex.y + 1), false, raw_intensity);
                    }
                }
            }
        }
    }
}

template<typename Reader, typename Sink>
void LightCalculation<Reader, Sink>::MarkLightEndNeighbor(mpos hex, bool north_south, int32_t raw_intensity)
{
    const auto& field = (*_read)(hex);

    if (field.HasWall) {
        auto corner = field.Corner;

        if ((north_south && (corner == CornerType::NorthSouth || corner == CornerType::North || corner == CornerType::West)) || (!north_south && (corner == CornerType::EastWest || corner == CornerType::East)) || corner == CornerType::South) {
            MarkLight(hex, raw_intensity / 2);
        }
    }
}

template<typename Reader, typename Sink>
void LightCalculation<Reader, Sink>::MarkLight(mpos hex, int32_t raw_intensity)
{
    int32_t clamped_raw_intensity = std::clamp(raw_intensity, 0, LIGHT_RAW_INTENSITY_MAX);
    int32_t clamped_capacity = std::clamp(_result->Capacity, 0, LIGHT_CAPACITY_MAX);
    int64_t scaled_light_value = numeric_cast<int64_t>(clamped_raw_intensity) * LIGHT_HEX_COLOR_MAX * clamped_capacity;
    int32_t light_value = numeric_cast<int32_t>(scaled_light_value / LIGHT_RAW_INTENSITY_MAX / LIGHT_CAPACITY_MAX);
    int32_t scaled_light_value_r = light_value * numeric_cast<int32_t>(_result->CenterColor.comp.r) / LIGHT_COLOR_CHANNEL_MAX;
    int32_t scaled_light_value_g = light_value * numeric_cast<int32_t>(_result->CenterColor.comp.g) / LIGHT_COLOR_CHANNEL_MAX;
    int32_t scaled_light_value_b = light_value * numeric_cast<int32_t>(_result->CenterColor.comp.b) / LIGHT_COLOR_CHANNEL_MAX;
    uint8_t light_value_r = numeric_cast<uint8_t>(std::clamp(scaled_light_value_r, 0, LIGHT_COLOR_CHANNEL_MAX));
    uint8_t light_value_g = numeric_cast<uint8_t>(std::clamp(scaled_light_value_g, 0, LIGHT_COLOR_CHANNEL_MAX));
    uint8_t light_value_b = numeric_cast<uint8_t>(std::clamp(scaled_light_value_b, 0, LIGHT_COLOR_CHANNEL_MAX));
    ucolor light_color = ucolor {light_value_r, light_value_g, light_value_b, 0};

    (*_mark)(hex, light_color);
}

FO_END_NAMESPACE

