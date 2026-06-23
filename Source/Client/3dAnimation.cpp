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

#include "3dAnimation.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

void ModelAnimation::Load(DataReader& reader, HashResolver& hash_resolver)
{
    FO_STACK_TRACE_ENTRY();

    string tmp;
    uint32_t len = 0;

    reader.ReadPtr(&len, sizeof(len));
    _animFileName.resize(len);
    reader.ReadPtr(_animFileName.data(), len);
    reader.ReadPtr(&len, sizeof(len));
    _animName.resize(len);
    reader.ReadPtr(_animName.data(), len);
    reader.ReadPtr(&_duration, sizeof(_duration));
    reader.ReadPtr(&len, sizeof(len));

    _bonesHierarchy.resize(len);

    for (uint32_t i = 0, j = len; i < j; i++) {
        reader.ReadPtr(&len, sizeof(len));
        _bonesHierarchy[i].resize(len);

        for (uint32_t k = 0, l = len; k < l; k++) {
            reader.ReadPtr(&len, sizeof(len));
            tmp.resize(len);
            reader.ReadPtr(tmp.data(), len);
            _bonesHierarchy[i][k] = hash_resolver.ToHashedString(tmp);
        }
    }

    reader.ReadPtr(&len, sizeof(len));
    _boneOutputs.resize(len);

    for (uint32_t i = 0, j = len; i < j; i++) {
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

ModelAnimationController::ModelAnimationController(int32_t track_count)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track_count >= 0, "Track count is negative", track_count);

    if (track_count != 0) {
        _anims = SafeAlloc::MakeShared<vector<pair<ModelAnimation*, bool>>>();
        _outputs = SafeAlloc::MakeShared<deque<Output>>();
        _tracks.resize(track_count);
    }
}

auto ModelAnimationController::Copy() const -> unique_ptr<ModelAnimationController>
{
    FO_STACK_TRACE_ENTRY();

    auto clone = SafeAlloc::MakeUnique<ModelAnimationController>(0);
    clone->_anims = _anims;
    clone->_outputs = SafeAlloc::MakeShared<deque<Output>>(*_outputs);
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
    o.Scale.resize(_tracks.size(), vec3 {1.0f, 1.0f, 1.0f});
    o.Rotation.resize(_tracks.size(), quaternion {1.0f, 0.0f, 0.0f, 0.0f});
    o.Translation.resize(_tracks.size());
}

auto ModelAnimationController::RegisterAnimation(ModelAnimation* animation, bool reversed) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    _anims->emplace_back(animation, reversed);
    return numeric_cast<int32_t>(_anims->size() - 1);
}

auto ModelAnimationController::GetAnimationBones(int32_t index) const -> const vector<vector<hstring>>&
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(index >= 0, "Index is negative", index);
    FO_VERIFY_AND_THROW(index < numeric_cast<int32_t>(_anims->size()), "Animation index is outside animation table bounds", index, _anims->size());

    return (*_anims)[index].first->GetBonesHierarchy();
}

auto ModelAnimationController::GetAnimationDuration(int32_t index) const -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(index >= 0, "Index is negative", index);
    FO_VERIFY_AND_THROW(index < numeric_cast<int32_t>(_anims->size()), "Animation index is outside animation table bounds", index, _anims->size());

    return (*_anims)[index].first->GetDuration();
}

auto ModelAnimationController::GetTrackEnable(int32_t track) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    return _tracks[track].Enabled;
}

auto ModelAnimationController::GetTrackSpeed(int32_t track) const -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    return _tracks[track].Speed;
}

auto ModelAnimationController::GetTrackPosition(int32_t track) const -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    return _tracks[track].Position;
}

auto ModelAnimationController::GetAnimationsCount() const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    return numeric_cast<int32_t>(_anims->size());
}

