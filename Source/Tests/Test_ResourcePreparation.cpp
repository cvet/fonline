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

#include "Application.h"
#include "Client.h"
#include "DefaultSprites.h"
#include "MapLoader.h"
#include "MapView.h"
#include "ResourcePreparation.h"
#include "Test_BakerHelpers.h"
#include "Test_ClientEngineHelpers.h"

FO_BEGIN_NAMESPACE

static auto MakePreparationInput(string path, ucolor pixel = {17, 29, 43, 255}, uint64_t generation = 1) -> ResourcePreparation::Input
{
    ResourcePreparation::Input input;
    input.Identity = {std::move(path), ResourcePreparation::Kind::Sprite, 1, generation, 1};
    data_writer writer {input.Bytes};
    writer.write<uint8_t>(SPRITE_RESOURCE_MAGIC);
    writer.write<uint8_t>(SPRITE_RESOURCE_VERSION);
    writer.write<uint16_t>(uint16_t {1});
    writer.write<uint16_t>(uint16_t {80});
    writer.write<uint8_t>(uint8_t {1});
    writer.write<uint8_t>(uint8_t {0});
    writer.write<int16_t>(int16_t {3});
    writer.write<int16_t>(int16_t {-2});
    writer.write<uint16_t>(uint16_t {1});
    writer.write<uint16_t>(uint16_t {1});
    writer.write<int16_t>(int16_t {0});
    writer.write<int16_t>(int16_t {0});
    writer.write<ucolor>(pixel);
    writer.write<uint8_t>(static_cast<uint8_t>(SpriteMeshKind::Quad));
    writer.write<uint8_t>(SPRITE_RESOURCE_MAGIC);
    return input;
}

class PreparationDecodeGate final
{
public:
    PreparationDecodeGate() :
        _enteredFuture {_entered.get_future()},
        _releaseFuture {_release.get_future().share()}
    {
    }

    [[nodiscard]] auto WaitEntered() -> bool { return _enteredFuture.wait_for(std::chrono::seconds {10}) == std::future_status::ready; }

    [[nodiscard]] auto GetCalls() const -> size_t { return _calls.load(); }

    void Enter()
    {
        if (_calls.fetch_add(1) == 0) {
            _entered.set_value();
        }

        if (_releaseFuture.wait_for(std::chrono::seconds {10}) != std::future_status::ready) {
            throw WorkSchedulerException("Preparation test release deadline expired");
        }
    }

    void Open() noexcept
    {
        if (!_opened.exchange(true)) {
            _release.set_value();
        }
    }

private:
    std::promise<void> _entered {};
    std::promise<void> _release {};
    std::future<void> _enteredFuture;
    std::shared_future<void> _releaseFuture;
    std::atomic<size_t> _calls {};
    std::atomic<bool> _opened {};
};

TEST_CASE("ResourcePreparationAdmissionCoalescesAndRetainsCompletedReservations", "[resource-preparation]")
{
    if constexpr (FO_WEB) {
        SKIP("This build has no worker threads");
    }

    WorkScheduler scheduler {"preparation", 1};
    auto gate = safe_alloc::make_shared<PreparationDecodeGate>();
    ResourcePreparation::Limits limits;
    limits.Requests = 2;
    ResourcePreparation lane {&scheduler, limits, [gate](const_span<uint8_t> bytes) mutable {
                                  gate->Enter();
                                  return ReadSpriteResource(bytes);
                              }};
    auto unblock = scope_exit([&]() noexcept { gate->Open(); });
    auto input = MakePreparationInput("first.png");
    auto key = input.Identity;
    auto first = lane.Request(std::move(input));
    REQUIRE(first.Status == ResourcePreparation::Admission::Accepted);
    REQUIRE(gate->WaitEntered());
    CHECK(lane.Poll(first.Ticket) == ResourcePreparation::Completion::Pending);
    CHECK(lane.FindRequest(key) == first.Ticket);
    input.Bytes.assign(100, uint8_t {0});

    auto duplicate = lane.Request(MakePreparationInput("first.png"));
    CHECK(duplicate.Status == ResourcePreparation::Admission::Coalesced);
    CHECK(duplicate.Ticket == first.Ticket);
    auto second = lane.Request(MakePreparationInput("second.png"));
    REQUIRE(second.Status == ResourcePreparation::Admission::Accepted);
    CHECK(lane.Request(MakePreparationInput("full.png")).Status == ResourcePreparation::Admission::Full);
    size_t reserved = lane.GetDiagnostics().ReservedBytes;
    CHECK(reserved > 0);

    gate->Open();
    lane.Drain();
    CHECK(gate->GetCalls() == 2);
    CHECK(lane.GetDiagnostics().Outstanding == 2);
    CHECK(lane.GetDiagnostics().ReservedBytes == reserved);
    CHECK(lane.Request(MakePreparationInput("still-full.png")).Status == ResourcePreparation::Admission::Full);
    bool published = false;
    CHECK(lane.Consume(first.Ticket, [&](SpriteResourceData&& data) {
        published = true;
        CHECK(lane.GetDiagnostics().ReservedBytes == reserved);
        REQUIRE(data.Directions.size() == 1);
        REQUIRE(data.Directions[0].Frames.size() == 1);
        const auto& frame = data.Directions[0].Frames[0];
        CHECK(frame.Offset == ipos32 {3, -2});
        REQUIRE(frame.Pixels.size() == 1);
        CHECK(frame.Pixels[0] == ucolor {17, 29, 43, 255});
    }));
    CHECK(published);
    CHECK(lane.Poll(first.Ticket) == ResourcePreparation::Completion::Stale);
    CHECK(lane.GetDiagnostics().ReservedBytes < reserved);
    CHECK(lane.Request(MakePreparationInput("replacement.png")).Status == ResourcePreparation::Admission::Accepted);
}

