from pathlib import Path
import shutil
import subprocess

import pytest


SOURCE = Path(__file__).resolve().parents[2] / 'Source/Common/NetBuffer.cpp'


@pytest.fixture(scope='module')
def frame_probe(tmp_path_factory: pytest.TempPathFactory) -> Path:
    if not shutil.which('cmake'):
        pytest.skip('CMake and a C++ compiler are required')

    folder = tmp_path_factory.mktemp('net-out-start')
    source = SOURCE.read_text(encoding='utf-8')
    start = source.index('void NetOutBuffer::StartMsg(')
    end = source.index('\nvoid NetOutBuffer::EndMsg()', start)
    abort_start = source.index('void NetOutBuffer::AbortMsg()')
    abort_end = source.index('\nvoid NetOutBuffer::WriteHashedString(', abort_start)
    methods = source[start:end] + '\n' + source[abort_start:abort_end]
    (folder / 'probe.cpp').write_text(
        r'''
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <vector>
using std::size_t;
using std::uint32_t;
#define FO_VERIFY_AND_THROW(condition, ...) do { if (!(condition)) throw std::runtime_error("verification"); } while (false)
#define FO_STRONG_ASSERT(condition, ...) do { if (!(condition)) std::abort(); } while (false)
template<typename T> class scope_fail {
public:
    explicit scope_fail(T callback) : _callback(callback), _exceptions(std::uncaught_exceptions()) {}
    ~scope_fail() noexcept { if (std::uncaught_exceptions() > _exceptions) _callback(); }
private:
    T _callback;
    int _exceptions;
};
enum class NetMessage { Ping };
class NetOutBuffer {
public:
    static constexpr uint32_t NETMSG_SIGNATURE {0x011E9422};
    void StartMsg(NetMessage msg);
    void AbortMsg() noexcept;
    template<typename T> void Write(T) {
        if (++writes == fail_at) throw std::runtime_error("injected write failure");
        if (_bufEndPos == bytes.size()) bytes.push_back(42);
        else bytes[_bufEndPos] = 42;
        ++_bufEndPos;
    }
    bool _msgStarted {};
    size_t _startedBufPos {};
    size_t _bufEndPos {2};
    std::vector<int> bytes {7, 8};
    int fail_at {};
    int writes {};
};
''' + methods + r'''
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    int mode {std::atoi(argv[1])};
    NetOutBuffer buffer {};
    buffer.fail_at = mode;
    try {
        buffer.StartMsg(NetMessage::Ping);
        if (mode != 0) return 3;
    }
    catch (const std::runtime_error&) {
        if (mode == 0) return 4;
        if (buffer._msgStarted || buffer._bufEndPos != 2 || buffer.bytes[0] != 7 || buffer.bytes[1] != 8) return 5;
        buffer.fail_at = 0;
        buffer.writes = 0;
        buffer.StartMsg(NetMessage::Ping);
    }
    if (!buffer._msgStarted || buffer._bufEndPos != 5) return 6;
    try {
        buffer.StartMsg(NetMessage::Ping);
        return 7;
    }
    catch (const std::runtime_error&) {
        if (!buffer._msgStarted || buffer._bufEndPos != 5) return 8;
    }
    buffer.AbortMsg();
    return buffer._msgStarted || buffer._bufEndPos != 2 ? 9 : 0;
}
''', encoding='utf-8')
    (folder / 'CMakeLists.txt').write_text(
        'cmake_minimum_required(VERSION 3.22)\n'
        'project(NetOutStartFaults CXX)\n'
        'add_executable(frame_probe probe.cpp)\n'
        'target_compile_features(frame_probe PRIVATE cxx_std_20)\n',
        encoding='utf-8',
    )
    build = folder / 'build'
    for command in (['cmake', '-S', str(folder), '-B', str(build)],
                    ['cmake', '--build', str(build), '--config', 'Release']):
        result = subprocess.run(command, capture_output=True, text=True, timeout=180)
        assert result.returncode == 0, result.stdout + result.stderr
    candidates = [path for path in build.rglob('frame_probe*')
                  if path.is_file() and path.name in ('frame_probe', 'frame_probe.exe')]
    assert len(candidates) == 1, candidates
    return candidates[0]


@pytest.mark.parametrize('failure_at', [0, 1, 2, 3])
def test_start_preserves_prior_frame_and_recovers(frame_probe: Path, failure_at: int) -> None:
    result = subprocess.run([str(frame_probe), str(failure_at)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, f'failure_at={failure_at}, exit={result.returncode}: {result.stdout}{result.stderr}'
