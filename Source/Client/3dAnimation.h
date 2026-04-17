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

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

class ModelAnimation final
{
public:
    struct BoneOutput
    {
        hstring BoneName {};
        vector<float32_t> ScaleTime {};
        vector<vec3> ScaleValue {};
        vector<float32_t> RotationTime {};
        vector<quaternion> RotationValue {};
        vector<float32_t> TranslationTime {};
        vector<vec3> TranslationValue {};
    };

    ModelAnimation() = default;
    ModelAnimation(const ModelAnimation&) = delete;
    ModelAnimation(ModelAnimation&&) noexcept = default;
    auto operator=(const ModelAnimation&) = delete;
    auto operator=(ModelAnimation&&) noexcept = delete;
    ~ModelAnimation() = default;

    [[nodiscard]] auto GetFileName() const noexcept -> string_view { return _animFileName; }
    [[nodiscard]] auto GetName() const noexcept -> string_view { return _animName; }
    [[nodiscard]] auto GetBoneOutputs() const noexcept -> const vector<BoneOutput>& { return _boneOutputs; }
    [[nodiscard]] auto GetDuration() const noexcept -> float32_t { return _duration; }
    [[nodiscard]] auto GetBonesHierarchy() const noexcept -> const vector<vector<hstring>>& { return _bonesHierarchy; }

    void Load(DataReader& reader, HashResolver& hash_resolver);

private:
    string _animFileName {};
    string _animName {};
    float32_t _duration {};
    vector<BoneOutput> _boneOutputs {};
    vector<vector<hstring>> _bonesHierarchy {};
};

class ModelAnimationController final
{
public:
    explicit ModelAnimationController(int32_t track_count);
    ModelAnimationController(const ModelAnimationController&) = delete;
    ModelAnimationController(ModelAnimationController&&) noexcept = default;
    auto operator=(const ModelAnimationController&) = delete;
    auto operator=(ModelAnimationController&&) noexcept = delete;
    ~ModelAnimationController() = default;

    [[nodiscard]] auto Copy() const -> unique_ptr<ModelAnimationController>;
    [[nodiscard]] auto GetAnimationBones(int32_t index) const -> const vector<vector<hstring>>&;
    [[nodiscard]] auto GetAnimationDuration(int32_t index) const -> float32_t;
    [[nodiscard]] auto GetTrackEnable(int32_t track) const -> bool;
    [[nodiscard]] auto GetTrackPosition(int32_t track) const -> float32_t;
    [[nodiscard]] auto GetTrackSpeed(int32_t track) const -> float32_t;
    [[nodiscard]] auto GetAnimationsCount() const -> int32_t;

    void RegisterAnimationOutput(hstring bone_name, mat44& output_matrix);
    auto RegisterAnimation(ModelAnimation* animation, bool reversed) -> int32_t;
    void SetTrackAnimation(int32_t track, int32_t anim_index, const unordered_set<hstring>* allowed_bones);
    void ResetBonesTransition(int32_t skip_track, const vector<hstring>& bone_names);
    void ResetEvents();
    void AddEventEnable(int32_t track, bool enable, float32_t start_time);
    void AddEventSpeed(int32_t track, float32_t speed, float32_t start_time, float32_t smooth_time);
    void AddEventWeight(int32_t track, float32_t weight, float32_t start_time, float32_t smooth_time);
    void SetTrackEnable(int32_t track, bool enable);
    void SetTrackPosition(int32_t track, float32_t position);
    void SetTrackSpeed(int32_t track, float32_t speed);
    void SetInterpolation(bool enabled);
    void AdvanceTime(float32_t time);

private:
    struct Output
    {
        hstring BoneName {};
        raw_ptr<mat44> Matrix {};
        vector<bool> Valid {};
        vector<float32_t> Factor {};
        vector<vec3> Scale {};
        vector<quaternion> Rotation {};
        vector<vec3> Translation {};
    };

    struct Track
    {
        enum class EventType : uint8_t
        {
            Enable,
            Speed,
            Weight
        };

        struct Event
        {
            EventType Type {};
            float32_t ValueTo {};
            float32_t StartTime {};
            float32_t SmoothTime {};
            optional<float32_t> ValueFrom {};
        };

        bool Enabled {};
        float32_t Speed {};
        float32_t Weight {};
        float32_t Position {};
        raw_ptr<const ModelAnimation> Anim {};
        bool Reversed {};
        vector<raw_ptr<Output>> AnimOutput {};
        vector<Event> Events {};
    };

    void Interpolate(quaternion& q1, const quaternion& q2, float32_t factor) const;
    void Interpolate(vec3& v1, const vec3& v2, float32_t factor) const;

    template<typename T>
    void FindSrtValue(float32_t time, float32_t duration, bool reserved, const vector<float32_t>& times, const vector<T>& values, T& result);

    shared_ptr<vector<pair<ModelAnimation*, bool>>> _anims {};
    shared_ptr<vector<Output>> _outputs {};
    vector<Track> _tracks {};
    float32_t _eventsTime {};
    bool _interpolationDisabled {};
};

template<typename T>
void ModelAnimationController::FindSrtValue(float32_t time, float32_t duration, bool reserved, const vector<float32_t>& times, const vector<T>& values, T& result)
{
    FO_RUNTIME_ASSERT(times.size() == values.size());
    FO_RUNTIME_ASSERT(!times.empty());

    if (reserved) {
        for (auto i = numeric_cast<int32_t>(times.size() - 1); i >= 0; i--) {
            if (i >= 1) {
                const auto rtime = duration - time;

                if (rtime <= times[i] && rtime > times[i - 1]) {
                    result = values[i];
                    const T& value = values[i - 1];
                    const auto factor = (rtime - times[i]) / (times[i] - times[i - 1]);
                    Interpolate(result, value, factor);
                    break;
                }
            }
            else {
                result = values[i];
                break;
            }
        }
    }
    else {
        for (size_t i = 0; i < times.size(); i++) {
            if (i + 1 < times.size()) {
                if (time >= times[i] && time < times[i + 1]) {
                    result = values[i];
                    const T& value = values[i + 1];
                    const auto factor = (time - times[i]) / (times[i + 1] - times[i]);
                    Interpolate(result, value, factor);
                    break;
                }
            }
            else {
                result = values[i];
                break;
            }
        }
    }
}

FO_END_NAMESPACE

#endif