TEST_CASE("ResourcePreparationCancelledGenerationCannotReleaseActiveReservation", "[resource-preparation]")
{
    if constexpr (FO_WEB) {
        SKIP("This build has no worker threads");
    }

    WorkScheduler scheduler {"preparation-cancel", 1};
    auto gate = safe_alloc::make_shared<PreparationDecodeGate>();
    ResourcePreparation::Limits limits;
    limits.Requests = 2;
    ResourcePreparation lane {&scheduler, limits, [gate](const_span<uint8_t> bytes) mutable {
                                  gate->Enter();
                                  return ReadSpriteResource(bytes);
                              }};
    auto unblock = scope_exit([&]() noexcept { gate->Open(); });
    auto active = lane.Request(MakePreparationInput("same.png"));
    REQUIRE(active.Status == ResourcePreparation::Admission::Accepted);
    REQUIRE(gate->WaitEntered());
    size_t active_reservation = lane.GetDiagnostics().ReservedBytes;
    auto queued = lane.Request(MakePreparationInput("queued.png"));
    REQUIRE(queued.Status == ResourcePreparation::Admission::Accepted);

    lane.CancelGeneration(1);
    CHECK(lane.GetDiagnostics().ReservedBytes == active_reservation);
    CHECK(lane.GetDiagnostics().Outstanding == 1);
    CHECK(lane.Poll(active.Ticket) == ResourcePreparation::Completion::Cancelled);
    CHECK(gate->GetCalls() == 1);
    CHECK(lane.Request(MakePreparationInput("same.png")).Status == ResourcePreparation::Admission::Stale);
    auto current = lane.Request(MakePreparationInput("same.png", {201, 19, 7, 255}, 2));
    REQUIRE(current.Status == ResourcePreparation::Admission::Accepted);
    CHECK(current.Ticket != active.Ticket);
    CHECK(lane.Request(MakePreparationInput("full.png", {}, 2)).Status == ResourcePreparation::Admission::Full);

    gate->Open();
    lane.Drain();
    CHECK(gate->GetCalls() == 2);
    CHECK(lane.Poll(active.Ticket) == ResourcePreparation::Completion::Stale);
    bool published = false;
    CHECK(lane.Consume(current.Ticket, [&](SpriteResourceData&& data) {
        published = true;
        REQUIRE(data.Directions[0].Frames[0].Pixels.size() == 1);
        CHECK(data.Directions[0].Frames[0].Pixels[0] == ucolor {201, 19, 7, 255});
    }));
    CHECK(published);
    CHECK(lane.GetDiagnostics().Outstanding == 0);
    CHECK(lane.GetDiagnostics().ReservedBytes == 0);
}

TEST_CASE("ResourcePreparationDistinguishesMalformedMissingAndBudgetDecline", "[resource-preparation]")
{
    if constexpr (FO_WEB) {
        SKIP("This build has no worker threads");
    }

    WorkScheduler scheduler {"preparation-errors", 1};
    ResourcePreparation lane {&scheduler};
    int32_t malformed_kind = GENERATE(0, 1, 2, 3);
    auto malformed = MakePreparationInput("bad.png");
    string_view expected_diagnostic;

    switch (malformed_kind) {
    case 0:
        malformed.Bytes[0] = 0;
        expected_diagnostic = "Sprite resource header magic is invalid";
        break;
    case 1:
        malformed.Bytes.resize(22);
        expected_diagnostic = "Sprite frame pixel data is truncated";
        break;
    case 2:
        malformed.Bytes[7] = 1;
        malformed.Bytes[8] = malformed.Bytes[9] = 0;
        expected_diagnostic = "Sprite frame reference points outside previously decoded frames";
        break;
    case 3: {
        SpriteMeshData mesh {{3, 3}, {1, 1}, {{0, 0}, {1, 0}, {0, 1}}, {0, 1, 3}};
        malformed.Bytes = BakerTests::MakeMinimalBakedSprite(1, 1, SpriteMeshKind::Mesh, mesh);
        expected_diagnostic = "Sprite mesh index is outside the vertex array";
        break;
    }
    default:
        FAIL("Unexpected malformed resource variant");
    }

    auto failed = lane.Request(std::move(malformed));
    REQUIRE(failed.Status == ResourcePreparation::Admission::Accepted);
    lane.Drain();
    CHECK(lane.Poll(failed.Ticket) == ResourcePreparation::Completion::Failed);
    CHECK_THROWS_WITH(lane.Consume(failed.Ticket, [](SpriteResourceData&&) { FAIL("Malformed input must not publish"); }), Catch::Matchers::ContainsSubstring(std::string {expected_diagnostic}));
    CHECK(lane.GetDiagnostics().Outstanding == 0);

    auto expanded = MakePreparationInput("expanded.png");
    expanded.Bytes[2] = expanded.Bytes[3] = expanded.Bytes[6] = 255;
    CHECK(lane.Request(std::move(expanded)).Status == ResourcePreparation::Admission::BudgetDeclined);
    auto missing = MakePreparationInput("missing.png");
    missing.Bytes.clear();
    missing.Missing = true;
    auto absent = lane.Request(std::move(missing));
    REQUIRE(absent.Status == ResourcePreparation::Admission::Accepted);
    CHECK(lane.Poll(absent.Ticket) == ResourcePreparation::Completion::Missing);
    CHECK(lane.Consume(absent.Ticket, [](SpriteResourceData&&) { FAIL("Missing input must not publish"); }));
    auto valid = lane.Request(MakePreparationInput("valid.png"));
    REQUIRE(valid.Status == ResourcePreparation::Admission::Accepted);
    lane.Drain();
    CHECK(lane.Poll(valid.Ticket) == ResourcePreparation::Completion::Ready);
    lane.Shutdown();
    CHECK(lane.GetDiagnostics().Outstanding == 0);
    CHECK(lane.GetDiagnostics().ReservedBytes == 0);
    CHECK(lane.Request(MakePreparationInput("stopped.png")).Status == ResourcePreparation::Admission::Stopped);
    CHECK_NOTHROW(lane.Shutdown());
}

