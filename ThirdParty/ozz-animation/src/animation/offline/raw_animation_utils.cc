//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) Guillaume Blanc                                              //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included in //
// all copies or substantial portions of the Software.                        //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//----------------------------------------------------------------------------//

#include "ozz/animation/offline/raw_animation_utils.h"

#include <algorithm>
#include <limits>
#include <numeric>

#include "ozz/animation/runtime/skeleton.h"

namespace ozz {
namespace animation {
namespace offline {

// Translation interpolation method.
// This must be the same Lerp as the one used by the sampling job.
math::Float3 LerpTranslation(const math::Float3& _a, const math::Float3& _b,
                             float _alpha) {
  return math::Lerp(_a, _b, _alpha);
}

// Rotation interpolation method.
// This must be the same Lerp as the one used by the sampling job.
// The goal is to take the shortest path between _a and _b. This code replicates
// this behavior that is actually not done at runtime, but when building the
// animation.
math::Quaternion LerpRotation(const math::Quaternion& _a,
                              const math::Quaternion& _b, float _alpha) {
  // Finds the shortest path. This is done by the AnimationBuilder for runtime
  // animations.
  const float dot = _a.x * _b.x + _a.y * _b.y + _a.z * _b.z + _a.w * _b.w;
  return math::NLerp(_a, dot < 0.f ? -_b : _b, _alpha);  // _b an -_b are the
                                                         // same rotation.
}

// Scale interpolation method.
// This must be the same Lerp as the one used by the sampling job.
math::Float3 LerpScale(const math::Float3& _a, const math::Float3& _b,
                       float _alpha) {
  return math::Lerp(_a, _b, _alpha);
}

namespace {

// The next functions are used to sample a RawAnimation. This feature is not
// part of ozz sdk, as RawAnimation is a intermediate format used to build the
// runtime animation.

// Less comparator, used by search algorithm to walk through track sorted
// keyframes
template <typename _Key>
bool Less(const _Key& _left, const _Key& _right) {
  return _left.time < _right.time;
}

// Samples a component (translation, rotation or scale) of a track.
template <typename _Track, typename _Lerp>
typename _Track::value_type::Value SampleComponent(const _Track& _track,
                                                   const _Lerp& _lerp,
                                                   float _time) {
  if (_track.size() == 0) {
    // Return identity if there's no key for this track.
    return _Track::value_type::identity();
  } else if (_time <= _track.front().time) {
    // Returns the first keyframe if _time is before the first keyframe.
    return _track.front().value;
  } else if (_time >= _track.back().time) {
    // Returns the last keyframe if _time is before the last keyframe.
    return _track.back().value;
  } else {
    // Needs to interpolate the 2 keyframes before and after _time.
    assert(_track.size() >= 2);
    // First find the 2 keys.
    const typename _Track::value_type cmp = {_time,
                                             _Track::value_type::identity()};
    typename _Track::const_pointer it =
        std::lower_bound(array_begin(_track), array_end(_track), cmp,
                         Less<typename _Track::value_type>);
    assert(it > array_begin(_track) && it < array_end(_track));

    // Then interpolate them at t = _time.
    const typename _Track::const_reference right = it[0];
    const typename _Track::const_reference left = it[-1];
    const float alpha = (_time - left.time) / (right.time - left.time);
    return _lerp(left.value, right.value, alpha);
  }
}

void SampleTrack_NoValidate(const RawAnimation::JointTrack& _track, float _time,
                            ozz::math::Transform* _transform) {
  _transform->translation =
      SampleComponent(_track.translations, LerpTranslation, _time);
  _transform->rotation = SampleComponent(_track.rotations, LerpRotation, _time);
  _transform->scale = SampleComponent(_track.scales, LerpScale, _time);
}
}  // namespace

bool SampleTrack(const RawAnimation::JointTrack& _track, float _time,
                 ozz::math::Transform* _transform) {
  if (!_track.Validate(std::numeric_limits<float>::infinity())) {
    return false;
  }

  SampleTrack_NoValidate(_track, _time, _transform);
  return true;
}

bool SampleAnimation(const RawAnimation& _animation, float _time,
                     const span<ozz::math::Transform>& _transforms) {
  if (!_animation.Validate()) {
    return false;
  }
  if (_animation.tracks.size() > _transforms.size()) {
    return false;
  }

  // Samples each track.
  for (size_t i = 0; i < _animation.tracks.size(); ++i) {
    SampleTrack_NoValidate(_animation.tracks[i], _time,
                           _transforms.begin() + i);
  }

  // Fills remaining transforms with identity.
  for (size_t i = _animation.tracks.size(); i < _transforms.size(); ++i) {
    _transforms[i] = ozz::math::Transform::identity();
  }
  return true;
}

std::pair<bool, ozz::vector<std::pair<float, math::Float4x4>>>
SampleTrackModelSpace(const RawAnimation& _animation,
                      const ozz::animation::Skeleton& _skeleton, int _joint) {
  if (!_animation.Validate() ||
      _animation.num_tracks() != _skeleton.num_joints() || _joint < 0 ||
      _joint >= _skeleton.num_joints()) {
    return {false, {}};
  }

  // Builds joint's hierarchy from root to _joint.
  ozz::vector<int16_t> hierarchy;
  for (int16_t parent = static_cast<int16_t>(_joint);
       parent != Skeleton::kNoParent;
       parent = _skeleton.joint_parents()[parent]) {
    hierarchy.push_back(parent);
  }
  std::reverse(hierarchy.begin(), hierarchy.end());

  // Track needs to be sampled allong all time points of the animation, not only
  // its own keyframes as it can be influenced by parent joints.
  const auto [valid, time_points] =
      ExtractTimePoints(_animation, make_span(hierarchy));
  if (!valid) {
    return {false, {}};
  }

  // For each time point, samples the whole skeleton in local space, then
  // converts the requested joint to model space.
  ozz::vector<std::pair<float, math::Float4x4>> models;
  for (const float time : time_points) {
    // Samples starting from the root joint, accumulate to model space.
    auto model = ozz::math::Float4x4::identity();
    for (size_t h = 0; h < hierarchy.size(); ++h) {
      ozz::math::Transform local;
      SampleTrack_NoValidate(_animation.tracks[hierarchy[h]], time, &local);

      // Computes model space.
      model = model * math::Float4x4::FromAffine(local);
    }
    models.push_back({time, model});
  }

  return {true, models};
}

std::pair<bool, ozz::vector<float>> ExtractTimePoints(
    const RawAnimation& _animation) {
  ozz::vector<int16_t> joints(_animation.num_tracks());
  std::iota(joints.begin(), joints.end(), int16_t{0});
  return ExtractTimePoints(_animation, make_span(joints));
}

std::pair<bool, ozz::vector<float>> ExtractTimePoints(
    const RawAnimation& _animation, const ozz::span<const int16_t>& _joints) {
  if (!_animation.Validate()) {
    return {false, {}};
  }

  // Gets union of all possible keyframe times.
  ozz::vector<float> times;
  for (const auto i : _joints) {
    if (i < 0 || i >= _animation.num_tracks()) {
      return {false, {}};
    }
    const RawAnimation::JointTrack& track = _animation.tracks[i];
    auto copyfn = [&times](const auto& k) { times.push_back(k.time); };
    std::for_each(track.translations.begin(), track.translations.end(), copyfn);
    std::for_each(track.rotations.begin(), track.rotations.end(), copyfn);
    std::for_each(track.scales.begin(), track.scales.end(), copyfn);

    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end()), times.end());
  }

  return {true, times};
}

FixedRateSamplingTime::FixedRateSamplingTime(float _duration, float _frequency)
    : duration_(_duration),
      period_(1.f / _frequency),
      num_keys_(static_cast<size_t>(std::ceil(1.f + _duration * _frequency))) {}

}  // namespace offline
}  // namespace animation
}  // namespace ozz
