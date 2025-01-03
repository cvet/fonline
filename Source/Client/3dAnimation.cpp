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

#include "3dAnimation.h"

#if FO_ENABLE_3D

void ModelAnimation::Load(DataReader& reader, HashResolver& hash_resolver)
{
    STACK_TRACE_ENTRY();

    string tmp;
    uint len = 0;

    reader.ReadPtr(&len, sizeof(len));
    _animFileName.resize(len);
    reader.ReadPtr(_animFileName.data(), len);
    reader.ReadPtr(&len, sizeof(len));
    _animName.resize(len);
    reader.ReadPtr(_animName.data(), len);
    reader.ReadPtr(&_duration, sizeof(_duration));
    reader.ReadPtr(&len, sizeof(len));

    _bonesHierarchy.resize(len);

    for (uint i = 0, j = len; i < j; i++) {
        reader.ReadPtr(&len, sizeof(len));
        _bonesHierarchy[i].resize(len);

        for (uint k = 0, l = len; k < l; k++) {
            reader.ReadPtr(&len, sizeof(len));
            tmp.resize(len);
            reader.ReadPtr(tmp.data(), len);
            _bonesHierarchy[i][k] = hash_resolver.ToHashedString(tmp);
        }
    }

    reader.ReadPtr(&len, sizeof(len));
    _boneOutputs.resize(len);

    for (uint i = 0, j = len; i < j; i++) {
        auto& o = _boneOutputs[i];
        reader.ReadPtr(&len, sizeof(len));
        tmp.resize(len);
        reader.ReadPtr(tmp.data(), len);
        o.BoneName = hash_resolver.ToHashedString(tmp);
        reader.ReadPtr(&len, sizeof(len));
        o.ScaleTime.resize(len);
        o.ScaleValue.resize(len);
        reader.ReadPtr(o.ScaleTime.data(), len * sizeof(o.ScaleTime[0]));
        reader.ReadPtr(o.ScaleValue.data(), len * sizeof(o.ScaleValue[0]));
        reader.ReadPtr(&len, sizeof(len));
        o.RotationTime.resize(len);
        o.RotationValue.resize(len);
        reader.ReadPtr(o.RotationTime.data(), len * sizeof(o.RotationTime[0]));
        reader.ReadPtr(o.RotationValue.data(), len * sizeof(o.RotationValue[0]));
        reader.ReadPtr(&len, sizeof(len));
        o.TranslationTime.resize(len);
        o.TranslationValue.resize(len);
        reader.ReadPtr(o.TranslationTime.data(), len * sizeof(o.TranslationTime[0]));
        reader.ReadPtr(o.TranslationValue.data(), len * sizeof(o.TranslationValue[0]));
    }
}

auto ModelAnimation::GetFileName() const noexcept -> string_view
{
    NO_STACK_TRACE_ENTRY();

    return _animFileName;
}

auto ModelAnimation::GetName() const noexcept -> string_view
{
    NO_STACK_TRACE_ENTRY();

    return _animName;
}

auto ModelAnimation::GetBoneOutputs() const noexcept -> const vector<BoneOutput>&
{
    NO_STACK_TRACE_ENTRY();

    return _boneOutputs;
}

auto ModelAnimation::GetDuration() const noexcept -> float
{
    NO_STACK_TRACE_ENTRY();

    return _duration;
}

auto ModelAnimation::GetBonesHierarchy() const noexcept -> const vector<vector<hstring>>&
{
    NO_STACK_TRACE_ENTRY();

    return _bonesHierarchy;
}

ModelAnimationController::ModelAnimationController(uint track_count)
{
    STACK_TRACE_ENTRY();

    if (track_count != 0u) {
        _sets = SafeAlloc::MakeRaw<vector<ModelAnimation*>>();
        _outputs = SafeAlloc::MakeRaw<vector<Output>>();
        _tracks.resize(track_count);
    }
}

ModelAnimationController::~ModelAnimationController()
{
    STACK_TRACE_ENTRY();

    if (!_cloned) {
        delete _sets;
        delete _outputs;
    }
}

auto ModelAnimationController::Clone() const -> ModelAnimationController*
{
    STACK_TRACE_ENTRY();

    auto* clone = SafeAlloc::MakeRaw<ModelAnimationController>(0);
    clone->_cloned = true;
    clone->_sets = _sets;
    clone->_outputs = _outputs;
    clone->_tracks = _tracks;
    clone->_curTime = 0.0f;
    clone->_interpolationDisabled = _interpolationDisabled;
    return clone;
}

void ModelAnimationController::RegisterAnimationOutput(hstring bone_name, mat44& output_matrix)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto& o = _outputs->emplace_back();
    o.BoneName = bone_name;
    o.Matrix = &output_matrix;
    o.Valid.resize(_tracks.size());
    o.Factor.resize(_tracks.size());
    o.Scale.resize(_tracks.size());
    o.Rotation.resize(_tracks.size());
    o.Translation.resize(_tracks.size());
}