TEST_CASE("ResourcePreparationShutdownDrainsOwnedDecoderBeforeItsDependencies", "[resource-preparation]")
{
    if constexpr (FO_WEB) {
        SKIP("This build has no worker threads");
    }

    bool explicit_shutdown = GENERATE(false, true);
    WorkScheduler scheduler {"preparation-shutdown", 1};
    auto gate = safe_alloc::make_shared<PreparationDecodeGate>();
    auto exited = safe_alloc::make_shared<std::atomic<bool>>();
    unique_nptr<ResourcePreparation> lane = safe_alloc::make_unique<ResourcePreparation>(&scheduler, ResourcePreparation::Limits {}, [gate, exited](const_span<uint8_t> bytes) mutable {
        gate->Enter();
        auto result = ReadSpriteResource(bytes);
        exited->store(true);
        return result;
    });
    auto unblock = scope_exit([&]() noexcept { gate->Open(); });
    auto task = lane->Request(MakePreparationInput("shutdown.png"));
    REQUIRE(task.Status == ResourcePreparation::Admission::Accepted);
    REQUIRE(gate->WaitEntered());
    auto release = run_thread("PreparationRelease", [gate]() mutable { gate->Open(); });
    auto join = scope_exit([&]() noexcept { release.join(); });

    if (explicit_shutdown) {
        lane->Shutdown();
        CHECK(exited->load());
        CHECK(lane->GetDiagnostics().Outstanding == 0);
        CHECK(lane->GetDiagnostics().ReservedBytes == 0);
        CHECK(lane->Request(MakePreparationInput("after-shutdown.png")).Status == ResourcePreparation::Admission::Stopped);
        CHECK_NOTHROW(lane->Shutdown());
    }
    else {
        lane.reset();
        CHECK(exited->load());
    }
}

TEST_CASE("ResourcePreparationPublicationUnwindKeepsReservationUntilCallbackExit", "[resource-preparation]")
{
    if constexpr (FO_WEB) {
        SKIP("This build has no worker threads");
    }

    WorkScheduler scheduler {"preparation-publication", 1};
    ResourcePreparation lane {&scheduler};
    auto first = lane.Request(MakePreparationInput("publish.png"));
    REQUIRE(first.Status == ResourcePreparation::Admission::Accepted);
    size_t first_reservation = lane.GetDiagnostics().ReservedBytes;
    REQUIRE(lane.Request(MakePreparationInput("discard.png")).Status == ResourcePreparation::Admission::Accepted);
    lane.Drain();

    CHECK_THROWS_AS(lane.Consume(first.Ticket,
                        [&](SpriteResourceData&& resource) {
                            lane.CancelAll();
                            CHECK(lane.GetDiagnostics().Outstanding == 1);
                            CHECK(lane.GetDiagnostics().ReservedBytes == first_reservation);
                            CHECK(resource.Directions[0].Frames[0].Pixels[0] == ucolor {17, 29, 43, 255});
                            throw GenericException("Expected publication failure");
                        }),
        GenericException);
    CHECK(lane.GetDiagnostics().Outstanding == 0);
    CHECK(lane.GetDiagnostics().ReservedBytes == 0);
    CHECK(lane.GetDiagnostics().Consumed == 0);
    CHECK(lane.Request(MakePreparationInput("recovery.png")).Status == ResourcePreparation::Admission::Accepted);
}

