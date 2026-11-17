from pathlib import Path
import shutil
import subprocess

import pytest


SOURCE = Path(__file__).resolve().parents[2] / 'Source/Server/DataBase.cpp'


@pytest.fixture(scope='module')
def recovery_probe(tmp_path_factory: pytest.TempPathFactory) -> Path:
    if not shutil.which('cmake'):
        pytest.skip('CMake and a C++ compiler are required')

    folder = tmp_path_factory.mktemp('recovery-log-io')
    source = SOURCE.read_text(encoding='utf-8')
    usable_start = source.index('auto DataBaseImpl::RecoveryLogHandle::IsUsable()')
    usable_end = source.index('\nauto DataBaseImpl::RecoveryLogHandle::GetLinesCount()', usable_start)
    snapshot_start = source.index('auto DataBaseImpl::RecoveryLogHandle::SnapshotContent()')
    snapshot_end = source.index('\nauto DataBaseImpl::RecoveryLogHandle::Read()', snapshot_start)
    start = source.index('auto DataBaseImpl::RecoveryLogHandle::Truncate()')
    end = source.index('\nauto ConnectToDataBase(', start)
    methods = source[usable_start:usable_end] + '\n' + source[snapshot_start:snapshot_end] + '\n' + source[start:end]
    (folder / 'probe.cpp').write_text(
        r'''
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
using std::string;
using std::string_view;
using std::vector;
using std::int64_t;
using std::size_t;
using std::uint64_t;
using std::pair;
using std::scoped_lock;
class DataBaseException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
#define FO_TRACE_ZONE(...)
#define FO_STRONG_ASSERT(condition, ...) do { if (!(condition)) std::abort(); } while (false)
template<typename T> auto make_ptr(T* p) -> T* { return p; }
class strex {
public:
    explicit strex(string_view text) : value(text) {}
    auto normalize_line_endings() -> strex& { return *this; }
    auto str() const -> string { return value; }
private:
    string value;
};
namespace osfile {
    int mode {};
    int calls {};
    int writes {};
    int truncates {};
    string disk {};
    auto seek_file_end(int) -> int64_t { ++calls; return mode == 1 ? -1 : static_cast<int64_t>(disk.size()); }
    auto write_file_chunk(int, const char* data, size_t size) -> int64_t {
        ++calls;
        ++writes;
        if (mode == 2 || (mode == 3 && writes == 2)) return -1;
        auto count = mode == 3 ? size / 2 : size;
        disk.append(data, count);
        return static_cast<int64_t>(count);
    }
    auto truncate_file(int) -> bool { ++calls; ++truncates; if (mode == 5 || (mode == 8 && truncates == 2) || (mode == 9 && truncates == 1)) return false; disk.clear(); return true; }
    auto seek_file_begin(int) -> bool { ++calls; return mode != 6; }
    auto sync_file(int) -> bool { ++calls; return mode != 4 && mode != 7; }
}
class DataBaseImpl {
public:
    class RecoveryLogHandle {
    public:
        enum class ReplayAppendResult { Appended, RevisionChanged, Failed };
        enum class ConditionalAppendResult { NoPending, Appended, Failed };
        RecoveryLogHandle() = default;
        explicit RecoveryLogHandle(vector<string> content) : _content(std::move(content)) {}
        auto IsUsable() const noexcept -> bool;
        auto IsUsableAtRevision(uint64_t revision) const noexcept -> bool;
        auto HasTruncatedSince(uint64_t revision) const noexcept -> bool;
        auto SnapshotContent() const -> vector<string>;
        auto SnapshotContentWithRevision() const -> pair<vector<string>, uint64_t>;
        auto Truncate() noexcept -> bool;
        auto TruncateIfRevision(uint64_t expected_revision) noexcept -> bool;
        auto Append(string_view text) noexcept -> bool;
        auto AppendIfPending(string_view text) noexcept -> ConditionalAppendResult;
        auto AppendReplayProgress(RecoveryLogHandle& committed, uint64_t expected_revision, uint64_t expected_committed_revision, string_view text) noexcept -> ReplayAppendResult;
        auto MarkCleanupAndTruncate(RecoveryLogHandle& committed, uint64_t expected_revision, uint64_t expected_committed_revision, uint64_t& cleanup_revision) noexcept -> bool;
        auto CompleteMarkedCleanup(RecoveryLogHandle& committed, uint64_t expected_revision, uint64_t expected_committed_revision, bool add_committed_marker, uint64_t& cleanup_revision) noexcept -> bool;
        auto CompleteLegacyCleanup(RecoveryLogHandle& committed, uint64_t expected_revision, uint64_t expected_committed_revision) noexcept -> bool;
    private:
        auto TruncateLocked() noexcept -> bool;
        auto AppendLocked(string_view text) noexcept -> bool;
        int _fd {1};
        std::atomic_bool _ioFailed {};
        mutable std::mutex _locker {};
        size_t _textSize {};
        vector<string> _content {};
        uint64_t _revision {};
        uint64_t _lastTruncationRevision {};
    };
};
''' + methods + r'''
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    osfile::mode = std::atoi(argv[1]);
    DataBaseImpl::RecoveryLogHandle log {};
    if (osfile::mode >= 15 && osfile::mode <= 18) {
        osfile::mode -= 14;
        osfile::disk = "insert x 1 {}\n";
        DataBaseImpl::RecoveryLogHandle pending {vector<string> {"insert x 1 {}"}};
        auto result = pending.AppendIfPending("update x 1 {}\n");
        if (result != DataBaseImpl::RecoveryLogHandle::ConditionalAppendResult::Failed || pending.IsUsable()) return 26;
        int calls = osfile::calls;
        if (pending.AppendIfPending("update x 1 {}\n") != DataBaseImpl::RecoveryLogHandle::ConditionalAppendResult::Failed) return 27;
        if (osfile::calls != calls) return 28;
        return 0;
    }
    if (osfile::mode == 14) {
        osfile::mode = 1;
        if (log.Append("update x 1 {}\n") || log.IsUsable()) return 23;
        osfile::mode = 0;
        DataBaseImpl::RecoveryLogHandle committed {};
        int calls = osfile::calls;
        if (log.AppendReplayProgress(committed, 0, 0, "update x 1 {}\n") != DataBaseImpl::RecoveryLogHandle::ReplayAppendResult::Failed) return 24;
        if (!committed.IsUsable() || osfile::calls != calls) return 25;
        return 0;
    }
    if (osfile::mode >= 10 && osfile::mode <= 13) {
        osfile::mode -= 9;
        DataBaseImpl::RecoveryLogHandle committed {};
        auto result = log.AppendReplayProgress(committed, 0, 0, "update x 1 {}\n");
        if (result != DataBaseImpl::RecoveryLogHandle::ReplayAppendResult::Failed) return 19;
        if (!log.IsUsable() || committed.IsUsable()) return 20;
        int calls = osfile::calls;
        if (log.AppendReplayProgress(committed, 0, 0, "update x 1 {}\n") != DataBaseImpl::RecoveryLogHandle::ReplayAppendResult::Failed) return 21;
        if (osfile::calls != calls) return 22;
        return 0;
    }
    if (osfile::mode >= 8) {
        DataBaseImpl::RecoveryLogHandle committed {vector<string> {"insert x 1 {}", "@reset-v1"}};
        if (log.CompleteLegacyCleanup(committed, 0, 0)) return 16;
        if (osfile::mode == 8 && (!log.IsUsable() || committed.IsUsable() || osfile::truncates != 2)) return 17;
        if (osfile::mode == 9 && (log.IsUsable() || !committed.IsUsable() || osfile::truncates != 1)) return 18;
        return 0;
    }
    if (osfile::mode == 0) {
        if (log.Append("invalid\n") || !log.IsUsable()) return 3;
        if (!log.Append("insert x 1 {}\n") || !log.IsUsable()) return 4;
        if (log.SnapshotContent() != vector<string> {"insert x 1 {}"}) return 12;
        auto before {log.SnapshotContentWithRevision()};
        if (!log.Append("update x 1 {}\n")) return 13;
        if (log.TruncateIfRevision(before.second)) return 14;
        auto after {log.SnapshotContentWithRevision()};
        if (after.first != vector<string> {"insert x 1 {}", "update x 1 {}"}) return 15;
        if (!log.TruncateIfRevision(after.second) || !log.IsUsable()) return 5;
        return 0;
    }
    osfile::disk = "baseline";
    const bool ok = osfile::mode <= 4 ? log.Append("insert x 1 {}\n") : log.Truncate();
    if (ok || log.IsUsable() || log.IsUsableAtRevision(0)) return 6;
    try {
        (void)log.SnapshotContent();
        return 11;
    }
    catch (const DataBaseException&) {
    }
    if (osfile::mode == 3 && osfile::disk.size() <= string("baseline").size()) return 9;
    if ((osfile::mode == 6 || osfile::mode == 7) && !osfile::disk.empty()) return 10;
    const int calls = osfile::calls;
    const string disk = osfile::disk;
    if (log.Append("update x 1 {}\n") || log.Truncate() || log.TruncateIfRevision(0)) return 7;
    if (osfile::calls != calls || osfile::disk != disk) return 8;
    return 0;
}
''', encoding='utf-8')
    (folder / 'CMakeLists.txt').write_text(
        'cmake_minimum_required(VERSION 3.22)\n'
        'project(RecoveryLogFaults CXX)\n'
        'add_executable(recovery_probe probe.cpp)\n'
        'target_compile_features(recovery_probe PRIVATE cxx_std_20)\n',
        encoding='utf-8',
    )
    build = folder / 'build'
    for command in (['cmake', '-S', str(folder), '-B', str(build)],
                    ['cmake', '--build', str(build), '--config', 'Release']):
        result = subprocess.run(command, capture_output=True, text=True, timeout=180)
        assert result.returncode == 0, result.stdout + result.stderr
    candidates = [p for p in build.rglob('recovery_probe*') if p.is_file() and p.name in ('recovery_probe', 'recovery_probe.exe')]
    assert len(candidates) == 1, candidates
    return candidates[0]


@pytest.mark.parametrize('mode', range(19), ids=['normal', 'seek-end', 'write', 'partial-write', 'append-sync', 'truncate', 'seek-begin', 'truncate-sync', 'legacy-committed-truncate', 'legacy-pending-truncate', 'replay-seek-end', 'replay-write', 'replay-partial-write', 'replay-sync', 'replay-pending-unusable', 'conditional-seek-end', 'conditional-write', 'conditional-partial-write', 'conditional-sync'])
def test_failed_log_io_disables_further_io(recovery_probe: Path, mode: int) -> None:
    result = subprocess.run([str(recovery_probe), str(mode)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, f'mode={mode}, exit={result.returncode}: {result.stdout}{result.stderr}'
