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

///@ ExportMethod
FO_SCRIPT_API void Common_Game_BreakIntoDebugger(ptr<BaseEngine> engine)
{
    ignore_unused(engine);

    BreakIntoDebugger();
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_Log(ptr<BaseEngine> engine, string_view text)
{
    ignore_unused(engine);

    WriteLog("{}", text);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RequestQuit(ptr<BaseEngine> engine, bool success = true)
{
    ignore_unused(engine);

    GetApp()->RequestQuit(success);
}

///@ ExportMethod
FO_SCRIPT_API bool Common_Game_IsResourcePresent(ptr<BaseEngine> engine, string_view resourcePath)
{
    return engine->Resources.IsFileExists(resourcePath);
}

///@ ExportMethod
FO_SCRIPT_API string Common_Game_ReadResource(ptr<BaseEngine> engine, string_view resourcePath)
{
    return engine->Resources.ReadFileText(resourcePath);
}

///@ ExportMethod
FO_SCRIPT_API map<string, string> Common_Game_ReadConfigSection(ptr<BaseEngine> engine, string_view resourcePath, string_view sectionName)
{
    string content = engine->Resources.ReadFileText(resourcePath);
    ConfigFile config(resourcePath, std::move(content));

    map<string, string> result;

    if (!config.HasSection(sectionName)) {
        return result;
    }

    for (const auto& [key, value] : config.GetSection(sectionName)) {
        result.emplace(string(key), string(value));
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API timespan Common_Game_GetModelAnimDuration(ptr<BaseEngine> engine, hstring modelName, CritterStateAnim stateAnim, CritterActionAnim actionAnim)
{
#if FO_ENABLE_3D
    const auto anim_info = engine->GetAnimInfo(modelName);

    if (!anim_info) {
        return {};
    }

    if (!anim_info->Model.has_value()) {
        return {};
    }

    const ModelAnimInfo& model_anim_info = *anim_info->Model;
    const auto anim_it = model_anim_info.AnimationDurations.find({stateAnim, actionAnim});
    return anim_it != model_anim_info.AnimationDurations.end() ? anim_it->second : timespan {};
#else
    ignore_unused(engine, modelName, stateAnim, actionAnim);
    return {};
#endif
}

///@ ExportMethod
FO_SCRIPT_API int32_t Common_Game_Random(ptr<BaseEngine> engine, int32_t minValue, int32_t maxValue)
{
    return engine->Random(minValue, maxValue);
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_DecodeUtf8(ptr<BaseEngine> engine, string_view text, int32_t& length)
{
    ignore_unused(engine);

    size_t decode_length = text.length();
    const auto ch = utf8::Decode(text.data(), decode_length); // NOLINT(bugprone-suspicious-stringview-data-usage)

    length = numeric_cast<int32_t>(decode_length);
    return ch;
}

///@ ExportMethod
FO_SCRIPT_API string Common_Game_EncodeUtf8(ptr<BaseEngine> engine, uint32_t ucs)
{
    ignore_unused(engine);

    char buf[4];
    const auto len = utf8::Encode(ucs, buf);
    return {buf, len};
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_OpenLink(ptr<BaseEngine> engine, string_view link)
{
    ignore_unused(engine);

    GetApp()->OpenLink(link);
}

///@ ExportMethod
FO_SCRIPT_API uint64_t Common_Game_GetUnixTime(ptr<BaseEngine> engine)
{
    ignore_unused(engine);

    return numeric_cast<uint64_t>(::time(nullptr));
}

///@ ExportMethod
FO_SCRIPT_API int32_t Common_Game_GetDistance(ptr<BaseEngine> engine, mpos hex1, mpos hex2)
{
    ignore_unused(engine);

    return GeometryHelper::GetDistance(hex1, hex2);
}

///@ ExportMethod
FO_SCRIPT_API mdir Common_Game_GetDirection(ptr<BaseEngine> engine, mpos fromHex, mpos toHex, float32_t offset = 0.0f)
{
    ignore_unused(engine);

    return mdir(iround<int32_t>(GeometryHelper::GetDirAngle(fromHex, toHex) + offset));
}

///@ ExportMethod
FO_SCRIPT_API mdir Common_Game_GetLineDirAngle(ptr<BaseEngine> engine, ipos32 fromPos, ipos32 toPos)
{
    ignore_unused(engine);

    return mdir(iround<int32_t>(GeometryHelper::GetLineDirAngle(fromPos.x, fromPos.y, toPos.x, toPos.y)));
}

///@ ExportMethod
FO_SCRIPT_API mdir Common_Game_RotateDirAngle(ptr<BaseEngine> engine, mdir dir, bool clockwise, int16_t step)
{
    ignore_unused(engine);

    auto rotated = dir.angle();

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

///@ ExportMethod
FO_SCRIPT_API int16_t Common_Game_GetDirAngleDiff(ptr<BaseEngine> engine, mdir dir1, mdir dir2)
{
    ignore_unused(engine);

    return numeric_cast<int16_t>(iround<int32_t>(GeometryHelper::GetDirAngleDiff(dir1.angle(), dir2.angle())));
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_GetHexInterval(ptr<BaseEngine> engine, mpos fromHex, mpos toHex, ipos32& hexOffset)
{
    ignore_unused(engine);

    hexOffset = GeometryHelper::GetHexOffset(fromHex, toHex);
}

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

    const auto start_offset = ipos16 {numeric_cast<int16_t>(startOffset.x), numeric_cast<int16_t>(startOffset.y)};
    const auto target_offset = ipos16 {numeric_cast<int16_t>(targetOffset.x), numeric_cast<int16_t>(targetOffset.y)};
    LineTracer tracer(fromHex, toHex, dirAngleOffset, mapSize, start_offset, target_offset);

    vector<mpos> line;
    line.reserve(numeric_cast<size_t>(dist));

    auto cur_hex = fromHex;

    for (auto i = 0; i < dist; i++) {
        const auto prev_hex = cur_hex;

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

    const auto start_offset = ipos16 {numeric_cast<int16_t>(startOffset.x), numeric_cast<int16_t>(startOffset.y)};
    const auto target_offset = ipos16 {numeric_cast<int16_t>(targetOffset.x), numeric_cast<int16_t>(targetOffset.y)};

    LineTracer tracer(fromHex, dirAngle, dist, mapSize, start_offset, target_offset);

    vector<mpos> line;
    line.reserve(numeric_cast<size_t>(dist));

    auto cur_hex = fromHex;

    for (auto i = 0; i < dist; i++) {
        const auto prev_hex = cur_hex;

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

///@ ExportMethod
FO_SCRIPT_API string Common_Game_GetClipboardText(ptr<BaseEngine> engine)
{
    ignore_unused(engine);

    return string {GetApp()->Input.GetClipboardText()};
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetClipboardText(ptr<BaseEngine> engine, string_view text)
{
    ignore_unused(engine);

    return GetApp()->Input.SetClipboardText(text);
}

///@ ExportMethod
FO_SCRIPT_API ptr<ProtoItem> Common_Game_GetProtoItem(ptr<BaseEngine> engine, hstring pid)
{
    auto proto = engine->GetProtoItem(pid);

    if (!proto) {
        throw ScriptException("Item proto not found (check CheckProtoItem first)", pid);
    }

    return make_ptr(const_cast<ProtoItem*>(std::addressof(*proto)));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_Game_CheckProtoItem(ptr<BaseEngine> engine, hstring pid)
{
    return !!engine->GetProtoItem(pid);
}

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

///@ ExportMethod
FO_SCRIPT_API ptr<ProtoCritter> Common_Game_GetProtoCritter(ptr<BaseEngine> engine, hstring pid)
{
    auto proto = engine->GetProtoCritter(pid);

    if (!proto) {
        throw ScriptException("Critter proto not found (check CheckProtoCritter first)", pid);
    }

    return make_ptr(const_cast<ProtoCritter*>(std::addressof(*proto)));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_Game_CheckProtoCritter(ptr<BaseEngine> engine, hstring pid)
{
    return !!engine->GetProtoCritter(pid);
}

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

///@ ExportMethod
FO_SCRIPT_API ptr<ProtoMap> Common_Game_GetProtoMap(ptr<BaseEngine> engine, hstring pid)
{
    auto proto = engine->GetProtoMap(pid);

    if (!proto) {
        throw ScriptException("Map proto not found (check CheckProtoMap first)", pid);
    }

    return make_ptr(const_cast<ProtoMap*>(std::addressof(*proto)));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_Game_CheckProtoMap(ptr<BaseEngine> engine, hstring pid)
{
    return !!engine->GetProtoMap(pid);
}

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

///@ ExportMethod
FO_SCRIPT_API ptr<ProtoLocation> Common_Game_GetProtoLocation(ptr<BaseEngine> engine, hstring pid)
{
    auto proto = engine->GetProtoLocation(pid);

    if (!proto) {
        throw ScriptException("Location proto not found (check CheckProtoLocation first)", pid);
    }

    return make_ptr(const_cast<ProtoLocation*>(std::addressof(*proto)));
}

///@ ExportMethod
FO_SCRIPT_API bool Common_Game_CheckProtoLocation(ptr<BaseEngine> engine, hstring pid)
{
    return !!engine->GetProtoLocation(pid);
}

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

///@ ExportMethod
FO_SCRIPT_API nanotime Common_Game_GetPrecisionTime(ptr<BaseEngine> engine)
{
    ignore_unused(engine);

    return nanotime::now();
}

///@ ExportMethod
FO_SCRIPT_API nanotime Common_Game_PackTime(ptr<BaseEngine> engine, int32_t year, int32_t month, int32_t day, int32_t hour, int32_t minute, int32_t second, int32_t millisecond, int32_t microsecond, int32_t nanosecond)
{
    ignore_unused(engine);

    return nanotime::now() + make_time_offset(year, month, day, hour, minute, second, millisecond, microsecond, nanosecond, true);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_UnpackTime(ptr<BaseEngine> engine, nanotime time, int32_t& year, int32_t& month, int32_t& day, int32_t& hour, int32_t& minute, int32_t& second, int32_t& millisecond, int32_t& microsecond, int32_t& nanosecond)
{
    ignore_unused(engine);

    const auto time_desc = time.desc(true);
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

///@ ExportMethod
FO_SCRIPT_API synctime Common_Game_PackSynchronizedTime(ptr<BaseEngine> engine, int32_t year, int32_t month, int32_t day, int32_t hour, int32_t minute, int32_t second, int32_t millisecond)
{
    return engine->GameTime.GetSynchronizedTime() + make_time_offset(year, month, day, hour, minute, second, millisecond, 0, 0, true);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_UnpackSynchronizedTime(ptr<BaseEngine> engine, synctime time, int32_t& year, int32_t& month, int32_t& day, int32_t& hour, int32_t& minute, int32_t& second, int32_t& millisecond)
{
    const auto time_desc = make_time_desc(time - engine->GameTime.GetSynchronizedTime(), true);
    year = time_desc.year;
    month = time_desc.month;
    day = time_desc.day;
    hour = time_desc.hour;
    minute = time_desc.minute;
    second = time_desc.second;
    millisecond = time_desc.millisecond;
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, ScriptFunc<void> func)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, {}, {});
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, ScriptFunc<void, any_t> func, any_t data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, ScriptFunc<void, vector<any_t>> func, readonly_vector<any_t> data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, {}, to_vector(data));
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, ScriptFunc<void, ptr<TimeEventContext>> func)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, {}, {});
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, ScriptFunc<void, ptr<TimeEventContext>> func, any_t data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, ScriptFunc<void, ptr<TimeEventContext>> func, readonly_vector<any_t> data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, {}, to_vector(data));
}

///@ ExportMethod
FO_SCRIPT_API LanguageName Common_Game_GetLanguage(ptr<BaseEngine> engine)
{
    return LanguageName {engine->Hashes.ToHashedString(engine->Settings->Language)};
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, timespan repeat, ScriptFunc<void> func)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, repeat, {});
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, timespan repeat, ScriptFunc<void, any_t> func, any_t data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, repeat, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, timespan repeat, ScriptFunc<void, vector<any_t>> func, readonly_vector<any_t> data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, repeat, to_vector(data));
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, timespan repeat, ScriptFunc<void, ptr<TimeEventContext>> func)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, repeat, {});
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, timespan repeat, ScriptFunc<void, ptr<TimeEventContext>> func, any_t data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, repeat, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Common_Game_StartTimeEvent(ptr<BaseEngine> engine, timespan delay, timespan repeat, ScriptFunc<void, ptr<TimeEventContext>> func, readonly_vector<any_t> data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, std::move(func), delay, repeat, to_vector(data));
}

///@ ExportMethod
FO_SCRIPT_API int32_t Common_Game_CountTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void> func)
{
    return numeric_cast<int32_t>(engine->TimeEventMngr.CountTimeEvent(engine, func.GetName(), {}));
}

///@ ExportMethod
FO_SCRIPT_API int32_t Common_Game_CountTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, any_t> func)
{
    return numeric_cast<int32_t>(engine->TimeEventMngr.CountTimeEvent(engine, func.GetName(), {}));
}

///@ ExportMethod
FO_SCRIPT_API int32_t Common_Game_CountTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, vector<any_t>> func)
{
    return numeric_cast<int32_t>(engine->TimeEventMngr.CountTimeEvent(engine, func.GetName(), {}));
}

///@ ExportMethod
FO_SCRIPT_API int32_t Common_Game_CountTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, ptr<TimeEventContext>> func)
{
    return numeric_cast<int32_t>(engine->TimeEventMngr.CountTimeEvent(engine, func.GetName(), {}));
}

///@ ExportMethod
FO_SCRIPT_API int32_t Common_Game_CountTimeEvent(ptr<BaseEngine> engine, uint32_t id)
{
    return numeric_cast<int32_t>(engine->TimeEventMngr.CountTimeEvent(engine, {}, id));
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void> func)
{
    engine->TimeEventMngr.StopTimeEvent(engine, func.GetName(), {});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, any_t> func)
{
    engine->TimeEventMngr.StopTimeEvent(engine, func.GetName(), {});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, vector<any_t>> func)
{
    engine->TimeEventMngr.StopTimeEvent(engine, func.GetName(), {});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, ptr<TimeEventContext>> func)
{
    engine->TimeEventMngr.StopTimeEvent(engine, func.GetName(), {});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(ptr<BaseEngine> engine, uint32_t id)
{
    engine->TimeEventMngr.StopTimeEvent(engine, {}, id);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void> func, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func.GetName(), {}, repeat, std::nullopt);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, any_t> func, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func.GetName(), {}, repeat, std::nullopt);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, vector<any_t>> func, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func.GetName(), {}, repeat, std::nullopt);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(ptr<BaseEngine> engine, ScriptFunc<void, ptr<TimeEventContext>> func, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func.GetName(), {}, repeat, std::nullopt);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(ptr<BaseEngine> engine, uint32_t id, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, {}, id, repeat, std::nullopt);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(ptr<BaseEngine> engine, ScriptFunc<void> func, any_t data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func.GetName(), {}, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(ptr<BaseEngine> engine, ScriptFunc<void, vector<any_t>> func, readonly_vector<any_t> data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func.GetName(), {}, {}, to_vector(data));
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(ptr<BaseEngine> engine, ScriptFunc<void, ptr<TimeEventContext>> func, any_t data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func.GetName(), {}, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(ptr<BaseEngine> engine, ScriptFunc<void, ptr<TimeEventContext>> func, readonly_vector<any_t> data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func.GetName(), {}, {}, to_vector(data));
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(ptr<BaseEngine> engine, uint32_t id, any_t data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, {}, id, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(ptr<BaseEngine> engine, uint32_t id, readonly_vector<any_t> data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, {}, id, {}, to_vector(data));
}

FO_END_NAMESPACE
