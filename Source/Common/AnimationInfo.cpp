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

#include "AnimationInfo.h"
#include "ConfigFile.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

static auto GetRequiredSpriteInfoValue(const map<string_view, string_view>& values, string_view file_name, string_view section_name, string_view key) -> string_view
{
    auto it = values.find(key);
    FO_VERIFY_AND_THROW(it != values.end(), "Sprite info section is missing a required key", file_name, section_name, key);
    return it->second;
}

static auto ParseSpriteInfoIntValues(const map<string_view, string_view>& values, string_view file_name, string_view section_name, string_view key) -> vector<int32_t>
{
    vector<string_view> tokens = strvex(GetRequiredSpriteInfoValue(values, file_name, section_name, key)).split(' ');
    vector<int32_t> result;
    result.reserve(tokens.size());

    for (string_view token : tokens) {
        int32_t value {};
        auto token_begin = make_nptr(token.data());
        ptr<const char> token_end = token_begin.offset(token.size());
        auto parse_result = std::from_chars(token_begin.get(), token_end.get(), value);
        FO_VERIFY_AND_THROW(parse_result.ec == std::errc {} && parse_result.ptr == token_end.get(), "Sprite info integer contains invalid text", file_name, section_name, key, token);
        result.emplace_back(value);
    }

    return result;
}

static auto ParseSpriteInfoScalar(const map<string_view, string_view>& values, string_view file_name, string_view section_name, string_view key) -> int32_t
{
    vector<int32_t> parsed_values = ParseSpriteInfoIntValues(values, file_name, section_name, key);
    FO_VERIFY_AND_THROW(parsed_values.size() == 1, "Sprite info scalar must contain exactly one value", file_name, section_name, key, parsed_values.size());
    return parsed_values.front();
}

auto GetSpriteDirDuration(const SpriteDirInfo& direction) -> timespan
{
    FO_VERIFY_AND_THROW(!direction.Frames.empty() && direction.Frames.size() <= 65535, "Sprite direction requires 1..65535 frames", direction.Frames.size());

    timespan total {};
    timespan maximum {std::chrono::milliseconds {std::numeric_limits<uint32_t>::max()}};

    for (const auto& frame : direction.Frames) {
        FO_VERIFY_AND_THROW(frame.Duration > timespan::zero, "Sprite frame duration must be positive");
        FO_VERIFY_AND_THROW(frame.Duration <= maximum - total, "Sprite direction duration exceeds uint32 milliseconds");
        total += frame.Duration;
    }

    return total;
}

auto GetSpriteDurationMilliseconds(timespan duration) -> uint32_t
{
    int64_t milliseconds = duration.milliseconds();
    FO_VERIFY_AND_THROW(milliseconds > 0 && milliseconds <= std::numeric_limits<uint32_t>::max(), "Sprite duration must fit positive uint32 milliseconds", duration);
    FO_VERIFY_AND_THROW(duration == timespan {std::chrono::milliseconds {milliseconds}}, "Sprite duration must contain whole milliseconds", duration);

    return numeric_cast<uint32_t>(milliseconds);
}

static auto ParseSpriteInfoDurations(const map<string_view, string_view>& values, string_view file_name, string_view section_name) -> vector<uint32_t>
{
    vector<uint32_t> result;

    for (string_view token : strvex(GetRequiredSpriteInfoValue(values, file_name, section_name, "FrameDurationsMs")).split(' ')) {
        uint32_t value {};
        auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
        FO_VERIFY_AND_THROW(!token.empty() && parsed.ec == std::errc {} && parsed.ptr == token.data() + token.size() && value > 0, "Sprite info requires positive uint32 frame durations", file_name, section_name, token);
        result.emplace_back(value);
    }

    return result;
}