TEST_CASE("ResourcePreparationCompletionOrderDoesNotPublishOnWorkers", "[resource-preparation]")
{
    if constexpr (FO_WEB) {
        SKIP("This build has no worker threads");
    }

    WorkScheduler scheduler {"preparation-order", 2};
    auto first_gate = safe_alloc::make_shared<PreparationDecodeGate>();
    ResourcePreparation lane {&scheduler, ResourcePreparation::Limits {}, [first_gate](const_span<uint8_t> bytes) mutable {
                                  auto result = ReadSpriteResource(bytes);

                                  if (result.Directions[0].Frames[0].Pixels[0] == ucolor {17, 29, 43, 255}) {
                                      first_gate->Enter();
                                  }

                                  return result;
                              }};
    auto unblock = scope_exit([&]() noexcept { first_gate->Open(); });
    auto first = lane.Request(MakePreparationInput("first.png"));
    REQUIRE(first.Status == ResourcePreparation::Admission::Accepted);
    REQUIRE(first_gate->WaitEntered());
    auto second = lane.Request(MakePreparationInput("second.png", {201, 19, 7, 255}));
    REQUIRE(second.Status == ResourcePreparation::Admission::Accepted);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds {10};

    while (lane.Poll(second.Ticket) == ResourcePreparation::Completion::Pending && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    REQUIRE(lane.Poll(second.Ticket) == ResourcePreparation::Completion::Ready);
    CHECK(lane.Poll(first.Ticket) == ResourcePreparation::Completion::Pending);
    CHECK(lane.GetDiagnostics().Consumed == 0);
    first_gate->Open();
    lane.Drain();
    std::thread::id owner = std::this_thread::get_id();
    vector<ucolor> published;
    auto publish = [&](SpriteResourceData&& result) {
        CHECK(std::this_thread::get_id() == owner);
        published.emplace_back(result.Directions[0].Frames[0].Pixels[0]);
    };
    CHECK(lane.Consume(first.Ticket, publish));
    CHECK(lane.Consume(second.Ticket, publish));
    REQUIRE(published.size() == 2);
    CHECK(published[0] == ucolor {17, 29, 43, 255});
    CHECK(published[1] == ucolor {201, 19, 7, 255});
    CHECK(lane.GetDiagnostics().ReservedBytes == 0);
}

TEST_CASE("ResourcePreparationPreservesOriginalExceptionTypeAndFullDiagnostic", "[resource-preparation]")
{
    if constexpr (FO_WEB) {
        SKIP("This build has no worker threads");
    }

    bool acquisition_failure = GENERATE(false, true);
    WorkScheduler scheduler {"preparation-exception", 1};
    std::string diagnostic(8192, 'q');
    ResourcePreparation lane {&scheduler, ResourcePreparation::Limits {}, [diagnostic](const_span<uint8_t>) -> SpriteResourceData { throw std::logic_error(diagnostic); }};
    auto input = MakePreparationInput("failure.png");

    if (acquisition_failure) {
        input.AcquisitionError = std::make_exception_ptr(std::logic_error(diagnostic));
    }

    auto failed = lane.Request(std::move(input));
    REQUIRE(failed.Status == ResourcePreparation::Admission::Accepted);
    size_t reserved = lane.GetDiagnostics().ReservedBytes;
    lane.Drain();
    CHECK(lane.Poll(failed.Ticket) == ResourcePreparation::Completion::Failed);
    CHECK(lane.GetDiagnostics().Outstanding == 1);
    CHECK(lane.GetDiagnostics().ReservedBytes == reserved);
    bool caught_original = false;

    try {
        lane.Consume(failed.Ticket, [](SpriteResourceData&&) { FAIL("A failed resource must not publish"); });
    }
    catch (const std::logic_error& error) {
        caught_original = true;
        CHECK(std::string {error.what()} == diagnostic);
    }

    CHECK(caught_original);
    CHECK(lane.GetDiagnostics().Outstanding == 0);
    CHECK(lane.GetDiagnostics().ReservedBytes == 0);
}

TEST_CASE("ResourcePreparationCapacityOracleAndDriftFallback", "[resource-preparation]")
{
    auto input = MakePreparationInput("capacity.png");
    auto estimate = EstimateSpriteResourceStorage(input.Bytes, 16 * 1024 * 1024);
    REQUIRE(estimate);
    auto data = ReadSpriteResource(input.Bytes);
    auto actual = MeasureSpriteResourceStorage(data, *estimate);
    REQUIRE(actual);
    CHECK(*actual <= *estimate);
    CHECK(data.Directions.capacity() == 1);
    CHECK(data.Directions[0].Frames.capacity() == 1);
    CHECK(data.Directions[0].Frames[0].Pixels.capacity() == 1);
    REQUIRE(data.Animation.Sprite);
    CHECK(data.Animation.Sprite->Directions.capacity() == 1);
    CHECK(data.Animation.Sprite->Directions[0].Frames.capacity() == 1);
    SpriteMeshData mesh {{3, 3}, {1, 1}, {{0, 0}, {1, 0}, {0, 1}}, {0, 1, 2}};
    auto mesh_bytes = BakerTests::MakeMinimalBakedSprite(1, 1, SpriteMeshKind::Mesh, mesh);
    auto mesh_estimate = EstimateSpriteResourceStorage(mesh_bytes, 16 * 1024 * 1024);
    REQUIRE(mesh_estimate);
    auto mesh_data = ReadSpriteResource(mesh_bytes);
    REQUIRE(MeasureSpriteResourceStorage(mesh_data, *mesh_estimate));
    REQUIRE(mesh_data.Directions[0].Frames[0].Mesh);
    CHECK(mesh_data.Directions[0].Frames[0].Mesh->Vertices.capacity() == 3);
    CHECK(mesh_data.Directions[0].Frames[0].Mesh->Indices.capacity() == 3);
    logging::write("Preparation capacity oracle: resource={}, direction={}, frame={}, infoDirection={}, infoFrame={}, estimate={}, actual={}, scheduler={}", sizeof(SpriteResourceData), sizeof(SpriteResourceDirectionData), sizeof(SpriteResourceFrameData), sizeof(SpriteDirInfo), sizeof(SpriteFrameInfo), *estimate, *actual, WorkScheduler::EstimateBackgroundStorage());

    if constexpr (FO_WEB) {
        return;
    }

    WorkScheduler scheduler {"preparation-capacity", 1};
    ResourcePreparation lane {&scheduler, ResourcePreparation::Limits {}, [](const_span<uint8_t> bytes) {
                                  auto result = ReadSpriteResource(bytes);
                                  result.Directions[0].Frames[0].Pixels.reserve(4096);
                                  return result;
                              }};
    auto request = lane.Request(std::move(input));
    REQUIRE(request.Status == ResourcePreparation::Admission::Accepted);
    lane.Drain();
    CHECK(lane.Poll(request.Ticket) == ResourcePreparation::Completion::BudgetDeclined);
    CHECK_FALSE(lane.Consume(request.Ticket, [](SpriteResourceData&&) { FAIL("Capacity drift must use direct fallback"); }));
    CHECK(lane.GetDiagnostics().ReservedBytes == 0);
}

TEST_CASE("ResourcePreparationKeysAndByteLimitsKeepTheirDistinctMeaning", "[resource-preparation]")
{
    if constexpr (FO_WEB) {
        SKIP("This build has no worker threads");
    }

    WorkScheduler scheduler {"preparation-keys", 1};
    ResourcePreparation lane {&scheduler};
    auto initial = lane.Request(MakePreparationInput("key.png"));
    REQUIRE(initial.Status == ResourcePreparation::Admission::Accepted);
    size_t reservation = lane.GetDiagnostics().ReservedBytes;
    set<uint64_t> tickets {initial.Ticket};

    for (int32_t variant = 0; variant < 4; variant++) {
        auto input = MakePreparationInput("key.png");

        switch (variant) {
        case 0:
            input.Identity.Type = ResourcePreparation::Kind::Quad;
            break;
        case 1:
            input.Identity.AtlasIntent++;
            break;
        case 2:
            input.Identity.Generation++;
            break;
        case 3:
            input.Identity.Epoch++;
            break;
        default:
            FAIL("Unexpected key variant");
        }

        auto request = lane.Request(std::move(input));
        REQUIRE(request.Status == ResourcePreparation::Admission::Accepted);
        CHECK(tickets.insert(request.Ticket).second);
    }

    lane.Drain();
    CHECK(lane.GetDiagnostics().Outstanding == 5);
    lane.CancelAll();
    CHECK(lane.GetDiagnostics().ReservedBytes == 0);

    for (int32_t limit_kind = 0; limit_kind < 4; limit_kind++) {
        ResourcePreparation::Limits limits;
        auto input = MakePreparationInput("key.png");

        switch (limit_kind) {
        case 0:
            limits.Bytes = reservation - 1;
            break;
        case 1:
            limits.EncodedBytes = input.Bytes.size() - 1;
            break;
        case 2:
            limits.DecodedBytes = *EstimateSpriteResourceStorage(input.Bytes, 16 * 1024 * 1024) - 1;
            break;
        case 3:
            limits.PathBytes = input.Identity.Path.size() - 1;
            break;
        default:
            FAIL("Unexpected budget variant");
        }

        std::atomic<bool> decoded {};
        ResourcePreparation bounded {&scheduler, limits, [&](const_span<uint8_t> bytes) {
                                         decoded.store(true);
                                         return ReadSpriteResource(bytes);
                                     }};
        CHECK(bounded.Request(std::move(input)).Status == ResourcePreparation::Admission::BudgetDeclined);
        bounded.Drain();
        CHECK_FALSE(decoded.load());
        CHECK(bounded.GetDiagnostics().Outstanding == 0);
    }
}

struct PreparationReadObservation
{
    nptr<MapView> Map {};
    std::thread::id Owner {std::this_thread::get_id()};
    vector<pair<string, size_t>> SpriteReads {};
    size_t HeaderReads {};
    size_t FileReads {};
    std::atomic<bool> ForeignRead {};
};

class PreparationMapSource final : public DataSource
{
public:
    explicit PreparationMapSource(shared_ptr<PreparationReadObservation> observation) :
        _observation {std::move(observation)}
    {
    }

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _files.GetPackName(); }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override { return _files.IsFileExists(path); }
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override
    {
        auto observation = _observation;

        if (std::this_thread::get_id() != observation->Owner) {
            observation->ForeignRead.store(true);
        }
        else {
            observation->HeaderReads++;
        }

        return _files.GetFileInfo(path, size, write_time);
    }
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override { return _files.GetFileNames(dir, recursive, ext); }

    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override
    {
        auto observation = _observation;

        if (std::this_thread::get_id() != observation->Owner) {
            observation->ForeignRead.store(true);
        }
        else {
            observation->FileReads++;

            if (observation->Map && path.ends_with(".png")) {
                observation->SpriteReads.emplace_back(string(path), observation->Map->GetItems().size());
            }
        }

        return _files.OpenFile(path, size, write_time);
    }

    void AddFile(string_view path, vector<uint8_t> bytes) { _files.AddFile(path, std::move(bytes)); }

private:
    BakerTests::MemoryDataSource _files {"PreparationMap"};
    shared_ptr<PreparationReadObservation> _observation;
};

