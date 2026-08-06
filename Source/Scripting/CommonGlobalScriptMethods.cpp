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

#include "Common.h"

#include "Application.h"
#include "ConfigFile.h"
#include "EngineBase.h"
#include "Geometry.h"
#include "LineTracer.h"
#include "ScriptSystem.h"
#include "TextPack.h"
#include "TimeEvents.h"

FO_BEGIN_NAMESPACE

// Development-only debugger trap; intentionally not an embedding-project compatibility contract.
///@ ApiContract script.method.common.Game.BreakIntoDebugger internal
// Triggers the platform debugger break primitive for the current process.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_BreakIntoDebugger(ptr<BaseEngine> engine)
{
    ignore_unused(engine);

    BreakIntoDebugger();
}

// Writes the supplied text as one Engine log message.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_Log(ptr<BaseEngine> engine, string_view text)
{
    ignore_unused(engine);

    WriteLog("{}", text);
}

// Requests application shutdown and marks the eventual process result as success or failure according to the argument.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_RequestQuit(ptr<BaseEngine> engine, bool success = true)
{
    ignore_unused(engine);

    GetApp()->RequestQuit(success);
}

// Returns whether the Engine resource provider can resolve a file at the supplied resource path.
///@ ExportMethod
FO_SCRIPT_API bool Common_Game_IsResourcePresent(ptr<BaseEngine> engine, string_view resourcePath)
{
    return engine->Resources.IsFileExists(resourcePath);
}

// Reads and returns a text file through the Engine resource provider.
///@ ExportMethod
FO_SCRIPT_API string Common_Game_ReadResource(ptr<BaseEngine> engine, string_view resourcePath)
{
    return engine->Resources.ReadFileText(resourcePath);
}

// Reads a resource as Engine config syntax and returns the requested section as key/value strings, or an empty map when that section is absent.
///@ ExportMethod
FO_SCRIPT_API map<string, string> Common_Game_ReadConfigSection(ptr<BaseEngine> engine, string_view resourcePath, string_view sectionName)
{
    string content = engine->Resources.ReadFileText(resourcePath);
    ConfigFile config(std::move(content));

    map<string, string> result;

    if (!config.HasSection(sectionName)) {
        return result;
    }

    for (const auto& [key, value] : config.GetSection(sectionName)) {
        result.emplace(string(key), string(value));
    }

    return result;
}

// Returns the baked duration for a model's state/action animation tuple, or zero when the model metadata, tuple, or 3D support is unavailable.
///@ ExportMethod
FO_SCRIPT_API timespan Common_Game_GetModelAnimDuration(ptr<BaseEngine> engine, hstring modelName, CritterStateAnim stateAnim, CritterActionAnim actionAnim)
{
#if FO_ENABLE_3D
    auto anim_info = engine->GetAnimationInfo(modelName);

    if (!anim_info) {
        return {};
    }

    if (!anim_info->Model.has_value()) {
        return {};
    }

    const ModelAnimationInfo& model_anim_info = *anim_info->Model;
    auto anim_it = model_anim_info.AnimationDurations.find({stateAnim, actionAnim});
    return anim_it != model_anim_info.AnimationDurations.end() ? anim_it->second : timespan {};
#else
    ignore_unused(engine, modelName, stateAnim, actionAnim);
    return {};
#endif
}

// Returns an Engine random integer in the inclusive range defined by the supplied minimum and maximum.
///@ ExportMethod
FO_SCRIPT_API int32_t Common_Game_Random(ptr<BaseEngine> engine, int32_t minValue, int32_t maxValue)
{
    return engine->Random(minValue, maxValue);
}

// Decodes the first UTF-8 code point from the supplied text, writes the consumed byte count, and returns the Unicode scalar value.
///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_DecodeUtf8(ptr<BaseEngine> engine, string_view text, int32_t& length)
{
    ignore_unused(engine);

    size_t decode_length = text.length();
    uint32_t ch = utf8::Decode(text.data(), decode_length); // NOLINT(bugprone-suspicious-stringview-data-usage)

    length = numeric_cast<int32_t>(decode_length);
    return ch;
}

// Encodes one Unicode scalar value as a UTF-8 string.
///@ ExportMethod
FO_SCRIPT_API string Common_Game_EncodeUtf8(ptr<BaseEngine> engine, uint32_t ucs)
{
    ignore_unused(engine);

    char buf[4];
    size_t len = utf8::Encode(ucs, buf);
    return {buf, len};
}

