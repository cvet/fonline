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

#include "Common.h"

#include "catch_amalgamated.hpp"

#if FO_WINDOWS
#include "WinApi.h"
#endif

FO_BEGIN_NAMESPACE

static auto MakeTempTestDir(string_view name) -> string
{
    auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs::path_to_string(base);
}

TEST_CASE("DiskFileSystem")
{
    SECTION("DurableNewWriteAndAtomicReplacementPreserveExistingTargets")
    {
        string temp_dir = MakeTempTestDir("diskfs_atomic_replace");
        string target_path = strex(temp_dir).combine_path("Юникод/slot.bin").str();
        string staging_path = strex(temp_dir).combine_path("Юникод/slot.stage").str();
        string missing_path = strex(temp_dir).combine_path("Юникод/missing.stage").str();
        vector<uint8_t> old_content {'o', 'l', 'd'};
        vector<uint8_t> new_content {'n', 'e', 'w'};
        vector<uint8_t> rejected_content {'b', 'a', 'd'};

        auto cleanup = scope_exit([&]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
        REQUIRE(fs::write_file(target_path, old_content));
        REQUIRE(fs::write_file_durable_new(staging_path, new_content));
        CHECK_FALSE(fs::write_file_durable_new(staging_path, rejected_content));
        REQUIRE(fs::read_file(staging_path).has_value());
        CHECK(*fs::read_file(staging_path) == "new");

        REQUIRE(fs::replace_file(staging_path, target_path));
        CHECK_FALSE(fs::exists(staging_path));
        REQUIRE(fs::read_file(target_path).has_value());
        CHECK(*fs::read_file(target_path) == "new");

        CHECK_FALSE(fs::replace_file(missing_path, target_path));
        REQUIRE(fs::read_file(target_path).has_value());
        CHECK(*fs::read_file(target_path) == "new");
        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("DurableNewWriteSupportsEmptyBytesAndRejectsInvalidPaths")
    {
        string temp_dir = MakeTempTestDir("durable_new");
        auto cleanup = scope_exit([&]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
        string path = fs::path_to_string(std::filesystem::path {fs::make_path(temp_dir)} / std::filesystem::path {fs::make_path(" leading space")});
        vector<uint8_t> empty;
        CHECK_FALSE(fs::write_file_durable_new("", empty));
        CHECK_FALSE(fs::write_file_durable_new(string_view {"bad\0path", 8}, empty));
        REQUIRE(fs::write_file_durable_new(path, empty));
        CHECK(fs::read_file(path) == optional<string> {string {}});
        CHECK_FALSE(fs::write_file_durable_new(temp_dir, empty));
#if !FO_WINDOWS
        // Keep this native name raw: path_to_string normalizes backslashes as resource separators
        string literal_backslash = temp_dir + "/native\\name\\";
        REQUIRE(fs::write_file_durable_new(literal_backslash, empty));
        CHECK(fs::exists(literal_backslash));
#endif
    }

    SECTION("ExclusiveFileLockPreservesBytesAndTransfersOwnership")
    {
#if !FO_WEB
        string temp_dir = MakeTempTestDir("exclusive_lock");
        auto cleanup = scope_exit([&]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
        string lock_path = strex(temp_dir).combine_path("owner.lock").str();
        string other_path = strex(temp_dir).combine_path("other.lock").str();
        REQUIRE(fs::write_file(lock_path, "sentinel"));

        auto first = fs::try_lock_file(lock_path);
        REQUIRE(first.owns_lock());
        CHECK_FALSE(fs::try_lock_file(lock_path).owns_lock());
        auto moved = std::move(first);
        CHECK_FALSE(first.owns_lock()); // FO_USE_AFTER_MOVE_SUPPRESS: test verifies the moved-from lock ownership contract
        CHECK(moved.owns_lock());
        CHECK_FALSE(fs::try_lock_file(lock_path).owns_lock());

        auto assigned = fs::try_lock_file(other_path);
        REQUIRE(assigned.owns_lock());
        assigned = std::move(moved);
        CHECK_FALSE(moved.owns_lock()); // FO_USE_AFTER_MOVE_SUPPRESS: test verifies the moved-from lock ownership contract
        CHECK(assigned.owns_lock());
        CHECK(fs::try_lock_file(other_path).owns_lock());
        CHECK_FALSE(fs::try_lock_file(lock_path).owns_lock());
        assigned.reset();
        assigned.reset();
        CHECK_FALSE(assigned.owns_lock());
        CHECK(fs::read_file(lock_path) == optional<string> {"sentinel"});
        {
            auto scoped = fs::try_lock_file(lock_path);
            REQUIRE(scoped.owns_lock());
        }
        CHECK(fs::try_lock_file(lock_path).owns_lock());
#else
        CHECK_FALSE(fs::try_lock_file("owner.lock").owns_lock());
#endif
    }

    SECTION("ExclusiveFileLockRejectsInvalidAndMissingParentPaths")
    {
        CHECK_FALSE(fs::try_lock_file("").owns_lock());
        CHECK_FALSE(fs::try_lock_file("dir/").owns_lock());
        CHECK_FALSE(fs::try_lock_file("dir\\").owns_lock());
        CHECK_FALSE(fs::try_lock_file(string_view {"lock\0suffix", 11}).owns_lock());
        string missing = strex(MakeTempTestDir("missing_lock_parent")).combine_path("child/owner.lock").str();
        CHECK_FALSE(fs::try_lock_file(missing).owns_lock());
    }

    SECTION("ExclusiveFileLockSupportsLongUnicodePaths")
    {
#if !FO_WEB
        string temp_dir = MakeTempTestDir("unicode_lock");
        auto cleanup = scope_exit([&]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
        string dir = temp_dir;
        for (int32_t i = 0; i != 18; i++) {
            dir = strex(dir).combine_path("long-directory-segment").str();
        }
        REQUIRE(fs::create_directories(dir));
        string path = strex(dir).combine_path("блокировка.lock").str();
        REQUIRE(path.size() > 320);
        auto lock = fs::try_lock_file(path);
        REQUIRE(lock.owns_lock());
        CHECK_FALSE(fs::try_lock_file(path).owns_lock());
        lock.reset();
        CHECK(fs::try_lock_file(path).owns_lock());
#endif
    }

    SECTION("AtomicStringWritePreservesEmbeddedZerosAndPublishesEmptyContent")
    {
        string temp_dir = MakeTempTestDir("diskfs_atomic_string");
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
        string file_path = strex(temp_dir).combine_path("data.bin").str();
        string_view content {"old\0bytes", 9};
        REQUIRE(fs::write_file(file_path, content));
        CHECK(fs::read_file(file_path) == optional<string> {string(content)});
        fs::disk_read_file reader {file_path};
        REQUIRE(static_cast<bool>(reader));

        REQUIRE(fs::write_file(file_path, string_view {}));
        CHECK(fs::read_file(file_path) == optional<string> {string {}});
        array<uint8_t, 9> old_bytes {};
        REQUIRE(reader.read_at(0, old_bytes));
        CHECK((string_view {make_ptr(old_bytes.data()).reinterpret_as<const char>().get(), old_bytes.size()} == content));
        CHECK(fs::list_dir_file_names(temp_dir).size() == 1);
    }

    SECTION("AtomicReplacementWithOpenReadersHandlesEveryWideNameAlignment")
    {
        string temp_dir = MakeTempTestDir("diskfs_atomic_name_alignment");
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });

        for (size_t name_length = 1; name_length <= 4; ++name_length) {
            string file_path = strex(temp_dir).combine_path(string(name_length, 'x') + ".bin").str();
            CAPTURE(file_path);
            REQUIRE(fs::write_file(file_path, string_view {"old"}));
            fs::disk_read_file reader {file_path};
            REQUIRE(static_cast<bool>(reader));

            REQUIRE(fs::write_file(file_path, string_view {"replacement"}));
            CHECK(fs::read_file(file_path) == optional<string> {"replacement"});
            array<uint8_t, 3> old_bytes {};
            REQUIRE(reader.read_at(0, old_bytes));
            CHECK((string_view {make_ptr(old_bytes.data()).reinterpret_as<const char>().get(), old_bytes.size()} == "old"));
        }

        CHECK(fs::list_dir_file_names(temp_dir).size() == 4);
    }

    SECTION("AtomicWriteKeepsExistingReadersOnThePreviousFile")
    {
        string temp_dir = MakeTempTestDir("diskfs_atomic");
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
        string file_path = strex(temp_dir).combine_path("data.bin").str();
        array<uint8_t, 3> before = {'o', 'l', 'd'};
        array<uint8_t, 3> after = {'n', 'e', 'w'};
        REQUIRE(fs::write_file(file_path, before));
        fs::disk_read_file reader {file_path};
        REQUIRE(static_cast<bool>(reader));

        REQUIRE(fs::write_file(file_path, after));
        CHECK(fs::compare_file_content(file_path, after));
        array<uint8_t, 3> old_bytes {};
        REQUIRE(reader.read_at(0, old_bytes));
        CHECK(old_bytes == before);
        reader.close();
        CHECK(fs::list_dir_file_names(temp_dir).size() == 1);
    }

    SECTION("AtomicWriteFailurePreservesTheDestinationAndRemovesItsStage")
    {
        string temp_dir = MakeTempTestDir("diskfs_atomic_failure");
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
        string destination = strex(temp_dir).combine_path("occupied").str();
        string child = strex(destination).combine_path("original.bin").str();
        array<uint8_t, 3> bytes = {'o', 'l', 'd'};
        REQUIRE(fs::write_file(child, bytes));

        CHECK_FALSE(fs::write_file(destination, bytes));
        CHECK(fs::is_dir(destination));
        CHECK(fs::compare_file_content(child, bytes));
        CHECK(fs::list_dir_file_names(temp_dir, true).size() == 1);
    }

    SECTION("AtomicWritePublishesIntoADirectoryWithALiteralLeadingSpace")
    {
        string dir_name = strex(" fo_diskfs_literal_{}", std::chrono::steady_clock::now().time_since_epoch().count()).str();
        auto cleanup = scope_exit([&dir_name]() noexcept { (void)fs::remove_dir_tree(dir_name); });
        REQUIRE(fs::create_directories(dir_name));

        // Raw concatenation on purpose: a path combiner normalizes the literal leading space away
        string file_path = dir_name + "/data.bin";
        array<uint8_t, 3> before = {'o', 'l', 'd'};
        array<uint8_t, 3> after = {'n', 'e', 'w'};

        {
            fs::disk_write_file writer {file_path};
            REQUIRE(static_cast<bool>(writer));
            REQUIRE(writer.write(before));
            REQUIRE(writer.flush());
        }

        fs::disk_read_file reader {file_path};
        REQUIRE(static_cast<bool>(reader));
        array<uint8_t, 3> seeded {};
        REQUIRE(reader.read_at(0, seeded));
        CHECK(seeded == before);

        REQUIRE(fs::write_file(file_path, after));
        CHECK(fs::compare_file_content(file_path, after));
        array<uint8_t, 3> old_bytes {};
        REQUIRE(reader.read_at(0, old_bytes));
        CHECK(old_bytes == before);
        reader.close();
        CHECK(fs::list_dir_file_names(dir_name).size() == 1);
    }

#if !FO_WINDOWS
    SECTION("AtomicWritePublishesIntoADirectoryWithALiteralBackslashSegment")
    {
        string temp_dir = MakeTempTestDir("diskfs_atomic_backslash_dir");
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });

        // Raw strings throughout: fs::path_to_string and combine_path normalize the literal backslash away
        string dir_path = temp_dir + "/back\\slash";
        string file_path = dir_path + "/data.bin";
        REQUIRE(fs::create_directories(dir_path));
        array<uint8_t, 3> before = {'o', 'l', 'd'};
        array<uint8_t, 3> after = {'n', 'e', 'w'};

        {
            fs::disk_write_file writer {file_path};
            REQUIRE(static_cast<bool>(writer));
            REQUIRE(writer.write(before));
            REQUIRE(writer.flush());
        }

        fs::disk_read_file reader {file_path};
        REQUIRE(static_cast<bool>(reader));
        array<uint8_t, 3> seeded {};
        REQUIRE(reader.read_at(0, seeded));
        CHECK(seeded == before);

        REQUIRE(fs::write_file(file_path, after));
        CHECK(fs::compare_file_content(file_path, after));
        array<uint8_t, 3> old_bytes {};
        REQUIRE(reader.read_at(0, old_bytes));
        CHECK(old_bytes == before);
        reader.close();
        CHECK(fs::list_dir_file_names(temp_dir, true).size() == 1);
        CHECK_FALSE(fs::is_dir(temp_dir + "/back"));
    }

    SECTION("AtomicWritePublishesToAFileNameWithALiteralBackslash")
    {
        string temp_dir = MakeTempTestDir("diskfs_atomic_backslash_name");
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
        REQUIRE(fs::create_directories(temp_dir));

        // Raw concatenation on purpose: a path combiner normalizes the literal backslash away
        string file_path = temp_dir + "/na\\me.bin";
        array<uint8_t, 3> before = {'o', 'l', 'd'};
        array<uint8_t, 3> after = {'n', 'e', 'w'};

        {
            fs::disk_write_file writer {file_path};
            REQUIRE(static_cast<bool>(writer));
            REQUIRE(writer.write(before));
            REQUIRE(writer.flush());
        }

        fs::disk_read_file reader {file_path};
        REQUIRE(static_cast<bool>(reader));
        array<uint8_t, 3> seeded {};
        REQUIRE(reader.read_at(0, seeded));
        CHECK(seeded == before);

        REQUIRE(fs::write_file(file_path, after));
        CHECK(fs::compare_file_content(file_path, after));
        array<uint8_t, 3> old_bytes {};
        REQUIRE(reader.read_at(0, old_bytes));
        CHECK(old_bytes == before);
        reader.close();
        CHECK(fs::list_dir_file_names(temp_dir).size() == 1);
        CHECK_FALSE(fs::is_dir(temp_dir + "/na"));
    }
#endif

#if FO_WINDOWS
    SECTION("AtomicWriteRefusesCrtReadersWithoutDeleteSharingAndPreservesTheDestination")
    {
        string temp_dir = MakeTempTestDir("diskfs_atomic_crt_reader");
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
        array<uint8_t, 3> before {'o', 'l', 'd'};
        array<uint8_t, 3> after {'n', 'e', 'w'};

        for (size_t name_length = 1; name_length <= 4; ++name_length) {
            string file_path = strex(temp_dir).combine_path(string(name_length, 'x') + ".bin").str();
            string probe_path = file_path + ".probe";
            CAPTURE(file_path);
            REQUIRE(fs::write_file(file_path, before));
            auto reader = fs::open_ifstream(file_path);
            REQUIRE(reader.is_open());
            REQUIRE(fs::write_file(probe_path, after));

            REQUIRE_FALSE(fs::rename(probe_path, file_path));
            CHECK_FALSE(fs::write_file(file_path, after));
            CHECK(fs::compare_file_content(file_path, before));
            array<uint8_t, 3> old_bytes {};
            REQUIRE(fs::stream_read_exact(reader, old_bytes));
            CHECK(old_bytes == before);
            reader.close();
            REQUIRE(fs::remove_file(probe_path));
            REQUIRE(fs::write_file(file_path, after));
            CHECK(fs::compare_file_content(file_path, after));
        }

        CHECK(fs::list_dir_file_names(temp_dir).size() == 4);
    }

    SECTION("AtomicPublicationPreservesMappedLibrariesAndTheDestinationOnRefusal")
    {
        auto system_root = winapi::get_environment_variable("SystemRoot");
        REQUIRE(system_root.has_value());
        string library_path = strex(*system_root).combine_path("System32/version.dll").str();
        auto image = fs::read_file(library_path);
        REQUIRE(image.has_value());
        string temp_dir = MakeTempTestDir("diskfs_atomic_mapped_image");
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
        array<uint8_t, 3> after {'n', 'e', 'w'};

        for (size_t name_length = 1; name_length <= 4; ++name_length) {
            string file_path = strex(temp_dir).combine_path(string(name_length, 'x') + ".dll").str();
            CAPTURE(file_path);
            REQUIRE(fs::write_file(file_path, *image));

            {
                auto module = winapi::load_library(file_path);
                REQUIRE(static_cast<bool>(module));
                auto unload = scope_exit([module]() noexcept { winapi::free_library(module); });
                CHECK(*module.reinterpret_as<const uint8_t>() == 'M');

                if (fs::write_file(file_path, after)) {
                    CHECK(fs::compare_file_content(file_path, after));
                }
                else {
                    CHECK(fs::read_file(file_path) == image);
                }

                CHECK(*module.reinterpret_as<const uint8_t>() == 'M');
            }

            REQUIRE(fs::write_file(file_path, after));
            CHECK(fs::compare_file_content(file_path, after));
        }

        CHECK(fs::list_dir_file_names(temp_dir).size() == 4);
    }
#endif

    SECTION("ExclusiveWriterDoesNotTruncateAnExistingFile")
    {
        string temp_dir = MakeTempTestDir("diskfs_exclusive");
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
        string file_path = strex(temp_dir).combine_path("original.bin").str();
        REQUIRE(fs::write_file(file_path, string_view {"original"}));

        fs::disk_write_file writer {file_path, fs::disk_write_mode::create_new};
        CHECK_FALSE(static_cast<bool>(writer));
        CHECK(fs::read_file(file_path) == optional<string> {"original"});
    }

    SECTION("ReadWriteRenameAndRemoveRoundtrip")
    {
        string temp_dir = MakeTempTestDir("diskfs_roundtrip");
        string file_path = strex(temp_dir).combine_path("nested/data.txt").str();
        string renamed_path = strex(temp_dir).combine_path("nested/renamed.txt").str();
        string_view content {"hello filesystem"};

        bool removed_before_roundtrip = fs::remove_dir_tree(temp_dir);
        ignore_unused(removed_before_roundtrip);

        REQUIRE(fs::write_file(file_path, content));
        CHECK(fs::exists(file_path));
        CHECK_FALSE(fs::is_dir(file_path));
        REQUIRE(fs::file_size(file_path).has_value());
        CHECK(*fs::file_size(file_path) == content.size());
        REQUIRE(fs::read_file(file_path).has_value());
        CHECK(*fs::read_file(file_path) == content);
        CHECK(fs::compare_file_content(file_path, {reinterpret_cast<const uint8_t*>(content.data()), content.size()}));

        REQUIRE(fs::rename(file_path, renamed_path));
        CHECK_FALSE(fs::exists(file_path));
        CHECK(fs::exists(renamed_path));
        REQUIRE(fs::remove_file(renamed_path));
        CHECK_FALSE(fs::exists(renamed_path));
        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("LongUnicodePathsSupportRelativeAndAbsoluteFileOperations")
    {
        auto temp_leaf = std::filesystem::path {fs::make_path(MakeTempTestDir("long_paths"))}.filename();
        string temp_dir = fs::path_to_string(std::filesystem::current_path() / temp_leaf);
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
        string relative_file {"Юникод"};

        while (relative_file.size() < 350) {
            relative_file = strex(relative_file).combine_path("nested-directory-segment").str();
        }

        relative_file = strex(relative_file).combine_path("данные.bin").str();
        string file_path = strex(temp_dir).combine_path(relative_file).str();
        string renamed_path = strex(temp_dir).combine_path("renamed.bin").str();
        string control_path = strex(temp_dir).combine_path("control.bin").str();
        string_view content {"long path payload"};
        auto native_file = std::filesystem::path {fs::make_path(file_path)};
        auto relative_native = native_file.lexically_relative(std::filesystem::current_path());
        string relative_input = fs::path_to_string(relative_native);
        REQUIRE(native_file.native().size() > 320);
        REQUIRE_FALSE(relative_input.empty());
        CHECK(fs::is_absolute_path(file_path));
        CHECK(fs::is_relative_path(relative_input));
        REQUIRE(fs::write_file(control_path, content));
        REQUIRE(fs::write_file(relative_input, content));
        REQUIRE(fs::exists(file_path));
        REQUIRE(fs::file_size(file_path).has_value());
        CHECK(*fs::file_size(file_path) == content.size());
        REQUIRE(fs::read_file(file_path).has_value());
        CHECK(*fs::read_file(file_path) == content);
        REQUIRE(fs::read_file_bounded(relative_input, content.size()).has_value());
        CHECK(*fs::read_file_bounded(relative_input, content.size()) == content);
        CHECK_FALSE(fs::read_file_bounded(relative_input, content.size() - 1).has_value());
        REQUIRE(fs::hash_file(relative_input).has_value());
        CHECK(fs::hash_file(relative_input) == fs::hash_file(control_path));
        REQUIRE(fs::touch_file(file_path));
        CHECK(fs::last_write_time(file_path) != 0);
        {
            auto stream = fs::open_ifstream(relative_input);
            REQUIRE(stream.is_open());
            CHECK(fs::stream_get_size(stream) == content.size());
        }

        vector<string> entries;
        fs::iterate_dir(temp_dir, true, [&](string_view name, size_t, uint64_t) { entries.emplace_back(name); });
        CHECK(entries.size() == 2);
        CHECK(std::ranges::find(entries, relative_file) != entries.end());
        CHECK(fs::make_io_path(control_path) == control_path);
#if FO_WINDOWS
        string io_path = fs::make_io_path(file_path);
        CHECK(io_path.starts_with("\\\\?\\"));
        {
            std::ifstream io_stream {std::filesystem::path {fs::make_path(io_path)}, std::ios::binary};
            CHECK(io_stream.is_open());
        }
        CHECK(fs::read_file(file_path + ". ") == fs::read_file(file_path));
        CHECK(fs::read_file(control_path + ". ") == fs::read_file(control_path));
        string extended_path = string {"\\\\?\\"} + file_path;
        std::ranges::replace(extended_path, '/', '\\');
        CHECK(fs::read_file(extended_path) == fs::read_file(file_path));
        string literal_path = extended_path + ". ";
        string_view literal_content {"literal extended path"};
        REQUIRE(fs::write_file(literal_path, literal_content));
        REQUIRE(fs::read_file(literal_path).has_value());
        CHECK(*fs::read_file(literal_path) == literal_content);
        CHECK(fs::read_file(file_path + ". ") == fs::read_file(file_path));
        REQUIRE(fs::remove_file(literal_path));
        string literal_dir = extended_path + ".dir. ";
        string literal_child = literal_dir + "\\child.bin";
        REQUIRE(fs::write_file(literal_child, literal_content));
        vector<string> literal_entries;
        fs::iterate_dir(literal_dir, true, [&](string_view name, size_t, uint64_t) { literal_entries.emplace_back(name); });
        REQUIRE(literal_entries.size() == 1);
        CHECK(literal_entries.front() == "child.bin");
        REQUIRE(fs::remove_dir_tree(literal_dir));
        string short_dir = strex(temp_dir).combine_path("short-dir").str();
        string literal_short_dir = string {"\\\\?\\"} + short_dir + ". ";
        std::ranges::replace(literal_short_dir, '/', '\\');
        string literal_short_child = literal_short_dir + "\\child.bin";
        REQUIRE(fs::write_file(strex(short_dir).combine_path("child.bin").str(), content));
        REQUIRE(fs::write_file(literal_short_child, literal_content));
        REQUIRE(fs::remove_dir_tree(short_dir + ". "));
        CHECK_FALSE(fs::exists(short_dir));
        REQUIRE(fs::read_file(literal_short_child).has_value());
        CHECK(*fs::read_file(literal_short_child) == literal_content);
        REQUIRE(fs::remove_dir_tree(literal_short_dir));
#else
        CHECK(fs::make_io_path(file_path) == file_path);
#endif

        REQUIRE(fs::rename(file_path, renamed_path));
        CHECK_FALSE(fs::exists(file_path));
        REQUIRE(fs::read_file(renamed_path).has_value());
        CHECK(*fs::read_file(renamed_path) == content);
        REQUIRE(fs::remove_file(renamed_path));
        string long_dir = fs::path_to_string(native_file.parent_path());
        REQUIRE(fs::exists(long_dir));
        REQUIRE(fs::remove_dir_tree(temp_dir));
        CHECK_FALSE(fs::exists(long_dir));
        CHECK_FALSE(fs::exists(temp_dir));
    }

    SECTION("IterateDirRespectsRecursiveFlag")
    {
        string temp_dir = MakeTempTestDir("diskfs_iterate");
        string top_file = strex(temp_dir).combine_path("top.txt").str();
        string nested_file = strex(temp_dir).combine_path("sub/nested.txt").str();

        bool removed_before_iterate = fs::remove_dir_tree(temp_dir);
        ignore_unused(removed_before_iterate);
        REQUIRE(fs::write_file(top_file, string_view {"top"}));
        REQUIRE(fs::write_file(nested_file, string_view {"nested"}));

        vector<string> flat_entries;
        fs::iterate_dir(temp_dir, false, [&](string_view path, size_t, uint64_t) { flat_entries.emplace_back(path); });
        CHECK(flat_entries.size() == 1);
        CHECK(flat_entries.front() == "top.txt");

        vector<string> recursive_entries;
        fs::iterate_dir(temp_dir, true, [&](string_view path, size_t, uint64_t) { recursive_entries.emplace_back(path); });
        CHECK(recursive_entries.size() == 2);
        CHECK(std::ranges::find(recursive_entries, string {"top.txt"}) != recursive_entries.end());
        CHECK(std::ranges::find(recursive_entries, string {"sub/nested.txt"}) != recursive_entries.end());

        CHECK(fs::remove_dir_tree(temp_dir));
    }

#if FO_LINUX || FO_MAC
    SECTION("IterateDirPropagatesFilesystemErrors")
    {
        string temp_dir = MakeTempTestDir("diskfs_failed_lookup");
        REQUIRE(fs::create_directories(temp_dir));
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
        auto loop_path = std::filesystem::path {fs::make_path(temp_dir)} / "loop";
        std::filesystem::create_directory_symlink("loop", loop_path);
        bool visited = false;
        CHECK_THROWS_AS(fs::iterate_dir(fs::path_to_string(loop_path), false, [&](string_view, size_t, uint64_t) { visited = true; }), std::filesystem::filesystem_error);
        CHECK_FALSE(visited);
    }
#endif

    SECTION("TouchAndStreamHelpersWork")
    {
        string temp_dir = MakeTempTestDir("diskfs_stream");
        string file_path = strex(temp_dir).combine_path("touch.bin").str();

        bool removed_before_stream = fs::remove_dir_tree(temp_dir);
        ignore_unused(removed_before_stream);
        REQUIRE(fs::create_directories(temp_dir));
        REQUIRE(fs::touch_file(file_path));
        CHECK(fs::exists(file_path));
        CHECK(fs::last_write_time(file_path) != 0);

        std::istringstream stream {std::string {"abcdef"}, std::ios::binary};
        CHECK(fs::stream_get_size(stream) == 6);
        CHECK(fs::stream_get_read_pos(stream) == 0);

        array<char, 3> buf {};
        auto buf_data = make_nptr(buf.data());
        REQUIRE(fs::stream_read_exact(stream, make_span(buf_data, buf.size())));
        CHECK(string_view {buf.data(), buf.size()} == "abc");
        CHECK(fs::stream_get_read_pos(stream) == 3);
        REQUIRE(fs::stream_set_read_pos(stream, 1, std::ios_base::cur));
        CHECK(fs::stream_get_read_pos(stream) == 4);
        REQUIRE(fs::stream_read_exact(stream, make_span(buf_data, 2)));
        CHECK(string_view {buf.data(), 2} == "ef");

        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("FileHashMatchesInMemoryReference")
    {
        string temp_dir = MakeTempTestDir("diskfs_hash");
        string file_path = strex(temp_dir).combine_path("hash.bin").str();
        bool removed_before_hash = fs::remove_dir_tree(temp_dir);
        ignore_unused(removed_before_hash);

        auto check_hash = [&file_path](size_t size) {
            vector<uint8_t> data(size);

            for (size_t index = 0; index < size; index++) {
                data[index] = numeric_cast<uint8_t>((index * 37u + 11u) & 0xFFu);
            }

            REQUIRE(fs::write_file(file_path, data));
            REQUIRE(fs::hash_file(file_path).has_value());
            CHECK(*fs::hash_file(file_path) == fs::hash_data(data));
        };

        for (auto size : {size_t(0), size_t(1), size_t(3), size_t(4), size_t(15), size_t(16), size_t(17), size_t(47), size_t(48), size_t(49), size_t(63), size_t(64), size_t(65), size_t(96), size_t(97), size_t(70000)}) {
            check_hash(size);
        }

        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("MakeWritablePathLayersRelativeUnderRoot")
    {
        string root = strex("/data").combine_path("user").str();
        string nested_relative = strex("Resources").combine_path("Sub").str();

        // Portable layout (empty root): the relative path is returned unchanged, written next to the exe
        CHECK(fs::make_writable_path("", "Cache") == "Cache");
        CHECK(fs::make_writable_path("", nested_relative) == nested_relative);

        // Installed layout: the relative path is layered under the writable root
        CHECK(fs::make_writable_path(root, "Cache") == strex(root).combine_path("Cache").str());
        CHECK(fs::make_writable_path(root, nested_relative) == strex(root).combine_path(nested_relative).str());

        // An already-absolute relative path is never relocated under the root, in either layout
        string absolute_input = MakeTempTestDir("diskfs_writable_abs");
        CHECK(fs::is_absolute_path(absolute_input));
        CHECK(fs::make_writable_path(root, absolute_input) == absolute_input);
        CHECK(fs::make_writable_path("", absolute_input) == absolute_input);
    }
}

// Which primitive lands the requested name and which keeps whatever the entry is already called, on both
// filesystem kinds — the split callers addressing files by exact name depend on
static auto IsCaseInsensitiveFs(string_view dir) -> bool
{
    string upper_probe = strex(dir).combine_path("CaseProbe.tmp").str();
    string lower_probe = strex(dir).combine_path("caseprobe.tmp").str();

    if (!fs::write_file(upper_probe, string_view {"probe"})) {
        return false;
    }

    bool case_insensitive = fs::exists(lower_probe);
    ignore_unused(fs::remove_file(upper_probe));

    return case_insensitive;
}

static auto HasExactDirEntry(string_view dir, string_view name) -> bool
{
    std::error_code ec;

    for (const auto& entry : std::filesystem::directory_iterator {std::filesystem::path {fs::make_path(dir)}, ec}) {
        if (fs::path_to_string(entry.path().filename()) == name) {
            return true;
        }
    }

    return false;
}

TEST_CASE("DiskFileSystemNameCase")
{
    SECTION("RenameLandsTheRequestedNameOverADifferentlyCasedTarget")
    {
        string temp_dir = MakeTempTestDir("diskfs_case_rename");
        string upper_path = strex(temp_dir).combine_path("Data.txt").str();
        string lower_path = strex(temp_dir).combine_path("data.txt").str();
        string source_path = strex(temp_dir).combine_path("Source.tmp").str();

        ignore_unused(fs::remove_dir_tree(temp_dir));
        REQUIRE(fs::create_directories(temp_dir));

        bool case_insensitive = IsCaseInsensitiveFs(temp_dir);

        REQUIRE(fs::write_file(upper_path, string_view {"before"}));
        REQUIRE(fs::write_file(source_path, string_view {"after"}));
        REQUIRE(fs::rename(source_path, lower_path));

        // Renaming replaces the whole directory entry, so unlike an overwriting write it does establish the
        // requested spelling — Updater::ReplaceFileSafely relies on exactly this
        CHECK(HasExactDirEntry(temp_dir, "data.txt"));
        REQUIRE(fs::read_file(lower_path).has_value());
        CHECK(*fs::read_file(lower_path) == "after");
        CHECK_FALSE(fs::exists(source_path));

        if (case_insensitive) {
            CHECK_FALSE(HasExactDirEntry(temp_dir, "Data.txt"));
        }

        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("WriteFileTargetsTheSameFileRegardlessOfNameCase")
    {
        string temp_dir = MakeTempTestDir("diskfs_case_write");
        string upper_path = strex(temp_dir).combine_path("Data.txt").str();
        string lower_path = strex(temp_dir).combine_path("data.txt").str();

        ignore_unused(fs::remove_dir_tree(temp_dir));
        REQUIRE(fs::create_directories(temp_dir));

        bool case_insensitive = IsCaseInsensitiveFs(temp_dir);

        REQUIRE(fs::write_file(upper_path, string_view {"first"}));
        REQUIRE(fs::write_file(lower_path, string_view {"second"}));

        // The content written last always wins under the name it was written with; which directory entry name
        // survives is deliberately not guaranteed, so a caller needing the exact name reconciles it itself
        REQUIRE(fs::read_file(lower_path).has_value());
        CHECK(*fs::read_file(lower_path) == "second");
        REQUIRE(fs::read_file(upper_path).has_value());
        CHECK(*fs::read_file(upper_path) == (case_insensitive ? "second" : "first"));

        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("CreateDirectoriesKeepsAnExistingDifferentlyCasedDirectory")
    {
        string temp_dir = MakeTempTestDir("diskfs_case_dir");
        string upper_dir = strex(temp_dir).combine_path("Data").str();
        string lower_dir = strex(temp_dir).combine_path("data").str();

        ignore_unused(fs::remove_dir_tree(temp_dir));
        REQUIRE(fs::create_directories(temp_dir));

        bool case_insensitive = IsCaseInsensitiveFs(temp_dir);

        REQUIRE(fs::create_directories(upper_dir));
        REQUIRE(fs::create_directories(lower_dir));

        // Creating a directory never re-spells an existing one, so a case-only rename of a content directory keeps
        // the old spelling for every path underneath until a caller reconciles it, as the baker does
        CHECK(HasExactDirEntry(temp_dir, "Data"));
        CHECK(HasExactDirEntry(temp_dir, "data") == !case_insensitive);

        CHECK(fs::remove_dir_tree(temp_dir));
    }
}

TEST_CASE("DiskFilePrimitives")
{
    SECTION("WriteHandleAppendsWhileReadHandleSeeksFreely")
    {
        string temp_dir = MakeTempTestDir("diskfs_handles");
        string file_path = strex(temp_dir).combine_path("pack.bin").str();

        ignore_unused(fs::remove_dir_tree(temp_dir));
        REQUIRE(fs::create_directories(temp_dir));

        array<uint8_t, 4> head_bytes = {'a', 'b', 'c', 'd'};
        array<uint8_t, 4> tail_bytes = {'e', 'f', 'g', 'h'};
        array<uint8_t, 1> patch_byte = {'A'};

        {
            fs::disk_write_file writer {file_path};
            REQUIRE(static_cast<bool>(writer));
            REQUIRE(writer.preallocate(8));
            REQUIRE(writer.write(const_span<uint8_t> {head_bytes.data(), head_bytes.size()}));
            REQUIRE(writer.write(const_span<uint8_t> {tail_bytes.data(), tail_bytes.size()}));
            REQUIRE(writer.seek_to_begin());
            REQUIRE(writer.write(const_span<uint8_t> {patch_byte.data(), patch_byte.size()}));
            REQUIRE(writer.flush());
        }

        REQUIRE(fs::file_size(file_path).has_value());
        CHECK(*fs::file_size(file_path) == 8);

        fs::disk_read_file reader {file_path};
        REQUIRE(static_cast<bool>(reader));
        CHECK(reader.get_size() == 8);

        // Reads carry their own offset, so they neither disturb each other nor depend on the order they run in
        array<uint8_t, 4> read_tail = {};
        array<uint8_t, 4> read_head = {};
        REQUIRE(reader.read_at(4, span<uint8_t> {read_tail.data(), read_tail.size()}));
        REQUIRE(reader.read_at(0, span<uint8_t> {read_head.data(), read_head.size()}));
        CHECK(string_view {reinterpret_cast<const char*>(read_head.data()), read_head.size()} == "Abcd");
        CHECK(string_view {reinterpret_cast<const char*>(read_tail.data()), read_tail.size()} == "efgh");

        // A read running past the end fails outright rather than reporting a short count
        CHECK_FALSE(reader.read_at(6, span<uint8_t> {read_tail.data(), read_tail.size()}));

        // Windows refuses to unlink a file while a handle on it is open, so the reader goes first
        reader.close();
        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("HandlesAndDurableRenameTakeUnicodeAndLongPaths")
    {
        // A writable root under a profile such as C:/Users/Иван is the common case for a Russian player, and the
        // engine's own long paths pass through these handles as they do through every other fs call
        string temp_dir = MakeTempTestDir("diskfs_handles_unicode");
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
        string dir = strex(temp_dir).combine_path("Игрок").str();

        // Bytes, not characters: 600 of mostly two-byte letters still passes MAX_PATH as UTF-16
        while (dir.size() < 600) {
            dir = strex(dir).combine_path("вложенный-каталог").str();
        }

        REQUIRE(fs::create_directories(dir));
        string file_path = strex(dir).combine_path("Ресурсы.fores").str();
        string moved_path = strex(dir).combine_path("Перемещён.fores").str();
        array<uint8_t, 3> bytes = {'a', 'b', 'c'};

        {
            fs::disk_write_file writer {file_path};
            REQUIRE(static_cast<bool>(writer));
            REQUIRE(writer.write(const_span<uint8_t> {bytes.data(), bytes.size()}));
            REQUIRE(writer.flush());
        }

        REQUIRE(fs::rename_durable(file_path, moved_path));
        CHECK_FALSE(fs::exists(file_path));

        fs::disk_read_file reader {moved_path};
        REQUIRE(static_cast<bool>(reader));
        array<uint8_t, 3> read_back = {};
        REQUIRE(reader.read_at(0, span<uint8_t> {read_back.data(), read_back.size()}));
        CHECK(read_back == bytes);
        CHECK(fs::available_space(dir).has_value());

        array<uint8_t, 3> replacement = {'x', 'y', 'z'};
        REQUIRE(fs::write_file(moved_path, replacement));
        CHECK(fs::compare_file_content(moved_path, replacement));
        REQUIRE(reader.read_at(0, span<uint8_t> {read_back.data(), read_back.size()}));
        CHECK(read_back == bytes);
        reader.close();
    }

    SECTION("SyncParentPersistsEntriesOnEveryPlatform")
    {
        // An answer of true has to mean the directory was flushed, so a directory that is not there answers false
        string temp_dir = MakeTempTestDir("diskfs_sync_parent");
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
        string dir = strex(temp_dir).combine_path("Каталог").str();
        REQUIRE(fs::create_directories(dir));
        string file_path = strex(dir).combine_path("Файл.bin").str();

        REQUIRE(fs::write_file(file_path, string_view {"x"}));
        CHECK(fs::sync_parent(file_path));
        REQUIRE(fs::remove_file(file_path));
        CHECK(fs::sync_parent(file_path));
        CHECK_FALSE(fs::sync_parent(strex(temp_dir).combine_path("Нет").combine_path("Файл.bin").str()));
    }

    SECTION("OpeningAMissingFileLeavesTheHandleClosed")
    {
        string temp_dir = MakeTempTestDir("diskfs_missing");
        fs::disk_read_file reader {strex(temp_dir).combine_path("absent.bin").str()};

        CHECK_FALSE(static_cast<bool>(reader));
    }

    SECTION("ListingNamesKeepsWhatIterationHides")
    {
        string temp_dir = MakeTempTestDir("diskfs_listing");

        ignore_unused(fs::remove_dir_tree(temp_dir));
        REQUIRE(fs::create_directories(temp_dir));
        REQUIRE(fs::create_directories(strex(temp_dir).combine_path("sub").str()));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("plain.bin").str(), string_view {"a"}));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("~temp.bin").str(), string_view {"b"}));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path(".hidden").str(), string_view {"c"}));

        vector<string> listed = fs::list_dir_file_names(temp_dir);
        vector<string> iterated;

        fs::iterate_dir(temp_dir, false, [&](string_view path, size_t size, uint64_t write_time) {
            ignore_unused(size, write_time);
            iterated.emplace_back(path);
        });

        // The updater sweeps its own '~' temp files, which the filtering iteration never shows it
        CHECK(std::find(listed.begin(), listed.end(), "~temp.bin") != listed.end());
        CHECK(std::find(listed.begin(), listed.end(), ".hidden") != listed.end());
        CHECK(std::find(iterated.begin(), iterated.end(), "~temp.bin") == iterated.end());
        CHECK(listed.size() == 3);

        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("ContainedRelativePathRefusesWhatWouldResolveOutside")
    {
        CHECK(fs::is_contained_relative_path("Core.fores"));
        CHECK(fs::is_contained_relative_path("Packs/Core.fores"));
        CHECK(fs::is_contained_relative_path("Packs\\Core.fores"));

        CHECK_FALSE(fs::is_contained_relative_path(""));
        CHECK_FALSE(fs::is_contained_relative_path("../Core.fores"));
        CHECK_FALSE(fs::is_contained_relative_path("Packs/../../Core.fores"));
        CHECK_FALSE(fs::is_contained_relative_path("Packs\\..\\Core.fores"));
        CHECK_FALSE(fs::is_contained_relative_path("/etc/passwd"));
        CHECK(fs::is_contained_relative_path("Ресурсы/Core.fores"));

        // A server-sent name is placed on disk, so what a native path would read differently is refused everywhere
        CHECK_FALSE(fs::is_contained_relative_path(string_view {"x.dll\0.fores", 12}));
        CHECK_FALSE(fs::is_contained_relative_path("C:Core.fores"));
        CHECK_FALSE(fs::is_contained_relative_path("Core.fores:stream"));
        CHECK_FALSE(fs::is_contained_relative_path("Core\xFF.fores"));

#if FO_WINDOWS
        // A drive-qualified path is absolute here and an ordinary file name on POSIX, so it is asserted
        // only where the answer is the interesting one
        CHECK_FALSE(fs::is_contained_relative_path("C:\\Windows\\System32\\evil.dll"));
#endif
    }

    SECTION("RecursiveFileListingIncludesUnfilteredRelativeNames")
    {
        string temp_dir = MakeTempTestDir("diskfs_recursive_listing");
        auto cleanup = scope_exit([&]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("_Nested/.Hidden/File-backup").str(), "data"));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("~Pending").str(), "data"));
        vector<string> direct = fs::list_dir_file_names(temp_dir);
        vector<string> recursive = fs::list_dir_file_names(temp_dir, true);
        CHECK(std::find(direct.begin(), direct.end(), "~Pending") != direct.end());
        CHECK(std::find(direct.begin(), direct.end(), "_Nested/.Hidden/File-backup") == direct.end());
        CHECK(std::find(recursive.begin(), recursive.end(), "~Pending") != recursive.end());
        CHECK(std::find(recursive.begin(), recursive.end(), "_Nested/.Hidden/File-backup") != recursive.end());
    }

    SECTION("AvailableSpaceAnswersForAnExistingDirectoryOnly")
    {
        string temp_dir = MakeTempTestDir("diskfs_space");

        ignore_unused(fs::remove_dir_tree(temp_dir));
        REQUIRE(fs::create_directories(temp_dir));

        auto available = fs::available_space(temp_dir);
        REQUIRE(available.has_value());
        CHECK(*available > 0);

        CHECK_FALSE(fs::available_space(strex(temp_dir).combine_path("no/such/place").str()).has_value());

        CHECK(fs::remove_dir_tree(temp_dir));
    }
}