void ModelAnimationController::SetTrackAnimation(int32_t track, int32_t anim_index, const unordered_set<hstring>* allowed_bones)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());
    FO_VERIFY_AND_THROW(anim_index >= 0, "Animation index is negative", anim_index);
    FO_VERIFY_AND_THROW(anim_index < numeric_cast<int32_t>(_anims->size()), "Animation index is outside animation table bounds", anim_index, _anims->size());

    const auto* anim = (*_anims)[anim_index].first;
    const auto reversed = (*_anims)[anim_index].second;
    const size_t invalid_output_index = std::numeric_limits<size_t>::max();

    _tracks[track].Anim = anim;
    _tracks[track].Reversed = reversed;
    const auto& outputs = anim->GetBoneOutputs();
    _tracks[track].AnimOutput.assign(outputs.size(), invalid_output_index);

    for (size_t i = 0; i < outputs.size(); i++) {
        const auto link_name = outputs[i].BoneName;
        size_t output_index = invalid_output_index;

        if (allowed_bones == nullptr || allowed_bones->count(link_name) != 0) {
            size_t candidate_index = 0;
            for (auto& o : *_outputs) {
                if (o.BoneName == link_name) {
                    output_index = candidate_index;
                    break;
                }

                candidate_index++;
            }
        }

        _tracks[track].AnimOutput[i] = output_index;
    }
}

void ModelAnimationController::ResetBonesTransition(int32_t skip_track, const vector<hstring>& bone_names)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(skip_track >= 0, "Skip track is negative", skip_track);
    FO_VERIFY_AND_THROW(skip_track < numeric_cast<int32_t>(_tracks.size()), "Skipped animation track index is outside track table bounds", skip_track, _tracks.size());

    // Turn off fast transition bones on other tracks
    for (auto bone_name : bone_names) {
        for (size_t i = 0; i < _tracks.size(); i++) {
            if (i == numeric_cast<size_t>(skip_track)) {
                continue;
            }

            const size_t invalid_output_index = std::numeric_limits<size_t>::max();

            for (size_t j = 0; j < _tracks[i].AnimOutput.size(); j++) {
                const size_t output_index = _tracks[i].AnimOutput[j];

                if (output_index != invalid_output_index && output_index < _outputs->size()) {
                    Output& output = (*_outputs)[output_index];

                    if (output.BoneName == bone_name) {
                        if (output.Valid.size() > i) {
                            output.Valid[i] = false;
                        }

                        _tracks[i].AnimOutput[j] = invalid_output_index;
                    }
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

void ModelAnimationController::AddEventEnable(int32_t track, bool enable, float32_t start_time)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    _tracks[track].Events.emplace_back(Track::Event {.Type = Track::EventType::Enable, .ValueTo = enable ? 1.0f : -1.0f, .StartTime = start_time, .SmoothTime = 0.0f});
}

void ModelAnimationController::AddEventSpeed(int32_t track, float32_t speed, float32_t start_time, float32_t smooth_time)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    _tracks[track].Events.emplace_back(Track::Event {.Type = Track::EventType::Speed, .ValueTo = speed, .StartTime = start_time, .SmoothTime = smooth_time});
}

void ModelAnimationController::AddEventWeight(int32_t track, float32_t weight, float32_t start_time, float32_t smooth_time)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    _tracks[track].Events.emplace_back(Track::Event {.Type = Track::EventType::Weight, .ValueTo = weight, .StartTime = start_time, .SmoothTime = smooth_time});
}

void ModelAnimationController::SetTrackEnable(int32_t track, bool enable)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    _tracks[track].Enabled = enable;
}

void ModelAnimationController::SetTrackPosition(int32_t track, float32_t position)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    _tracks[track].Position = position;
}

void ModelAnimationController::SetTrackSpeed(int32_t track, float32_t speed)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    _tracks[track].Speed = speed;
}

void ModelAnimationController::SetInterpolation(bool enabled)
{
    FO_STACK_TRACE_ENTRY();

    _interpolationDisabled = !enabled;
}