// Asks the host application to open the supplied link using its platform integration.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_OpenLink(ptr<BaseEngine> engine, string_view link)
{
    ignore_unused(engine);

    GetApp()->OpenLink(link);
}

// Returns the current Unix timestamp in whole seconds.
///@ ExportMethod
FO_SCRIPT_API uint64_t Common_Game_GetUnixTime(ptr<BaseEngine> engine)
{
    ignore_unused(engine);

    return numeric_cast<uint64_t>(::time(nullptr));
}

// Returns the hex-grid distance between two map positions.
///@ ExportMethod
FO_SCRIPT_API int32_t Common_Game_GetDistance(ptr<BaseEngine> engine, mpos hex1, mpos hex2)
{
    ignore_unused(engine);

    return GeometryHelper::GetDistance(hex1, hex2);
}

// Returns the rounded angular map direction from one hex to another after applying the optional degree offset.
///@ ExportMethod
FO_SCRIPT_API mdir Common_Game_GetDirection(ptr<BaseEngine> engine, mpos fromHex, mpos toHex, float32_t offset = 0.0f)
{
    ignore_unused(engine);

    return mdir(iround<int32_t>(GeometryHelper::GetDirAngle(fromHex, toHex) + offset));
}

// Returns the rounded angular direction of a line between two integer pixel positions.
///@ ExportMethod
FO_SCRIPT_API mdir Common_Game_GetLineDirAngle(ptr<BaseEngine> engine, ipos32 fromPos, ipos32 toPos)
{
    ignore_unused(engine);

    return mdir(iround<int32_t>(GeometryHelper::GetLineDirAngle(fromPos.x, fromPos.y, toPos.x, toPos.y)));
}

// Rotates an angular direction clockwise or counterclockwise by the supplied degree step and normalizes it to the zero-through-359 range.
///@ ExportMethod
FO_SCRIPT_API mdir Common_Game_RotateDirAngle(ptr<BaseEngine> engine, mdir dir, bool clockwise, int16_t step)
{
    ignore_unused(engine);

    int16_t rotated = dir.angle();

    if (clockwise) {
        rotated += step;
    }
    else {
        rotated -= step;
    }

    while (rotated < 0) {
        rotated += 360;
    }
    while (rotated >= 360) {
        rotated -= 360;
    }

    return mdir(rotated);
}

// Returns the rounded signed shortest angular difference between two map directions in degrees.
///@ ExportMethod
FO_SCRIPT_API int16_t Common_Game_GetDirAngleDiff(ptr<BaseEngine> engine, mdir dir1, mdir dir2)
{
    ignore_unused(engine);

    return numeric_cast<int16_t>(iround<int32_t>(GeometryHelper::GetDirAngleDiff(dir1.angle(), dir2.angle())));
}

// Writes the pixel-space hex-grid offset from one map position to another.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_GetHexInterval(ptr<BaseEngine> engine, mpos fromHex, mpos toHex, ipos32& hexOffset)
{
    ignore_unused(engine);

    hexOffset = GeometryHelper::GetHexOffset(fromHex, toHex);
}

// Traces up to a nonnegative number of in-bounds hexes toward a target with angular and pixel offsets, excluding the start hex and stopping when the tracer cannot advance.
///@ ExportMethod
FO_SCRIPT_API vector<mpos> Common_Game_TraceHexLine(ptr<BaseEngine> engine, msize mapSize, mpos fromHex, mpos toHex, int32_t dist, float32_t dirAngleOffset, ipos32 startOffset, ipos32 targetOffset)
{
    ignore_unused(engine);

    if (dist < 0) {
        throw ScriptException("Trace distance must be non-negative");
    }

    if (startOffset.x < std::numeric_limits<int16_t>::min() || startOffset.x > std::numeric_limits<int16_t>::max() || //
        startOffset.y < std::numeric_limits<int16_t>::min() || startOffset.y > std::numeric_limits<int16_t>::max() || //
        targetOffset.x < std::numeric_limits<int16_t>::min() || targetOffset.x > std::numeric_limits<int16_t>::max() || //
        targetOffset.y < std::numeric_limits<int16_t>::min() || targetOffset.y > std::numeric_limits<int16_t>::max()) {
        throw ScriptException("Hex offset arg out of range", startOffset, targetOffset);
    }

    ipos16 start_offset = ipos16 {numeric_cast<int16_t>(startOffset.x), numeric_cast<int16_t>(startOffset.y)};
    ipos16 target_offset = ipos16 {numeric_cast<int16_t>(targetOffset.x), numeric_cast<int16_t>(targetOffset.y)};
    LineTracer tracer(fromHex, toHex, dirAngleOffset, mapSize, start_offset, target_offset);

    vector<mpos> line;
    line.reserve(numeric_cast<size_t>(dist));

    mpos cur_hex = fromHex;

    for (int32_t i = 0; i < dist; i++) {
        mpos prev_hex = cur_hex;

        if (!tracer.GetNextHex(cur_hex).has_value()) {
            break;
        }

        if (cur_hex == prev_hex) {
            break;
        }

        line.emplace_back(cur_hex);
    }

    return line;
}

