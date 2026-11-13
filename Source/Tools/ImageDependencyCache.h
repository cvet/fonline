// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "AnimationInfo.h"
#include "ImageDependencyGraph.h"

FO_BEGIN_NAMESPACE

auto GetImageDependencyCachePath(const BakingSettings& settings, const ImageOutputCandidate& output) -> string;
auto ReadImageDependencySnapshot(const BakingSettings& settings, const ImageOutputCandidate& output, string_view fingerprint) -> optional<SpriteInfoFileEntry>;
auto IsOutputCurrent(const BakingSettings& settings, const ImageOutputCandidate& output, string_view fingerprint) -> bool;
void WriteImageDependencySnapshot(const BakingSettings& settings, const ImageOutputCandidate& output, string_view fingerprint, const SpriteInfoFileEntry& entry);
auto IsImagePackInfoCurrent(const BakingSettings& settings, string_view pack, string_view fingerprint) -> bool;
void WriteImagePackInfoSnapshot(const BakingSettings& settings, string_view pack, string_view fingerprint);

FO_END_NAMESPACE