auto ReadSpriteInfoFile(string_view file_name, string_view content) -> vector<SpriteInfoFileEntry>
{
    FO_TRACE_ZONE(Render);

    ConfigFile config {string {content}};
    vector<SpriteInfoFileEntry> result;
    set<string> source_paths;
    set<string> resource_paths;

    for (const auto& [section_name, values] : *config.GetSections()) {
        if (section_name.empty()) {
            FO_VERIFY_AND_THROW(values.empty(), "Sprite info resource has entries outside a section", file_name);
            continue;
        }

        SpriteInfoFileEntry entry;
        entry.SourcePath = GetRequiredSpriteInfoValue(values, file_name, section_name, "SourcePath");
        entry.ResourcePath = section_name;
        FO_VERIFY_AND_THROW(!entry.SourcePath.empty(), "Sprite info source path is empty", file_name, section_name);

        bool first_source = source_paths.emplace(entry.SourcePath).second;
        FO_VERIFY_AND_THROW(first_source, "Sprite info resource contains a duplicate source path", file_name, entry.SourcePath);
        bool first_resource = resource_paths.emplace(entry.ResourcePath).second;
        FO_VERIFY_AND_THROW(first_resource, "Sprite info resource contains a duplicate resource path", file_name, entry.ResourcePath);

        int32_t version = ParseSpriteInfoScalar(values, file_name, section_name, "InfoVersion");
        FO_VERIFY_AND_THROW(version == SPRITE_INFO_VERSION, "Sprite info version is unsupported", file_name, section_name, version, SPRITE_INFO_VERSION);

        auto counts = ParseSpriteInfoIntValues(values, file_name, section_name, "DirectionFrameCounts");
        FO_VERIFY_AND_THROW(!counts.empty() && counts.size() <= 255, "Sprite info direction count is invalid", file_name, section_name, counts.size());
        size_t total = 0;

        for (int32_t count : counts) {
            FO_VERIFY_AND_THROW(count > 0 && count <= 65535, "Sprite info direction requires 1..65535 frames", file_name, section_name, count);
            total += numeric_cast<size_t>(count);
        }

        auto shared = ParseSpriteInfoIntValues(values, file_name, section_name, "SharedFrameIndices");
        auto xs = ParseSpriteInfoIntValues(values, file_name, section_name, "OffsetsX");
        auto ys = ParseSpriteInfoIntValues(values, file_name, section_name, "OffsetsY");
        auto widths = ParseSpriteInfoIntValues(values, file_name, section_name, "Widths");
        auto heights = ParseSpriteInfoIntValues(values, file_name, section_name, "Heights");
        auto next_xs = ParseSpriteInfoIntValues(values, file_name, section_name, "NextOffsetsX");
        auto next_ys = ParseSpriteInfoIntValues(values, file_name, section_name, "NextOffsetsY");
        auto durations = ParseSpriteInfoDurations(values, file_name, section_name);
        FO_VERIFY_AND_THROW(shared.size() == total && xs.size() == total && ys.size() == total && widths.size() == total && heights.size() == total && next_xs.size() == total && next_ys.size() == total && durations.size() == total, "Sprite info frame arrays have different sizes", file_name, section_name, total);

        entry.Info.Directions.resize(counts.size());
        size_t index = 0;

        for (size_t dir = 0; dir < counts.size(); dir++) {
            auto& direction = entry.Info.Directions[dir];
            direction.Frames.resize(numeric_cast<size_t>(counts[dir]));

            for (size_t frame_index = 0; frame_index < direction.Frames.size(); frame_index++, index++) {
                auto& frame = direction.Frames[frame_index];
                frame.Offset = {xs[index], ys[index]};
                frame.Size = {widths[index], heights[index]};
                frame.NextOffset = {next_xs[index], next_ys[index]};
                frame.Duration = std::chrono::milliseconds {durations[index]};
                FO_VERIFY_AND_THROW(frame.Size.width > 0 && frame.Size.width <= 65535 && frame.Size.height > 0 && frame.Size.height <= 65535, "Sprite info frame dimensions must be positive uint16 values", file_name, section_name, index, frame.Size);
                FO_VERIFY_AND_THROW(frame.NextOffset.x >= -32768 && frame.NextOffset.x <= 32767 && frame.NextOffset.y >= -32768 && frame.NextOffset.y <= 32767, "Sprite info movement must fit int16", file_name, section_name, index);

                if (shared[index] >= 0) {
                    FO_VERIFY_AND_THROW(numeric_cast<size_t>(shared[index]) < frame_index, "Sprite info shared frame points outside previously decoded frames", file_name, section_name, index, shared[index]);
                    frame.SharedFrameIndex = numeric_cast<uint16_t>(shared[index]);
                    FO_VERIFY_AND_THROW(frame.Size == direction.Frames[*frame.SharedFrameIndex].Size, "Sprite info shared pixel dimensions differ from its source", file_name, section_name, index);
                }
                else {
                    FO_VERIFY_AND_THROW(shared[index] == -1, "Sprite info shared frame index is invalid", file_name, section_name, index, shared[index]);
                }
            }

            (void)GetSpriteDirDuration(direction);
        }

        result.emplace_back(std::move(entry));
    }

    return result;
}