// Traces up to a nonnegative number of in-bounds hexes at an explicit angle, excludes the start hex, and writes the last reached hex or the start when no step succeeds.
///@ ExportMethod
FO_SCRIPT_API vector<mpos> Common_Game_TraceHexLine(ptr<BaseEngine> engine, msize mapSize, mpos fromHex, float32_t dirAngle, int32_t dist, ipos32 startOffset, ipos32 targetOffset, mpos& targetHex)
{
    ignore_unused(engine);

    if (dist < 0) {
        throw ScriptException("Trace distance must be non-negative");
    }

    if (startOffset.x < std::numeric_limits<int16_t>::min() || startOffset.x > std::numeric_limits<int16_t>::max() || //
        startOffset.y < std::numeric_limits<int16_t>::min() || startOffset.y > std::numeric_limits<int16_t>::max() || //
        targetOffset.x < std::numeric_limits<int16_t>::min() || targetOffset.x > std::numeric_limits<int16_t>::max() || //
        targetOffset.y < std::numeric_limits<int16_t>::min() || targetOffset.y > std::numeric_limits<int16_t>::max()) {
        throw ScriptException("Hex offset arg out of range", startOffset, targetOffset);
    }

    ipos16 start_offset = ipos16 {numeric_cast<int16_t>(startOffset.x), numeric_cast<int16_t>(startOffset.y)};
    ipos16 target_offset = ipos16 {numeric_cast<int16_t>(targetOffset.x), numeric_cast<int16_t>(targetOffset.y)};

    LineTracer tracer(fromHex, dirAngle, dist, mapSize, start_offset, target_offset);

    vector<mpos> line;
    line.reserve(numeric_cast<size_t>(dist));

    mpos cur_hex = fromHex;

    for (int32_t i = 0; i < dist; i++) {
        mpos prev_hex = cur_hex;

        if (!tracer.GetNextHex(cur_hex).has_value()) {
            break;
        }

        if (cur_hex == prev_hex) {
            break;
        }

        line.emplace_back(cur_hex);
    }

    targetHex = line.empty() ? fromHex : line.back();
    return line;
}

// Returns text currently provided by the host application's clipboard integration.
///@ ExportMethod
FO_SCRIPT_API string Common_Game_GetClipboardText(ptr<BaseEngine> engine)
{
    ignore_unused(engine);

    return string {GetApp()->Input.GetClipboardText()};
}

// Replaces text in the host application's clipboard through its platform integration.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetClipboardText(ptr<BaseEngine> engine, string_view text)
{
    ignore_unused(engine);

    return GetApp()->Input.SetClipboardText(text);
}

// Returns the registered item prototype for the supplied id; an unknown id throws and can be tested first with CheckProtoItem.
///@ ExportMethod
FO_SCRIPT_API ptr<ProtoItem> Common_Game_GetProtoItem(ptr<BaseEngine> engine, hstring pid)
{
    auto proto = engine->GetProtoItem(pid);

    if (!proto) {
        throw ScriptException("Item proto not found (check CheckProtoItem first)", pid);
    }

    return make_ptr(const_cast<ProtoItem*>(std::addressof(*proto)));
}

// Returns whether an item prototype with the supplied id is registered.
///@ ExportMethod
FO_SCRIPT_API bool Common_Game_CheckProtoItem(ptr<BaseEngine> engine, hstring pid)
{
    return !!engine->GetProtoItem(pid);
}

