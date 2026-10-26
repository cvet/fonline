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

#pragma once

#include "Common.h"

#include "SpriteResource.h"
#include "WorkScheduler.h"

FO_BEGIN_NAMESPACE

class ResourcePreparation final
{
public:
    enum class Kind : uint8_t
    {
        Sprite,
        Quad,
    };

    enum class Admission : uint8_t
    {
        Accepted,
        Coalesced,
        Full,
        BudgetDeclined,
        Stopped,
        Stale,
    };

    enum class Completion : uint8_t
    {
        Pending,
        Ready,
        Missing,
        Failed,
        Cancelled,
        Stale,
        BudgetDeclined,
    };

    struct Key
    {
        string Path {};
        Kind Type {};
        uint32_t AtlasIntent {};
        uint64_t Generation {};
        uint64_t Epoch {};
    };

    struct Input
    {
        Key Identity {};
        vector<uint8_t> Bytes {};
        bool Missing {};
        std::exception_ptr AcquisitionError {};
    };

    struct RequestResult
    {
        Admission Status {};
        uint64_t Ticket {};
    };

    struct Limits
    {
        size_t Requests {32};
        size_t Bytes {64 * 1024 * 1024};
        size_t EncodedBytes {8 * 1024 * 1024};
        size_t DecodedBytes {16 * 1024 * 1024};
        size_t PathBytes {4096};
    };

    struct Diagnostics
    {
        size_t Outstanding {};
        size_t ReservedBytes {};
        uint64_t Submitted {};
        uint64_t Consumed {};
    };

    // A decoder is reentrant CPU work over owned bytes, without owner/provider/renderer captures
    using Decoder = function<SpriteResourceData(const_span<uint8_t>)>;

    explicit ResourcePreparation(ptr<WorkScheduler> scheduler);
    ResourcePreparation(ptr<WorkScheduler> scheduler, Limits limits, Decoder decoder);
    ResourcePreparation(const ResourcePreparation&) = delete;
    ResourcePreparation(ResourcePreparation&&) noexcept = delete;
    auto operator=(const ResourcePreparation&) -> ResourcePreparation& = delete;
    auto operator=(ResourcePreparation&&) noexcept -> ResourcePreparation& = delete;
    ~ResourcePreparation();

    [[nodiscard]] auto GetDiagnostics() const -> Diagnostics;

    // Reclaim terminal cancelled requests on the owner without waiting; active/publication charges remain
    [[nodiscard]] auto HasAvailableRequestSlot() -> bool;
    [[nodiscard]] auto FindRequest(const Key& key) const -> uint64_t;
    [[nodiscard]] auto Poll(uint64_t ticket) const -> Completion;

    auto Request(Input input) -> RequestResult;
    auto Request(Key key, const_span<uint8_t> bytes) -> RequestResult;
    // The reservation stays charged for the complete owner publication callback, including unwind
    auto Consume(uint64_t ticket, const function<void(SpriteResourceData&&)>& publish) -> bool;
    void Drain();
    void Release(uint64_t ticket);
    void CancelGeneration(uint64_t generation);
    void CancelAll();
    void Shutdown();

private:
    struct Job
    {
        Key Identity {};
        vector<uint8_t> Bytes {};
        optional<SpriteResourceData> Data {};
        std::exception_ptr AcquisitionError {};
        shared_ptr<WorkScheduler::BackgroundGroup> Group {};
        shared_ptr<WorkScheduler::BackgroundTask> Task {};
        uint64_t Ticket {};
        size_t Reservation {};
        bool Missing {};
        bool BudgetDeclined {};
        bool Publishing {};
        std::atomic<bool> Cancelled {};
    };

    void ValidateOwner() const;
    [[nodiscard]] auto EstimateReservation(const Key& key, const_span<uint8_t> bytes, size_t capacity) const -> optional<pair<size_t, size_t>>;
    [[nodiscard]] auto IsFinished(const Job& job) const -> bool;
    void ReapCancelled();

    std::thread::id _ownerThread {std::this_thread::get_id()};
    ptr<WorkScheduler> _scheduler;
    Limits _limits;
    shared_ptr<Decoder> _decoder;
    vector<shared_ptr<Job>> _jobs {};
    size_t _reservedBytes {};
    uint64_t _nextTicket {};
    uint64_t _cancelledThroughGeneration {};
    bool _stopped {};
    uint64_t _submitted {};
    uint64_t _consumed {};
};

FO_END_NAMESPACE

