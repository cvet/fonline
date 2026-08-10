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

#include "HashedString.h"

FO_BEGIN_NAMESPACE

hstring::entry hstring::_zeroEntry;

HashStorage::HashStorage(HashFunc hash_func) :
    _hashFunc {hash_func}
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_hashFunc, "Hash function is null");
}

auto HashStorage::DefaultHash(const_span<uint8_t> data) noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return hashing_ex::hash(data.data(), data.size());
}

auto HashStorage::CheckHashedString(string_view s) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (s.empty()) {
        return false;
    }

    uint64_t hash_value = _hashFunc(const_span<uint8_t> {make_ptr(s.data()).reinterpret_as<uint8_t>().get(), s.length()});

    shared_lock locker {_hashStorageLocker};

    return _hashStorage.find(hash_value) != _hashStorage.end();
}

auto HashStorage::ToHashedString(string_view s) -> hstring
{
    FO_NO_STACK_TRACE_ENTRY();

    static_assert(std::same_as<hstring::hash_t, decltype(_hashFunc({}))>);

    if (s.empty()) {
        return {};
    }

    uint64_t hash_value = _hashFunc(const_span<uint8_t> {make_ptr(s.data()).reinterpret_as<uint8_t>().get(), s.length()});
    FO_VERIFY_AND_THROW(hash_value != 0, "Hashed string value is zero");

    {
        shared_lock locker {_hashStorageLocker};

        if (auto it = _hashStorage.find(hash_value); it != _hashStorage.end()) {
#if FO_DEBUG
            bool collision_detected = s != it->second->Str;
#else
            const auto collision_detected = s.length() != it->second->Str.length();
#endif

            if (collision_detected) {
                throw HashCollisionException("Hash collision", s, it->second->Str, hash_value);
            }

            return hstring(it->second.get());
        }
    }

    {
        // Add new entry
        auto entry = SafeAlloc::MakeUnique<hstring::entry>();
        entry->Hash = hash_value;
        entry->Str = string(s);

        scoped_lock locker {_hashStorageLocker};

        const auto [it, inserted] = _hashStorage.emplace(hash_value, std::move(entry));
        ignore_unused(inserted); // Do not assert because somebody else can insert it already

        return hstring(it->second.get());
    }
}

auto HashStorage::ResolveHash(hstring::hash_t h) const -> hstring
{
    FO_NO_STACK_TRACE_ENTRY();

    if (h == 0) {
        return {};
    }

    {
        shared_lock locker {_hashStorageLocker};

        if (auto it = _hashStorage.find(h); it != _hashStorage.end()) {
            return hstring(it->second.get());
        }
    }

    HandleResolveHashFailure(h);

    BreakIntoDebugger();

    throw HashResolveException("Can't resolve hash", h);
}

auto HashStorage::ResolveHash(hstring::hash_t h, nptr<bool> failed) const noexcept -> hstring
{
    FO_NO_STACK_TRACE_ENTRY();

    if (h == 0) {
        return {};
    }

    {
        shared_lock locker {_hashStorageLocker};

        if (auto it = _hashStorage.find(h); it != _hashStorage.end()) {
            return hstring(it->second.get());
        }
    }

    HandleResolveHashFailure(h);

    BreakIntoDebugger();

    if (failed) {
        *failed = true;
    }

    return {};
}

void HashStorage::SetResolveHashFailureHandler(ResolveHashFailureHandler handler)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_resolveHashFailureHandlerLocker};

    _resolveHashFailureHandler = std::move(handler);
}

void HashStorage::HandleResolveHashFailure(hstring::hash_t h) const noexcept
{
    FO_STACK_TRACE_ENTRY();

    shared_lock locker {_resolveHashFailureHandlerLocker};

    if (!_resolveHashFailureHandler) {
        return;
    }

    try {
        _resolveHashFailureHandler(h);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

FO_END_NAMESPACE