// Ready argument types isolate the function contract from potentially throwing string_view conversions
static_assert(noexcept(fs::try_lock_file(std::declval<string_view>())));
static_assert(noexcept(fs::exists(std::declval<string_view>())) && noexcept(fs::is_dir(std::declval<string_view>())));
static_assert(noexcept(fs::is_absolute_path(std::declval<string_view>())) && noexcept(fs::is_relative_path(std::declval<string_view>())) && noexcept(fs::is_contained_relative_path(std::declval<string_view>())));
static_assert(noexcept(fs::create_directories(std::declval<string_view>())) && noexcept(fs::last_write_time(std::declval<string_view>())));
static_assert(noexcept(fs::file_size(std::declval<string_view>())) && noexcept(fs::available_space(std::declval<string_view>())));
static_assert(noexcept(fs::hash_data(const_span<uint8_t> {})));
static_assert(noexcept(fs::write_file(std::declval<string_view>(), string_view {})) && noexcept(fs::write_file(std::declval<string_view>(), const_span<uint8_t> {})));
static_assert(noexcept(fs::write_file_durable_new(std::declval<string_view>(), const_span<uint8_t> {})));
static_assert(noexcept(fs::remove_file(std::declval<string_view>())) && noexcept(fs::remove_dir_tree(std::declval<string_view>())) && noexcept(fs::touch_file(std::declval<string_view>())));
static_assert(noexcept(fs::sync_parent(std::declval<string_view>())) && noexcept(fs::rename(std::declval<string_view>(), std::declval<string_view>())) && noexcept(fs::rename_durable(std::declval<string_view>(), std::declval<string_view>())) && noexcept(fs::replace_file(std::declval<string_view>(), std::declval<string_view>())));
static_assert(noexcept(fs::list_dir_file_names(std::declval<string_view>())));
static_assert(std::is_nothrow_destructible_v<fs::exclusive_file_lock> && std::is_nothrow_destructible_v<fs::disk_read_file> && std::is_nothrow_destructible_v<fs::disk_write_file> && std::is_nothrow_destructible_v<fs::disk_directory_lock>);
static_assert(std::is_nothrow_constructible_v<fs::disk_read_file, string_view> && std::is_nothrow_constructible_v<fs::disk_read_file, string_view, uint64_t, uint64_t>);
static_assert(std::is_nothrow_constructible_v<fs::disk_write_file, string_view> && std::is_nothrow_constructible_v<fs::disk_directory_lock, string_view>);
static_assert(!noexcept(fs::make_path(std::declval<string_view>())) && !noexcept(fs::path_to_string(std::declval<const std::filesystem::path&>())));
static_assert(!noexcept(fs::resolve_path(std::declval<string_view>())) && !noexcept(fs::make_io_path(std::declval<string_view>())) && !noexcept(fs::make_writable_path(std::declval<string_view>(), std::declval<string_view>())));
static_assert(noexcept(fs::read_file(std::declval<string_view>())) && noexcept(fs::read_file_bounded(std::declval<string_view>(), 0)) && noexcept(fs::compare_file_content(std::declval<string_view>(), const_span<uint8_t> {})));
static_assert(noexcept(fs::hash_file(std::declval<string_view>())));
static_assert(!noexcept(fs::open_ifstream(std::declval<string_view>())));
static_assert(!noexcept(fs::iterate_dir(std::declval<string_view>(), false, std::declval<const fs::file_visitor&>())));
static_assert(!noexcept(fs::stream_read_exact(std::declval<std::istream&>(), span<uint8_t> {})));
static_assert(!noexcept(fs::stream_get_size(std::declval<std::istream&>())) && !noexcept(fs::stream_get_read_pos(std::declval<std::istream&>())));
static_assert(!noexcept(fs::stream_set_read_pos(std::declval<std::istream&>(), 0, std::ios_base::beg)));