static auto MakePreparationMapResources(shared_ptr<PreparationReadObservation> observation) -> FileSystem
{
    auto metadata = BakerTests::MakeEmptyMetadataBlob();
    auto compiler_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("PreparationCompiler");
    compiler_source->AddFile("Metadata.fometa-client", metadata);
    FileSystem compiler_resources;
    compiler_resources.AddCustomSource(std::move(compiler_source));
    BakerClientEngine compiler {compiler_resources};
    auto source = safe_alloc::make_unique<PreparationMapSource>(std::move(observation));
    source->AddFile("Metadata.fometa-client", metadata);
#if FO_ANGELSCRIPT_SCRIPTING
    source->AddFile("Preparation.fos-bin-client", BakerTests::CompileInlineScripts(&compiler, "PreparationScripts", {{"Scripts/Preparation.fos", "void PreparationEntry() {}"}}, [](string_view message) { FAIL(message); }));
#endif
    vector<pair<string, function<void(ProtoMap&)>>> maps;
    maps.emplace_back("PreparationMap", [](ProtoMap& proto) { proto.SetSize(msize {20, 20}); });
    source->AddFile("PreparationMap.fopro-bin-client", BakerTests::MakeMultiProtoResourceBlob<ProtoMap>(compiler, compiler.Hashes.to_hashed_string("Map"), maps));
    auto configure_item = [&compiler](ProtoItem& proto) {
        proto.SetStatic(true);
        proto.SetOwnership(ItemOwnership::MapHex);
        proto.SetHex(mpos {4, 4});
        proto.SetPicMap(compiler.Hashes.to_hashed_string("First.png"));
    };
    vector<pair<string, function<void(ProtoItem&)>>> items;
    items.emplace_back("PreparationItem", configure_item);
    source->AddFile("PreparationItem.fopro-bin-client", BakerTests::MakeMultiProtoResourceBlob<ProtoItem>(compiler, compiler.Hashes.to_hashed_string("Item"), items));
    ProtoItem proto {compiler.Hashes.to_hashed_string("PreparationItem"), compiler.GetPropertyRegistrar(compiler.Hashes.to_hashed_string("Item"))};
    configure_item(proto);
    vector<uint8_t> records;
    data_writer records_writer {records};
    set<hstring> hashes;

    for (uint32_t index = 0; index < 3; index++) {
        vector<uint8_t> properties;

        if (index == 1) {
            properties = {0};
        }
        else {
            auto props = proto.GetProperties()->Copy();
            props.ApplyFromText(map<string, string> {{"Hex", "4 4"}});
            ItemProperties item_props {props};

            if (index == 2) {
                item_props.SetHex(mpos {6, 4});
                item_props.SetPicMap(compiler.Hashes.to_hashed_string("Second.png"));
            }

            props.StoreAllData(properties, hashes);
        }

        records_writer.write<ident_t::underlying_type>(numeric_cast<ident_t::underlying_type>(101 + index));
        records_writer.write<hstring::hash_t>(proto.GetProtoId().as_hash());
        records_writer.write<uint32_t>(numeric_cast<uint32_t>(properties.size()));
        records_writer.write_bytes(properties);
    }

    vector<uint8_t> map_bytes;
    data_writer writer {map_bytes};
    writer.write<uint32_t>(BAKED_MAP_FILE_MAGIC);
    writer.write<uint32_t>(BAKED_MAP_FILE_VERSION);
    writer.write<uint32_t>(numeric_cast<uint32_t>(hashes.size()));

    for (hstring hash : hashes) {
        writer.write_string(hash.as_str());
    }

    writer.write<uint32_t>(uint32_t {3});
    writer.write_bytes(records);
    source->AddFile("PreparationMap.fomap-bin-client", std::move(map_bytes));
    source->AddFile("First.png", MakePreparationInput("First.png").Bytes);
    source->AddFile("Second.png", MakePreparationInput("Second.png", {201, 19, 7, 255}).Bytes);
    source->AddFile("Third.png", MakePreparationInput("Third.png", {77, 81, 99, 255}).Bytes);
    source->AddFile("Fourth.png", MakePreparationInput("Fourth.png", {8, 27, 66, 255}).Bytes);
    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    return resources;
}