// Returns handles to every registered item prototype.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<ProtoItem>> Common_Game_GetProtoItems(ptr<BaseEngine> engine)
{
    const auto& protos = engine->GetProtoItems();

    vector<ptr<const ProtoItem>> result;
    result.reserve(protos.size());

    for (ptr<const ProtoItem> proto : protos | std::views::values) {
        result.emplace_back(proto);
    }

    return MakeMutableScriptHandleVector<ProtoItem>(result);
}

// Returns registered item prototypes whose selected integer-convertible property equals the supplied value.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<ProtoItem>> Common_Game_GetProtoItems(ptr<BaseEngine> engine, ItemProperty property, int32_t propertyValue)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<ItemProperties>(engine, property);
    const auto& protos = engine->GetProtoItems();

    vector<ptr<const ProtoItem>> result;
    result.reserve(protos.size());

    for (ptr<const ProtoItem> proto : protos | std::views::values) {
        if (proto->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(proto);
        }
    }

    return MakeMutableScriptHandleVector<ProtoItem>(result);
}

// Returns the registered critter prototype for the supplied id; an unknown id throws and can be tested first with CheckProtoCritter.
///@ ExportMethod
FO_SCRIPT_API ptr<ProtoCritter> Common_Game_GetProtoCritter(ptr<BaseEngine> engine, hstring pid)
{
    auto proto = engine->GetProtoCritter(pid);

    if (!proto) {
        throw ScriptException("Critter proto not found (check CheckProtoCritter first)", pid);
    }

    return make_ptr(const_cast<ProtoCritter*>(std::addressof(*proto)));
}

// Returns whether a critter prototype with the supplied id is registered.
///@ ExportMethod
FO_SCRIPT_API bool Common_Game_CheckProtoCritter(ptr<BaseEngine> engine, hstring pid)
{
    return !!engine->GetProtoCritter(pid);
}

// Returns handles to every registered critter prototype.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<ProtoCritter>> Common_Game_GetProtoCritters(ptr<BaseEngine> engine)
{
    const auto& protos = engine->GetProtoCritters();

    vector<ptr<const ProtoCritter>> result;
    result.reserve(protos.size());

    for (ptr<const ProtoCritter> proto : protos | std::views::values) {
        result.emplace_back(proto);
    }

    return MakeMutableScriptHandleVector<ProtoCritter>(result);
}

// Returns registered critter prototypes whose selected integer-convertible property equals the supplied value.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<ProtoCritter>> Common_Game_GetProtoCritters(ptr<BaseEngine> engine, CritterProperty property, int32_t propertyValue)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<CritterProperties>(engine, property);
    const auto& protos = engine->GetProtoCritters();

    vector<ptr<const ProtoCritter>> result;
    result.reserve(protos.size());

    for (ptr<const ProtoCritter> proto : protos | std::views::values) {
        if (proto->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(proto);
        }
    }

    return MakeMutableScriptHandleVector<ProtoCritter>(result);
}

// Returns the registered map prototype for the supplied id; an unknown id throws and can be tested first with CheckProtoMap.
///@ ExportMethod
FO_SCRIPT_API ptr<ProtoMap> Common_Game_GetProtoMap(ptr<BaseEngine> engine, hstring pid)
{
    auto proto = engine->GetProtoMap(pid);

    if (!proto) {
        throw ScriptException("Map proto not found (check CheckProtoMap first)", pid);
    }

    return make_ptr(const_cast<ProtoMap*>(std::addressof(*proto)));
}

// Returns whether a map prototype with the supplied id is registered.
///@ ExportMethod
FO_SCRIPT_API bool Common_Game_CheckProtoMap(ptr<BaseEngine> engine, hstring pid)
{
    return !!engine->GetProtoMap(pid);
}

// Returns handles to every registered map prototype.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<ProtoMap>> Common_Game_GetProtoMaps(ptr<BaseEngine> engine)
{
    const auto& protos = engine->GetProtoMaps();

    vector<ptr<const ProtoMap>> result;
    result.reserve(protos.size());

    for (ptr<const ProtoMap> proto : protos | std::views::values) {
        result.emplace_back(proto);
    }

    return MakeMutableScriptHandleVector<ProtoMap>(result);
}