void ModelAnimationController::RegisterAnimationSet(ModelAnimation* animation)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _sets->emplace_back(animation);
}

auto ModelAnimationController::GetAnimationSet(uint index) const noexcept -> const ModelAnimation*
{
    NO_STACK_TRACE_ENTRY();

    if (index >= _sets->size()) {
        return nullptr;
    }

    return (*_sets)[index];
}

auto ModelAnimationController::GetAnimationSetByName(string_view name) const noexcept -> const ModelAnimation*
{
    NO_STACK_TRACE_ENTRY();

    for (const auto* set : *_sets) {
        if (set->GetName() == name) {
            return set;
        }
    }

    return nullptr;
}

auto ModelAnimationController::GetTrackEnable(uint track) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _tracks[track].Enabled;
}

auto ModelAnimationController::GetTrackPosition(uint track) const noexcept -> float
{
    NO_STACK_TRACE_ENTRY();

    return _tracks[track].Position;
}

auto ModelAnimationController::GetNumAnimationSets() const noexcept -> uint
{
    NO_STACK_TRACE_ENTRY();

    return static_cast<uint>(_sets->size());
}

void ModelAnimationController::SetTrackAnimationSet(uint track, const ModelAnimation* anim, const unordered_set<hstring>* allowed_bones)
{
    STACK_TRACE_ENTRY();

    _tracks[track].Anim = anim;
    const auto& outputs = anim->GetBoneOutputs();
    _tracks[track].AnimOutput.resize(outputs.size());

    for (size_t i = 0; i < outputs.size(); i++) {
        const auto link_name = outputs[i].BoneName;
        Output* output = nullptr;

        if (allowed_bones == nullptr || allowed_bones->count(link_name) != 0) {
            for (auto& o : *_outputs) {
                if (o.BoneName == link_name) {
                    output = &o;
                    break;
                }
            }
        }

        _tracks[track].AnimOutput[i] = output;
    }
}

void ModelAnimationController::ResetBonesTransition(uint skip_track, const vector<hstring>& bone_names)
{
    STACK_TRACE_ENTRY();

    // Turn off fast transition bones on other tracks
    for (auto bone_name : bone_names) {
        for (uint i = 0, j = static_cast<uint>(_tracks.size()); i < j; i++) {
            if (i == skip_track) {
                continue;
            }

            for (uint k = 0, l = static_cast<uint>(_tracks[i].AnimOutput.size()); k < l; k++) {
                if (_tracks[i].AnimOutput[k] != nullptr && _tracks[i].AnimOutput[k]->BoneName == bone_name) {
                    _tracks[i].AnimOutput[k]->Valid[i] = false;
                    _tracks[i].AnimOutput[k] = nullptr;
                }
            }
        }
    }
}

void ModelAnimationController::Reset()
{
    STACK_TRACE_ENTRY();

    _curTime = 0.0f;

    for (auto& t : _tracks) {
        t.Events.clear();
    }
}

auto ModelAnimationController::GetTime() const noexcept -> float
{
    NO_STACK_TRACE_ENTRY();

    return _curTime;
}

void ModelAnimationController::AddEventEnable(uint track, bool enable, float start_time)
{
    STACK_TRACE_ENTRY();

    _tracks[track].Events.emplace_back(Track::Event {Track::EventType::Enable, enable ? 1.0f : -1.0f, start_time, 0.0f});
}

void ModelAnimationController::AddEventSpeed(uint track, float speed, float start_time, float smooth_time)
{
    STACK_TRACE_ENTRY();

    _tracks[track].Events.emplace_back(Track::Event {Track::EventType::Speed, speed, start_time, smooth_time});
}

void ModelAnimationController::AddEventWeight(uint track, float weight, float start_time, float smooth_time)
{
    STACK_TRACE_ENTRY();

    _tracks[track].Events.emplace_back(Track::Event {Track::EventType::Weight, weight, start_time, smooth_time});
}

void ModelAnimationController::SetTrackEnable(uint track, bool enable)
{
    STACK_TRACE_ENTRY();

    _tracks[track].Enabled = enable;
}

void ModelAnimationController::SetTrackPosition(uint track, float position)
{
    STACK_TRACE_ENTRY();

    _tracks[track].Position = position;
}

void ModelAnimationController::SetInterpolation(bool enabled)
{
    STACK_TRACE_ENTRY();

    _interpolationDisabled = !enabled;
}