static auto ReadPreparationAtlasPixel(ptr<const Sprite> sprite) -> ucolor
{
    auto atlas_sprite = sprite.dyn_cast<const AtlasSprite>();
    REQUIRE(atlas_sprite);
    auto texture = atlas_sprite->GetBatchTexture();
    REQUIRE(texture);
    frect32 rect = atlas_sprite->GetAtlasRect();
    ipos32 pixel {iround<int32_t>(rect.x * numeric_cast<float32_t>(texture->Size.width)), iround<int32_t>(rect.y * numeric_cast<float32_t>(texture->Size.height))};
    auto pixels = texture->GetTextureRegion(pixel, {1, 1});
    REQUIRE(pixels.size() == 1);
    return pixels[0];
}

TEST_CASE("ResourcePreparationSpriteManagerRecoversFullCancelledRequestsWithoutDrain", "[resource-preparation]")
{
    STATIC_REQUIRE(details::function_target_fits_inline<decltype(&ReadSpriteResource)>);

    if constexpr (FO_WEB) {
        SKIP("This build has no worker threads");
    }

    GlobalSettings settings {false};
    settings.ApplyDefaultSettings();
    BakerTests::ApplySelfContainedClientSettings(settings);
    auto window = GetAppWindowStub(settings);
    FileSystem resources;
    GameTimer timer {&settings};
    hash_storage hashes;
    EffectManager effects {&settings, &resources, window->GetRender()};
    WorkScheduler scheduler {"preparation-recovery", 1};
    auto gate = safe_alloc::make_shared<PreparationDecodeGate>();
    SpriteManager sprites {&settings, window, &resources, &timer, &effects, &hashes, &scheduler, [gate](const_span<uint8_t> bytes) mutable {
                               gate->Enter();
                               return ReadSpriteResource(bytes);
                           }};
    auto unblock = scope_exit([&]() noexcept { gate->Open(); });
    sprites.RegisterSpriteFactory(safe_alloc::make_unique<DefaultSpriteFactory>(&sprites));
    auto observation = safe_alloc::make_shared<PreparationReadObservation>();
    auto source = safe_alloc::make_unique<PreparationMapSource>(observation);
    vector<hstring> paths;

    for (size_t index = 0; index <= ResourcePreparation::Limits {}.Requests; index++) {
        string path = strex("Recovery{}.png", index).str();
        source->AddFile(path, MakePreparationInput(path).Bytes);
        paths.push_back(hashes.to_hashed_string(path));
    }

    resources.AddCustomSource(std::move(source));
    REQUIRE(sprites.PrefetchSpriteResource(paths.front(), AtlasType::MapSprites));
    REQUIRE(gate->WaitEntered());

    for (size_t index = 1; index < ResourcePreparation::Limits {}.Requests; index++) {
        REQUIRE(sprites.PrefetchSpriteResource(paths[index], AtlasType::MapSprites));
    }

    auto full = sprites.GetResourcePreparationDiagnostics();
    CHECK(full.Outstanding == 32);
    CHECK(full.Submitted == 32);
    REQUIRE(full.ReservedBytes > 0);

    // All consumption is still pending: direct publication cancels each prepared request, but cannot reap it
    for (size_t index = 0; index < ResourcePreparation::Limits {}.Requests; index++) {
        auto sprite = sprites.LoadSprite(paths[index], AtlasType::MapSprites);
        REQUIRE(sprite);
        CHECK(ReadPreparationAtlasPixel(sprite.as_ptr()) == ucolor {17, 29, 43, 255});
    }

    CHECK(gate->GetCalls() == 1);
    CHECK(sprites.GetResourcePreparationDiagnostics().Outstanding == full.Outstanding);
    CHECK(sprites.GetResourcePreparationDiagnostics().ReservedBytes == full.ReservedBytes);
    CHECK(sprites.GetResourcePreparationDiagnostics().Consumed == 0);
    size_t headers = observation->HeaderReads;
    size_t reads = observation->FileReads;
    CHECK_FALSE(sprites.PrefetchSpriteResource(paths.back(), AtlasType::MapSprites));
    CHECK(observation->HeaderReads == headers);
    CHECK(observation->FileReads == reads);
    CHECK(sprites.GetResourcePreparationDiagnostics().ReservedBytes == full.ReservedBytes);

    gate->Open();

    // A FIFO sentinel on the sole worker proves every earlier task is terminal without touching lane cleanup
    auto sentinel_group = scheduler.CreateBackgroundGroup();
    shared_ptr<WorkScheduler::BackgroundTask> sentinel;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds {10};

    while (!sentinel && std::chrono::steady_clock::now() < deadline) {
        sentinel = scheduler.TrySubmitBackground(sentinel_group, []() {});
        std::this_thread::yield();
    }

    REQUIRE(sentinel);

    while (scheduler.PollBackground(sentinel).Status != WorkScheduler::BackgroundStatus::Completed && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    REQUIRE(scheduler.PollBackground(sentinel).Status == WorkScheduler::BackgroundStatus::Completed);
    CHECK(gate->GetCalls() == 1);
    CHECK(sprites.GetResourcePreparationDiagnostics().Outstanding == full.Outstanding);
    CHECK_FALSE(sprites.PrefetchSpriteResource(paths.front(), AtlasType::MapSprites));
    CHECK(observation->HeaderReads == headers);
    CHECK(observation->FileReads == reads);
    CHECK(sprites.GetResourcePreparationDiagnostics().ReservedBytes == full.ReservedBytes);

    // Cold admission itself must reclaim the completed cancelled slots before its bounded provider read
    REQUIRE(sprites.PrefetchSpriteResource(paths.back(), AtlasType::MapSprites));
    CHECK(sprites.GetResourcePreparationDiagnostics().Outstanding == 1);
    CHECK(sprites.GetResourcePreparationDiagnostics().Submitted == 33);
    CHECK(sprites.GetResourcePreparationDiagnostics().ReservedBytes < full.ReservedBytes);
    CHECK(observation->HeaderReads == headers + 1);
    CHECK(observation->FileReads == reads + 1);
    CHECK_FALSE(observation->ForeignRead.load());

    sprites.DrainPreparedSpriteResources();
    auto recovered = sprites.LoadSprite(paths.back(), AtlasType::MapSprites);
    REQUIRE(recovered);
    CHECK(ReadPreparationAtlasPixel(recovered.as_ptr()) == ucolor {17, 29, 43, 255});
    CHECK(sprites.GetResourcePreparationDiagnostics().Consumed == 1);
    CHECK(sprites.GetResourcePreparationDiagnostics().Outstanding == 0);
    CHECK(sprites.GetResourcePreparationDiagnostics().ReservedBytes == 0);
}

class PreparationClientEngine final : public ClientEngine
{
public:
    using ClientEngine::ClientEngine;
    using ClientEngine::UnloadMap;
};

TEST_CASE("ResourcePreparationActualStaticMapPreservesPixelsOrderAndFailurePrefix", "[resource-preparation]")
{
    int32_t mode = GENERATE(0, 1, 2);
    bool remove_corrupt_record = GENERATE(false, true);
    CAPTURE(mode, remove_corrupt_record);

    if constexpr (FO_WEB) {
        if (mode == 2) {
            SKIP("This build has no worker threads");
        }
    }

    ClientEngineTests::CheckMinimalClientDiagnostics([&] {
        GlobalSettings settings {false};
        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();
        BakerTests::ApplySelfContainedClientSettings(settings);
        BakerTests::OverrideSetting(settings.Common.Packaged, false);
        BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
        BakerTests::OverrideSetting(settings.Client.Multithreading, mode != 0);
        BakerTests::OverrideSetting(settings.Client.MultithreadingMaxWorkers, mode == 1 ? 0 : 1);
        auto observation = safe_alloc::make_shared<PreparationReadObservation>();
        auto client = safe_alloc::make_refcounted<PreparationClientEngine>(&settings, MakePreparationMapResources(observation), &GetApp()->MainWindow);
        auto shutdown = scope_exit([&]() noexcept { safe_call([&] { client->Shutdown(); }); });
        auto map = safe_alloc::make_refcounted<MapView>(client, ident_t {5001}, client->GetProtoMap(client->Hashes.to_hashed_string("PreparationMap")), isize32 {320, 200});
        observation->Map = map;
        auto destroy_map = scope_exit([&]() noexcept {
            observation->Map = nullptr;
            safe_call([&] { map->DestroySelf(); });
        });

        if (remove_corrupt_record) {
            map->SetRemovedStaticItemIds(vector<ident_t> {ident_t {102}});
            REQUIRE_NOTHROW(map->LoadStaticData());
        }
        else {
            CHECK_THROWS_MATCHES(map->LoadStaticData(), DataReadingException, Catch::Matchers::Predicate<DataReadingException>([](const DataReadingException& ex) { return ex.message() == "Unexpected end of buffer"; }));
        }

        auto first = map->GetItem(ident_t {101});
        REQUIRE(first);
        CHECK(first->GetPicMap().as_str() == "First.png");
        CHECK(first->GetAnim()->GetOffset() == ipos32 {3, -2});
        CHECK(ReadPreparationAtlasPixel(first->GetAnim()) == ucolor {17, 29, 43, 255});
        CHECK_FALSE(map->GetItem(ident_t {102}));
        CHECK(map->GetItems().size() == (remove_corrupt_record ? 2 : 1));
        CHECK(map->GetItems()[0]->GetId() == ident_t {101});
        CHECK_FALSE(observation->ForeignRead.load());
        REQUIRE(observation->SpriteReads.size() == (remove_corrupt_record ? 2 : 1));
        CHECK(observation->SpriteReads[0] == pair<string, size_t> {"First.png", 0});

        if (remove_corrupt_record) {
            auto second = map->GetItem(ident_t {103});
            REQUIRE(second);
            CHECK(second->GetPicMap().as_str() == "Second.png");
            CHECK(ReadPreparationAtlasPixel(second->GetAnim()) == ucolor {201, 19, 7, 255});
            CHECK(map->GetItems()[1]->GetId() == ident_t {103});
            CHECK(observation->SpriteReads[1] == pair<string, size_t> {"Second.png", mode == 2 ? 0 : 1});
        }

        auto diagnostics = client->SprMngr.GetResourcePreparationDiagnostics();
        CHECK(diagnostics.Submitted == (mode == 2 ? remove_corrupt_record ? 2 : 1 : 0));
        CHECK(diagnostics.Consumed == diagnostics.Submitted);
        CHECK(diagnostics.Outstanding == 0);
        CHECK(diagnostics.ReservedBytes == 0);

        if (mode == 2 && remove_corrupt_record) {
            hstring third_path = client->Hashes.to_hashed_string("Third.png");
            REQUIRE(client->SprMngr.PrefetchSpriteResource(third_path, AtlasType::MapSprites));
            REQUIRE(client->SprMngr.PrefetchSpriteResource(third_path, AtlasType::MapSprites));
            client->SprMngr.DrainPreparedSpriteResources();
            auto held = client->SprMngr.GetResourcePreparationDiagnostics();
            REQUIRE(held.Outstanding == 1);
            size_t reads_before_warm = observation->SpriteReads.size();
            auto warm_map = safe_alloc::make_refcounted<MapView>(client, ident_t {5002}, client->GetProtoMap(client->Hashes.to_hashed_string("PreparationMap")), isize32 {320, 200});
            observation->Map = warm_map;
            auto destroy_warm_map = scope_exit([&]() noexcept {
                observation->Map = map;
                safe_call([&] { warm_map->DestroySelf(); });
            });
            warm_map->SetRemovedStaticItemIds(vector<ident_t> {ident_t {102}});
            REQUIRE_NOTHROW(warm_map->LoadStaticData());
            REQUIRE(warm_map->GetItem(ident_t {103}));
            CHECK(ReadPreparationAtlasPixel(warm_map->GetItem(ident_t {103})->GetAnim()) == ucolor {201, 19, 7, 255});
            CHECK(observation->SpriteReads.size() == reads_before_warm);
            auto after_warm = client->SprMngr.GetResourcePreparationDiagnostics();
            CHECK(after_warm.Submitted == held.Submitted);
            CHECK(after_warm.Consumed == held.Consumed);
            CHECK(after_warm.Outstanding == 1);
            CHECK(after_warm.ReservedBytes == held.ReservedBytes);
            auto third = client->SprMngr.LoadSprite(third_path, AtlasType::MapSprites);
            REQUIRE(third);
            CHECK(ReadPreparationAtlasPixel(third.as_ptr()) == ucolor {77, 81, 99, 255});
            CHECK(observation->SpriteReads.size() == reads_before_warm);

            hstring fourth_path = client->Hashes.to_hashed_string("Fourth.png");
            REQUIRE(client->SprMngr.PrefetchSpriteResource(fourth_path, AtlasType::MapSprites));
            client->SprMngr.DrainPreparedSpriteResources();
            bool cancelled_before_event = false;
            Entity::EventCallbackData observer;
            observer.Callback = [&](FuncCallData&) {
                cancelled_before_event = client->SprMngr.GetResourcePreparationDiagnostics().Outstanding == 0;
                return Entity::EventResult::ContinueChain;
            };
            observer.SubscriptionPtr = 1;
            client->OnMapUnload.Subscribe(std::move(observer));
            auto unsubscribe = scope_exit([&]() noexcept { client->OnMapUnload.Unsubscribe(1); });
            client->UnloadMap();
            CHECK(cancelled_before_event);
            REQUIRE(client->SprMngr.PrefetchSpriteResource(fourth_path, AtlasType::MapSprites));
            client->SprMngr.DrainPreparedSpriteResources();
            auto fourth = client->SprMngr.LoadSprite(fourth_path, AtlasType::MapSprites);
            REQUIRE(fourth);
            CHECK(ReadPreparationAtlasPixel(fourth.as_ptr()) == ucolor {8, 27, 66, 255});
        }
    });
}

FO_END_NAMESPACE