TEST_CASE("DiskFileSystemRejectsInvalidUtf8")
{
    string root = MakeTempTestDir("diskfs_invalid_utf8");
    auto cleanup = scope_exit([&]() noexcept { (void)fs::remove_dir_tree(root); });
    string bad = root + "/bad-\xFF.bin";
    string replacement = root + "/bad-\xEF\xBF\xBD.bin";
    string source = root + "/source.bin";
    array<uint8_t, 1> bytes {'x'};
    for (string_view invalid : {string_view {"\xED\xA0\x80"}, string_view {"\xC0\x80"}, string_view {"\xF4\x90\x80\x80"}, string_view {"\xEF\xBF\xBD"}}) {
        CHECK_THROWS_AS(fs::make_path(invalid), std::filesystem::filesystem_error);
        CHECK_THROWS_AS(fs::make_writable_path("", invalid), std::filesystem::filesystem_error);
        CHECK_FALSE(fs::is_relative_path(invalid));
    }

    CHECK_FALSE(fs::write_file(replacement, "sentinel"));
    CHECK_FALSE(fs::exists(replacement));
    CHECK_FALSE(fs::read_file(replacement).has_value());
    CHECK_THROWS_AS(fs::path_to_string(std::filesystem::path {u8"bad-\uFFFD.bin"}), std::filesystem::filesystem_error);
    REQUIRE(fs::write_file(source, "source"));
    CHECK_FALSE(fs::exists(bad));
    CHECK_FALSE(fs::is_dir(bad));
    CHECK_FALSE(fs::is_absolute_path(bad));
    CHECK_FALSE(fs::is_relative_path(bad));
    CHECK(fs::last_write_time(bad) == 0);
    CHECK_FALSE(fs::file_size(bad).has_value());
    CHECK_FALSE(fs::available_space(bad).has_value());
    CHECK_FALSE(fs::create_directories(bad));
    CHECK_FALSE(fs::write_file(bad, bytes));
    CHECK_FALSE(fs::write_file_durable_new(bad, bytes));
    CHECK_FALSE(fs::remove_file(bad));
    CHECK_FALSE(fs::remove_dir_tree(bad));
    CHECK_FALSE(fs::touch_file(bad));
    CHECK_FALSE(fs::sync_parent(bad));
    CHECK_FALSE(fs::rename(source, bad));
    CHECK_FALSE(fs::rename_durable(source, bad));
    CHECK_FALSE(fs::replace_file(source, bad));
    CHECK_FALSE(fs::try_lock_file(bad).owns_lock());
    CHECK_FALSE(static_cast<bool>(fs::disk_read_file {bad}));
    CHECK_FALSE(static_cast<bool>(fs::disk_write_file {bad}));
    CHECK_FALSE(static_cast<bool>(fs::disk_directory_lock {bad}));
    CHECK(fs::list_dir_file_names(bad).empty());
    CHECK_FALSE(fs::read_file(bad).has_value());
    CHECK_FALSE(fs::open_ifstream(bad).is_open());
    CHECK_THROWS_AS(fs::make_path(bad), std::filesystem::filesystem_error);
    CHECK_THROWS_AS(fs::make_io_path(bad), std::filesystem::filesystem_error);
    CHECK(fs::read_file(source) == optional<string> {"source"});
}