void ModelAnimationController::AdvanceTime(float time)
{
    STACK_TRACE_ENTRY();

    // Animation time
    _curTime += time;

    // Track events
    for (auto& track : _tracks) {
        // Events
        for (auto it = track.Events.begin(); it != track.Events.end();) {
            auto& e = *it;
            if (_curTime >= e.StartTime) {
                if (e.SmoothTime > 0.0f && is_float_equal(e.ValueFrom, -1.0f)) {
                    if (e.Type == Track::EventType::Speed) {
                        e.ValueFrom = track.Speed;
                    }
                    else if (e.Type == Track::EventType::Weight) {
                        e.ValueFrom = track.Weight;
                    }
                }

                auto erase = false;
                auto value = e.ValueTo;
                if (_curTime < e.StartTime + e.SmoothTime) {
                    if (e.ValueTo > e.ValueFrom) {
                        value = e.ValueFrom + (e.ValueTo - e.ValueFrom) / e.SmoothTime * (_curTime - e.StartTime);
                    }
                    else {
                        value = e.ValueFrom - (e.ValueFrom - e.ValueTo) / e.SmoothTime * (_curTime - e.StartTime);
                    }
                }
                else {
                    erase = true;
                }

                if (e.Type == Track::EventType::Enable) {
                    track.Enabled = value > 0.0f;
                }
                else if (e.Type == Track::EventType::Speed) {
                    track.Speed = value;
                }
                else if (e.Type == Track::EventType::Weight) {
                    track.Weight = value;
                }

                if (erase) {
                    it = track.Events.erase(it);
                    continue;
                }
            }

            ++it;
        }

        // Add track time
        if (track.Enabled) {
            track.Position += time * track.Speed;
        }
    }

    // Track animation
    for (size_t i = 0; i < _tracks.size(); i++) {
        auto& track = _tracks[i];

        for (auto& o : *_outputs) {
            o.Valid[i] = false;
        }

        if (!track.Enabled || track.Weight <= 0.0f || track.Anim == nullptr) {
            continue;
        }

        const auto& anim_outputs = track.Anim->GetBoneOutputs();

        for (size_t j = 0; j < anim_outputs.size(); j++) {
            if (track.AnimOutput[j] == nullptr) {
                continue;
            }

            const auto& anim_output = anim_outputs[j];
            const auto t = std::fmod(track.Position, track.Anim->GetDuration());

            FindSrtValue<vec3>(t, anim_output.ScaleTime, anim_output.ScaleValue, track.AnimOutput[j]->Scale[i]);
            FindSrtValue<quaternion>(t, anim_output.RotationTime, anim_output.RotationValue, track.AnimOutput[j]->Rotation[i]);
            FindSrtValue<vec3>(t, anim_output.TranslationTime, anim_output.TranslationValue, track.AnimOutput[j]->Translation[i]);

            track.AnimOutput[j]->Valid[i] = true;
            track.AnimOutput[j]->Factor[i] = track.Weight;
        }
    }

    // Blend tracks
    for (auto& o : *_outputs) {
        // Todo: add interpolation for tracks more than two
        if (_tracks.size() >= 2 && o.Valid[0] && o.Valid[1]) {
            auto factor = o.Factor[1];
            Interpolate(o.Scale[0], o.Scale[1], factor);
            Interpolate(o.Rotation[0], o.Rotation[1], factor);
            Interpolate(o.Translation[0], o.Translation[1], factor);
            mat44 ms;
            mat44 mr;
            mat44 mt;
            mat44::Scaling(o.Scale[0], ms);
            mr = mat44(o.Rotation[0].GetMatrix());
            mat44::Translation(o.Translation[0], mt);
            *o.Matrix = mt * mr * ms;
        }
        else {
            for (size_t i = 0; i < _tracks.size(); i++) {
                if (o.Valid[i]) {
                    mat44 ms;
                    mat44 mr;
                    mat44 mt;
                    mat44::Scaling(o.Scale[i], ms);
                    mr = mat44(o.Rotation[i].GetMatrix());
                    mat44::Translation(o.Translation[i], mt);
                    *o.Matrix = mt * mr * ms;
                    break;
                }
            }
        }
    }
}

void ModelAnimationController::Interpolate(quaternion& q1, const quaternion& q2, float factor) const
{
    STACK_TRACE_ENTRY();

    if (!_interpolationDisabled) {
        quaternion::Interpolate(q1, q1, q2, factor);
    }
    else if (factor >= 0.5f) {
        q1 = q2;
    }
}

void ModelAnimationController::Interpolate(vec3& v1, const vec3& v2, float factor) const
{
    STACK_TRACE_ENTRY();

    if (!_interpolationDisabled) {
        v1.x = v1.x + (v2.x - v1.x) * factor;
        v1.y = v1.y + (v2.y - v1.y) * factor;
        v1.z = v1.z + (v2.z - v1.z) * factor;
    }
    else if (factor >= 0.5f) {
        v1 = v2;
    }
}

#endif