// Returns registered map prototypes whose selected integer-convertible property equals the supplied value.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<ProtoMap>> Common_Game_GetProtoMaps(ptr<BaseEngine> engine, MapProperty property, int32_t propertyValue)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<MapProperties>(engine, property);
    const auto& protos = engine->GetProtoMaps();

    vector<ptr<const ProtoMap>> result;
    result.reserve(protos.size());

    for (ptr<const ProtoMap> proto : protos | std::views::values) {
        if (proto->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(proto);
        }
    }

    return MakeMutableScriptHandleVector<ProtoMap>(result);
}

// Returns the registered location prototype for the supplied id; an unknown id throws and can be tested first with CheckProtoLocation.
///@ ExportMethod
FO_SCRIPT_API ptr<ProtoLocation> Common_Game_GetProtoLocation(ptr<BaseEngine> engine, hstring pid)
{
    auto proto = engine->GetProtoLocation(pid);

    if (!proto) {
        throw ScriptException("Location proto not found (check CheckProtoLocation first)", pid);
    }

    return make_ptr(const_cast<ProtoLocation*>(std::addressof(*proto)));
}

// Returns whether a location prototype with the supplied id is registered.
///@ ExportMethod
FO_SCRIPT_API bool Common_Game_CheckProtoLocation(ptr<BaseEngine> engine, hstring pid)
{
    return !!engine->GetProtoLocation(pid);
}

// Returns handles to every registered location prototype.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<ProtoLocation>> Common_Game_GetProtoLocations(ptr<BaseEngine> engine)
{
    const auto& protos = engine->GetProtoLocations();

    vector<ptr<const ProtoLocation>> result;
    result.reserve(protos.size());

    for (ptr<const ProtoLocation> proto : protos | std::views::values) {
        result.emplace_back(proto);
    }

    return MakeMutableScriptHandleVector<ProtoLocation>(result);
}

// Returns registered location prototypes whose selected integer-convertible property equals the supplied value.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<ProtoLocation>> Common_Game_GetProtoLocations(ptr<BaseEngine> engine, LocationProperty property, int32_t propertyValue)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<LocationProperties>(engine, property);
    const auto& protos = engine->GetProtoLocations();

    vector<ptr<const ProtoLocation>> result;
    result.reserve(protos.size());

    for (ptr<const ProtoLocation> proto : protos | std::views::values) {
        if (proto->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(proto);
        }
    }

    return MakeMutableScriptHandleVector<ProtoLocation>(result);
}

// Returns the current high-resolution monotonic time point for measuring process-local elapsed time.
///@ ExportMethod
FO_SCRIPT_API nanotime Common_Game_GetPrecisionTime(ptr<BaseEngine> engine)
{
    ignore_unused(engine);

    return nanotime::now();
}

// Converts a local calendar date and subsecond fields into the corresponding high-resolution time point; an invalid date throws.
///@ ExportMethod
FO_SCRIPT_API nanotime Common_Game_PackTime(ptr<BaseEngine> engine, int32_t year, int32_t month, int32_t day, int32_t hour, int32_t minute, int32_t second, int32_t millisecond, int32_t microsecond, int32_t nanosecond)
{
    ignore_unused(engine);

    return nanotime::now() + make_time_offset(year, month, day, hour, minute, second, millisecond, microsecond, nanosecond, true);
}

// Decomposes a high-resolution time point into local calendar and subsecond fields.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_UnpackTime(ptr<BaseEngine> engine, nanotime time, int32_t& year, int32_t& month, int32_t& day, int32_t& hour, int32_t& minute, int32_t& second, int32_t& millisecond, int32_t& microsecond, int32_t& nanosecond)
{
    ignore_unused(engine);

    time_desc_t time_desc = time.desc(true);
    year = time_desc.year;
    month = time_desc.month;
    day = time_desc.day;
    hour = time_desc.hour;
    minute = time_desc.minute;
    second = time_desc.second;
    millisecond = time_desc.millisecond;
    microsecond = time_desc.microsecond;
    nanosecond = time_desc.nanosecond;
}

// Converts a local calendar date through milliseconds into the Engine synchronized-time domain; an invalid date throws.
///@ ExportMethod
FO_SCRIPT_API synctime Common_Game_PackSynchronizedTime(ptr<BaseEngine> engine, int32_t year, int32_t month, int32_t day, int32_t hour, int32_t minute, int32_t second, int32_t millisecond)
{
    return engine->GameTime.GetSynchronizedTime() + make_time_offset(year, month, day, hour, minute, second, millisecond, 0, 0, true);
}