auto WriteSpriteInfoFile(const vector<SpriteInfoFileEntry>& entries) -> string
{
    FO_TRACE_ZONE(Baking);

    vector<size_t> order(entries.size());
    std::iota(order.begin(), order.end(), size_t {});
    std::sort(order.begin(), order.end(), [&entries](size_t left, size_t right) { return entries[left].ResourcePath < entries[right].ResourcePath; });
    set<string_view> sources;
    set<string_view> resources;
    string result;

    for (size_t entry_index : order) {
        const auto& entry = entries[entry_index];
        FO_VERIFY_AND_THROW(!entry.SourcePath.empty() && !entry.ResourcePath.empty(), "Sprite info file entry contains an empty path", entry.SourcePath, entry.ResourcePath);
        bool first_source = sources.emplace(entry.SourcePath).second;
        FO_VERIFY_AND_THROW(first_source, "Sprite info file contains a duplicate source path", entry.SourcePath);
        bool first_resource = resources.emplace(entry.ResourcePath).second;
        FO_VERIFY_AND_THROW(first_resource, "Sprite info file contains a duplicate resource path", entry.ResourcePath);
        FO_VERIFY_AND_THROW(!entry.Info.Directions.empty() && entry.Info.Directions.size() <= 255, "Sprite info direction count is invalid", entry.ResourcePath);
        string counts, shared, xs, ys, widths, heights, next_xs, next_ys, durations;

        auto append = [](string& output, auto value) {
            if (!output.empty()) {
                output += ' ';
            }

            output += strex("{}", value);
        };

        for (const auto& direction : entry.Info.Directions) {
            (void)GetSpriteDirDuration(direction);
            append(counts, direction.Frames.size());

            for (size_t index = 0; index < direction.Frames.size(); index++) {
                const auto& frame = direction.Frames[index];
                FO_VERIFY_AND_THROW(frame.Size.width > 0 && frame.Size.width <= 65535 && frame.Size.height > 0 && frame.Size.height <= 65535, "Sprite info frame dimensions must be positive uint16 values", entry.ResourcePath, index, frame.Size);
                FO_VERIFY_AND_THROW(frame.NextOffset.x >= -32768 && frame.NextOffset.x <= 32767 && frame.NextOffset.y >= -32768 && frame.NextOffset.y <= 32767, "Sprite info movement must fit int16", entry.ResourcePath, index);

                if (frame.SharedFrameIndex) {
                    FO_VERIFY_AND_THROW(*frame.SharedFrameIndex < index, "Sprite info shared frame points outside a previously decoded frame", entry.ResourcePath, index, *frame.SharedFrameIndex);
                    FO_VERIFY_AND_THROW(frame.Size == direction.Frames[*frame.SharedFrameIndex].Size, "Sprite info shared pixel dimensions differ from its source", entry.ResourcePath, index);
                }

                append(shared, frame.SharedFrameIndex ? numeric_cast<int32_t>(*frame.SharedFrameIndex) : -1);
                append(xs, frame.Offset.x);
                append(ys, frame.Offset.y);
                append(widths, frame.Size.width);
                append(heights, frame.Size.height);
                append(next_xs, frame.NextOffset.x);
                append(next_ys, frame.NextOffset.y);
                append(durations, GetSpriteDurationMilliseconds(frame.Duration));
            }
        }

        result += strex("[{}]\nSourcePath = {}\nInfoVersion = {}\nDirectionFrameCounts = {}\nFrameDurationsMs = {}\nSharedFrameIndices = {}\nOffsetsX = {}\nOffsetsY = {}\nWidths = {}\nHeights = {}\nNextOffsetsX = {}\nNextOffsetsY = {}\n\n", entry.ResourcePath, entry.SourcePath, SPRITE_INFO_VERSION, counts, durations, shared, xs, ys, widths, heights, next_xs, next_ys);
    }

    return result;
}

