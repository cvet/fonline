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

std::mutex InterthreadListenersLocker;
map<uint16, function<InterthreadDataCallback(InterthreadDataCallback)>> InterthreadListeners;

alignas(uint32_t) static volatile constexpr char PACKAGED_MARK[] = "###NOT_PACKAGED###";
static bool HasNotPackagedMark = strex().assignVolatile(PACKAGED_MARK, sizeof(PACKAGED_MARK)).str().find("NOT_PACKAGED") != string::npos;
bool IsTestingInProgress {};

auto IsPackaged() -> bool
{
    FO_STACK_TRACE_ENTRY();

    return !HasNotPackagedMark;
}

void ForcePackaged()
{
    HasNotPackagedMark = false;
}

FrameBalancer::FrameBalancer(bool enabled, int32 sleep, int32 fixed_fps) :
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
        const timespan target_time = std::chrono::nanoseconds(iround<uint64>(1000.0 / numeric_cast<float64>(_fixedFps) * 1000000.0));
        const auto idle_time = target_time - _loopDuration + _idleTimeBalance;

        if (idle_time > timespan::zero) {
            const auto sleep_start = nanotime::now();

            std::this_thread::sleep_for(idle_time.value());

            const auto sleep_duration = nanotime::now() - sleep_start;

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

    const auto dir = strex(fname).extract_dir().str();

    if (!dir.empty()) {
        const auto dir_ok = fs_create_directories(dir);
        FO_RUNTIME_ASSERT(dir_ok);
    }

    std::ofstream file {std::filesystem::path {fs_make_path(fname)}, std::ios::binary | std::ios::trunc};
    FO_RUNTIME_ASSERT(file);

    const uint8 header[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
        numeric_cast<uint8>(size.width % 256), numeric_cast<uint8>(size.width / 256), //
        numeric_cast<uint8>(size.height % 256), numeric_cast<uint8>(size.height / 256), 4 * 8, 0x20};
    file.write(reinterpret_cast<const char*>(header), static_cast<std::streamsize>(sizeof(header)));
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size() * sizeof(uint32)));
    FO_RUNTIME_ASSERT(file);
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
