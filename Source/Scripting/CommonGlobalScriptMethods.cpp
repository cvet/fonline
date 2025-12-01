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

#include "Common.h"

#include "Application.h"
#include "EngineBase.h"
#include "Geometry.h"
#include "ScriptSystem.h"
#include "Version-Include.h"

FO_BEGIN_NAMESPACE();

///@ ExportMethod
FO_SCRIPT_API void Common_Game_BreakIntoDebugger(BaseEngine* engine)
{
    ignore_unused(engine);

    BreakIntoDebugger();
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_Log(BaseEngine* engine, string_view text)
{
    ignore_unused(engine);

    const auto* st_entry = GetStackTraceEntry(1);

    if (st_entry != nullptr) {
        const string module_name = strex(st_entry->file).extract_file_name().erase_file_extension();

        WriteLog("{}: {}", module_name, text);
    }
    else {
        WriteLog("{}", text);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RequestQuit(BaseEngine* engine)
{
    ignore_unused(engine);

    App->RequestQuit();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_Game_IsResourcePresent(BaseEngine* engine, string_view resourcePath)
{
    ignore_unused(engine);

    return engine->Resources.IsFileExists(resourcePath);
}

///@ ExportMethod
FO_SCRIPT_API string Common_Game_ReadResource(BaseEngine* engine, string_view resourcePath)
{
    ignore_unused(engine);

    return engine->Resources.ReadFileText(resourcePath);
}

///@ ExportMethod
FO_SCRIPT_API int32 Common_Game_Random(BaseEngine* engine, int32 minValue, int32 maxValue)
{
    ignore_unused(engine);

    return GenericUtils::Random(minValue, maxValue);
}

///@ ExportMethod
FO_SCRIPT_API uint32 Common_Game_DecodeUtf8(BaseEngine* engine, string_view text, int32& length)
{
    ignore_unused(engine);

    size_t decode_length = text.length();
    const auto ch = utf8::Decode(text.data(), decode_length); // NOLINT(bugprone-suspicious-stringview-data-usage)

    length = numeric_cast<int32>(decode_length);
    return ch;
}

///@ ExportMethod
FO_SCRIPT_API string Common_Game_EncodeUtf8(BaseEngine* engine, uint32 ucs)
{
    ignore_unused(engine);

    char buf[4];
    const auto len = utf8::Encode(ucs, buf);
    return {buf, len};
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_OpenLink(BaseEngine* engine, string_view link)
{
    ignore_unused(engine);

    App->OpenLink(link);
}

///@ ExportMethod
FO_SCRIPT_API uint64 Common_Game_GetUnixTime(BaseEngine* engine)
{
    ignore_unused(engine);

    return numeric_cast<uint64>(::time(nullptr));
}

///@ ExportMethod
FO_SCRIPT_API int32 Common_Game_GetDistance(BaseEngine* engine, mpos hex1, mpos hex2)
{
    ignore_unused(engine);

    return GeometryHelper::GetDistance(hex1, hex2);
}

///@ ExportMethod
FO_SCRIPT_API uint8 Common_Game_GetDirection(BaseEngine* engine, mpos fromHex, mpos toHex)
{
    ignore_unused(engine);

    return GeometryHelper::GetDir(fromHex, toHex);
}

///@ ExportMethod
FO_SCRIPT_API uint8 Common_Game_GetDirection(BaseEngine* engine, mpos fromHex, mpos toHex, float32 offset)
{
    ignore_unused(engine);

    return GeometryHelper::GetDir(fromHex, toHex, offset);
}

///@ ExportMethod
FO_SCRIPT_API int16 Common_Game_GetDirAngle(BaseEngine* engine, mpos fromHex, mpos toHex)
{
    ignore_unused(engine);

    return numeric_cast<int16>(iround<int32>(GeometryHelper::GetDirAngle(fromHex, toHex)));
}

///@ ExportMethod
FO_SCRIPT_API int16 Common_Game_GetLineDirAngle(BaseEngine* engine, ipos32 fromPos, ipos32 toPos)
{
    ignore_unused(engine);

    return numeric_cast<int16>(iround<int32>(engine->Geometry.GetLineDirAngle(fromPos.x, fromPos.y, toPos.x, toPos.y)));
}

///@ ExportMethod
FO_SCRIPT_API uint8 Common_Game_AngleToDir(BaseEngine* engine, int16 dirAngle)
{
    ignore_unused(engine);

    return GeometryHelper::AngleToDir(dirAngle);
}

///@ ExportMethod
FO_SCRIPT_API int16 Common_Game_DirToAngle(BaseEngine* engine, uint8 dir)
{
    ignore_unused(engine);

    return GeometryHelper::DirToAngle(dir);
}

///@ ExportMethod
FO_SCRIPT_API int16 Common_Game_RotateDirAngle(BaseEngine* engine, int16 dirAngle, bool clockwise, int16 step)
{
    ignore_unused(engine);

    auto rotated = numeric_cast<int32>(dirAngle);

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

    return numeric_cast<int16>(rotated);
}

///@ ExportMethod
FO_SCRIPT_API int16 Common_Game_GetDirAngleDiff(BaseEngine* engine, int16 dirAngle1, int16 dirAngle2)
{
    ignore_unused(engine);

    return numeric_cast<int16>(iround<int32>(GeometryHelper::GetDirAngleDiff(dirAngle1, dirAngle2)));
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_GetHexInterval(BaseEngine* engine, mpos fromHex, mpos toHex, ipos32& hexOffset)
{
    ignore_unused(engine);

    hexOffset = engine->Geometry.GetHexOffset(fromHex, toHex);
}

///@ ExportMethod
FO_SCRIPT_API string Common_Game_GetClipboardText(BaseEngine* engine)
{
    ignore_unused(engine);

    return App->Input.GetClipboardText();
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetClipboardText(BaseEngine* engine, string_view text)
{
    ignore_unused(engine);

    return App->Input.SetClipboardText(text);
}

///@ ExportMethod
FO_SCRIPT_API ProtoItem* Common_Game_GetProtoItem(BaseEngine* engine, hstring pid)
{
    return const_cast<ProtoItem*>(engine->ProtoMngr.GetProtoItemSafe(pid));
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoItem*> Common_Game_GetProtoItems(BaseEngine* engine)
{
    const auto& protos = engine->ProtoMngr.GetProtoItems();

    vector<ProtoItem*> result;
    result.reserve(protos.size());

    for (const auto& proto : protos | std::views::values) {
        result.emplace_back(const_cast<ProtoItem*>(proto.get()));
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoItem*> Common_Game_GetProtoItems(BaseEngine* engine, ItemComponent component)
{
    const auto& protos = engine->ProtoMngr.GetProtoItems();

    vector<ProtoItem*> result;
    result.reserve(protos.size());

    for (const auto& proto : protos | std::views::values) {
        if (proto->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.emplace_back(const_cast<ProtoItem*>(proto.get()));
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoItem*> Common_Game_GetProtoItems(BaseEngine* engine, ItemProperty property, int32 propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<ItemProperties>(engine, property);
    const auto& protos = engine->ProtoMngr.GetProtoItems();

    vector<ProtoItem*> result;
    result.reserve(protos.size());

    for (const auto& proto : protos | std::views::values) {
        if (proto->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(const_cast<ProtoItem*>(proto.get()));
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API ProtoCritter* Common_Game_GetProtoCritter(BaseEngine* engine, hstring pid)
{
    return const_cast<ProtoCritter*>(engine->ProtoMngr.GetProtoCritterSafe(pid));
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoCritter*> Common_Game_GetProtoCritters(BaseEngine* engine)
{
    const auto& protos = engine->ProtoMngr.GetProtoCritters();

    vector<ProtoCritter*> result;
    result.reserve(protos.size());

    for (const auto& proto : protos | std::views::values) {
        result.emplace_back(const_cast<ProtoCritter*>(proto.get()));
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoCritter*> Common_Game_GetProtoCritters(BaseEngine* engine, CritterComponent component)
{
    const auto& protos = engine->ProtoMngr.GetProtoCritters();

    vector<ProtoCritter*> result;
    result.reserve(protos.size());

    for (const auto& proto : protos | std::views::values) {
        if (proto->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.emplace_back(const_cast<ProtoCritter*>(proto.get()));
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoCritter*> Common_Game_GetProtoCritters(BaseEngine* engine, CritterProperty property, int32 propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<CritterProperties>(engine, property);
    const auto& protos = engine->ProtoMngr.GetProtoCritters();

    vector<ProtoCritter*> result;
    result.reserve(protos.size());

    for (const auto& proto : protos | std::views::values) {
        if (proto->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(const_cast<ProtoCritter*>(proto.get()));
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API ProtoMap* Common_Game_GetProtoMap(BaseEngine* engine, hstring pid)
{
    return const_cast<ProtoMap*>(engine->ProtoMngr.GetProtoMapSafe(pid));
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoMap*> Common_Game_GetProtoMaps(BaseEngine* engine)
{
    const auto& protos = engine->ProtoMngr.GetProtoMaps();

    vector<ProtoMap*> result;
    result.reserve(protos.size());

    for (const auto& proto : protos | std::views::values) {
        result.emplace_back(const_cast<ProtoMap*>(proto.get()));
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoMap*> Common_Game_GetProtoMaps(BaseEngine* engine, MapComponent component)
{
    const auto& protos = engine->ProtoMngr.GetProtoMaps();

    vector<ProtoMap*> result;
    result.reserve(protos.size());

    for (const auto& proto : protos | std::views::values) {
        if (proto->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.emplace_back(const_cast<ProtoMap*>(proto.get()));
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoMap*> Common_Game_GetProtoMaps(BaseEngine* engine, MapProperty property, int32 propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<MapProperties>(engine, property);
    const auto& protos = engine->ProtoMngr.GetProtoMaps();

    vector<ProtoMap*> result;
    result.reserve(protos.size());

    for (const auto& proto : protos | std::views::values) {
        if (proto->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(const_cast<ProtoMap*>(proto.get()));
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API ProtoLocation* Common_Game_GetProtoLocation(BaseEngine* engine, hstring pid)
{
    return const_cast<ProtoLocation*>(engine->ProtoMngr.GetProtoLocationSafe(pid));
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoLocation*> Common_Game_GetProtoLocations(BaseEngine* engine)
{
    const auto& protos = engine->ProtoMngr.GetProtoLocations();

    vector<ProtoLocation*> result;
    result.reserve(protos.size());

    for (const auto& proto : protos | std::views::values) {
        result.emplace_back(const_cast<ProtoLocation*>(proto.get()));
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoLocation*> Common_Game_GetProtoLocations(BaseEngine* engine, LocationComponent component)
{
    const auto& protos = engine->ProtoMngr.GetProtoLocations();

    vector<ProtoLocation*> result;
    result.reserve(protos.size());

    for (const auto& proto : protos | std::views::values) {
        if (proto->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.emplace_back(const_cast<ProtoLocation*>(proto.get()));
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoLocation*> Common_Game_GetProtoLocations(BaseEngine* engine, LocationProperty property, int32 propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<LocationProperties>(engine, property);
    const auto& protos = engine->ProtoMngr.GetProtoLocations();

    vector<ProtoLocation*> result;
    result.reserve(protos.size());

    for (const auto& proto : protos | std::views::values) {
        if (proto->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(const_cast<ProtoLocation*>(proto.get()));
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API nanotime Common_Game_GetPrecisionTime(BaseEngine* engine)
{
    ignore_unused(engine);

    return nanotime::now();
}

///@ ExportMethod
FO_SCRIPT_API nanotime Common_Game_PackTime(BaseEngine* engine, int32 year, int32 month, int32 day, int32 hour, int32 minute, int32 second, int32 millisecond, int32 microsecond, int32 nanosecond)
{
    ignore_unused(engine);

    return nanotime::now() + make_time_offset(year, month, day, hour, minute, second, millisecond, microsecond, nanosecond, true);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_UnpackTime(BaseEngine* engine, nanotime time, int32& year, int32& month, int32& day, int32& hour, int32& minute, int32& second, int32& millisecond, int32& microsecond, int32& nanosecond)
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
FO_SCRIPT_API synctime Common_Game_PackSynchronizedTime(BaseEngine* engine, int32 year, int32 month, int32 day, int32 hour, int32 minute, int32 second, int32 millisecond)
{
    ignore_unused(engine);

    return engine->GameTime.GetSynchronizedTime() + make_time_offset(year, month, day, hour, minute, second, millisecond, 0, 0, true);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_UnpackSynchronizedTime(BaseEngine* engine, synctime time, int32& year, int32& month, int32& day, int32& hour, int32& minute, int32& second, int32& millisecond)
{
    ignore_unused(engine);

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
FO_SCRIPT_API uint32 Common_Game_StartTimeEvent(BaseEngine* engine, timespan delay, ScriptFuncName<void> func)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, func, delay, {}, {});
}

///@ ExportMethod
FO_SCRIPT_API uint32 Common_Game_StartTimeEvent(BaseEngine* engine, timespan delay, ScriptFuncName<void, any_t> func, any_t data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, func, delay, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API uint32 Common_Game_StartTimeEvent(BaseEngine* engine, timespan delay, ScriptFuncName<void, vector<any_t>> func, const vector<any_t>& data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, func, delay, {}, data);
}

///@ ExportMethod
FO_SCRIPT_API uint32 Common_Game_StartTimeEvent(BaseEngine* engine, timespan delay, timespan repeat, ScriptFuncName<void> func)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, func, delay, repeat, {});
}

///@ ExportMethod
FO_SCRIPT_API uint32 Common_Game_StartTimeEvent(BaseEngine* engine, timespan delay, timespan repeat, ScriptFuncName<void, any_t> func, any_t data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, func, delay, repeat, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API uint32 Common_Game_StartTimeEvent(BaseEngine* engine, timespan delay, timespan repeat, ScriptFuncName<void, vector<any_t>> func, const vector<any_t>& data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, func, delay, repeat, data);
}

///@ ExportMethod
FO_SCRIPT_API int32 Common_Game_CountTimeEvent(BaseEngine* engine, ScriptFuncName<void> func)
{
    return numeric_cast<int32>(engine->TimeEventMngr.CountTimeEvent(engine, func, {}));
}

///@ ExportMethod
FO_SCRIPT_API int32 Common_Game_CountTimeEvent(BaseEngine* engine, ScriptFuncName<void, any_t> func)
{
    return numeric_cast<int32>(engine->TimeEventMngr.CountTimeEvent(engine, func, {}));
}

///@ ExportMethod
FO_SCRIPT_API int32 Common_Game_CountTimeEvent(BaseEngine* engine, ScriptFuncName<void, vector<any_t>> func)
{
    return numeric_cast<int32>(engine->TimeEventMngr.CountTimeEvent(engine, func, {}));
}

///@ ExportMethod
FO_SCRIPT_API int32 Common_Game_CountTimeEvent(BaseEngine* engine, uint32 id)
{
    return numeric_cast<int32>(engine->TimeEventMngr.CountTimeEvent(engine, {}, id));
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(BaseEngine* engine, ScriptFuncName<void> func)
{
    engine->TimeEventMngr.StopTimeEvent(engine, func, {});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(BaseEngine* engine, ScriptFuncName<void, any_t> func)
{
    engine->TimeEventMngr.StopTimeEvent(engine, func, {});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(BaseEngine* engine, ScriptFuncName<void, vector<any_t>> func)
{
    engine->TimeEventMngr.StopTimeEvent(engine, func, {});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(BaseEngine* engine, uint32 id)
{
    engine->TimeEventMngr.StopTimeEvent(engine, {}, id);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(BaseEngine* engine, ScriptFuncName<void> func, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func, {}, repeat, std::nullopt);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(BaseEngine* engine, ScriptFuncName<void, any_t> func, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func, {}, repeat, std::nullopt);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(BaseEngine* engine, ScriptFuncName<void, vector<any_t>> func, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func, {}, repeat, std::nullopt);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(BaseEngine* engine, uint32 id, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, {}, id, repeat, std::nullopt);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(BaseEngine* engine, ScriptFuncName<void> func, any_t data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func, {}, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(BaseEngine* engine, ScriptFuncName<void, vector<any_t>> func, const vector<any_t>& data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func, {}, {}, data);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(BaseEngine* engine, uint32 id, any_t data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, {}, id, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(BaseEngine* engine, uint32 id, const vector<any_t>& data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, {}, id, {}, data);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopCurrentTimeEvent(BaseEngine* engine)
{
    if (auto&& [entity, te] = engine->TimeEventMngr.GetCurTimeEvent(); entity != nullptr) {
        engine->TimeEventMngr.StopTimeEvent(entity, {}, te->Id);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatCurrentTimeEvent(BaseEngine* engine, timespan repeat)
{
    if (auto&& [entity, te] = engine->TimeEventMngr.GetCurTimeEvent(); entity != nullptr) {
        engine->TimeEventMngr.ModifyTimeEvent(engine, {}, te->Id, repeat, std::nullopt);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetCurrentTimeEventData(BaseEngine* engine, any_t data)
{
    if (auto&& [entity, te] = engine->TimeEventMngr.GetCurTimeEvent(); entity != nullptr) {
        engine->TimeEventMngr.ModifyTimeEvent(engine, {}, te->Id, std::nullopt, vector<any_t> {std::move(data)});
    }
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetCurrentTimeEventData(BaseEngine* engine, const vector<any_t>& data)
{
    if (auto&& [entity, te] = engine->TimeEventMngr.GetCurTimeEvent(); entity != nullptr) {
        engine->TimeEventMngr.ModifyTimeEvent(engine, {}, te->Id, std::nullopt, data);
    }
}

///@ ExportMethod
FO_SCRIPT_API int32 Common_Game_ResolveGenericValue(BaseEngine* engine, string_view str)
{
    return engine->ResolveGenericValue(str);
}

FO_END_NAMESPACE();