#if FO_ENABLE_3D

constexpr string_view MODEL_ANIMATION_INFO_FILE_NAME = "ModelAnimationInfo.foinfo";

auto ReadModelAnimationInfo(const FileSystem& resources, hash_resolver& hashes) -> unordered_map<hstring, ModelAnimationInfo>
{
    FO_TRACE_ZONE(Model);

    if (!resources.IsFileExists(MODEL_ANIMATION_INFO_FILE_NAME)) {
        logging::write(logging::type::info, "Model animation info document '{}' is not present", MODEL_ANIMATION_INFO_FILE_NAME);
        return {};
    }

    File info_file = resources.ReadFile(MODEL_ANIMATION_INFO_FILE_NAME);
    FO_VERIFY_AND_THROW(info_file, "Model animation info resource is not readable", MODEL_ANIMATION_INFO_FILE_NAME);

    auto config = ConfigFile(info_file.GetStr());
    unordered_map<hstring, ModelAnimationInfo> model_anim_infos;
    constexpr array<string_view, 12> model_bounds_keys {
        "ModelBoundsMinX",
        "ModelBoundsMinY",
        "ModelBoundsMinZ",
        "ModelBoundsMaxX",
        "ModelBoundsMaxY",
        "ModelBoundsMaxZ",
        "ViewBoundsMinX",
        "ViewBoundsMinY",
        "ViewBoundsMinZ",
        "ViewBoundsMaxX",
        "ViewBoundsMaxY",
        "ViewBoundsMaxZ",
    };
    constexpr array<string_view, 3> animation_duration_keys {
        "StateAnimations",
        "ActionAnimations",
        "DurationsMs",
    };
    constexpr array<string_view, 8> animation_bounds_keys {
        "BoundsStateAnimations",
        "BoundsActionAnimations",
        "BoundsMinX",
        "BoundsMinY",
        "BoundsMinZ",
        "BoundsMaxX",
        "BoundsMaxY",
        "BoundsMaxZ",
    };

    for (const auto& [section_name, key_values] : *config.GetSections()) {
        if (section_name.empty()) {
            FO_VERIFY_AND_THROW(key_values.empty(), "Model animation info resource has entries outside a section", MODEL_ANIMATION_INFO_FILE_NAME);
            continue;
        }

        FO_VERIFY_AND_THROW(key_values.count("BoundsVersion") != 0, "Model animation info section is missing its version", MODEL_ANIMATION_INFO_FILE_NAME, section_name);

        for (string_view key : model_bounds_keys) {
            FO_VERIFY_AND_THROW(key_values.count(key) != 0, "Model animation info section is missing a required bounds key", MODEL_ANIMATION_INFO_FILE_NAME, section_name, key);
        }

        auto get_value = [&key_values, section_name](string_view key) -> string_view {
            auto it = key_values.find(key);
            FO_VERIFY_AND_THROW(it != key_values.end(), "Model animation info key lookup failed", MODEL_ANIMATION_INFO_FILE_NAME, section_name, key);
            return it->second;
        };
        auto parse_int_values = [&get_value, section_name](string_view key) -> vector<int32_t> {
            vector<string_view> tokens = strvex(get_value(key)).split(' ');
            vector<int32_t> values;
            values.reserve(tokens.size());

            for (string_view token : tokens) {
                int32_t value {};
                auto token_begin = make_nptr(token.data());
                ptr<const char> token_end = token_begin.offset(token.size());
                auto parse_result = std::from_chars(token_begin.get(), token_end.get(), value);
                FO_VERIFY_AND_THROW(parse_result.ec == std::errc {} && parse_result.ptr == token_end.get(), "Model animation info integer contains invalid text", MODEL_ANIMATION_INFO_FILE_NAME, section_name, key, token);
                values.emplace_back(value);
            }

            return values;
        };
        auto parse_float_values = [&get_value, section_name](string_view key) -> vector<float32_t> {
            vector<string_view> tokens = strvex(get_value(key)).split(' ');
            vector<float32_t> values;
            values.reserve(tokens.size());

            for (string_view token : tokens) {
                float32_t value {};
                auto token_begin = make_nptr(token.data());
                ptr<const char> token_end = token_begin.offset(token.size());
                auto parse_result = std::from_chars(token_begin.get(), token_end.get(), value);
                FO_VERIFY_AND_THROW(parse_result.ec == std::errc {} && parse_result.ptr == token_end.get() && std::isfinite(value), "Model animation info scalar contains invalid text", MODEL_ANIMATION_INFO_FILE_NAME, section_name, key, token);
                values.emplace_back(value);
            }

            return values;
        };
        auto has_any_key = [&key_values](const auto& keys) -> bool { return std::ranges::any_of(keys, [&key_values](string_view key) { return key_values.count(key) != 0; }); };
        bool has_animation_durations = has_any_key(animation_duration_keys);
        bool has_animation_bounds = has_any_key(animation_bounds_keys);

        vector<int32_t> versions = parse_int_values("BoundsVersion");
        FO_VERIFY_AND_THROW(versions.size() == 1 && versions.front() == numeric_cast<int32_t>(MODEL_BOUNDS_VERSION), "Model animation info version is unsupported", MODEL_ANIMATION_INFO_FILE_NAME, section_name, versions.size(), versions.empty() ? 0 : versions.front());

        auto parse_scalar = [&parse_float_values, section_name](string_view key) -> float32_t {
            vector<float32_t> values = parse_float_values(key);
            FO_VERIFY_AND_THROW(values.size() == 1, "Model animation info bounds scalar must contain exactly one value", MODEL_ANIMATION_INFO_FILE_NAME, section_name, key, values.size());
            return values.front();
        };

        ModelAnimationInfo section_info {
            .ModelBounds =
                {
                    .Min = {parse_scalar("ModelBoundsMinX"), parse_scalar("ModelBoundsMinY"), parse_scalar("ModelBoundsMinZ")},
                    .Max = {parse_scalar("ModelBoundsMaxX"), parse_scalar("ModelBoundsMaxY"), parse_scalar("ModelBoundsMaxZ")},
                },
            .ViewBounds =
                {
                    .Min = {parse_scalar("ViewBoundsMinX"), parse_scalar("ViewBoundsMinY"), parse_scalar("ViewBoundsMinZ")},
                    .Max = {parse_scalar("ViewBoundsMaxX"), parse_scalar("ViewBoundsMaxY"), parse_scalar("ViewBoundsMaxZ")},
                },
        };
        FO_VERIFY_AND_THROW(IsValidModelBounds(section_info.ModelBounds), "Model bounds minimum exceeds maximum or contains a non-finite coordinate", MODEL_ANIMATION_INFO_FILE_NAME, section_name, section_info.ModelBounds.Min.x, section_info.ModelBounds.Min.y, section_info.ModelBounds.Min.z, section_info.ModelBounds.Max.x, section_info.ModelBounds.Max.y, section_info.ModelBounds.Max.z);
        FO_VERIFY_AND_THROW(HasModelBoundsExtent(section_info.ModelBounds), "Model bounds record is degenerate", MODEL_ANIMATION_INFO_FILE_NAME, section_name);
        FO_VERIFY_AND_THROW(IsValidModelBounds(section_info.ViewBounds), "Model view bounds minimum exceeds maximum or contains a non-finite coordinate", MODEL_ANIMATION_INFO_FILE_NAME, section_name, section_info.ViewBounds.Min.x, section_info.ViewBounds.Min.y, section_info.ViewBounds.Min.z, section_info.ViewBounds.Max.x, section_info.ViewBounds.Max.y, section_info.ViewBounds.Max.z);
        FO_VERIFY_AND_THROW(HasModelBoundsExtent(section_info.ViewBounds), "Model view bounds record is degenerate", MODEL_ANIMATION_INFO_FILE_NAME, section_name);

        if (has_animation_durations) {
            for (string_view key : animation_duration_keys) {
                FO_VERIFY_AND_THROW(key_values.count(key) != 0, "Model animation duration section is missing a required key", MODEL_ANIMATION_INFO_FILE_NAME, section_name, key);
            }

            vector<int32_t> state_anims = parse_int_values("StateAnimations");
            vector<int32_t> action_anims = parse_int_values("ActionAnimations");
            vector<int32_t> durations_ms = parse_int_values("DurationsMs");
            size_t duration_count = state_anims.size();
            FO_VERIFY_AND_THROW(duration_count != 0, "Model animation duration section contains no records", MODEL_ANIMATION_INFO_FILE_NAME, section_name);
            FO_VERIFY_AND_THROW(action_anims.size() == duration_count && durations_ms.size() == duration_count, "Model animation duration arrays have different sizes", MODEL_ANIMATION_INFO_FILE_NAME, section_name, duration_count, action_anims.size(), durations_ms.size());
            section_info.AnimationDurations.reserve(duration_count);

            for (size_t i = 0; i < duration_count; i++) {
                CritterStateAnim state_anim = static_cast<CritterStateAnim>(numeric_cast<uint16_t>(state_anims[i]));
                CritterActionAnim action_anim = static_cast<CritterActionAnim>(numeric_cast<uint16_t>(action_anims[i]));
                FO_VERIFY_AND_THROW(durations_ms[i] > 0, "Model animation duration must be positive", MODEL_ANIMATION_INFO_FILE_NAME, section_name, state_anims[i], action_anims[i], durations_ms[i]);

                bool inserted = section_info.AnimationDurations.emplace(pair {state_anim, action_anim}, std::chrono::milliseconds {durations_ms[i]}).second;
                FO_VERIFY_AND_THROW(inserted, "Model animation duration section contains a duplicate animation pair", MODEL_ANIMATION_INFO_FILE_NAME, section_name, state_anims[i], action_anims[i]);
            }
        }

        if (has_animation_bounds) {
            for (string_view key : animation_bounds_keys) {
                FO_VERIFY_AND_THROW(key_values.count(key) != 0, "Model animation bounds section is missing a required key", MODEL_ANIMATION_INFO_FILE_NAME, section_name, key);
            }

            vector<int32_t> state_anims = parse_int_values("BoundsStateAnimations");
            vector<int32_t> action_anims = parse_int_values("BoundsActionAnimations");
            vector<float32_t> min_x = parse_float_values("BoundsMinX");
            vector<float32_t> min_y = parse_float_values("BoundsMinY");
            vector<float32_t> min_z = parse_float_values("BoundsMinZ");
            vector<float32_t> max_x = parse_float_values("BoundsMaxX");
            vector<float32_t> max_y = parse_float_values("BoundsMaxY");
            vector<float32_t> max_z = parse_float_values("BoundsMaxZ");
            size_t bounds_count = state_anims.size();
            FO_VERIFY_AND_THROW(bounds_count != 0, "Model animation bounds section contains no records", MODEL_ANIMATION_INFO_FILE_NAME, section_name);
            FO_VERIFY_AND_THROW(action_anims.size() == bounds_count && min_x.size() == bounds_count && min_y.size() == bounds_count && min_z.size() == bounds_count && max_x.size() == bounds_count && max_y.size() == bounds_count && max_z.size() == bounds_count, "Model animation bounds arrays have different sizes", MODEL_ANIMATION_INFO_FILE_NAME, section_name, bounds_count, action_anims.size(), min_x.size(), min_y.size(), min_z.size(), max_x.size(), max_y.size(), max_z.size());
            section_info.AnimationBounds.reserve(bounds_count);

            for (size_t i = 0; i < bounds_count; i++) {
                CritterStateAnim state_anim = static_cast<CritterStateAnim>(numeric_cast<uint16_t>(state_anims[i]));
                CritterActionAnim action_anim = static_cast<CritterActionAnim>(numeric_cast<uint16_t>(action_anims[i]));
                ModelBounds3D bounds {
                    .Min = {min_x[i], min_y[i], min_z[i]},
                    .Max = {max_x[i], max_y[i], max_z[i]},
                };
                FO_VERIFY_AND_THROW(IsValidModelBounds(bounds), "Model animation bounds minimum exceeds maximum or contains a non-finite coordinate", MODEL_ANIMATION_INFO_FILE_NAME, section_name, state_anims[i], action_anims[i], bounds.Min.x, bounds.Min.y, bounds.Min.z, bounds.Max.x, bounds.Max.y, bounds.Max.z);
                FO_VERIFY_AND_THROW(HasModelBoundsExtent(bounds), "Model animation bounds record is degenerate", MODEL_ANIMATION_INFO_FILE_NAME, section_name, state_anims[i], action_anims[i]);

                bool inserted = section_info.AnimationBounds.emplace(pair {state_anim, action_anim}, bounds).second;
                FO_VERIFY_AND_THROW(inserted, "Model animation bounds section contains a duplicate animation pair", MODEL_ANIMATION_INFO_FILE_NAME, section_name, state_anims[i], action_anims[i]);
            }
        }

        hstring resource_name = hashes.to_hashed_string(section_name);
        bool inserted = model_anim_infos.emplace(resource_name, std::move(section_info)).second;
        FO_VERIFY_AND_THROW(inserted, "Model animation info resource contains a duplicate section", MODEL_ANIMATION_INFO_FILE_NAME, section_name);
    }

    FO_VERIFY_AND_THROW(!model_anim_infos.empty(), "Model animation info resource contains no model sections", MODEL_ANIMATION_INFO_FILE_NAME);

    return model_anim_infos;
}

