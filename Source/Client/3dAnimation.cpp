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

FO_BEGIN_NAMESPACE();

void ModelAnimation::Load(DataReader& reader, HashResolver& hash_resolver)
{
    FO_STACK_TRACE_ENTRY();

    string tmp;
    uint32 len = 0;

    reader.ReadPtr(&len, sizeof(len));
    _animFileName.resize(len);
    reader.ReadPtr(_animFileName.data(), len);
    reader.ReadPtr(&len, sizeof(len));
    _animName.resize(len);
    reader.ReadPtr(_animName.data(), len);
    reader.ReadPtr(&_duration, sizeof(_duration));
    reader.ReadPtr(&len, sizeof(len));

    _bonesHierarchy.resize(len);

    for (uint32 i = 0, j = len; i < j; i++) {
        reader.ReadPtr(&len, sizeof(len));
        _bonesHierarchy[i].resize(len);

        for (uint32 k = 0, l = len; k < l; k++) {
            reader.ReadPtr(&len, sizeof(len));
            tmp.resize(len);
            reader.ReadPtr(tmp.data(), len);
            _bonesHierarchy[i][k] = hash_resolver.ToHashedString(tmp);
        }
    }

    reader.ReadPtr(&len, sizeof(len));
    _boneOutputs.resize(len);

    for (uint32 i = 0, j = len; i < j; i++) {
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

ModelAnimationController::ModelAnimationController(int32 track_count)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(track_count >= 0);

    if (track_count != 0) {
        _anims = SafeAlloc::MakeShared<vector<pair<ModelAnimation*, bool>>>();
        _outputs = SafeAlloc::MakeShared<vector<Output>>();
        _tracks.resize(track_count);
    }
}

auto ModelAnimationController::Copy() const -> unique_ptr<ModelAnimationController>
{
    FO_STACK_TRACE_ENTRY();

    auto clone = SafeAlloc::MakeUnique<ModelAnimationController>(0);
    clone->_anims = _anims;
    clone->_outputs = _outputs;
    clone->_tracks.resize(_tracks.size());
    clone->_eventsTime = 0.0f;
    clone->_interpolationDisabled = _interpolationDisabled;
    return clone;
}

void ModelAnimationController::RegisterAnimationOutput(hstring bone_name, mat44& output_matrix)
{
    FO_STACK_TRACE_ENTRY();

    auto& o = _outputs->emplace_back();
    o.BoneName = bone_name;
    o.Matrix = &output_matrix;
    o.Valid.resize(_tracks.size());
    o.Factor.resize(_tracks.size());
    o.Scale.resize(_tracks.size());
    o.Rotation.resize(_tracks.size());
    o.Translation.resize(_tracks.size());
}

auto ModelAnimationController::RegisterAnimation(ModelAnimation* animation, bool reversed) -> int32
{
    FO_STACK_TRACE_ENTRY();

    _anims->emplace_back(animation, reversed);
    return numeric_cast<int32>(_anims->size() - 1);
}

auto ModelAnimationController::GetAnimationBones(int32 index) const -> const vector<vector<hstring>>&
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(index >= 0);
    FO_RUNTIME_ASSERT(index < numeric_cast<int32>(_anims->size()));

    return (*_anims)[index].first->GetBonesHierarchy();
}

auto ModelAnimationController::GetAnimationDuration(int32 index) const -> float32
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(index >= 0);
    FO_RUNTIME_ASSERT(index < numeric_cast<int32>(_anims->size()));

    return (*_anims)[index].first->GetDuration();
}

auto ModelAnimationController::GetTrackEnable(int32 track) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(track >= 0);
    FO_RUNTIME_ASSERT(track < numeric_cast<int32>(_tracks.size()));

    return _tracks[track].Enabled;
}

auto ModelAnimationController::GetTrackSpeed(int32 track) const -> float32
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(track >= 0);
    FO_RUNTIME_ASSERT(track < numeric_cast<int32>(_tracks.size()));

    return _tracks[track].Speed;
}

auto ModelAnimationController::GetTrackPosition(int32 track) const -> float32
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(track >= 0);
    FO_RUNTIME_ASSERT(track < numeric_cast<int32>(_tracks.size()));

    return _tracks[track].Position;
}

auto ModelAnimationController::GetAnimationsCount() const -> int32
{
    FO_STACK_TRACE_ENTRY();

    return numeric_cast<int32>(_anims->size());
}

void ModelAnimationController::SetTrackAnimation(int32 track, int32 anim_index, const unordered_set<hstring>* allowed_bones)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(track >= 0);
    FO_RUNTIME_ASSERT(track < numeric_cast<int32>(_tracks.size()));
    FO_RUNTIME_ASSERT(anim_index >= 0);
    FO_RUNTIME_ASSERT(anim_index < numeric_cast<int32>(_anims->size()));

    const auto* anim = (*_anims)[anim_index].first;
    const auto reversed = (*_anims)[anim_index].second;

    _tracks[track].Anim = anim;
    _tracks[track].Reversed = reversed;
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

void ModelAnimationController::ResetBonesTransition(int32 skip_track, const vector<hstring>& bone_names)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(skip_track >= 0);
    FO_RUNTIME_ASSERT(skip_track < numeric_cast<int32>(_tracks.size()));

    // Turn off fast transition bones on other tracks
    for (auto bone_name : bone_names) {
        for (size_t i = 0; i < _tracks.size(); i++) {
            if (i == numeric_cast<size_t>(skip_track)) {
                continue;
            }

            for (size_t j = 0; j < _tracks[i].AnimOutput.size(); j++) {
                if (_tracks[i].AnimOutput[j] && _tracks[i].AnimOutput[j]->BoneName == bone_name) {
                    _tracks[i].AnimOutput[j]->Valid[i] = false;
                    _tracks[i].AnimOutput[j] = nullptr;
                }
            }
        }
    }
}

