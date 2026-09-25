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

#include "catch_amalgamated.hpp"

#include "ResourcePack.h"

FO_BEGIN_NAMESPACE

static auto MakeTempPackPath(string_view name) -> string
{
    auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}.fores", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs::path_to_string(base);
}

static auto MakeBytes(string_view text) -> const_span<uint8_t>
{
    return const_span<uint8_t> {reinterpret_cast<const uint8_t*>(text.data()), text.size()};
}

// Compressible far past the threshold, so the writer is forced down the Deflate path
static auto MakeCompressiblePayload() -> vector<uint8_t>
{
    return vector<uint8_t>(4096, uint8_t {0x41});
}

// Random enough that deflate cannot win, so the writer is forced to store it raw
static auto MakeIncompressiblePayload() -> vector<uint8_t>
{
    vector<uint8_t> payload(4096);
    uint32_t state = 0x12345678;

    for (size_t i = 0; i < payload.size(); ++i) {
        state = state * 1664525u + 1013904223u;
        payload[i] = static_cast<uint8_t>(state >> 24);
    }

    return payload;
}

static auto ReadWholeFile(const ResourcePackSource& pack, string_view path) -> vector<uint8_t>
{
    size_t size = 0;
    uint64_t write_time = 0;
    auto buf = pack.OpenFile(path, size, write_time);
    REQUIRE(buf);
    auto data = const_span<uint8_t> {buf.get(), size};
    return vector<uint8_t> {data.begin(), data.end()};
}