#endif

auto ReadAnimationInfo(const FileSystem& resources, hash_resolver& hashes) -> unordered_map<hstring, AnimationInfo>
{
    FO_TRACE_ZONE(Core);

    unordered_map<hstring, AnimationInfo> anim_infos;

    for (const FileHeader& file_header : resources.FilterFiles("foinfo", SPRITE_INFO_DIRECTORY)) {
        File info_file = File::Load(file_header);
        FO_VERIFY_AND_THROW(info_file, "Sprite info resource is not readable", file_header.GetPath());

        vector<SpriteInfoFileEntry> sprite_entries = ReadSpriteInfoFile(info_file.GetPath(), info_file.GetStr());

        for (SpriteInfoFileEntry& sprite_entry : sprite_entries) {
            hstring resource_name = hashes.to_hashed_string(sprite_entry.ResourcePath);
            AnimationInfo& anim_info = anim_infos[resource_name];

            if (!anim_info.Sprite.has_value()) {
                anim_info.Sprite = std::move(sprite_entry.Info);
            }
        }
    }

#if FO_ENABLE_3D
    auto model_anim_infos = ReadModelAnimationInfo(resources, hashes);
    anim_infos.reserve(anim_infos.size() + model_anim_infos.size());

    for (auto& [resource_name, model_info] : model_anim_infos) {
        AnimationInfo& anim_info = anim_infos[resource_name];
        FO_VERIFY_AND_THROW(!anim_info.Model.has_value(), "Animation info already contains model data", resource_name);
        anim_info.Model = std::move(model_info);
    }
#endif

    return anim_infos;
}

FO_END_NAMESPACE
