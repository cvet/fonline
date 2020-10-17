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

#include "3dAnimation.h"
#include "GenericUtils.h"

void ModelAnimation::Load(DataReader& reader)
{
    uint len = 0;
    reader.ReadPtr(&len, sizeof(len));
    _animFileName.resize(len);
    reader.ReadPtr(&_animFileName[0], len);
    reader.ReadPtr(&len, sizeof(len));
    _animName.resize(len);
    reader.ReadPtr(&_animName[0], len);
    reader.ReadPtr(&_durationTicks, sizeof(_durationTicks));
    reader.ReadPtr(&_ticksPerSecond, sizeof(_ticksPerSecond));
    reader.ReadPtr(&len, sizeof(len));

    _bonesHierarchy.resize(len);
    for (uint i = 0, j = len; i < j; i++) {
        reader.ReadPtr(&len, sizeof(len));
        _bonesHierarchy[i].resize(len);
        reader.ReadPtr(&_bonesHierarchy[i][0], len * sizeof(_bonesHierarchy[0][0]));
    }

    reader.ReadPtr(&len, sizeof(len));
    _boneOutputs.resize(len);

    for (uint i = 0, j = len; i < j; i++) {
        auto& o = _boneOutputs[i];
        reader.ReadPtr(&o.NameHash, sizeof(o.NameHash));
        reader.ReadPtr(&len, sizeof(len));
        o.ScaleTime.resize(len);
        o.ScaleValue.resize(len);
        reader.ReadPtr(&o.ScaleTime[0], len * sizeof(o.ScaleTime[0]));
        reader.ReadPtr(&o.ScaleValue[0], len * sizeof(o.ScaleValue[0]));
        reader.ReadPtr(&len, sizeof(len));
        o.RotationTime.resize(len);
        o.RotationValue.resize(len);
        reader.ReadPtr(&o.RotationTime[0], len * sizeof(o.RotationTime[0]));
        reader.ReadPtr(&o.RotationValue[0], len * sizeof(o.RotationValue[0]));
        reader.ReadPtr(&len, sizeof(len));
        o.TranslationTime.resize(len);
        o.TranslationValue.resize(len);
        reader.ReadPtr(&o.TranslationTime[0], len * sizeof(o.TranslationTime[0]));
        reader.ReadPtr(&o.TranslationValue[0], len * sizeof(o.TranslationValue[0]));
    }
}

void ModelAnimation::SetData(const string& fname, const string& name, float ticks, float tps)
{
    _animFileName = fname;
    _animName = name;
    _durationTicks = ticks;
    _ticksPerSecond = tps;
}

void ModelAnimation::AddBoneOutput(vector<hash> hierarchy, const vector<float>& st, const vector<vec3>& sv, const vector<float>& rt, const vector<quaternion>& rv, const vector<float>& tt, const vector<vec3>& tv)
{
    auto& o = _boneOutputs.emplace_back();
    o.NameHash = hierarchy.back();
    o.ScaleTime = st;
    o.ScaleValue = sv;
    o.RotationTime = rt;
    o.RotationValue = rv;
    o.TranslationTime = tt;
    o.TranslationValue = tv;
    _bonesHierarchy.push_back(hierarchy);
}

auto ModelAnimation::GetFileName() const -> const string&
{
    return _animFileName;
}

auto ModelAnimation::GetName() const -> const string&
{
    return _animName;
}

auto ModelAnimation::GetBoneOutputCount() const -> uint
{
    return static_cast<uint>(_boneOutputs.size());
}

auto ModelAnimation::GetDuration() const -> float
{
    return _durationTicks / _ticksPerSecond;
}

auto ModelAnimation::GetBonesHierarchy() const -> const vector<vector<hash>>&
{
    return _bonesHierarchy;
}

ModelAnimationController::ModelAnimationController(uint track_count)
{
    if (track_count != 0u) {
        _sets = new vector<ModelAnimation*>();
        _outputs = new vector<Output>();
        _tracks.resize(track_count);
    }
}

ModelAnimationController::~ModelAnimationController()
{
    if (!_cloned) {
        delete _sets;
        delete _outputs;
    }
}

auto ModelAnimationController::Clone() const -> ModelAnimationController*
{
    auto* clone = new ModelAnimationController(0);
    clone->_cloned = true;
    clone->_sets = _sets;
    clone->_outputs = _outputs;
    clone->_tracks = _tracks;
    clone->_curTime = 0.0f;
    clone->_interpolationDisabled = _interpolationDisabled;
    return clone;
}

void ModelAnimationController::RegisterAnimationOutput(hash bone_name_hash, mat44& output_matrix)
{
    NON_CONST_METHOD_HINT(_cloned);

    auto& o = _outputs->emplace_back();
    o.NameHash = bone_name_hash;
    o.Matrix = &output_matrix;
    o.Valid.resize(_tracks.size());
    o.Factor.resize(_tracks.size());
    o.Scale.resize(_tracks.size());
    o.Rotation.resize(_tracks.size());
    o.Translation.resize(_tracks.size());
}

void ModelAnimationController::RegisterAnimationSet(ModelAnimation* animation)
{
    NON_CONST_METHOD_HINT(_cloned);

    _sets->push_back(animation);
}

auto ModelAnimationController::GetAnimationSet(uint index) const -> ModelAnimation*
{
    if (index >= _sets->size()) {
        return nullptr;
    }
    return (*_sets)[index];
}

auto ModelAnimationController::GetAnimationSetByName(const string& name) const -> ModelAnimation*
{
    for (auto& s : *_sets) {
        if (s->_animName == name) {
            return s;
        }
    }
    return nullptr;
}

