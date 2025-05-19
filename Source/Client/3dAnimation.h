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
        vector<float> ScaleTime {};
        vector<vec3> ScaleValue {};
        vector<float> RotationTime {};
        vector<quaternion> RotationValue {};
        vector<float> TranslationTime {};
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
    [[nodiscard]] auto GetDuration() const noexcept -> float { return _duration; }
    [[nodiscard]] auto GetBonesHierarchy() const noexcept -> const vector<vector<hstring>>& { return _bonesHierarchy; }

    void Load(DataReader& reader, HashResolver& hash_resolver);

private:
    string _animFileName {};
    string _animName {};
    float _duration {};
    vector<BoneOutput> _boneOutputs {};
    vector<vector<hstring>> _bonesHierarchy {};
};

class ModelAnimationController final
{
public:
    explicit ModelAnimationController(uint track_count);
    ModelAnimationController(const ModelAnimationController&) = delete;
    ModelAnimationController(ModelAnimationController&&) noexcept = default;
    auto operator=(const ModelAnimationController&) = delete;
    auto operator=(ModelAnimationController&&) noexcept = delete;
    ~ModelAnimationController() = default;

    [[nodiscard]] auto Clone() const -> unique_ptr<ModelAnimationController>;
    [[nodiscard]] auto GetAnimationSet(size_t index) const noexcept -> const ModelAnimation*;
    [[nodiscard]] auto GetAnimationSetByName(string_view name) const noexcept -> const ModelAnimation*;
    [[nodiscard]] auto GetTrackEnable(uint track) const noexcept -> bool;
    [[nodiscard]] auto GetTrackPosition(uint track) const noexcept -> float;
    [[nodiscard]] auto GetAnimationSetCount() const noexcept -> size_t;
    [[nodiscard]] auto GetTime() const noexcept -> float { return _curTime; }

    void Reset();
    void RegisterAnimationOutput(hstring bone_name, mat44& output_matrix);
    void RegisterAnimationSet(ModelAnimation* animation);
    void SetTrackAnimationSet(uint track, const ModelAnimation* anim, const unordered_set<hstring>* allowed_bones);
    void ResetBonesTransition(uint skip_track, const vector<hstring>& bone_names);
    void AddEventEnable(uint track, bool enable, float start_time);
    void AddEventSpeed(uint track, float speed, float start_time, float smooth_time);
    void AddEventWeight(uint track, float weight, float start_time, float smooth_time);
    void SetTrackEnable(uint track, bool enable);
    void SetTrackPosition(uint track, float position);
    void SetInterpolation(bool enabled);
    void AdvanceTime(float time);

private:
    struct Output
    {
        hstring BoneName {};
        mat44* Matrix {};
        vector<bool> Valid {};
        vector<float> Factor {};
        vector<vec3> Scale {};
        vector<quaternion> Rotation {};
        vector<vec3> Translation {};
    };

    struct Track
    {
        enum class EventType
        {
            Enable,
            Speed,
            Weight
        };

        struct Event
        {
            EventType Type {};
            float ValueTo {};
            float StartTime {};
            float SmoothTime {};
            float ValueFrom {-1.0f};
        };

        bool Enabled {};
        float Speed {};
        float Weight {};
        float Position {};
        const ModelAnimation* Anim {};
        vector<Output*> AnimOutput {};
        vector<Event> Events {};
    };

    void Interpolate(quaternion& q1, const quaternion& q2, float factor) const;
    void Interpolate(vec3& v1, const vec3& v2, float factor) const;

    template<class T>
    void FindSrtValue(float time, const vector<float>& times, const vector<T>& values, T& result)
    {
        for (size_t n = 0; n < times.size(); n++) {
            if (n + 1 < times.size()) {
                if (time >= times[n] && time < times[n + 1]) {
                    result = values[n];
                    const T& value = values[n + 1];
                    const auto factor = (time - times[n]) / (times[n + 1] - times[n]);
                    Interpolate(result, value, factor);
                    return;
                }
            }
            else {
                result = values[n];
            }
        }
    }

    bool _cloned {};
    shared_ptr<vector<ModelAnimation*>> _sets {};
    shared_ptr<vector<Output>> _outputs {};
    vector<Track> _tracks {};
    float _curTime {};
    bool _interpolationDisabled {};
};

FO_END_NAMESPACE();

#endif