void ModelAnimationController::ResetEvents()
{
    FO_STACK_TRACE_ENTRY();

    _eventsTime = 0.0f;

    for (auto& t : _tracks) {
        t.Events.clear();
    }
}

void ModelAnimationController::AddEventEnable(int32 track, bool enable, float32 start_time)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(track >= 0);
    FO_RUNTIME_ASSERT(track < numeric_cast<int32>(_tracks.size()));

    _tracks[track].Events.emplace_back(Track::Event {.Type = Track::EventType::Enable, .ValueTo = enable ? 1.0f : -1.0f, .StartTime = start_time, .SmoothTime = 0.0f});
}

void ModelAnimationController::AddEventSpeed(int32 track, float32 speed, float32 start_time, float32 smooth_time)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(track >= 0);
    FO_RUNTIME_ASSERT(track < numeric_cast<int32>(_tracks.size()));

    _tracks[track].Events.emplace_back(Track::Event {.Type = Track::EventType::Speed, .ValueTo = speed, .StartTime = start_time, .SmoothTime = smooth_time});
}

void ModelAnimationController::AddEventWeight(int32 track, float32 weight, float32 start_time, float32 smooth_time)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(track >= 0);
    FO_RUNTIME_ASSERT(track < numeric_cast<int32>(_tracks.size()));

    _tracks[track].Events.emplace_back(Track::Event {.Type = Track::EventType::Weight, .ValueTo = weight, .StartTime = start_time, .SmoothTime = smooth_time});
}

void ModelAnimationController::SetTrackEnable(int32 track, bool enable)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(track >= 0);
    FO_RUNTIME_ASSERT(track < numeric_cast<int32>(_tracks.size()));

    _tracks[track].Enabled = enable;
}

void ModelAnimationController::SetTrackPosition(int32 track, float32 position)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(track >= 0);
    FO_RUNTIME_ASSERT(track < numeric_cast<int32>(_tracks.size()));

    _tracks[track].Position = position;
}

void ModelAnimationController::SetInterpolation(bool enabled)
{
    FO_STACK_TRACE_ENTRY();

    _interpolationDisabled = !enabled;
}

void ModelAnimationController::AdvanceTime(float32 time)
{
    FO_STACK_TRACE_ENTRY();

    _eventsTime += time;

    for (auto& track : _tracks) {
        for (auto it = track.Events.begin(); it != track.Events.end();) {
            auto& event = *it;

            if (_eventsTime >= event.StartTime) {
                bool erase = false;
                float32 value;

                if (_eventsTime < event.StartTime + event.SmoothTime) {
                    FO_RUNTIME_ASSERT(event.SmoothTime > 0.0f);

                    if (!event.ValueFrom.has_value()) {
                        if (event.Type == Track::EventType::Speed) {
                            event.ValueFrom = track.Speed;
                        }
                        else if (event.Type == Track::EventType::Weight) {
                            event.ValueFrom = track.Weight;
                        }
                        else {
                            event.ValueFrom = 0.0f;
                        }
                    }

                    value = lerp(event.ValueFrom.value(), event.ValueTo, (_eventsTime - event.StartTime) / event.SmoothTime);
                }
                else {
                    erase = true;
                    value = event.ValueTo;
                }

                if (event.Type == Track::EventType::Enable) {
                    track.Enabled = value > 0.0f;
                }
                else if (event.Type == Track::EventType::Speed) {
                    track.Speed = value;
                }
                else if (event.Type == Track::EventType::Weight) {
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

        if (!track.Enabled || track.Weight <= 0.0f || !track.Anim) {
            continue;
        }

        const auto& anim_outputs = track.Anim->GetBoneOutputs();

        for (size_t j = 0; j < anim_outputs.size(); j++) {
            if (!track.AnimOutput[j]) {
                continue;
            }

            const auto& anim_output = anim_outputs[j];
            const auto duration = track.Anim->GetDuration();
            const auto pos = std::fmod(track.Position, duration);

            FindSrtValue<vec3>(pos, duration, track.Reversed, anim_output.ScaleTime, anim_output.ScaleValue, track.AnimOutput[j]->Scale[i]);
            FindSrtValue<quaternion>(pos, duration, track.Reversed, anim_output.RotationTime, anim_output.RotationValue, track.AnimOutput[j]->Rotation[i]);
            FindSrtValue<vec3>(pos, duration, track.Reversed, anim_output.TranslationTime, anim_output.TranslationValue, track.AnimOutput[j]->Translation[i]);

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

void ModelAnimationController::Interpolate(quaternion& q1, const quaternion& q2, float32 factor) const
{
    FO_STACK_TRACE_ENTRY();

    if (!_interpolationDisabled) {
        quaternion::Interpolate(q1, q1, q2, factor);
    }
    else if (factor >= 0.5f) {
        q1 = q2;
    }
}

void ModelAnimationController::Interpolate(vec3& v1, const vec3& v2, float32 factor) const
{
    FO_STACK_TRACE_ENTRY();

    if (!_interpolationDisabled) {
        v1.x = v1.x + (v2.x - v1.x) * factor;
        v1.y = v1.y + (v2.y - v1.y) * factor;
        v1.z = v1.z + (v2.z - v1.z) * factor;
    }
    else if (factor >= 0.5f) {
        v1 = v2;
    }
}

FO_END_NAMESPACE();

#endif
