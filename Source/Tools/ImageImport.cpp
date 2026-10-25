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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "ImageImport.h"

#include "AnimationInfo.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

auto ImageImport::GetDirectionCount(const FrameCollection& collection) -> uint8_t
{
    FO_VERIFY_AND_THROW(!collection.Main.Frames.empty(), "Image collection requires a main sequence");

    uint8_t count = 1;
    bool gap = false;

    for (const auto& sequence : collection.Dirs) {
        if (sequence.Frames.empty()) {
            gap = true;
        }
        else {
            FO_VERIFY_AND_THROW(!gap, "Image collection contains a direction gap");
            count++;
        }
    }

    FO_VERIFY_AND_THROW(count == 1 || count == GameSettings::MAP_DIR_COUNT, "Image collection direction count is unsupported", count);
    return count;
}

auto ImageImport::GetDuration(const FrameSequence& sequence) -> timespan
{
    FO_VERIFY_AND_THROW(!sequence.Frames.empty() && sequence.Frames.size() <= 65535, "Image sequence requires 1..65535 frames", sequence.Frames.size());

    timespan duration {};
    timespan maximum {std::chrono::milliseconds {std::numeric_limits<uint32_t>::max()}};

    for (const auto& frame : sequence.Frames) {
        FO_VERIFY_AND_THROW(frame.Duration > timespan::zero, "Image frame duration must be positive");
        FO_VERIFY_AND_THROW(frame.Duration <= maximum - duration, "Image sequence duration exceeds uint32 milliseconds");
        duration += frame.Duration;
    }

    return duration;
}

void ImageImport::SetFrameSequenceDuration(FrameSequence& sequence, timespan duration)
{
    FO_VERIFY_AND_THROW(!sequence.Frames.empty() && sequence.Frames.size() <= 65535, "Image sequence requires 1..65535 frames", sequence.Frames.size());

    uint32_t duration_ms = GetSpriteDurationMilliseconds(duration);

    FO_VERIFY_AND_THROW(duration_ms >= sequence.Frames.size(), "Image sequence duration must provide at least one millisecond per frame", duration_ms, sequence.Frames.size());

    uint64_t count = sequence.Frames.size();

    for (size_t index = 0; index < sequence.Frames.size(); index++) {
        uint64_t begin = numeric_cast<uint64_t>(index) * duration_ms / count;
        uint64_t end = numeric_cast<uint64_t>(index + 1) * duration_ms / count;
        sequence.Frames[index].Duration = std::chrono::milliseconds {end - begin};
    }
}

void ImageImport::CheckEnabled(string_view fname)
{
    string ext = strex(fname).get_file_extension();
    ignore_unused(ext);

#if !FO_IMAGE_FRM
    if (ext == "frm" || ext == "fr0" || ext == "fr1" || ext == "fr2" || ext == "fr3" || ext == "fr4" || ext == "fr5" || ext == "fr6" || ext == "fr7") {
        throw ImageBakerException("Image importer is disabled; enable FO_IMAGE_FRM", fname);
    }
#endif
#if !FO_IMAGE_RIX
    if (ext == "rix") {
        throw ImageBakerException("Image importer is disabled; enable FO_IMAGE_RIX", fname);
    }
#endif
#if !FO_IMAGE_ART
    if (ext == "art") {
        throw ImageBakerException("Image importer is disabled; enable FO_IMAGE_ART", fname);
    }
#endif
#if !FO_IMAGE_SPR
    if (ext == "spr") {
        throw ImageBakerException("Image importer is disabled; enable FO_IMAGE_SPR", fname);
    }
#endif
#if !FO_IMAGE_ZAR
    if (ext == "zar") {
        throw ImageBakerException("Image importer is disabled; enable FO_IMAGE_ZAR", fname);
    }
#endif
#if !FO_IMAGE_TIL
    if (ext == "til") {
        throw ImageBakerException("Image importer is disabled; enable FO_IMAGE_TIL", fname);
    }
#endif
#if !FO_IMAGE_MOS
    if (ext == "mos") {
        throw ImageBakerException("Image importer is disabled; enable FO_IMAGE_MOS", fname);
    }
#endif
#if !FO_IMAGE_BAM
    if (ext == "bam") {
        throw ImageBakerException("Image importer is disabled; enable FO_IMAGE_BAM", fname);
    }
#endif
}

FO_END_NAMESPACE