auto ModelAnimationController::GetTrackPosition(uint track) const -> float
{
    return _tracks[track].Position;
}

auto ModelAnimationController::GetNumAnimationSets() const -> uint
{
    return static_cast<uint>(_sets->size());
}

void ModelAnimationController::SetTrackAnimationSet(uint track, ModelAnimation* anim)
{
    _tracks[track].Anim = anim;
    const auto count = anim->GetBoneOutputCount();
    _tracks[track].AnimOutput.resize(count);
    for (uint i = 0; i < count; i++) {
        const auto link_name_hash = anim->_boneOutputs[i].NameHash;
        Output* output = nullptr;
        for (auto& o : *_outputs) {
            if (o.NameHash == link_name_hash) {
                output = &o;
                break;
            }
        }
        _tracks[track].AnimOutput[i] = output;
    }
}

void ModelAnimationController::ResetBonesTransition(uint skip_track, const vector<hash>& bone_name_hashes)
{
    // Turn off fast transition bones on other tracks
    for (auto bone_name_hash : bone_name_hashes) {
        for (uint i = 0, j = static_cast<uint>(_tracks.size()); i < j; i++) {
            if (i == skip_track) {
                continue;
            }

            for (uint k = 0, l = static_cast<uint>(_tracks[i].AnimOutput.size()); k < l; k++) {
                if (_tracks[i].AnimOutput[k] != nullptr && _tracks[i].AnimOutput[k]->NameHash == bone_name_hash) {
                    _tracks[i].AnimOutput[k]->Valid[i] = false;
                    _tracks[i].AnimOutput[k] = nullptr;
                }
            }
        }
    }
}

void ModelAnimationController::Reset()
{
    _curTime = 0.0f;

    for (auto& t : _tracks) {
        t.Events.clear();
    }
}

auto ModelAnimationController::GetTime() const -> float
{
    return _curTime;
}

void ModelAnimationController::AddEventEnable(uint track, bool enable, float start_time)
{
    _tracks[track].Events.push_back({Track::EventType::Enable, enable ? 1.0f : -1.0f, start_time, 0.0f});
}

void ModelAnimationController::AddEventSpeed(uint track, float speed, float start_time, float smooth_time)
{
    _tracks[track].Events.push_back({Track::EventType::Speed, speed, start_time, smooth_time});
}

void ModelAnimationController::AddEventWeight(uint track, float weight, float start_time, float smooth_time)
{
    _tracks[track].Events.push_back({Track::EventType::Weight, weight, start_time, smooth_time});
}

void ModelAnimationController::SetTrackEnable(uint track, bool enable)
{
    _tracks[track].Enabled = enable;
}

void ModelAnimationController::SetTrackPosition(uint track, float position)
{
    _tracks[track].Position = position;
}

void ModelAnimationController::SetInterpolation(bool enabled)
{
    _interpolationDisabled = !enabled;
}

void ModelAnimationController::AdvanceTime(float time)
{
    // Animation time
    _curTime += time;

    // Track events
    for (auto& track : _tracks) {
        // Events
        for (auto it = track.Events.begin(); it != track.Events.end();) {
            auto& e = *it;
            if (_curTime >= e.StartTime) {
                if (e.SmoothTime > 0.0f && Math::FloatCompare(e.ValueFrom, -1.0f)) {
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
    for (uint i = 0, j = static_cast<uint>(_tracks.size()); i < j; i++) {
        auto& track = _tracks[i];

        for (auto& o : *_outputs) {
            o.Valid[i] = false;
        }

        if (!track.Enabled || track.Weight <= 0.0f || track.Anim == nullptr) {
            continue;
        }

        for (uint k = 0, l = static_cast<uint>(track.Anim->_boneOutputs.size()); k < l; k++) {
            if (track.AnimOutput[k] == nullptr) {
                continue;
            }

            auto& o = track.Anim->_boneOutputs[k];

            auto t = fmod(track.Position * track.Anim->_ticksPerSecond, track.Anim->_durationTicks);

            FindSrtValue<vec3>(t, o.ScaleTime, o.ScaleValue, track.AnimOutput[k]->Scale[i]);
            FindSrtValue<quaternion>(t, o.RotationTime, o.RotationValue, track.AnimOutput[k]->Rotation[i]);
            FindSrtValue<vec3>(t, o.TranslationTime, o.TranslationValue, track.AnimOutput[k]->Translation[i]);

            track.AnimOutput[k]->Valid[i] = true;
            track.AnimOutput[k]->Factor[i] = track.Weight;
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
            for (uint k = 0, l = static_cast<uint>(_tracks.size()); k < l; k++) {
                if (o.Valid[k]) {
                    mat44 ms;
                    mat44 mr;
                    mat44 mt;
                    mat44::Scaling(o.Scale[k], ms);
                    mr = mat44(o.Rotation[k].GetMatrix());
                    mat44::Translation(o.Translation[k], mt);
                    *o.Matrix = mt * mr * ms;
                    break;
                }
            }
        }
    }
}

void ModelAnimationController::Interpolate(quaternion& q1, const quaternion& q2, float factor) const
{
    if (!_interpolationDisabled) {
        quaternion::Interpolate(q1, q1, q2, factor);
    }
    else if (factor >= 0.5f) {
        q1 = q2;
    }
}

void ModelAnimationController::Interpolate(vec3& v1, const vec3& v2, float factor) const
{
    if (!_interpolationDisabled) {
        v1.x = v1.x + (v2.x - v1.x) * factor;
        v1.y = v1.y + (v2.y - v1.y) * factor;
        v1.z = v1.z + (v2.z - v1.z) * factor;
    }
    else if (factor >= 0.5f) {
        v1 = v2;
    }
}
