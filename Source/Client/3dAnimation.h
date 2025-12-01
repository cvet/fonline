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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE();

class ModelAnimation final
{
public:
    struct BoneOutput
    {
        hstring BoneName {};
        vector<float32> ScaleTime {};
        vector<vec3> ScaleValue {};
        vector<float32> RotationTime {};
        vector<quaternion> RotationValue {};
        vector<float32> TranslationTime {};
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
    [[nodiscard]] auto GetDuration() const noexcept -> float32 { return _duration; }
    [[nodiscard]] auto GetBonesHierarchy() const noexcept -> const vector<vector<hstring>>& { return _bonesHierarchy; }

    void Load(DataReader& reader, HashResolver& hash_resolver);

private:
    string _animFileName {};
    string _animName {};
    float32 _duration {};
    vector<BoneOutput> _boneOutputs {};
    vector<vector<hstring>> _bonesHierarchy {};
};

class ModelAnimationController final
{
public:
    explicit ModelAnimationController(int32 track_count);
    ModelAnimationController(const ModelAnimationController&) = delete;
    ModelAnimationController(ModelAnimationController&&) noexcept = default;
    auto operator=(const ModelAnimationController&) = delete;
    auto operator=(ModelAnimationController&&) noexcept = delete;
    ~ModelAnimationController() = default;

    [[nodiscard]] auto Copy() const -> unique_ptr<ModelAnimationController>;
    [[nodiscard]] auto GetAnimationBones(int32 index) const -> const vector<vector<hstring>>&;
    [[nodiscard]] auto GetAnimationDuration(int32 index) const -> float32;
    [[nodiscard]] auto GetTrackEnable(int32 track) const -> bool;
    [[nodiscard]] auto GetTrackPosition(int32 track) const -> float32;
    [[nodiscard]] auto GetTrackSpeed(int32 track) const -> float32;
    [[nodiscard]] auto GetAnimationsCount() const -> int32;

    void RegisterAnimationOutput(hstring bone_name, mat44& output_matrix);
    auto RegisterAnimation(ModelAnimation* animation, bool reversed) -> int32;
    void SetTrackAnimation(int32 track, int32 anim_index, const unordered_set<hstring>* allowed_bones);
    void ResetBonesTransition(int32 skip_track, const vector<hstring>& bone_names);
    void ResetEvents();
    void AddEventEnable(int32 track, bool enable, float32 start_time);
    void AddEventSpeed(int32 track, float32 speed, float32 start_time, float32 smooth_time);
    void AddEventWeight(int32 track, float32 weight, float32 start_time, float32 smooth_time);
    void SetTrackEnable(int32 track, bool enable);
    void SetTrackPosition(int32 track, float32 position);
    void SetInterpolation(bool enabled);
    void AdvanceTime(float32 time);

private:
    struct Output
    {
        hstring BoneName {};
        raw_ptr<mat44> Matrix {};
        vector<bool> Valid {};
        vector<float32> Factor {};
        vector<vec3> Scale {};
        vector<quaternion> Rotation {};
        vector<vec3> Translation {};
    };

    struct Track
    {
        enum class EventType : uint8
        {
            Enable,
            Speed,
            Weight
        };

        struct Event
        {
            EventType Type {};
            float32 ValueTo {};
            float32 StartTime {};
            float32 SmoothTime {};
            optional<float32> ValueFrom {};
        };

        bool Enabled {};
        float32 Speed {};
        float32 Weight {};
        float32 Position {};
        raw_ptr<const ModelAnimation> Anim {};
        bool Reversed {};
        vector<raw_ptr<Output>> AnimOutput {};
        vector<Event> Events {};
    };

    void Interpolate(quaternion& q1, const quaternion& q2, float32 factor) const;
    void Interpolate(vec3& v1, const vec3& v2, float32 factor) const;

    template<typename T>
    void FindSrtValue(float32 time, float32 duration, bool reserved, const vector<float32>& times, const vector<T>& values, T& result);

    shared_ptr<vector<pair<ModelAnimation*, bool>>> _anims {};
    shared_ptr<vector<Output>> _outputs {};
    vector<Track> _tracks {};
    float32 _eventsTime {};
    bool _interpolationDisabled {};
};

template<typename T>
void ModelAnimationController::FindSrtValue(float32 time, float32 duration, bool reserved, const vector<float32>& times, const vector<T>& values, T& result)
{
    FO_RUNTIME_ASSERT(times.size() == values.size());
    FO_RUNTIME_ASSERT(!times.empty());

    if (reserved) {
        for (auto i = numeric_cast<int32>(times.size() - 1); i >= 0; i--) {
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

FO_END_NAMESPACE();

#endif