TEST_CASE("ResourcePack")
{
    SECTION("RoundtripsEveryEntry")
    {
        string pack_path = MakeTempPackPath("roundtrip");
        vector<uint8_t> compressible = MakeCompressiblePayload();
        vector<uint8_t> incompressible = MakeIncompressiblePayload();

        {
            ResourcePackWriter writer {pack_path};
            writer.AddFile("Zeta/Last.bin", MakeBytes("last"));
            writer.AddFile("Alpha/First.bin", const_span<uint8_t> {compressible.data(), compressible.size()});
            writer.AddFile("Alpha/Noise.bin", const_span<uint8_t> {incompressible.data(), incompressible.size()});
            writer.AddFile("Empty.bin", const_span<uint8_t> {});
            writer.Finish();
        }

        {
            ResourcePackSource pack {pack_path};

            CHECK(pack.IsFileExists("Alpha/First.bin"));
            CHECK(pack.IsFileExists("Empty.bin"));
            CHECK_FALSE(pack.IsFileExists("Missing.bin"));
            CHECK_FALSE(pack.IsDiskDir());

            size_t size = 0;
            uint64_t write_time = 0;
            REQUIRE(pack.GetFileInfo("Alpha/First.bin", size, write_time));
            CHECK(size == compressible.size());

            CHECK(ReadWholeFile(pack, "Alpha/First.bin") == compressible);
            CHECK(ReadWholeFile(pack, "Alpha/Noise.bin") == incompressible);
            CHECK(ReadWholeFile(pack, "Zeta/Last.bin").size() == 4);
            CHECK(ReadWholeFile(pack, "Empty.bin").empty());

            auto snapshot = pack.GetIndexSnapshot();
            REQUIRE(snapshot.has_value());
            CHECK(snapshot->size() == 4);

            CHECK(pack.GetFileNames("Alpha", true, "").size() == 2);
            CHECK(pack.GetFileNames("", true, "bin").size() == 4);
        }

        // A mounted source holds the file open, so removal only succeeds once it is gone
        CHECK(fs::remove_file(pack_path));
    }

    SECTION("StoresWhatDeflateCannotShrink")
    {
        string compressible_path = MakeTempPackPath("codec_deflate");
        string incompressible_path = MakeTempPackPath("codec_stored");
        vector<uint8_t> compressible = MakeCompressiblePayload();
        vector<uint8_t> incompressible = MakeIncompressiblePayload();

        {
            ResourcePackWriter writer {compressible_path};
            writer.AddFile("Payload.bin", const_span<uint8_t> {compressible.data(), compressible.size()});
            writer.Finish();
        }
        {
            ResourcePackWriter writer {incompressible_path};
            writer.AddFile("Payload.bin", const_span<uint8_t> {incompressible.data(), incompressible.size()});
            writer.Finish();
        }

        // The pack that could be shrunk is far smaller on disk; the one that could not is barely over its input
        auto compressible_size = fs::file_size(compressible_path);
        auto incompressible_size = fs::file_size(incompressible_path);
        REQUIRE(compressible_size.has_value());
        REQUIRE(incompressible_size.has_value());
        CHECK(*compressible_size < compressible.size() / 2);
        CHECK(*incompressible_size >= incompressible.size());
        CHECK(*incompressible_size < incompressible.size() + 512);

        CHECK(fs::remove_file(compressible_path));
        CHECK(fs::remove_file(incompressible_path));
    }

    SECTION("HeaderReadsWithoutTheIndex")
    {
        string pack_path = MakeTempPackPath("header");

        {
            ResourcePackWriter writer {pack_path};
            writer.AddFile("Payload.bin", MakeBytes("payload"));
            writer.Finish();
        }

        ResourcePackHeader header;
        REQUIRE(ReadResourcePackHeader(pack_path, header));
        CHECK(header.VersionMajor == RESOURCE_PACK_VERSION_MAJOR);
        CHECK(header.EntryCount == 1);
        CHECK(header.PackHash != 0);

        {
            ResourcePackSource pack {pack_path};
            CHECK(pack.GetPackHash() == header.PackHash);
        }

        CHECK(fs::remove_file(pack_path));
    }
    SECTION("RejectsMalformedPacks")
    {
        string pack_path = MakeTempPackPath("malformed");

        {
            ResourcePackWriter writer {pack_path};
            writer.AddFile("Payload.bin", MakeBytes("payload"));
            writer.Finish();
        }

        auto original = fs::read_file(pack_path);
        REQUIRE(original.has_value());
        REQUIRE(original->size() > RESOURCE_PACK_HEADER_SIZE);

        string not_a_pack = MakeTempPackPath("not_a_pack");
        REQUIRE(fs::write_file(not_a_pack, string(RESOURCE_PACK_HEADER_SIZE + 16, '\0')));
        CHECK_THROWS_AS(ResourcePackSource {not_a_pack}, VerificationException);

        ResourcePackHeader ignored_header;
        CHECK_FALSE(ReadResourcePackHeader(not_a_pack, ignored_header));

        string too_short = MakeTempPackPath("too_short");
        REQUIRE(fs::write_file(too_short, original->substr(0, RESOURCE_PACK_HEADER_SIZE - 1)));
        CHECK_THROWS_AS(ResourcePackSource {too_short}, VerificationException);

        // One flipped header byte breaks the header checksum, which is what stops a bad offset being believed
        string bad_header = MakeTempPackPath("bad_header");
        string bad_header_data = *original;
        bad_header_data[20] = static_cast<char>(bad_header_data[20] ^ 0xFF);
        REQUIRE(fs::write_file(bad_header, bad_header_data));
        CHECK_THROWS_AS(ResourcePackSource {bad_header}, VerificationException);

        // A truncated body leaves the header intact, so the index read is what has to fail
        string truncated = MakeTempPackPath("truncated");
        REQUIRE(fs::write_file(truncated, original->substr(0, original->size() - 4)));
        CHECK_THROWS_AS(ResourcePackSource {truncated}, VerificationException);

        CHECK(fs::remove_file(pack_path));
        CHECK(fs::remove_file(not_a_pack));
        CHECK(fs::remove_file(too_short));
        CHECK(fs::remove_file(bad_header));
        CHECK(fs::remove_file(truncated));
    }

    SECTION("RoundtripsAPackWithNoEntries")
    {
        string pack_path = MakeTempPackPath("empty_pack");

        {
            ResourcePackWriter writer {pack_path};
            writer.Finish();
        }

        {
            ResourcePackSource pack {pack_path};

            CHECK_FALSE(pack.IsFileExists("Anything.bin"));
            CHECK(pack.GetFileNames("", true, "").empty());

            auto snapshot = pack.GetIndexSnapshot();
            REQUIRE(snapshot.has_value());
            CHECK(snapshot->empty());
        }

        // Packaging refuses to ship an empty pack, but the format still has to describe one rather than leave
        // a reader guessing whether a zero entry count means corruption
        ResourcePackHeader header;
        REQUIRE(ReadResourcePackHeader(pack_path, header));
        CHECK(header.EntryCount == 0);
        CHECK(header.DataSize == 0);
        CHECK(VerifyResourcePackFile(pack_path, header.PackHash));

        CHECK(fs::remove_file(pack_path));
    }

    SECTION("RoundtripsAUnicodePath")
    {
        string pack_path = MakeTempPackPath("unicode");

        // Written as bytes rather than as a literal, so the test asserts the pool holds UTF-8 whatever the
        // compiler decides a source literal means
        static constexpr array<uint8_t, 13> UNICODE_PATH_BYTES = {0xD0, 0x93, 0xD1, 0x80, 0x2F, 0xD0, 0xA1, 0xD0, 0xBF, 0x2E, 0x62, 0x69, 0x6E};
        string unicode_path {reinterpret_cast<const char*>(UNICODE_PATH_BYTES.data()), UNICODE_PATH_BYTES.size()};

        {
            ResourcePackWriter writer {pack_path};
            writer.AddFile(unicode_path, MakeBytes("payload"));
            writer.Finish();
        }

        {
            ResourcePackSource pack {pack_path};

            CHECK(pack.IsFileExists(unicode_path));
            CHECK(ReadWholeFile(pack, unicode_path).size() == 7);

            auto snapshot = pack.GetIndexSnapshot();
            REQUIRE(snapshot.has_value());
            REQUIRE(snapshot->size() == 1);
            CHECK((*snapshot)[0].Path == unicode_path);
        }

        CHECK(fs::remove_file(pack_path));
    }

    SECTION("RejectsExtentsThatCannotFitTheFile")
    {
        string pack_path = MakeTempPackPath("overflow");

        {
            ResourcePackWriter writer {pack_path};
            writer.AddFile("Payload.bin", MakeBytes("payload"));
            writer.Finish();
        }

        auto original = fs::read_file(pack_path);
        REQUIRE(original.has_value());

        // IndexOffset and DataOffset are the two header fields that address the file, and an offset this far
        // out must end the mount rather than reach a read - whether the extent check or the read refuses it
        static constexpr array<uint64_t, 3> HUGE_OFFSETS = {UINT64_MAX, UINT64_MAX - 64, UINT64_C(1) << 62};
        static constexpr array<size_t, 2> OFFSET_FIELDS = {16, 48};

        for (size_t field_offset : OFFSET_FIELDS) {
            for (uint64_t huge : HUGE_OFFSETS) {
                string patched_path = MakeTempPackPath("overflow_case");
                string patched = *original;

                for (size_t i = 0; i < 8; ++i) {
                    patched[field_offset + i] = static_cast<char>((huge >> (i * 8)) & 0xFF);
                }

                // Recomputed so the extent check is what refuses the file rather than the header checksum
                uint64_t checksum = fs::hash_data(const_span<uint8_t> {reinterpret_cast<const uint8_t*>(patched.data()), RESOURCE_PACK_HEADER_SIZE - 8});

                for (size_t i = 0; i < 8; ++i) {
                    patched[RESOURCE_PACK_HEADER_SIZE - 8 + i] = static_cast<char>((checksum >> (i * 8)) & 0xFF);
                }

                REQUIRE(fs::write_file(patched_path, patched));
                CHECK_THROWS_AS(ResourcePackSource {patched_path}, VerificationException);
                CHECK(fs::remove_file(patched_path));
            }
        }

        CHECK(fs::remove_file(pack_path));
    }

    SECTION("MatchesTheGoldenLayout")
    {
        // Produced by the packager from the same inputs with compression off, so both writers share one layout
        // clang-format off
        static constexpr array<uint8_t, 273> GOLDEN_PACK = {
            0x46, 0x4F, 0x52, 0x53, 0x02, 0x00, 0x00, 0x00, 0xD1, 0xD5, 0xB1, 0xAE, 0xFA, 0xE3, 0x44, 0x9B,
            0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
            0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x63, 0x15, 0x3C, 0x0B, 0xE1, 0x91, 0x46, 0x23, 0x23, 0x6A, 0xC3, 0xEA, 0x09, 0xE4, 0x20, 0x72,
            0x61, 0x6C, 0x70, 0x68, 0x61, 0x20, 0x70, 0x61, 0x79, 0x6C, 0x6F, 0x61, 0x64, 0x62, 0x65, 0x74,
            0x61, 0x90, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x60, 0x38, 0xFA, 0xC3, 0xAD, 0xC3,
            0x8B, 0x99, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x5D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x25, 0x23, 0x22, 0x84, 0xE4, 0x9C, 0xF2,
            0xCB, 0xA1, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x5D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA7, 0x20, 0x46, 0x95, 0x9B, 0x61, 0x27,
            0x76, 0x41, 0x6C, 0x70, 0x68, 0x61, 0x2E, 0x62, 0x69, 0x6E, 0x5A, 0x65, 0x74, 0x61, 0x2E, 0x62,
            0x69, 0x6E, 0x6E, 0x65, 0x73, 0x74, 0x65, 0x64, 0x2F, 0x42, 0x65, 0x74, 0x61, 0x2E, 0x62, 0x69,
            0x6E,
        };
        // clang-format on

        string pack_path = MakeTempPackPath("golden");

        {
            ResourcePackWriteSettings settings;
            settings.MinCompressGainPercent = 100;

            ResourcePackWriter writer {pack_path, settings};
            // Added in sorted order, which is what makes the file canonical: the index is sorted either way, but
            // the payloads are laid down as they arrive
            writer.AddFile("Alpha.bin", MakeBytes("alpha payload"));
            writer.AddFile("Zeta.bin", const_span<uint8_t> {});
            writer.AddFile("nested/Beta.bin", MakeBytes("beta"));
            writer.Finish();
        }

        auto written = fs::read_file(pack_path);
        REQUIRE(written.has_value());
        CHECK(written->size() == GOLDEN_PACK.size());
        CHECK(std::memcmp(written->data(), GOLDEN_PACK.data(), GOLDEN_PACK.size()) == 0);

        CHECK(fs::remove_file(pack_path));
    }

    SECTION("VerifiesADownloadedPack")
    {
        string pack_path = MakeTempPackPath("verify");
        vector<uint8_t> compressible = MakeCompressiblePayload();

        {
            ResourcePackWriter writer {pack_path};
            writer.AddFile("Payload.bin", const_span<uint8_t> {compressible.data(), compressible.size()});
            writer.Finish();
        }

        ResourcePackHeader header;
        REQUIRE(ReadResourcePackHeader(pack_path, header));

        CHECK(VerifyResourcePackFile(pack_path, header.PackHash));
        CHECK_FALSE(VerifyResourcePackFile(pack_path, header.PackHash ^ 1));
        CHECK_FALSE(VerifyResourcePackFile(MakeTempPackPath("absent"), header.PackHash));

        // A flipped body byte leaves the header intact, so only the body hash can tell the file is not the one
        // that was published
        auto original = fs::read_file(pack_path);
        REQUIRE(original.has_value());
        string damaged = *original;
        damaged[damaged.size() - 1] = static_cast<char>(damaged[damaged.size() - 1] ^ 0xFF);
        REQUIRE(fs::write_file(pack_path, damaged));

        ResourcePackHeader damaged_header;
        REQUIRE(ReadResourcePackHeader(pack_path, damaged_header));
        CHECK(damaged_header.PackHash == header.PackHash);
        CHECK_FALSE(VerifyResourcePackFile(pack_path, header.PackHash));

        CHECK(fs::remove_file(pack_path));
    }

    SECTION("RefusesADuplicatePath")
    {
        string pack_path = MakeTempPackPath("duplicate");
        ResourcePackWriter writer {pack_path};
        writer.AddFile("Payload.bin", MakeBytes("first"));
        writer.AddFile("Payload.bin", MakeBytes("second"));
        CHECK_THROWS(writer.Finish());
    }
}

