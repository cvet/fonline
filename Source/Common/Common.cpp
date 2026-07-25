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

FO_BEGIN_NAMESPACE

mutex InterthreadListenersLocker;
map<uint16_t, function<InterthreadDataCallback(InterthreadDataCallback)>> InterthreadListeners;

FO_KEEP_DATA_SYMBOL char PACKAGED_BUILD_NAME[128] = "###NotPackaged###"
                                                    "##############################################################################################################";
static auto ReadPackagedBuildName() -> string;
static const string PackagedBuildName = ReadPackagedBuildName();
bool IsTestingInProgress {};

auto IsPackaged() -> bool
{
    FO_STACK_TRACE_ENTRY();

    return !PackagedBuildName.empty();
}

auto GetPackagedRuntimeName() -> string
{
    FO_STACK_TRACE_ENTRY();

    return PackagedBuildName;
}

auto GetRemoteCallSimpleValueMinWireSize(const BaseTypeDesc& type) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsPrimitive || type.IsEnum) {
        FO_VERIFY_AND_THROW(type.Size != 0, "Remote call plain argument type has zero size", type.Name);
        return type.Size;
    }
    if (type.IsString || type.IsRefType) {
        return sizeof(uint32_t);
    }
    if (type.IsHashedString) {
        return sizeof(hstring::hash_t);
    }
    if (type.IsStruct) {
        FO_VERIFY_AND_THROW(type.StructLayout, "Remote call struct layout is null", type.Name);

        size_t min_size = 0;

        for (const auto& field : type.StructLayout->Fields) {
            size_t field_min_size = GetRemoteCallSimpleValueMinWireSize(field.Type);
            FO_VERIFY_AND_THROW(field_min_size <= std::numeric_limits<size_t>::max() - min_size, "Remote call struct minimum serialized size overflows", type.Name);
            min_size += field_min_size;
        }

        return min_size;
    }

    throw NotSupportedException("Unsupported simple remote call argument type", type.Name);
}

static auto ReadPackagedBuildName() -> string
{
    string raw = strex().assignVolatile(PACKAGED_BUILD_NAME, sizeof(PACKAGED_BUILD_NAME)).str();

    if (raw.find("NotPackaged") != string::npos) {
        return {};
    }

    if (auto null_pos = raw.find('\0'); null_pos != string::npos) {
        raw.resize(null_pos);
    }

    return raw;
}

FrameBalancer::FrameBalancer(bool enabled, int32_t sleep, int32_t fixed_fps) :
    _enabled {enabled && (sleep >= 0 || fixed_fps > 0)},
    _sleep {sleep},
    _fixedFps {fixed_fps}
{
    FO_STACK_TRACE_ENTRY();
}

void FrameBalancer::StartLoop()
{
    FO_STACK_TRACE_ENTRY();

    if (!_enabled) {
        return;
    }

    _loopStart = nanotime::now();
}

void FrameBalancer::EndLoop()
{
    FO_STACK_TRACE_ENTRY();

    if (!_enabled) {
        return;
    }

    _loopDuration = nanotime::now() - _loopStart;

    if (_sleep >= 0) {
        if (_sleep == 0) {
            std::this_thread::yield();
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(_sleep));
        }
    }
    else if (_fixedFps > 0) {
        timespan target_time = std::chrono::nanoseconds(iround<uint64_t>(1000.0 / numeric_cast<float64_t>(_fixedFps) * 1000000.0));
        timespan idle_time = target_time - _loopDuration + _idleTimeBalance;

        if (idle_time > timespan::zero) {
            nanotime sleep_start = nanotime::now();

            std::this_thread::sleep_for(idle_time.value());

            timespan sleep_duration = nanotime::now() - sleep_start;

            _idleTimeBalance += (target_time - _loopDuration) - sleep_duration;
        }
        else {
            _idleTimeBalance += target_time - _loopDuration;

            if (_idleTimeBalance < timespan(-std::chrono::milliseconds {1000})) {
                _idleTimeBalance = timespan(-std::chrono::milliseconds {1000});
            }
        }
    }
}

auto MakeSeededRandomGenerator() -> std::mt19937
{
    FO_STACK_TRACE_ENTRY();

    std::random_device random_device;
    return std::mt19937 {random_device()};
}

void WriteSimpleTga(string_view fname, isize32 size, vector<ucolor> data)
{
    FO_STACK_TRACE_ENTRY();

    string dir = strex(fname).extract_dir().str();

    if (!dir.empty()) {
        bool dir_ok = fs_create_directories(dir);
        FO_VERIFY_AND_THROW(dir_ok, "Failed to create output directory for TGA image", dir, fname);
    }

    std::ofstream file {std::filesystem::path {fs_make_path(fname)}, std::ios::binary | std::ios::trunc};
    FO_VERIFY_AND_THROW(file, "Failed to open TGA image file for writing", fname, size, data.size());

    // ucolor keeps pixels in R, G, B, A byte order, but a TrueColor TGA stores them as B, G, R, A
    // (matching the engine's own TgaLoad reader), so swap red and blue before writing the payload
    for (auto& pixel : data) {
        std::swap(pixel.comp.r, pixel.comp.b);
    }

    const uint8_t header[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
        numeric_cast<uint8_t>(size.width % 256), numeric_cast<uint8_t>(size.width / 256), //
        numeric_cast<uint8_t>(size.height % 256), numeric_cast<uint8_t>(size.height / 256), 4 * 8, 0x20};
    ptr<const uint8_t> header_bytes = header;
    file.write(header_bytes.reinterpret_as<char>().get(), static_cast<std::streamsize>(sizeof(header)));

    if (!data.empty()) {
        auto pixels = make_nptr(data.data());
        file.write(pixels.reinterpret_as<char>().get(), static_cast<std::streamsize>(data.size() * sizeof(uint32_t)));
    }

    FO_VERIFY_AND_THROW(file, "Failed while writing TGA image file", fname, size, data.size());
}

// Dummy symbols for web build to avoid linker errors
#if FO_WEB

FO_END_NAMESPACE

void emscripten_sleep(unsigned int ms)
{
    FO_STACK_TRACE_ENTRY();

    FO_UNREACHABLE_PLACE();
}

FO_BEGIN_NAMESPACE

#endif

FO_END_NAMESPACE
