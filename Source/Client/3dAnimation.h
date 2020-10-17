//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

class ModelAnimation final
{
    friend class ModelAnimationController;

public:
    ModelAnimation() = default;
    ModelAnimation(const ModelAnimation&) = delete;
    ModelAnimation(ModelAnimation&&) noexcept = default;
    auto operator=(const ModelAnimation&) = delete;
    auto operator=(ModelAnimation&&) noexcept = delete;
    ~ModelAnimation() = default;

    [[nodiscard]] auto GetFileName() const -> const string&;
    [[nodiscard]] auto GetName() const -> const string&;
    [[nodiscard]] auto GetBoneOutputCount() const -> uint;
    [[nodiscard]] auto GetDuration() const -> float;
    [[nodiscard]] auto GetBonesHierarchy() const -> const vector<vector<hash>>&;

    void Load(DataReader& reader);
    void SetData(const string& fname, const string& name, float ticks, float tps);
    void AddBoneOutput(vector<hash> hierarchy, const vector<float>& st, const vector<vec3>& sv, const vector<float>& rt, const vector<quaternion>& rv, const vector<float>& tt, const vector<vec3>& tv);

private:
    struct BoneOutput
    {
        hash NameHash {};
        vector<float> ScaleTime {};
        vector<vec3> ScaleValue {};
        vector<float> RotationTime {};
        vector<quaternion> RotationValue {};
        vector<float> TranslationTime {};
        vector<vec3> TranslationValue {};
    };

    string _animFileName {};
    string _animName {};
    float _durationTicks {};
    float _ticksPerSecond {};
    vector<BoneOutput> _boneOutputs {};
    vector<vector<hash>> _bonesHierarchy {};
};

class ModelAnimationController final
{
public:
    explicit ModelAnimationController(uint track_count);
    ModelAnimationController(const ModelAnimationController&) = delete;
    ModelAnimationController(ModelAnimationController&&) noexcept = default;
    auto operator=(const ModelAnimationController&) = delete;
    auto operator=(ModelAnimationController&&) noexcept = delete;
    ~ModelAnimationController();

    [[nodiscard]] auto Clone() const -> ModelAnimationController*;
    [[nodiscard]] auto GetAnimationSet(uint index) const -> ModelAnimation*;
    [[nodiscard]] auto GetAnimationSetByName(const string& name) const -> ModelAnimation*;
    [[nodiscard]] auto GetTrackPosition(uint track) const -> float;
    [[nodiscard]] auto GetNumAnimationSets() const -> uint;
    [[nodiscard]] auto GetTime() const -> float;

    void Reset();
    void RegisterAnimationOutput(hash bone_name_hash, mat44& output_matrix);
    void RegisterAnimationSet(ModelAnimation* animation);
    void SetTrackAnimationSet(uint track, ModelAnimation* anim);
    void ResetBonesTransition(uint skip_track, const vector<hash>& bone_name_hashes);
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
        hash NameHash {};
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
        ModelAnimation* Anim {};
        vector<Output*> AnimOutput {};
        vector<Event> Events {};
    };

    void Interpolate(quaternion& q1, const quaternion& q2, float factor) const;
    void Interpolate(vec3& v1, const vec3& v2, float factor) const;

    template<class T>
    void FindSrtValue(float time, vector<float>& times, vector<T>& values, T& result)
    {
        for (size_t n = 0; n < times.size(); n++) {
            if (n + 1 < times.size()) {
                if (time >= times[n] && time < times[n + 1]) {
                    result = values[n];
                    T& value = values[n + 1];
                    auto factor = (time - times[n]) / (times[n + 1] - times[n]);
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
    vector<ModelAnimation*>* _sets {};
    vector<Output>* _outputs {};
    vector<Track> _tracks {};
    float _curTime {};
    bool _interpolationDisabled {};
};
