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
#include "Version-Include.h"

FO_BEGIN_NAMESPACE();

map<uint16, function<InterthreadDataCallback(InterthreadDataCallback)>> InterthreadListeners;

alignas(uint32_t) static volatile constexpr char PACKAGED_MARK[] = "###NOT_PACKAGED###";
static bool HasNotPackagedMark = strex().assignVolatile(PACKAGED_MARK, sizeof(PACKAGED_MARK)).str().find("NOT_PACKAGED") != string::npos;

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

void GenericUtils::WriteSimpleTga(string_view fname, isize32 size, vector<ucolor> data)
{
    FO_STACK_TRACE_ENTRY();

    auto file = DiskFileSystem::OpenFile(fname, true);
    FO_RUNTIME_ASSERT(file);

    const uint8 header[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
        numeric_cast<uint8>(size.width % 256), numeric_cast<uint8>(size.width / 256), //
        numeric_cast<uint8>(size.height % 256), numeric_cast<uint8>(size.height / 256), 4 * 8, 0x20};
    file.Write(header);

    file.Write(data.data(), data.size() * sizeof(uint32));
}

// Default randomizer
static std::mt19937 RandomGenerator(std::random_device {}());
void GenericUtils::SetRandomSeed(int32 seed)
{
    FO_STACK_TRACE_ENTRY();

    RandomGenerator = std::mt19937(seed);
}

auto GenericUtils::Random(int32 minimum, int32 maximum) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::uniform_int_distribution {minimum, maximum}(RandomGenerator);
}

auto GenericUtils::Percent(int32 full, int32 peace) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(full >= 0);

    if (full == 0) {
        return 0;
    }

    const auto percent = peace * 100 / full;

    return std::clamp(percent, 0, 100);
}

auto GenericUtils::IntersectCircleLine(int32 cx, int32 cy, int32 radius, int32 x1, int32 y1, int32 x2, int32 y2) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto x01 = x1 - cx;
    const auto y01 = y1 - cy;
    const auto x02 = x2 - cx;
    const auto y02 = y2 - cy;
    const auto dx = x02 - x01;
    const auto dy = y02 - y01;
    const auto a = dx * dx + dy * dy;
    const auto b = 2 * (x01 * dx + y01 * dy);
    const auto c = x01 * x01 + y01 * y01 - radius * radius;

    if (-b < 0) {
        return c < 0;
    }
    if (-b < 2 * a) {
        return 4 * a * c - b * b < 0;
    }

    return a + b + c < 0;
}

auto GenericUtils::GetStepsCoords(ipos32 from_pos, ipos32 to_pos) noexcept -> fpos32
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto dx = numeric_cast<float32>(std::abs(to_pos.x - from_pos.x));
    const auto dy = numeric_cast<float32>(std::abs(to_pos.y - from_pos.y));

    auto sx = 1.0f;
    auto sy = 1.0f;

    if (dx < dy) {
        sx = dx / dy;
    }
    else {
        sy = dy / dx;
    }

    if (to_pos.x < from_pos.x) {
        sx = -sx;
    }
    if (to_pos.y < from_pos.y) {
        sy = -sy;
    }

    return {sx, sy};
}

auto GenericUtils::ChangeStepsCoords(fpos32 pos, float32 deq) noexcept -> fpos32
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto rad = deq * DEG_TO_RAD_FLOAT;
    const auto x = pos.x * std::cos(rad) - pos.y * std::sin(rad);
    const auto y = pos.x * std::sin(rad) + pos.y * std::cos(rad);

    return {x, y};
}

// Dummy symbols for web build to avoid linker errors
#if FO_WEB

FO_END_NAMESPACE();

void emscripten_sleep(unsigned int ms)
{
    FO_STACK_TRACE_ENTRY();

    FO_UNREACHABLE_PLACE();
}

FO_BEGIN_NAMESPACE();

#endif

FO_END_NAMESPACE();
