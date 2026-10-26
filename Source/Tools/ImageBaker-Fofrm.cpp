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

#include "ImageBaker-Fofrm.h"

#include "AnyData.h"
#include "ConfigFile.h"
#include "ImageRecipe.h"
#include "ImageTransforms.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

namespace ImageBakerFofrm
{
    using ImageImport::FrameCollection;
    using ImageImport::FrameSequence;
    using ImageImport::FrameShot;

    static auto ReadOffsetValue(string_view value, string_view fname, string_view section, string_view key) -> ipos32
    {
        auto parts = strex(value).replace('\t', ' ').split(' ');
        FO_VERIFY_AND_THROW(parts.size() == 2, "FOFRM offset pair requires two signed integers", fname, section, key, value);

        ipos32 offset;
        istringstream input(make_stream_string(value));
        input >> offset.x >> offset.y;
        bool parsed = !input.fail();
        input >> std::ws;
        FO_VERIFY_AND_THROW(parsed && input.eof(), "FOFRM offset pair requires two signed integers", fname, section, key, value);

        return offset;
    }

    static auto ReadOffset(const ConfigFile& fofrm, string_view section, ipos32 fallback, string_view fname) -> ipos32
    {
        return fofrm.HasKey(section, "Offset") ? ReadOffsetValue(fofrm.GetAsStr(section, "Offset"), fname, section, "Offset") : fallback;
    }

    static auto ReadOffsetInteger(string_view value, string_view fname, string_view section) -> int32_t
    {
        string_view digits = value.substr(value.starts_with('+') || value.starts_with('-') ? 1 : 0);
        bool valid = !digits.empty() && std::all_of(digits.begin(), digits.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
        FO_VERIFY_AND_THROW(valid, "FOFRM FrameOffsets requires signed integer tokens", fname, section, value);

        int32_t number;
        istringstream input(make_stream_string(value));
        input >> number;
        bool parsed = !input.fail();
        FO_VERIFY_AND_THROW(parsed && input.eof(), "FOFRM FrameOffsets requires signed integer tokens", fname, section, value);

        return number;
    }

    static auto ReadFrameOffsets(const ConfigFile& fofrm, string_view section, size_t frame_count, string_view fname) -> vector<ipos32>
    {
        vector<ipos32> offsets(frame_count);

        if (!fofrm.HasKey(section, "FrameOffsets")) {
            return offsets;
        }

        auto parsed_offsets = AnyData::ParseValue(string {fofrm.GetAsStr(section, "FrameOffsets")}, false, true, AnyData::ValueType::String);
        const auto& values = parsed_offsets.AsArray();
        FO_VERIFY_AND_THROW(frame_count != 0 && values.Size() == frame_count * 2, "FOFRM FrameOffsets requires one pair per Frame or Frames reference", fname, section, frame_count, values.Size());

        for (size_t index = 0; index < frame_count; index++) {
            offsets[index] = {
                ReadOffsetInteger(values[index * 2].AsString(), fname, section),
                ReadOffsetInteger(values[index * 2 + 1].AsString(), fname, section),
            };
        }

        return offsets;
    }

    static auto ReadSizeInteger(string_view value, string_view fname, string_view section, string_view key) -> int32_t
    {
        string_view digits = value.substr(value.starts_with('+') ? 1 : 0);
        FO_VERIFY_AND_THROW(!digits.empty(), "FOFRM size requires a positive integer token", fname, section, key, value);
        uint32_t number = 0;
        auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), number);
        FO_VERIFY_AND_THROW(parsed.ec == std::errc {} && parsed.ptr == digits.data() + digits.size() && number > 0 && number <= std::numeric_limits<uint16_t>::max(), "FOFRM size requires an integer token within uint16 range", fname, section, key, value);