#if !FO_WINDOWS
TEST_CASE("DiskFileSystemRejectsInvalidUtf8NativeBytes")
{
    // Built straight from native bytes: fs::make_path would refuse this fixture before path_to_string
    // sees it, and Windows cannot construct such a native path at all
    std::filesystem::path native_bad {std::filesystem::path::string_type {"bad-\xFF.bin"}};
    CHECK_THROWS_AS(fs::path_to_string(native_bad), std::filesystem::filesystem_error);
}
#endif

TEST_CASE("DiskFileSystemPathContracts")
{
    string root = MakeTempTestDir("diskfs_path_contracts");
    auto cleanup = scope_exit([&]() noexcept { (void)fs::remove_dir_tree(root); });
    string file = root + "/sentinel.bin";
    string other = root + "/other.bin";
    string bad_file = file + string(1, '\0') + "/ignored";
    string bad_dir = root + string(1, '\0') + "/ignored";
    array<uint8_t, 3> bytes {'o', 'l', 'd'};
    REQUIRE(fs::write_file(file, bytes));

    SECTION("QueriesAndHandlesRejectTheWholeName")
    {
        CHECK_FALSE(fs::exists(bad_file));
        CHECK_FALSE(fs::is_dir(bad_dir));
        CHECK_FALSE(fs::is_absolute_path(bad_file));
        CHECK_FALSE(fs::is_relative_path(string_view {"relative\0suffix", 15}));
        CHECK_FALSE(fs::is_contained_relative_path(bad_file));
        CHECK(fs::last_write_time(bad_file) == 0);
        CHECK_FALSE(fs::file_size(bad_file).has_value());
        CHECK_FALSE(fs::available_space(bad_dir).has_value());
        CHECK_FALSE(fs::hash_file(bad_file).has_value());
        CHECK_FALSE(fs::read_file(bad_file).has_value());
        CHECK_FALSE(fs::read_file_bounded(bad_file, 100).has_value());
        CHECK_FALSE(fs::compare_file_content(bad_file, bytes));
        CHECK_FALSE(fs::open_ifstream(bad_file).is_open());
        CHECK_FALSE(static_cast<bool>(fs::open_ifstream(bad_file)));
        CHECK_FALSE(static_cast<bool>(fs::disk_read_file {bad_file}));
        CHECK_FALSE(static_cast<bool>(fs::disk_read_file {bad_file, 0, 3}));
        CHECK_FALSE(static_cast<bool>(fs::disk_directory_lock {bad_dir}));
        CHECK_FALSE(fs::try_lock_file(bad_file).owns_lock());
        CHECK(fs::list_dir_file_names(bad_dir).empty());
        CHECK(fs::list_dir_file_names(bad_dir, true).empty());
    }

    SECTION("PurePathConversionsRejectEmbeddedNul")
    {
        CHECK_THROWS_AS(fs::make_path(bad_file), std::filesystem::filesystem_error);
        CHECK_THROWS_AS(fs::resolve_path(bad_file), std::filesystem::filesystem_error);
        CHECK_THROWS_AS(fs::make_io_path(bad_file), std::filesystem::filesystem_error);
        auto native = std::filesystem::path {fs::make_path(file)};
        native += std::filesystem::path::string_type(1, std::filesystem::path::value_type {});
        CHECK_THROWS_AS(fs::path_to_string(native), std::filesystem::filesystem_error);
        CHECK_THROWS_AS(fs::make_writable_path(bad_dir, "relative"), std::filesystem::filesystem_error);
        CHECK_THROWS_AS(fs::make_writable_path(bad_dir, file), std::filesystem::filesystem_error);
        CHECK_THROWS_AS(fs::make_writable_path(root, bad_file), std::filesystem::filesystem_error);
        CHECK_THROWS_AS(fs::make_writable_path("", bad_file), std::filesystem::filesystem_error);
    }

    SECTION("MutatorsPreserveExistingPrefixes")
    {
        for (auto mode : {fs::disk_write_mode::create, fs::disk_write_mode::append, fs::disk_write_mode::create_new}) {
            REQUIRE(fs::write_file(file, bytes));
            {
                fs::disk_write_file writer {bad_file, mode};
                CHECK_FALSE(static_cast<bool>(writer));
            }
            CHECK(fs::compare_file_content(file, bytes));
        }
        CHECK_FALSE(fs::write_file(bad_file, "replacement"));
        CHECK(fs::compare_file_content(file, bytes));
        CHECK_FALSE(fs::write_file(bad_file, bytes));
        CHECK(fs::compare_file_content(file, bytes));
        uint64_t before_touch = fs::last_write_time(file);
        CHECK_FALSE(fs::touch_file(bad_file));
        CHECK(fs::last_write_time(file) == before_touch);
        CHECK_FALSE(fs::remove_file(bad_file));
        CHECK(fs::compare_file_content(file, bytes));
        CHECK_FALSE(fs::remove_dir_tree(bad_dir));
        CHECK(fs::compare_file_content(file, bytes));
    }

    SECTION("InvalidNamesCannotCreateTheirPrefixesOrSyncADifferentParent")
    {
        string missing = root + "/missing";
        string bad_missing = missing + string(1, '\0') + "/child";
        CHECK_FALSE(fs::create_directories(bad_missing));
        CHECK_FALSE(fs::exists(missing));
        CHECK_FALSE(fs::touch_file(bad_missing));
        CHECK_FALSE(fs::exists(missing));
        CHECK_FALSE(fs::write_file_durable_new(bad_missing, bytes));
        CHECK_FALSE(fs::exists(missing));
        CHECK_FALSE(static_cast<bool>(fs::disk_write_file {bad_missing, fs::disk_write_mode::create_new}));
        CHECK_FALSE(fs::exists(missing));
        CHECK_FALSE(fs::sync_parent(bad_dir + "/leaf"));
    }

    SECTION("TwoPathOperationsValidateBothNamesBeforeMutation")
    {
        for (auto operation : {fs::rename, fs::rename_durable, fs::replace_file}) {
            for (int32_t invalid_side = 0; invalid_side != 3; ++invalid_side) {
                REQUIRE(fs::write_file(file, "source"));
                REQUIRE(fs::write_file(other, "destination"));
                string source = invalid_side != 1 ? bad_file : file;
                string target = invalid_side != 0 ? other + string(1, '\0') + "suffix" : other;
                CHECK_FALSE(operation(source, target));
                CHECK(fs::read_file(file) == optional<string> {"source"});
                CHECK(fs::read_file(other) == optional<string> {"destination"});
            }
        }
    }

    SECTION("DirectoryVisitorsNeverSeeAnInvalidPrefix")
    {
        bool visited = false;
        CHECK_THROWS_AS(fs::iterate_dir(bad_dir, true, [&](string_view, size_t, uint64_t) { visited = true; }), std::filesystem::filesystem_error);
        CHECK_FALSE(visited);
        CHECK_THROWS_AS(fs::iterate_dir(root, false, [](string_view, size_t, uint64_t) { throw std::runtime_error("visitor failure"); }), std::runtime_error);
    }

    SECTION("EmptyPathConventionsRemainValid")
    {
        CHECK(fs::create_directories(""));
        CHECK(fs::is_relative_path(""));
        CHECK_FALSE(fs::is_absolute_path(""));
        CHECK_FALSE(fs::is_contained_relative_path(""));
        CHECK_FALSE(fs::try_lock_file("").owns_lock());
        CHECK_FALSE(fs::write_file_durable_new("", bytes));
        CHECK(static_cast<bool>(fs::disk_directory_lock {""}));
    }

#if !FO_WINDOWS
    SECTION("PosixUtf8NamesAndBackslashParentsRemainValid")
    {
        // Raw concatenation keeps the backslash a literal directory name; the file name is valid UTF-8
        string parent = root + "/native\\parent";
        string path = parent + "/Юникод-\xEF\xBF\xBC.bin";
        REQUIRE(fs::write_file_durable_new(path, bytes));
        CHECK(fs::compare_file_content(path, bytes));
        CHECK_FALSE(fs::exists(root + "/native"));
        REQUIRE(fs::write_file(path, "next"));
        CHECK(fs::read_file(path) == optional<string> {"next"});
#if FO_LINUX || FO_MAC
        CHECK(fs::sync_parent(path));
#endif
    }
#endif
}