static void WritePatchTestPack(string_view path, const vector<pair<string, string>>& files, ResourcePackWriteSettings settings = {0, 100})
{
    ResourcePackWriter writer {path, settings};

    for (const auto& [name, text] : files) {
        writer.AddFile(name, MakeBytes(text));
    }

    writer.Finish();
}

static auto ApplyPatchTestUpdate(string_view base, string_view patch, string_view target_path) -> uint64_t
{
    ResourcePackSource target {target_path};
    ResourcePatchWriter writer {base, patch, target.GetEntryRefs(), target.GetContentHash(), {0, 100}};
    fs::disk_read_file target_file {target_path};
    uint64_t downloaded = 0;
    fs::disk_directory_lock patch_lock {strex(patch).extract_dir().str()};
    writer.Begin(patch_lock);

    // Payloads an interrupted append already wrote are not fetched again
    for (size_t i = writer.GetResumedDownloads(); i < writer.GetDownloads().size(); ++i) {
        const ResourcePackEntryRef& entry = writer.GetDownloads()[i];
        vector<uint8_t> data(numeric_cast<size_t>(entry.StoredSize));
        REQUIRE(target_file.read_at(entry.DataOffset, data));
        writer.AddEncodedFile(data);
        downloaded += entry.StoredSize;
    }

    writer.Finish();
    CHECK(fs::file_size(patch).value() == writer.GetFinalSize());
    return downloaded;
}