        return numeric_cast<int32_t>(number);
    }

    static auto ReadSizeValue(string_view value, string_view fname, string_view section, string_view key) -> isize32
    {
        auto parsed_size = AnyData::ParseValue(string {value}, false, true, AnyData::ValueType::String);
        const auto& values = parsed_size.AsArray();
        FO_VERIFY_AND_THROW(values.Size() == 2, "FOFRM size requires two positive integer tokens", fname, section, key, value);

        return {ReadSizeInteger(values[0].AsString(), fname, section, key), ReadSizeInteger(values[1].AsString(), fname, section, key)};
    }

    static auto ReadFrameSizes(const ConfigFile& fofrm, string_view section, size_t frame_count, optional<isize32> fallback, string_view fname) -> vector<optional<isize32>>
    {
        bool has_size = fofrm.HasKey(section, "Size");
        bool has_frame_sizes = fofrm.HasKey(section, "FrameSizes");
        FO_VERIFY_AND_THROW(!(has_size && has_frame_sizes), "FOFRM Size and FrameSizes are mutually exclusive in one section", fname, section);

        if (has_size) {
            fallback = ReadSizeValue(fofrm.GetAsStr(section, "Size"), fname, section, "Size");
        }

        vector<optional<isize32>> sizes(frame_count, fallback);

        if (!has_frame_sizes) {
            return sizes;
        }

        auto parsed_sizes = AnyData::ParseValue(string {fofrm.GetAsStr(section, "FrameSizes")}, false, true, AnyData::ValueType::String);
        const auto& values = parsed_sizes.AsArray();
        FO_VERIFY_AND_THROW(frame_count != 0 && values.Size() == frame_count * 2, "FOFRM FrameSizes requires one pair per Frame or Frames reference", fname, section, frame_count, values.Size());

        for (size_t index = 0; index < frame_count; index++) {
            sizes[index] = isize32 {ReadSizeInteger(values[index * 2].AsString(), fname, section, "FrameSizes"), ReadSizeInteger(values[index * 2 + 1].AsString(), fname, section, "FrameSizes")};
        }

        return sizes;
    }

    static auto MaterializeFrames(const FrameSequence& source, string_view fname) -> vector<FrameShot>
    {
        vector<FrameShot> frames;
        frames.reserve(source.Frames.size());

        for (size_t frame = 0; frame < source.Frames.size(); frame++) {
            size_t concrete = frame;
            vector<bool> visited(source.Frames.size());

            while (source.Frames[concrete].Shared) {
                FO_VERIFY_AND_THROW(!visited[concrete], "FOFRM shared frame cycle", fname, frame, concrete);
                visited[concrete] = true;
                concrete = source.Frames[concrete].SharedIndex;
                FO_VERIFY_AND_THROW(concrete < source.Frames.size(), "FOFRM shared frame index is out of range", fname, frame, concrete);
            }

            auto shot = source.Frames[frame];
            shot.Size = source.Frames[concrete].Size;
            shot.Data = source.Frames[concrete].Data;
            shot.PreserveHiddenColor = shot.PreserveHiddenColor || source.Frames[concrete].PreserveHiddenColor;
            shot.Shared = false;
            shot.SharedIndex = 0;
            frames.emplace_back(std::move(shot));
        }

        return frames;
    }

    static auto ReadTimelineInteger(string_view text, string_view fname, string_view section, string_view key) -> uint32_t
    {
        string_view digits = text.starts_with('+') ? text.substr(1) : text;
        uint32_t value {};
        auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
        FO_VERIFY_AND_THROW(!digits.empty() && parsed.ec == std::errc {} && parsed.ptr == digits.data() + digits.size() && value > 0, "FOFRM timeline requires positive uint32 milliseconds or fps", fname, section, key, text);

        return value;
    }

    static void ApplySequenceTimeline(FrameSequence& sequence, const ConfigFile& recipe, string_view section, string_view fname, bool source)
    {
        FO_VERIFY_AND_THROW(!sequence.Frames.empty() && sequence.Frames.size() <= 65535, "FOFRM timeline requires 1..65535 frames", fname, section, sequence.Frames.size());

        if (recipe.HasKey(section, "FrameDurations")) {
            auto parsed = AnyData::ParseValue(string {recipe.GetAsStr(section, "FrameDurations")}, false, true, AnyData::ValueType::String);
            FO_VERIFY_AND_THROW(parsed.AsArray().Size() == sequence.Frames.size(), "FOFRM FrameDurations requires one duration per final frame", fname, section, parsed.AsArray().Size(), sequence.Frames.size());
            vector<uint32_t> durations;
            uint64_t total = 0;

            for (const auto& value : parsed.AsArray()) {
                uint32_t duration = ReadTimelineInteger(value.AsString(), fname, section, "FrameDurations");
                total += duration;
                durations.emplace_back(duration);
            }

            FO_VERIFY_AND_THROW(total <= std::numeric_limits<uint32_t>::max(), "FOFRM FrameDurations sum exceeds uint32 milliseconds", fname, section, total);

            for (size_t index = 0; index < durations.size(); index++) {
                sequence.Frames[index].Duration = std::chrono::milliseconds {durations[index]};
            }
        }
        else {
            string_view owner = recipe.HasKey(section, "Duration") || recipe.HasKey(section, "Fps") ? section : string_view {};

            if (recipe.HasKey(owner, "Duration")) {
                auto parsed = AnyData::ParseValue(string {recipe.GetAsStr(owner, "Duration")}, false, true, AnyData::ValueType::String);
                FO_VERIFY_AND_THROW(parsed.AsArray().Size() == 1, "FOFRM Duration requires one positive uint32 token", fname, owner);
                ImageImport::SetFrameSequenceDuration(sequence, std::chrono::milliseconds {ReadTimelineInteger(parsed.AsArray()[0].AsString(), fname, owner, "Duration")});
            }
            else if (recipe.HasKey(owner, "Fps")) {
                auto parsed = AnyData::ParseValue(string {recipe.GetAsStr(owner, "Fps")}, false, true, AnyData::ValueType::String);
                FO_VERIFY_AND_THROW(parsed.AsArray().Size() == 1, "FOFRM Fps requires one positive uint32 token", fname, owner);
                uint32_t fps = ReadTimelineInteger(parsed.AsArray()[0].AsString(), fname, owner, "Fps");
                ImageImport::SetFrameSequenceDuration(sequence, std::chrono::milliseconds {numeric_cast<uint32_t>(sequence.Frames.size() * uint64_t {1000} / fps)});
            }
            else if (!source) {
                ImageImport::SetFrameSequenceDuration(sequence, std::chrono::milliseconds {numeric_cast<uint32_t>(sequence.Frames.size() * 100)});
            }
        }

        (void)ImageImport::GetDuration(sequence);
    }

    static auto LoadSourceCollection(const ImageRecipe& recipe, string_view fname, const ImageImport::Context& context) -> FrameCollection
    {
        const ConfigFile& fofrm = recipe.Values;

        FrameCollection collection = context.Resolve(strex(fname).extract_dir().combine_path(recipe.Source).str());
        collection.NewName = string {fname};

        auto root_transform = ReadImageTransforms(fofrm, "", 1, fname, true)[0];

        if (root_transform.InputDir) {
            collection.Main = SelectImageSequence(collection, root_transform.InputDir, fname);

            for (auto& sequence : collection.Dirs) {
                sequence.Frames.clear();
            }
        }

        auto original = collection;
        string_view prefix = FO_GEOMETRY == 1 ? "HexDir" : "SquareDir";
        int32_t directions = ImageImport::GetDirectionCount(collection);
        ipos32 root_offset = ReadOffset(fofrm, "", {}, fname);
        ipos32 root_frame_offset = ReadFrameOffsets(fofrm, "", 1, fname)[0];
        optional<isize32> root_size = ReadFrameSizes(fofrm, "", 1, {}, fname)[0];

        for (const auto& [section, entries] : fofrm.GetOrderedSections()) {
            ignore_unused(entries);
            bool present = section.empty();

            for (int32_t direction = 1; direction < directions && !present; direction++) {
                present = section == strex("{} {}", prefix, direction).str();
            }

            FO_VERIFY_AND_THROW(present, "FOFRM Source override requires an existing direction", fname, section);
            (void)ReadImageTransforms(fofrm, section, 1, fname, true);
            (void)ReadFrameOffsets(fofrm, section, 1, fname);
            (void)ReadFrameSizes(fofrm, section, 1, root_size, fname);
        }

        for (int32_t direction = 0; direction < directions; direction++) {
            string section = direction == 0 ? string {} : strex("{} {}", prefix, direction).str();
            auto& sequence = direction == 0 ? collection.Main : collection.Dirs[direction - 1];
            auto transform = ReadImageTransforms(fofrm, section, 1, fname, true)[0];
            const auto& input = transform.InputDir && !root_transform.InputDir ? SelectImageSequence(original, transform.InputDir, fname) : direction == 0 ? original.Main : original.Dirs[direction - 1];
            sequence = input;

            ipos32 offset = ReadOffset(fofrm, section, root_offset, fname);
            ipos32 frame_offset = fofrm.HasKey(section, "FrameOffsets") ? ReadFrameOffsets(fofrm, section, 1, fname)[0] : root_frame_offset;
            optional<isize32> size = fofrm.HasKey(section, "Size") || fofrm.HasKey(section, "FrameSizes") ? ReadFrameSizes(fofrm, section, 1, root_size, fname)[0] : root_size;
            transform.Size = size;

            auto frames = MaterializeFrames(sequence, fname);
            sequence.Frames.clear();

            for (const auto& frame : frames) {
                auto transformed = TransformImageFrame(frame, transform, fname);
                sequence.Frames.insert(sequence.Frames.end(), std::make_move_iterator(transformed.begin()), std::make_move_iterator(transformed.end()));
            }

            sequence.Frames = SelectImageFrames(std::move(sequence.Frames), fofrm, section, fname, true);

            sequence.OffsX = numeric_cast<int16_t>(numeric_cast<int64_t>(sequence.OffsX) + offset.x);
            sequence.OffsY = numeric_cast<int16_t>(numeric_cast<int64_t>(sequence.OffsY) + offset.y);

            for (FrameShot& shot : sequence.Frames) {
                shot.Offset = {
                    numeric_cast<int32_t>(numeric_cast<int64_t>(shot.Offset.x) + frame_offset.x),
                    numeric_cast<int32_t>(numeric_cast<int64_t>(shot.Offset.y) + frame_offset.y),
                };
            }
        }

        for (int32_t direction = 0; direction < directions; direction++) {
            string section = direction == 0 ? string {} : strex("{} {}", prefix, direction).str();
            ApplySequenceTimeline(direction == 0 ? collection.Main : collection.Dirs[direction - 1], fofrm, section, fname, true);
        }

        return collection;
    }

    auto Load(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> FrameCollection
    {
        FO_TRACE_ZONE(Baking);

        ignore_unused(opt);

        FrameCollection collection;

        auto recipe = ParseImageRecipe(fname, reader.GetStr());
        const ConfigFile& fofrm = recipe.Values;
        string_view direction_prefix = FO_GEOMETRY == 1 ? "HexDir" : "SquareDir";

        if (!recipe.Source.empty()) {
            return LoadSourceCollection(recipe, fname, context);
        }

        for (const auto& [section, entries] : fofrm.GetOrderedSections()) {
            ignore_unused(entries);
            size_t count = recipe.References.at(string {section}).size();
            (void)ReadFrameOffsets(fofrm, section, count, fname);
            (void)ReadFrameSizes(fofrm, section, count, {}, fname);
            (void)ReadImageTransforms(fofrm, section, count, fname, false);
        }

        ipos32 offset = ReadOffset(fofrm, "", {}, fname);
        optional<isize32> common_size;

        if (fofrm.HasKey("", "Size")) {
            common_size = ReadSizeValue(fofrm.GetAsStr("", "Size"), fname, "", "Size");
        }

        for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            string section = dir == 0 ? string {} : strex("{} {}", direction_prefix, dir).str();

            if (dir != 0 && !fofrm.HasSection(section)) {
                if (dir == 1) {
                    break;
                }

                throw ImageBakerException("FOFRM file invalid apps", fname);
            }

            if (dir != 0) {
                offset = ReadOffset(fofrm, section, offset, fname);
            }

            const auto& names = recipe.References.at(section);
            FO_VERIFY_AND_THROW(!names.empty(), "FOFRM file invalid data", fname, section);
            auto offsets = ReadFrameOffsets(fofrm, section, names.size(), fname);
            auto sizes = ReadFrameSizes(fofrm, section, names.size(), common_size, fname);
            auto transforms = ReadImageTransforms(fofrm, section, names.size(), fname, false);
            auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];
            sequence.OffsX = numeric_cast<int16_t>(offset.x);
            sequence.OffsY = numeric_cast<int16_t>(offset.y);

            for (size_t ref = 0; ref < names.size(); ref++) {
                FrameCollection child;

                try {
                    child = context.Resolve(strex(fname).extract_dir().combine_path(names[ref]).str());
                }
                catch (const std::exception& ex) {
                    throw ImageBakerException("FOFRM nested image loading failed", fname, names[ref], ex.what());
                }

                transforms[ref].Size = sizes[ref];
                auto frames = MaterializeFrames(SelectImageSequence(child, transforms[ref].InputDir, fname), fname);

                for (const auto& frame : frames) {
                    auto transformed = TransformImageFrame(frame, transforms[ref], fname);

                    for (auto& shot : transformed) {
                        shot.Offset = {numeric_cast<int32_t>(numeric_cast<int64_t>(shot.Offset.x) + offsets[ref].x), numeric_cast<int32_t>(numeric_cast<int64_t>(shot.Offset.y) + offsets[ref].y)};
                        FO_VERIFY_AND_THROW(sequence.Frames.size() < 65535, "FOFRM exceeds the frame-count limit", fname, section);
                        sequence.Frames.emplace_back(std::move(shot));
                    }
                }
            }

            sequence.Frames = SelectImageFrames(std::move(sequence.Frames), fofrm, section, fname, false);
            ApplySequenceTimeline(sequence, fofrm, section, fname, false);
        }

        return collection;
    }
}

FO_END_NAMESPACE