TEST_CASE("DiskFileSystemStreamExceptions")
{
    std::istringstream input {std::string {"x"}};
    input.exceptions(std::ios::failbit | std::ios::badbit);
    array<uint8_t, 2> bytes {};
    CHECK_THROWS_AS(fs::stream_read_exact(input, bytes), std::ios_base::failure);

    class failing_seek_buffer final : public std::streambuf
    {
        auto seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode) -> pos_type override { throw std::ios_base::failure("seek failure"); }
        auto seekpos(pos_type, std::ios_base::openmode) -> pos_type override { throw std::ios_base::failure("seek failure"); }
    } buffer;
    std::istream seeking {&buffer};
    seeking.exceptions(std::ios::badbit);
    CHECK_THROWS_AS(fs::stream_get_size(seeking), std::ios_base::failure);
    seeking.clear();
    CHECK_THROWS_AS(fs::stream_get_read_pos(seeking), std::ios_base::failure);
    seeking.clear();
    CHECK_THROWS_AS(fs::stream_set_read_pos(seeking, 0, std::ios_base::beg), std::ios_base::failure);
}

TEST_CASE("ExclusiveFileLockProcessProbe", "[.process]")
{
#if !FO_WEB
    auto get_probe_path = [](const string& name) -> optional<string> {
#if FO_WINDOWS
        return winapi::get_environment_variable(name);
#else
        auto value = make_nptr(std::getenv(name.c_str()));
        return value ? optional<string> {value.get()} : std::nullopt;
#endif
    };
    auto path = get_probe_path("FO_LOCK_PROBE_PATH");
    auto ready = get_probe_path("FO_LOCK_PROBE_READY");
    auto release = get_probe_path("FO_LOCK_PROBE_RELEASE");
    REQUIRE(path.has_value());
    REQUIRE(ready.has_value());
    REQUIRE(release.has_value());
    auto lock = fs::try_lock_file(*path);
    REQUIRE(lock.owns_lock());
    REQUIRE(fs::write_file(*ready, "held"));
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds {30};
    while (!fs::exists(*release) && std::chrono::steady_clock::now() < deadline) {
        coarse_sleep(std::chrono::milliseconds {10});
    }
    REQUIRE(fs::exists(*release));
#endif
}

FO_END_NAMESPACE
