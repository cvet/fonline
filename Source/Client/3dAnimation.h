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

class AnimSet final
{
    friend class AnimController;

public:
    AnimSet() = default;
    AnimSet(const AnimSet&) = delete;
    AnimSet(AnimSet&&) noexcept = default;
    auto operator=(const AnimSet&) = delete;
    auto operator=(AnimSet&&) noexcept = delete;
    ~AnimSet() = default;

    void Load(DataReader& reader);
    void SetData(const string& fname, const string& name, float ticks, float tps);
    void AddBoneOutput(HashVec hierarchy, const FloatVec& st, const VectorVec& sv, const FloatVec& rt, const QuaternionVec& rv, const FloatVec& tt, const VectorVec& tv);
    [[nodiscard]] auto GetFileName() const -> const string&;
    [[nodiscard]] auto GetName() const -> const string&;
    [[nodiscard]] auto GetBoneOutputCount() const -> uint;
    [[nodiscard]] auto GetDuration() const -> float;
    [[nodiscard]] auto GetBonesHierarchy() const -> const HashVecVec&;

private:
    struct BoneOutput
    {
        hash NameHash {};
        FloatVec ScaleTime {};
        VectorVec ScaleValue {};
        FloatVec RotationTime {};
        QuaternionVec RotationValue {};
        FloatVec TranslationTime {};
        VectorVec TranslationValue {};
    };

    string _animFileName {};
    string _animName {};
    float _durationTicks {};
    float _ticksPerSecond {};
    vector<BoneOutput> _boneOutputs {};
    HashVecVec _bonesHierarchy {};
};

class AnimController final
{
public:
    explicit AnimController(uint track_count);
    AnimController(const AnimController&) = delete;
    AnimController(AnimController&&) noexcept = default;
    auto operator=(const AnimController&) = delete;
    auto operator=(AnimController&&) noexcept = delete;
    ~AnimController();

    [[nodiscard]] auto Clone() const -> AnimController*;
    void RegisterAnimationOutput(hash bone_name_hash, Matrix& output_matrix) const;
    void RegisterAnimationSet(AnimSet* animation) const;
    [[nodiscard]] auto GetAnimationSet(uint index) const -> AnimSet*;
    [[nodiscard]] auto GetAnimationSetByName(const string& name) const -> AnimSet*;
    [[nodiscard]] auto GetTrackPosition(uint track) const -> float;
    [[nodiscard]] auto GetNumAnimationSets() const -> uint;
    void SetTrackAnimationSet(uint track, AnimSet* anim);
    void ResetBonesTransition(uint skip_track, const HashVec& bone_name_hashes);
    void Reset();
    [[nodiscard]] auto GetTime() const -> float;
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
        Matrix* Matrix {};
        BoolVec Valid {};
        FloatVec Factor {};
        VectorVec Scale {};
        QuaternionVec Rotation {};
        VectorVec Translation {};
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
        AnimSet* Anim {};
        vector<Output*> AnimOutput {};
        vector<Event> Events {};
    };

    void Interpolate(Quaternion& q1, const Quaternion& q2, float factor) const;
    void Interpolate(Vector& v1, const Vector& v2, float factor) const;

    template<class T>
    void FindSrtValue(float time, FloatVec& times, vector<T>& values, T& result)
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
    vector<AnimSet*>* _sets {};
    vector<Output>* _outputs {};
    vector<Track> _tracks {};
    float _curTime {};
    bool _interpolationDisabled {};
};