void ModelAnimationController::AdvanceTime(float32_t time)
{
    FO_STACK_TRACE_ENTRY();

    _eventsTime += time;

    for (auto& track : _tracks) {
        for (auto it = track.Events.begin(); it != track.Events.end();) {
            auto& event = *it;

            if (_eventsTime >= event.StartTime) {
                bool erase = false;
                float32_t value;

                if (_eventsTime < event.StartTime + event.SmoothTime) {
                    FO_VERIFY_AND_THROW(event.SmoothTime > 0.0f, "Event smooth time must be positive");

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

    const size_t track_count = _tracks.size();

    for (auto& output : *_outputs) {
        if (output.Valid.size() != track_count) {
            output.Valid.resize(track_count);
        }
        if (output.Factor.size() != track_count) {
            output.Factor.resize(track_count);
        }
        if (output.Scale.size() != track_count) {
            output.Scale.resize(track_count, vec3 {1.0f, 1.0f, 1.0f});
        }
        if (output.Rotation.size() != track_count) {
            output.Rotation.resize(track_count, quaternion {1.0f, 0.0f, 0.0f, 0.0f});
        }
        if (output.Translation.size() != track_count) {
            output.Translation.resize(track_count);
        }
    }

    // Track animation
    for (size_t i = 0; i < track_count; i++) {
        auto& track = _tracks[i];

        for (auto& o : *_outputs) {
            o.Valid[i] = false;
        }

        if (!track.Enabled || track.Weight <= 0.0f || !track.Anim) {
            continue;
        }

        const auto& anim_outputs = track.Anim->GetBoneOutputs();
        FO_VERIFY_AND_THROW(anim_outputs.size() == track.AnimOutput.size(), "Animation output count must match the track");

        const size_t anim_output_count = std::min(anim_outputs.size(), track.AnimOutput.size());
        const size_t invalid_output_index = std::numeric_limits<size_t>::max();

        for (size_t j = 0; j < anim_output_count; j++) {
            const size_t output_index = track.AnimOutput[j];

            if (output_index == invalid_output_index || output_index >= _outputs->size()) {
                continue;
            }

            Output* output = &(*_outputs)[output_index];

            const auto& anim_output = anim_outputs[j];
            const auto duration = track.Anim->GetDuration();

            if (duration <= 0.0f) {
                continue;
            }

            const auto pos = std::fmod(track.Position, duration);

            output->Scale[i] = vec3 {1.0f, 1.0f, 1.0f};
            output->Rotation[i] = quaternion {1.0f, 0.0f, 0.0f, 0.0f};
            output->Translation[i] = {};

            const bool has_scale = FindSrtValue<vec3>(pos, duration, track.Reversed, anim_output.ScaleTime, anim_output.ScaleValue, output->Scale[i]);
            const bool has_rotation = FindSrtValue<quaternion>(pos, duration, track.Reversed, anim_output.RotationTime, anim_output.RotationValue, output->Rotation[i]);
            const bool has_translation = FindSrtValue<vec3>(pos, duration, track.Reversed, anim_output.TranslationTime, anim_output.TranslationValue, output->Translation[i]);

            if (has_scale || has_rotation || has_translation) {
                output->Valid[i] = true;
                output->Factor[i] = track.Weight;
            }
        }
    }

    for (auto& o : *_outputs) {
        size_t base_idx = _tracks.size();

        for (size_t i = 0; i < _tracks.size(); i++) {
            if (o.Valid[i]) {
                base_idx = i;
                break;
            }
        }

        if (base_idx == _tracks.size()) {
            continue;
        }

        for (size_t i = base_idx + 1; i < _tracks.size(); i++) {
            if (o.Valid[i]) {
                const auto factor = o.Factor[i];
                Interpolate(o.Scale[base_idx], o.Scale[i], factor);
                Interpolate(o.Rotation[base_idx], o.Rotation[i], factor);
                Interpolate(o.Translation[base_idx], o.Translation[i], factor);
            }
        }

        const auto ms = glm::scale(mat44 {1.0f}, o.Scale[base_idx]);
        const auto mr = glm::mat4_cast(o.Rotation[base_idx]);
        const auto mt = glm::translate(mat44 {1.0f}, o.Translation[base_idx]);
        *o.Matrix = mt * mr * ms;
    }
}

void ModelAnimationController::Interpolate(quaternion& q1, const quaternion& q2, float32_t factor) const
{
    FO_STACK_TRACE_ENTRY();

    if (!_interpolationDisabled) {
        q1 = glm::normalize(glm::slerp(q1, q2, factor));
    }
    else if (factor >= 0.5f) {
        q1 = q2;
    }
}

void ModelAnimationController::Interpolate(vec3& v1, const vec3& v2, float32_t factor) const
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

FO_END_NAMESPACE

#endif
