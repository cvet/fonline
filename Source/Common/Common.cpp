// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "Application.h"

FO_BEGIN_NAMESPACE

static constexpr timespan IDLE_TIME_BALANCE_FLOOR {-std::chrono::milliseconds {1000}};

struct InterthreadData
{
    mutex ListenersLocker {};
    map<uint16_t, InterthreadListener> Listeners FO_TSA_GUARDED_BY(ListenersLocker) {};
};
FO_GLOBAL_DATA(InterthreadData, Interthread);

bool IsTestingInProgress {};

auto CommandLineArgs::GetSettingValue(string_view name, optional<string_view> implicit_value) const -> optional<string_view>
{
    for (size_t i {_args.size()}; i > 0; --i) {
        optional<CommandLineSetting> setting {GetSetting(i - 1)};

        if (!setting || setting->Name != name) {
            continue;
        }

        if (setting->Value) {
            return setting->Value;
        }

        if (implicit_value) {
            return implicit_value;
        }
    }

    return std::nullopt;
}

auto CommandLineArgs::GetSettings(string_view name) const -> vector<CommandLineSetting>
{
    vector<CommandLineSetting> result {};

    for (size_t i {0}; i < _args.size(); i++) {
        optional<CommandLineSetting> setting {GetSetting(i)};

        if (setting && (name.empty() || setting->Name == name)) {
            result.emplace_back(setting.value());
        }
    }

    return result;
}

auto CommandLineArgs::GetSetting(size_t index) const noexcept -> optional<CommandLineSetting>
{
    if (index >= _args.size()) {
        return std::nullopt;
    }

    string_view arg {Get(index)};

    if (!IsOption(arg)) {
        return std::nullopt;
    }

    string_view name {strvex(arg.substr(arg.starts_with("--") ? 2 : 1)).trim().strv()};
    optional<string_view> value {};

    // Checked on the raw next argument, so a negative number stays a value
    if (index + 1 < _args.size() && !IsOption(Get(index + 1))) {
        value = Get(index + 1);
    }

    return CommandLineSetting {name, value};
}

ProgramArgs::ProgramArgs(int32_t argc, nptr<char*> argv)
{
    optional<vector<string>> platform_args {platform::get_command_line_args()};

    if (platform_args.has_value()) {
        _values = std::move(platform_args.value());
    }
    else {
        CommandLineArgs narrow_args {argc, argv};

        for (size_t i {0}; i < narrow_args.size(); i++) {
            _values.emplace_back(narrow_args.Get(i));
        }
    }

    _pointers.reserve(_values.size());

    for (string& value : _values) {
        _pointers.emplace_back(value.data());
    }
}

auto AddInterthreadListener(uint16_t port, InterthreadListener listener) -> bool
{
    scoped_lock locker {Interthread->ListenersLocker};

    return Interthread->Listeners.emplace(port, std::move(listener)).second;
}

auto RemoveInterthreadListener(uint16_t port) -> bool
{
    scoped_lock locker {Interthread->ListenersLocker};

    return Interthread->Listeners.erase(port) != 0;
}

auto FindInterthreadListener(uint16_t port) -> optional<InterthreadListener>
{
    scoped_lock locker {Interthread->ListenersLocker};

    auto it {Interthread->Listeners.find(port)};

    if (it == Interthread->Listeners.end()) {
        return std::nullopt;
    }

    return it->second;
}

auto HasInterthreadListener(uint16_t port) -> bool
{
    scoped_lock locker {Interthread->ListenersLocker};

    return Interthread->Listeners.contains(port);
}

auto GetRemoteCallSimpleValueMinWireSize(const BaseTypeDesc& type) -> size_t
{
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

        size_t min_size {0};

        for (const auto& field : type.StructLayout->Fields) {
            size_t field_min_size {GetRemoteCallSimpleValueMinWireSize(field.Type)};
            FO_VERIFY_AND_THROW(field_min_size <= std::numeric_limits<size_t>::max() - min_size, "Remote call struct minimum serialized size overflows", type.Name);
            min_size += field_min_size;
        }

        return min_size;
    }

    throw NotSupportedException("Unsupported simple remote call argument type", type.Name);
}

FrameBalancer::FrameBalancer(bool enabled, int32_t sleep, int32_t fixed_fps) :
    _enabled {enabled && (sleep >= 0 || fixed_fps > 0)},
    _sleep {std::chrono::milliseconds {std::max(sleep, 0)}},
    _yieldAfterLoop {sleep == 0},
    _fixedFps {fixed_fps}
{
}

void FrameBalancer::StartLoop()
{
    if (!_enabled) {
        return;
    }

    _loopStart = nanotime::now();
}

void FrameBalancer::EndLoop()
{
    FO_TRACE_ZONE(App);

    if (!_enabled) {
        return;
    }

    _loopDuration = nanotime::now() - _loopStart;

    // A frame rate cap outranks the plain per-frame sleep, so a config that keeps Sleep 0 can still be capped
    if (_fixedFps > 0) {
        timespan target_time {std::chrono::nanoseconds(iround<uint64_t>(1000.0 / numeric_cast<float64_t>(_fixedFps) * 1000000.0))};
        timespan idle_time {target_time - _loopDuration + _idleTimeBalance};

        if (idle_time > timespan::zero) {
            nanotime sleep_start {nanotime::now()};

            precise_sleep(idle_time.value());

            timespan sleep_duration {nanotime::now() - sleep_start};

            _idleTimeBalance += (target_time - _loopDuration) - sleep_duration;
        }
        else {
            _idleTimeBalance += target_time - _loopDuration;

            if (_idleTimeBalance < IDLE_TIME_BALANCE_FLOOR) {
                _idleTimeBalance = IDLE_TIME_BALANCE_FLOOR;
            }
        }
    }
    else if (_yieldAfterLoop) {
        std::this_thread::yield();
    }
    else if (_sleep > timespan::zero) {
        coarse_sleep(_sleep.value());
    }
}

FO_END_NAMESPACE