// Decomposes an Engine synchronized time point into local calendar fields through milliseconds.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_UnpackSynchronizedTime(ptr<BaseEngine> engine, synctime time, int32_t& year, int32_t& month, int32_t& day, int32_t& hour, int32_t& minute, int32_t& second, int32_t& millisecond)
{
    time_desc_t time_desc = make_time_desc(time - engine->GameTime.GetSynchronizedTime(), true);
    year = time_desc.year;
    month = time_desc.month;
    day = time_desc.day;
    hour = time_desc.hour;
    minute = time_desc.minute;
    second = time_desc.second;
    millisecond = time_desc.millisecond;
}

// Schedules a one-shot time event on this Engine instance after the requested delay and returns its event id.
///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, ScriptFunc<void> func)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, {}, {});
}

// Schedules a one-shot Engine time event, passes one payload value directly to the callback, and returns its event id.
///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, ScriptFunc<void, any_t> func, any_t data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, {}, vector<any_t> {std::move(data)});
}

// Schedules a one-shot Engine time event, passes the payload array directly to the callback, and returns its event id.
///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, ScriptFunc<void, vector<any_t>> func, readonly_vector<any_t> data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, {}, to_vector(data));
}

// Schedules a one-shot Engine time event whose callback receives a context for inspecting or modifying that event, and returns its id.
///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, ScriptFunc<void, ptr<TimeEventContext>> func)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, {}, {});
}

// Schedules a one-shot Engine time event whose callback context exposes one payload value, and returns its event id.
///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, ScriptFunc<void, ptr<TimeEventContext>> func, any_t data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, {}, vector<any_t> {std::move(data)});
}

// Schedules a one-shot Engine time event whose callback context exposes the payload array, and returns its event id.
///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, ScriptFunc<void, ptr<TimeEventContext>> func, readonly_vector<any_t> data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, {}, to_vector(data));
}

// Returns the language configured for this Engine instance as a hashed LanguageName.
///@ ExportMethod
FO_SCRIPT_API LanguageName Common_Game_GetLanguage(ptr<BaseEngine> engine)
{
    return LanguageName {engine->Hashes.ToHashedString(engine->Settings->Language)};
}

// Schedules a repeating time event on this Engine instance, with its first firing after delay and later firings at repeat, and returns its id.
///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, timespan repeat, ScriptFunc<void> func)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, repeat, {});
}

// Schedules a repeating Engine time event with one direct callback payload value and returns its event id.
///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, timespan repeat, ScriptFunc<void, any_t> func, any_t data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, repeat, vector<any_t> {std::move(data)});
}

// Schedules a repeating Engine time event with a direct callback payload array and returns its event id.
///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, timespan repeat, ScriptFunc<void, vector<any_t>> func, readonly_vector<any_t> data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, repeat, to_vector(data));
}

// Schedules a repeating Engine time event whose callback receives a context for inspecting or modifying that event, and returns its id.
///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, timespan repeat, ScriptFunc<void, ptr<TimeEventContext>> func)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, repeat, {});
}

// Schedules a repeating Engine time event whose callback context exposes one payload value, and returns its event id.
///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, timespan repeat, ScriptFunc<void, ptr<TimeEventContext>> func, any_t data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, repeat, vector<any_t> {std::move(data)});
}

// Schedules a repeating Engine time event whose callback context exposes the payload array, and returns its event id.
///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, timespan repeat, ScriptFunc<void, ptr<TimeEventContext>> func, readonly_vector<any_t> data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, repeat, to_vector(data));
}

// Counts all time events on this Engine instance that use the selected callback.
///@ ExportMethod
FO_SCRIPT_API int32_t Common_Game_CountTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void> func)
{
    return numeric_cast<int32_t>(engine->TimeEventMngr.CountTimeEvent(engine, func.GetName(), {}));
}

// Counts all time events on this Engine instance that use the selected callback.
///@ ExportMethod
FO_SCRIPT_API int32_t Common_Game_CountTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, any_t> func)
{
    return numeric_cast<int32_t>(engine->TimeEventMngr.CountTimeEvent(engine, func.GetName(), {}));
}

