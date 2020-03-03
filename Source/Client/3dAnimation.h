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

#include "FileSystem.h"

class AnimSet : public NonCopyable
{
    friend class AnimController;

public:
    void Load(DataReader& reader);
    void SetData(const string& fname, const string& name, float ticks, float tps);
    void AddBoneOutput(HashVec hierarchy, const FloatVec& st, const VectorVec& sv, const FloatVec& rt,
        const QuaternionVec& rv, const FloatVec& tt, const VectorVec& tv);
    const string& GetFileName();
    const string& GetName();
    uint GetBoneOutputCount();
    float GetDuration();
    HashVecVec& GetBonesHierarchy();

private:
    struct BoneOutput
    {
        hash nameHash {};
        FloatVec scaleTime {};
        VectorVec scaleValue {};
        FloatVec rotationTime {};
        QuaternionVec rotationValue {};
        FloatVec translationTime {};
        VectorVec translationValue {};
    };

    string animFileName {};
    string animName {};
    float durationTicks {};
    float ticksPerSecond {};
    vector<BoneOutput> boneOutputs {};
    HashVecVec bonesHierarchy {};
};

class AnimController : public NonCopyable
{
public:
    AnimController(uint track_count);
    ~AnimController();
    AnimController* Clone();
    void RegisterAnimationOutput(hash bone_name_hash, Matrix& output_matrix);
    void RegisterAnimationSet(AnimSet* animation);
    AnimSet* GetAnimationSet(uint index);
    AnimSet* GetAnimationSetByName(const string& name);
    float GetTrackPosition(uint track);
    uint GetNumAnimationSets();
    void SetTrackAnimationSet(uint track, AnimSet* anim);
    void ResetBonesTransition(uint skip_track, const HashVec& bone_name_hashes);
    void Reset();
    float GetTime();
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
        hash nameHash {};
        Matrix* matrix {};
        BoolVec valid {};
        FloatVec factor {};
        VectorVec scale {};
        QuaternionVec rotation {};
        VectorVec translation {};
    };

    struct Track
    {
        struct Event
        {
            enum EType
            {
                Enable,
                Speed,
                Weight
            };

            EType type {};
            float valueTo {};
            float startTime {};
            float smoothTime {};
            float valueFrom {-1.0f};
        };

        bool enabled {};
        float speed {};
        float weight {};
        float position {};
        AnimSet* anim {};
        vector<Output*> animOutput {};
        vector<Event> events {};
    };

    void Interpolate(Quaternion& q1, const Quaternion& q2, float factor);
    void Interpolate(Vector& v1, const Vector& v2, float factor);

    template<class T>
    void FindSRTValue(float time, FloatVec& times, vector<T>& values, T& result)
    {
        for (uint n = 0, m = (uint)times.size(); n < m; n++)
        {
            if (n + 1 < m)
            {
                if (time >= times[n] && time < times[n + 1])
                {
                    result = values[n];
                    T& value = values[n + 1];
                    float factor = (time - times[n]) / (times[n + 1] - times[n]);
                    Interpolate(result, value, factor);
                    return;
                }
            }
            else
            {
                result = values[n];
            }
        }
    }

    bool cloned {};
    vector<AnimSet*>* sets {};
    vector<Output>* outputs {};
    vector<Track> tracks {};
    float curTime {};
    bool interpolationDisabled {};
};