TEST_CASE("ResourcePackPatch")
{
    string base = MakeTempPackPath("patch_base");
    string patch = GetResourcePatchPath(base);
    string target = MakeTempPackPath("patch_target");
    string recovery = MakeTempPackPath("patch_recovery");
    auto cleanup = scope_exit([&]() noexcept {
        (void)fs::remove_file(base);
        (void)fs::remove_file(patch);
        (void)fs::remove_file(target);
        (void)fs::remove_file(recovery);
    });
    WritePatchTestPack(base, {{"A.txt", "same"}, {"B.txt", "old"}, {"C.txt", "deleted"}});
    auto original_base = fs::read_file(base);
    REQUIRE(original_base);
    WritePatchTestPack(target, {{"A.txt", "same"}, {"B.txt", "changed"}, {"D.txt", "added"}});
    CHECK(ApplyPatchTestUpdate(base, patch, target) == 12);
    auto first_patch = fs::read_file(patch);
    REQUIRE(first_patch);

    SECTION("CompleteCatalogChoosesBothFilesAndDeletesBaseEntries")
    {
        ResourcePackSource view {base, patch};
        ResourcePackSource wanted {target};
        CHECK(view.GetContentHash() == wanted.GetContentHash());
        CHECK(ReadWholeFile(view, "A.txt") == vector<uint8_t> {'s', 'a', 'm', 'e'});
        CHECK(ReadWholeFile(view, "B.txt") == vector<uint8_t> {'c', 'h', 'a', 'n', 'g', 'e', 'd'});
        CHECK_FALSE(view.IsFileExists("C.txt"));
        auto entries = view.GetEntryRefs();
        REQUIRE(entries.size() == 3);
        CHECK(entries[0].Source == 0);
        CHECK(entries[1].Source == 1);
        CHECK(entries[2].Source == 1);
    }

    SECTION("SecondUpdateReusesPatchBytesAndRenamesWithoutDownload")
    {
        WritePatchTestPack(target, {{"A.txt", "same"}, {"B.txt", "changed"}, {"E.txt", "added"}});
        CHECK(ApplyPatchTestUpdate(base, patch, target) == 0);
        auto updated = fs::read_file(patch);
        REQUIRE(updated);
        CHECK(updated->starts_with(*first_patch));
        ResourcePackSource view {base, patch};
        CHECK_FALSE(view.IsFileExists("D.txt"));
        CHECK(ReadWholeFile(view, "E.txt") == vector<uint8_t> {'a', 'd', 'd', 'e', 'd'});
    }

    SECTION("EmptyCatalogDeletesEveryResourceWithoutDownloadingPayloads")
    {
        WritePatchTestPack(target, {});
        CHECK(ApplyPatchTestUpdate(base, patch, target) == 0);
        ResourcePackSource view {base, patch};
        CHECK(view.GetEntryRefs().empty());
        CHECK_FALSE(view.IsFileExists("A.txt"));
        CHECK_FALSE(view.IsFileExists("B.txt"));
        CHECK(view.GetContentHash() == ResourcePackSource(target).GetContentHash());
    }

    SECTION("CorruptedNewestCatalogDoesNotHideThePreviousCommit")
    {
        WritePatchTestPack(target, {{"A.txt", "same"}, {"B.txt", "newer"}});
        CHECK(ApplyPatchTestUpdate(base, patch, target) == 5);
        ResourcePackHeader header;
        REQUIRE(ReadResourcePackHeader(base, header));
        auto info = ReadResourcePatchInfo(patch, header);
        REQUIRE(info);
        auto bytes = fs::read_file(patch);
        REQUIRE(bytes);
        (*bytes)[numeric_cast<size_t>(info->IndexOffset)] ^= 1;
        REQUIRE(fs::write_file(patch, *bytes));
        ResourcePackSource view {base, patch};
        CHECK(ReadWholeFile(view, "B.txt") == vector<uint8_t> {'c', 'h', 'a', 'n', 'g', 'e', 'd'});
        REQUIRE(view.GetPatchInfo());
        CHECK(view.GetPatchInfo()->CommittedSize == first_patch->size());
    }

    SECTION("EveryTruncatedSecondAppendKeepsFirstCommit")
    {
        WritePatchTestPack(target, {{"A.txt", "same"}, {"B.txt", "newer"}});
        CHECK(ApplyPatchTestUpdate(base, patch, target) == 5);
        auto complete = fs::read_file(patch);
        REQUIRE(complete);

        for (size_t cut = first_patch->size(); cut < complete->size(); ++cut) {
            REQUIRE(fs::write_file(recovery, string_view {complete->data(), cut}));
            ResourcePackSource view {base, recovery};
            CHECK(ReadWholeFile(view, "B.txt") == vector<uint8_t> {'c', 'h', 'a', 'n', 'g', 'e', 'd'});
            CHECK(view.IsFileExists("D.txt"));
            REQUIRE(view.GetPatchInfo());
            CHECK(view.GetPatchInfo()->CommittedSize == first_patch->size());
        }
    }

    SECTION("RecoveryTrimsUncommittedTailBeforeNextAppend")
    {
        {
            fs::disk_write_file append {patch, fs::disk_write_mode::append};
            REQUIRE(append);
            vector<uint8_t> garbage(128 * 1024, uint8_t {'x'});
            REQUIRE(append.write(garbage));
        }

        WritePatchTestPack(target, {{"A.txt", "same"}, {"B.txt", "next"}});
        CHECK(ApplyPatchTestUpdate(base, patch, target) == 4);
        auto updated = fs::read_file(patch);
        REQUIRE(updated);
        CHECK(updated->starts_with(*first_patch));
        CHECK(updated->size() < first_patch->size() + 1024);
    }

    SECTION("TornPatchHeaderLeavesTheBaseMountedAndIsRecreated")
    {
        // A power loss can leave the header's bytes unwritten while the file length survives, which reads back
        // as zeros; the client must still start and the next update must rebuild the patch
        string torn = *first_patch;
        std::fill_n(torn.begin(), RESOURCE_PATCH_HEADER_SIZE, '\0');
        REQUIRE(fs::write_file(patch, torn));

        {
            ResourcePackSource view {base, patch};
            CHECK_FALSE(view.GetPatchInfo().has_value());
            CHECK(ReadWholeFile(view, "B.txt") == vector<uint8_t> {'o', 'l', 'd'});
            ResourcePackHeader header;
            REQUIRE(ReadResourcePackHeader(base, header));
            CHECK_FALSE(ReadResourcePatchInfo(patch, header).has_value());
        }

        CHECK(ApplyPatchTestUpdate(base, patch, target) == 12);
        ResourcePackSource repaired {base, patch};
        CHECK(repaired.GetContentHash() == ResourcePackSource(target).GetContentHash());
        CHECK(fs::read_file(patch) == first_patch);
    }

    SECTION("BadDownloadedContentCannotPublishOrDamagePreviousCommit")
    {
        WritePatchTestPack(target, {{"A.txt", "same"}, {"B.txt", "new"}});
        ResourcePackSource wanted {target};
        ResourcePatchWriter writer {base, patch, wanted.GetEntryRefs(), wanted.GetContentHash()};
        fs::disk_directory_lock patch_lock {strex(patch).extract_dir().str()};
        writer.Begin(patch_lock);
        REQUIRE_THROWS(writer.AddEncodedFile(MakeBytes("bad")));
        REQUIRE_THROWS(writer.Finish());
        ResourcePackSource view {base, patch};
        CHECK(ReadWholeFile(view, "B.txt") == vector<uint8_t> {'c', 'h', 'a', 'n', 'g', 'e', 'd'});
    }

    SECTION("WritersCannotTruncateEachOthersPatch")
    {
        ResourcePackSource wanted {target};
        ResourcePatchWriter first {base, patch, wanted.GetEntryRefs(), wanted.GetContentHash()};
        ResourcePatchWriter second {base, patch, wanted.GetEntryRefs(), wanted.GetContentHash()};
        fs::disk_directory_lock first_lock {strex(patch).extract_dir().str()};
        first.Begin(first_lock);
#if !FO_WEB
        // Another updater cannot take the directory, and one that proceeds anyway still finds the file held
        fs::disk_directory_lock second_lock {strex(patch).extract_dir().str()};
        CHECK_THROWS(second.Begin(second_lock));
#endif
        CHECK(fs::read_file(patch) == first_patch);
    }

    SECTION("RecompressedTargetReusesDecodedBaseContent")
    {
        string text(4096, 'A');
        WritePatchTestPack(base, {{"Long.txt", text}});
        original_base = fs::read_file(base);
        REQUIRE(fs::remove_file(patch));
        WritePatchTestPack(target, {{"Long.txt", text}, {"Renamed.txt", text}}, {6, 5});
        CHECK(ApplyPatchTestUpdate(base, patch, target) == 0);
        ResourcePackSource view {base, patch};
        CHECK(ReadWholeFile(view, "Renamed.txt").size() == text.size());
    }

    SECTION("ReaderKeepsItsCommittedViewWhileWriterAppends")
    {
        ResourcePackSource old_view {base, patch};
        WritePatchTestPack(target, {{"A.txt", "same"}, {"B.txt", "newer"}});
        CHECK(ApplyPatchTestUpdate(base, patch, target) == 5);
        CHECK(ReadWholeFile(old_view, "B.txt") == vector<uint8_t> {'c', 'h', 'a', 'n', 'g', 'e', 'd'});
        CHECK(old_view.IsFileExists("D.txt"));
        ResourcePackSource new_view {base, patch};
        CHECK(ReadWholeFile(new_view, "B.txt") == vector<uint8_t> {'n', 'e', 'w', 'e', 'r'});
        CHECK_FALSE(new_view.IsFileExists("D.txt"));
    }

    SECTION("CorruptReusablePayloadIsDownloadedAgain")
    {
        string damaged = *first_patch;
        damaged[RESOURCE_PATCH_HEADER_SIZE] ^= 1;
        REQUIRE(fs::write_file(patch, damaged));
        WritePatchTestPack(target, {{"A.txt", "same"}, {"B.txt", "changed"}, {"Renamed.txt", "added"}});
        CHECK(ApplyPatchTestUpdate(base, patch, target) == 7);
        ResourcePackSource view {base, patch};
        CHECK(ReadWholeFile(view, "B.txt") == vector<uint8_t> {'c', 'h', 'a', 'n', 'g', 'e', 'd'});
    }

#if !FO_WINDOWS && !FO_WEB
    SECTION("StalePatchResetPreservesPinnedOldInodes")
    {
        ResourcePackSource old_view {base, patch};
        fs::disk_read_file pinned_base {base};
        fs::disk_read_file pinned_patch {patch};
        ResourcePackHeader before;
        REQUIRE(ReadResourcePackHeader(pinned_base, before));
        REQUIRE(fs::rename(base, recovery));
        WritePatchTestPack(base, {{"A.txt", "fresh base"}});
        original_base = fs::read_file(base);
        WritePatchTestPack(target, {{"A.txt", "fresh base"}, {"New.txt", "fresh patch"}});
        CHECK(ApplyPatchTestUpdate(base, patch, target) == 11);
        CHECK(ReadWholeFile(old_view, "A.txt") == vector<uint8_t> {'s', 'a', 'm', 'e'});
        CHECK(ReadWholeFile(old_view, "B.txt") == vector<uint8_t> {'c', 'h', 'a', 'n', 'g', 'e', 'd'});
        ResourcePackHeader still_pinned;
        REQUIRE(ReadResourcePackHeader(pinned_base, still_pinned));
        CHECK(still_pinned.PackHash == before.PackHash);
        auto pinned_commit = ReadResourcePatchInfo(pinned_patch, still_pinned);
        REQUIRE(pinned_commit);
        CHECK(pinned_commit->ContentHash == old_view.GetContentHash());
        ResourcePackSource new_view {base, patch};
        CHECK_FALSE(new_view.IsFileExists("B.txt"));
        CHECK(new_view.IsFileExists("New.txt"));
    }
#endif

    SECTION("InterruptedAppendResumesFromThePayloadsItWrote")
    {
        WritePatchTestPack(target, {{"A.txt", "same"}, {"B.txt", "resumed"}, {"E.txt", "second payload"}, {"F.txt", "third payload"}});
        ResourcePackSource wanted {target};
        fs::disk_read_file target_file {target};
        auto fetch = [&target_file](const ResourcePackEntryRef& entry) {
            vector<uint8_t> data(numeric_cast<size_t>(entry.StoredSize));
            REQUIRE(target_file.read_at(entry.DataOffset, data));
            return data;
        };

        {
            ResourcePatchWriter interrupted {base, patch, wanted.GetEntryRefs(), wanted.GetContentHash()};
            REQUIRE(interrupted.GetDownloads().size() == 3);
            CHECK(interrupted.GetResumedDownloads() == 0);
            fs::disk_directory_lock lock {strex(patch).extract_dir().str()};
            interrupted.Begin(lock);
            interrupted.AddEncodedFile(fetch(interrupted.GetDownloads()[0]));
            interrupted.AddEncodedFile(fetch(interrupted.GetDownloads()[1]));
        }

        // The unfinished append commits nothing, so the previous view stays in force meanwhile
        {
            ResourcePackSource view {base, patch};
            CHECK(ReadWholeFile(view, "B.txt") == vector<uint8_t> {'c', 'h', 'a', 'n', 'g', 'e', 'd'});
        }

        ResourcePatchWriter resumed {base, patch, wanted.GetEntryRefs(), wanted.GetContentHash()};
        REQUIRE(resumed.GetResumedDownloads() == 2);
        uint64_t written = resumed.GetDownloads()[0].StoredSize + resumed.GetDownloads()[1].StoredSize;
        CHECK(resumed.GetAppendSize() == resumed.GetFinalSize() - first_patch->size() - written);
        fs::disk_directory_lock lock {strex(patch).extract_dir().str()};
        resumed.Begin(lock);
        resumed.AddEncodedFile(fetch(resumed.GetDownloads()[2]));
        resumed.Finish();

        CHECK(fs::file_size(patch).value() == resumed.GetFinalSize());
        ResourcePackSource view {base, patch};
        CHECK(view.GetContentHash() == wanted.GetContentHash());
        CHECK(ReadWholeFile(view, "E.txt").size() == string_view {"second payload"}.size());
        CHECK(ReadWholeFile(view, "F.txt").size() == string_view {"third payload"}.size());
    }

    SECTION("TornPayloadEndsTheResumedRun")
    {
        WritePatchTestPack(target, {{"A.txt", "same"}, {"B.txt", "resumed"}, {"E.txt", "second payload"}});
        ResourcePackSource wanted {target};
        fs::disk_read_file target_file {target};

        {
            ResourcePatchWriter interrupted {base, patch, wanted.GetEntryRefs(), wanted.GetContentHash()};
            fs::disk_directory_lock lock {strex(patch).extract_dir().str()};
            interrupted.Begin(lock);

            for (const ResourcePackEntryRef& entry : interrupted.GetDownloads()) {
                vector<uint8_t> data(numeric_cast<size_t>(entry.StoredSize));
                REQUIRE(target_file.read_at(entry.DataOffset, data));
                interrupted.AddEncodedFile(data);
            }
        }

        // A power loss mid-write leaves the last payload short
        uint64_t tail_size = fs::file_size(patch).value();
        {
            fs::disk_write_file cut {patch, fs::disk_write_mode::append};
            REQUIRE(cut.truncate_to(tail_size - 3));
        }

        ResourcePatchWriter resumed {base, patch, wanted.GetEntryRefs(), wanted.GetContentHash()};
        CHECK(resumed.GetResumedDownloads() == 1);
        CHECK(ApplyPatchTestUpdate(base, patch, target) == string_view {"second payload"}.size());
        CHECK(ResourcePackSource(base, patch).GetContentHash() == wanted.GetContentHash());
    }

    SECTION("InterruptedFirstAppendKeepsItsHeaderAndPayloads")
    {
        REQUIRE(fs::remove_file(patch));
        ResourcePackSource wanted {target};
        fs::disk_read_file target_file {target};

        {
            ResourcePatchWriter interrupted {base, patch, wanted.GetEntryRefs(), wanted.GetContentHash()};
            fs::disk_directory_lock lock {strex(patch).extract_dir().str()};
            interrupted.Begin(lock);
            const ResourcePackEntryRef& entry = interrupted.GetDownloads().front();
            vector<uint8_t> data(numeric_cast<size_t>(entry.StoredSize));
            REQUIRE(target_file.read_at(entry.DataOffset, data));
            interrupted.AddEncodedFile(data);
        }

        CHECK_FALSE(ResourcePackSource(base, patch).GetPatchInfo().has_value());
        ResourcePatchWriter resumed {base, patch, wanted.GetEntryRefs(), wanted.GetContentHash()};
        CHECK(resumed.GetResumedDownloads() == 1);
        CHECK(ApplyPatchTestUpdate(base, patch, target) == string_view {"added"}.size());
        CHECK(fs::read_file(patch) == first_patch);
    }

    SECTION("VerifierProvesAnIntactPairInBoundedSteps")
    {
        ResourcePairVerifier verifier {base, patch, true, true};
        CHECK(verifier.GetTotalBytes() == original_base->size() - RESOURCE_PACK_HEADER_SIZE + 12);
        size_t steps = 0;

        // A slice of the base, then the payloads one by one: a step reads past its budget only to finish what it began
        while (!verifier.IsFinished()) {
            verifier.Step(1);
            steps++;
        }

        CHECK(steps >= 3);
        CHECK(verifier.IsBaseIntact());
        CHECK(verifier.IsPatchIntact());
        CHECK(verifier.GetCheckedBytes() == verifier.GetTotalBytes());
    }

    SECTION("VerifierFindsADamagedBasePayloadTheCatalogStillNames")
    {
        string damaged = *original_base;
        damaged[RESOURCE_PACK_HEADER_SIZE] = static_cast<char>(damaged[RESOURCE_PACK_HEADER_SIZE] ^ 0x01);
        REQUIRE(fs::write_file(base, damaged));
        original_base = damaged;
        CHECK(ResourcePackSource(base, patch).GetContentHash() == ResourcePackSource(target).GetContentHash());

        ResourcePairVerifier verifier {base, patch, true, true};

        while (!verifier.IsFinished()) {
            verifier.Step(1024);
        }

        CHECK_FALSE(verifier.IsBaseIntact());
    }

    SECTION("VerifierFindsADamagedPatchPayload")
    {
        string damaged = *first_patch;
        damaged[RESOURCE_PATCH_HEADER_SIZE] = static_cast<char>(damaged[RESOURCE_PATCH_HEADER_SIZE] ^ 0x01);
        REQUIRE(fs::write_file(patch, damaged));

        ResourcePairVerifier verifier {base, patch, false, true};
        CHECK(verifier.GetTotalBytes() == 12);

        while (!verifier.IsFinished()) {
            verifier.Step(1024);
        }

        CHECK(verifier.IsBaseIntact());
        CHECK_FALSE(verifier.IsPatchIntact());
    }

    SECTION("VerifierCallsAnUnreadableBaseDamaged")
    {
        string torn = *original_base;
        std::fill_n(torn.begin(), RESOURCE_PACK_HEADER_SIZE, '\0');
        REQUIRE(fs::write_file(base, torn));
        original_base = torn;

        ResourcePairVerifier verifier {base, patch, false, false};
        CHECK(verifier.IsFinished());
        CHECK_FALSE(verifier.IsBaseIntact());
    }

    SECTION("ReplacementBaseExcludesLeftoverPatch")
    {
        WritePatchTestPack(base, {{"A.txt", "full reset"}});
        original_base = fs::read_file(base);
        ResourcePackSource view {base, patch};
        CHECK_FALSE(view.GetPatchInfo());
        CHECK_FALSE(view.IsFileExists("B.txt"));
        CHECK(ReadWholeFile(view, "A.txt") == vector<uint8_t> {'f', 'u', 'l', 'l', ' ', 'r', 'e', 's', 'e', 't'});
    }

    SECTION("IdenticalLogicalBaseWithNewOffsetsExcludesTheOldPatch")
    {
        ResourcePackHeader before;
        REQUIRE(ReadResourcePackHeader(base, before));
        WritePatchTestPack(base, {{"C.txt", "deleted"}, {"B.txt", "old"}, {"A.txt", "same"}});
        original_base = fs::read_file(base);
        ResourcePackSource view {base, patch};
        CHECK(view.GetContentHash() == before.ContentHash);
        CHECK(view.GetPackHash() != before.PackHash);
        CHECK_FALSE(view.GetPatchInfo());
        CHECK(ReadWholeFile(view, "B.txt") == vector<uint8_t> {'o', 'l', 'd'});
    }

    CHECK(fs::read_file(base) == original_base);
}

FO_END_NAMESPACE