// Counts all time events on this Engine instance that use the selected callback.
///@ ExportMethod
FO_SCRIPT_API int32_t Common_Game_CountTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, vector<any_t>> func)
{
    return numeric_cast<int32_t>(engine->TimeEventMngr.CountTimeEvent(engine, func.GetName(), {}));
}

// Counts all time events on this Engine instance that use the selected context callback.
///@ ExportMethod
FO_SCRIPT_API int32_t Common_Game_CountTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, ptr<TimeEventContext>> func)
{
    return numeric_cast<int32_t>(engine->TimeEventMngr.CountTimeEvent(engine, func.GetName(), {}));
}

// Returns one when this Engine instance owns a time event with the supplied id, or zero when it is absent.
///@ ExportMethod
FO_SCRIPT_API int32_t Common_Game_CountTimeEvent(ptr<BaseEngine> engine, uint32_t id)
{
    return numeric_cast<int32_t>(engine->TimeEventMngr.CountTimeEvent(engine, {}, id));
}

// Stops all time events on this Engine instance that use the selected callback; does nothing when none match.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void> func)
{
    engine->TimeEventMngr.StopTimeEvent(engine, func.GetName(), {});
}

// Stops all time events on this Engine instance that use the selected callback; does nothing when none match.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, any_t> func)
{
    engine->TimeEventMngr.StopTimeEvent(engine, func.GetName(), {});
}

// Stops all time events on this Engine instance that use the selected callback; does nothing when none match.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, vector<any_t>> func)
{
    engine->TimeEventMngr.StopTimeEvent(engine, func.GetName(), {});
}

// Stops all time events on this Engine instance that use the selected context callback; does nothing when none match.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, ptr<TimeEventContext>> func)
{
    engine->TimeEventMngr.StopTimeEvent(engine, func.GetName(), {});
}

// Stops the time event with this id on the Engine instance; does nothing when it is absent.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(ptr<BaseEngine> engine, uint32_t id)
{
    engine->TimeEventMngr.StopTimeEvent(engine, {}, id);
}

// Sets the repeat interval for every matching callback event and reschedules each next firing from now; does nothing when none match.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void> func, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func.GetName(), {}, repeat, std::nullopt);
}

// Sets the repeat interval for every matching callback event and reschedules each next firing from now; does nothing when none match.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, any_t> func, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func.GetName(), {}, repeat, std::nullopt);
}

// Sets the repeat interval for every matching callback event and reschedules each next firing from now; does nothing when none match.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, vector<any_t>> func, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func.GetName(), {}, repeat, std::nullopt);
}

// Sets the repeat interval for every matching context-callback event and reschedules each next firing from now; does nothing when none match.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, ptr<TimeEventContext>> func, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func.GetName(), {}, repeat, std::nullopt);
}

// Sets the repeat interval for the event with this id and reschedules its next firing from now; does nothing when it is absent.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(ptr<BaseEngine> engine, uint32_t id, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, {}, id, repeat, std::nullopt);
}

// Replaces the single direct payload value for every time event using the selected callback; does nothing when none match.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(ptr<BaseEngine> engine, ScriptFunc<void> func, any_t data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func.GetName(), {}, {}, vector<any_t> {std::move(data)});
}

// Replaces the direct payload array for every time event using the selected callback; does nothing when none match.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(ptr<BaseEngine> engine, ScriptFunc<void, vector<any_t>> func, readonly_vector<any_t> data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func.GetName(), {}, {}, to_vector(data));
}

// Replaces the single context payload value for every time event using the selected callback; does nothing when none match.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(ptr<BaseEngine> engine, ScriptFunc<void, ptr<TimeEventContext>> func, any_t data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func.GetName(), {}, {}, vector<any_t> {std::move(data)});
}

// Replaces the context payload array for every time event using the selected callback; does nothing when none match.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(ptr<BaseEngine> engine, ScriptFunc<void, ptr<TimeEventContext>> func, readonly_vector<any_t> data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func.GetName(), {}, {}, to_vector(data));
}

// Replaces the single payload value for the time event with this id; does nothing when it is absent.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(ptr<BaseEngine> engine, uint32_t id, any_t data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, {}, id, {}, vector<any_t> {std::move(data)});
}

// Replaces the payload array for the time event with this id; does nothing when it is absent.
///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(ptr<BaseEngine> engine, uint32_t id, readonly_vector<any_t> data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, {}, id, {}, to_vector(data));
}

FO_END_NAMESPACE
