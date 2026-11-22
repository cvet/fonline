// FOnline Engine
// SPDX-License-Identifier: MIT

#include "HashedString.h"

FO_BEGIN_NAMESPACE

hash_storage::hash_storage(hash_func func) :
    _hash_func {func}
{
    FO_VERIFY_AND_THROW(_hash_func, "Hash function is null");
}

auto hash_storage::default_hash(const_span<uint8_t> data) noexcept -> uint64_t
{
    return hashing_ex::hash(data.data(), data.size());
}

auto hash_storage::check_hashed_string(string_view s) const noexcept -> bool
{
    if (s.empty()) {
        return false;
    }

    uint64_t hash_value {_hash_func(const_span<uint8_t> {make_ptr(s.data()).reinterpret_as<uint8_t>().get(), s.length()})};

    shared_lock locker {_hash_storage_locker};

    return _hash_storage.find(hash_value) != _hash_storage.end();
}

auto hash_storage::to_hashed_string(string_view s) -> hstring
{
    static_assert(std::same_as<hstring::hash_t, decltype(_hash_func({}))>);

    if (s.empty()) {
        return {};
    }

    uint64_t hash_value {_hash_func(const_span<uint8_t> {make_ptr(s.data()).reinterpret_as<uint8_t>().get(), s.length()})};
    FO_VERIFY_AND_THROW(hash_value != 0, "Hashed string value is zero");

    {
        shared_lock locker {_hash_storage_locker};

        if (auto it {_hash_storage.find(hash_value)}; it != _hash_storage.end()) {
#if FO_DEBUG
            bool collision_detected {s != it->second->str};
#else
            bool collision_detected {s.length() != it->second->str.length()};
#endif

            if (collision_detected) {
                throw HashCollisionException("Hash collision", s, it->second->str, hash_value);
            }

            return hstring(it->second.get());
        }
    }

    {
        // Add new entry
        scoped_lock locker {_hash_storage_locker};

        if (auto it {_hash_storage.find(hash_value)}; it != _hash_storage.end()) {
#if FO_DEBUG
            bool collision_detected {s != it->second->str};
#else
            bool collision_detected {s.length() != it->second->str.length()};
#endif

            if (collision_detected) {
                throw HashCollisionException("Hash collision", s, it->second->str, hash_value);
            }

            return hstring(it->second.get());
        }

        auto entry {safe_alloc::make_unique<hstring::entry>()};
        entry->hash = hash_value;
        entry->str = string(s);

        auto [it, inserted] {_hash_storage.emplace(hash_value, std::move(entry))};
        FO_VERIFY_AND_THROW(inserted, "Hash storage insertion failed");

        return hstring(it->second.get());
    }
}

auto hash_storage::resolve_hash(hstring::hash_t h) const -> hstring
{
    if (h == 0) {
        return {};
    }

    {
        shared_lock locker {_hash_storage_locker};

        if (auto it {_hash_storage.find(h)}; it != _hash_storage.end()) {
            return hstring(it->second.get());
        }
    }

    handle_resolve_hash_failure(h);

    break_into_debugger();

    throw HashResolveException("Can't resolve hash", h);
}

auto hash_storage::resolve_hash(hstring::hash_t h, nptr<bool> failed) const noexcept -> hstring
{
    if (h == 0) {
        return {};
    }

    {
        shared_lock locker {_hash_storage_locker};

        if (auto it {_hash_storage.find(h)}; it != _hash_storage.end()) {
            return hstring(it->second.get());
        }
    }

    handle_resolve_hash_failure(h);

    break_into_debugger();

    if (failed) {
        *failed = true;
    }

    return {};
}

void hash_storage::set_resolve_hash_failure_handler(resolve_hash_failure_handler handler)
{
    scoped_lock locker {_resolve_hash_failure_handler_locker};

    _resolve_hash_failure_handler = std::move(handler);
}

void hash_storage::handle_resolve_hash_failure(hstring::hash_t h) const noexcept
{
    shared_lock locker {_resolve_hash_failure_handler_locker};

    if (!_resolve_hash_failure_handler) {
        return;
    }

    try {
        _resolve_hash_failure_handler(h);
    }
    catch (const std::exception& ex) {
        exceptions::report_and_continue(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

FO_END_NAMESPACE
