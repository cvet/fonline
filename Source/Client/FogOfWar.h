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

#include "SpriteManager.h"

FO_BEGIN_NAMESPACE

class FogOfWar final
{
public:
    enum class Kind
    {
        Look,
        Shoot,
    };

    struct Origin
    {
        bool Valid {};
        mpos BaseHex {};
        int32 LookDistance {};
    };

    struct Input
    {
        bool Enabled {true};
        int32 Distance {};
        int32 FogExtraLength {};
        int32 FogTransitionDuration {};
        int32 MapHexWidth {};
        int32 MapHexHeight {};
        msize MapSize {};
        nanotime FrameTime {};
        Origin FogOrigin {};
        function<mpos(mpos, mpos, int32, bool)> TraceBulletToBlock {};
    };

    explicit FogOfWar(Kind kind) noexcept :
        _kind {kind}
    {
    }
    FogOfWar(const FogOfWar&) = delete;
    FogOfWar(FogOfWar&&) noexcept = default;
    auto operator=(const FogOfWar&) -> FogOfWar& = delete;
    auto operator=(FogOfWar&&) noexcept -> FogOfWar& = default;

    [[nodiscard]] auto GetPoints() const noexcept -> const vector<PrimitivePoint>& { return _points; }

    void RequestRebuild() noexcept { _rebuildFog = true; }
    void SetDrawOffset(ipos32 offset) noexcept { _drawOffset = offset; }
    void SetBaseDrawOffset(ipos32 offset) noexcept { _baseDrawOffset = offset; }
    void Prepare(const Input& input);
    void Clear();

private:
    void BuildPoints(const Input& input, vector<PrimitivePoint>& points) const;
    void StartTransition(vector<PrimitivePoint>&& points, nanotime frame_time, int32 duration);
    void UpdateTransition(nanotime frame_time, int32 duration);
    void InterpolatePoints(const vector<PrimitivePoint>& from_points, const vector<PrimitivePoint>& to_points, float32 t, vector<PrimitivePoint>& result_points);
    auto MakeCollapsed(const vector<PrimitivePoint>& points) const -> vector<PrimitivePoint>;

    static auto GetCollapsePoint(const vector<PrimitivePoint>& points) -> PrimitivePoint;
    static auto SampleEdgePoint(const vector<PrimitivePoint>& points, size_t edge_count, size_t sample_edge_idx, size_t sample_edge_count, const PrimitivePoint& fallback) -> PrimitivePoint;
    static auto LerpFogColor(ucolor from, ucolor to, float32 t) -> ucolor;
    static auto LerpFogPoint(const PrimitivePoint& from, const PrimitivePoint& to, float32 t) -> PrimitivePoint;

    Kind _kind;
    bool _rebuildFog {};
    bool _lastEnabled {true};
    bool _transitionActive {};
    bool _collapsingToOff {};
    ipos32 _drawOffset {};
    ipos32 _baseDrawOffset {};
    Origin _lastOrigin {};
    int32 _lastDistance {};
    nanotime _transitionTime {};
    vector<PrimitivePoint> _points {};
    vector<PrimitivePoint> _startPoints {};
    vector<PrimitivePoint> _targetPoints {};
};

FO_END_NAMESPACE
