"""Exercise the production finalization loop against a delayed browser job queue.

This checks ordering and bounds; an actual Mono/browser shutdown still owns ABI,
GC transition and native entity lifetime qualification.
"""

from pathlib import Path
import shutil
import subprocess

import pytest


BACKEND = Path(__file__).resolve().parents[2] / "Source/Scripting/Managed/ManagedScriptBackend.cpp"
PREFIX = r'''
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using string = std::string;
using timespan = int;
struct time_meter
{
    int get_duration() const
    {
        return 0;
    }
};
struct MonoMethod { };
class ManagedScriptBackend
{
public:
    bool _domain = true;
    std::vector<int> _images {1};
    void FinalizeManagedObjects() noexcept;
};

int Live;
int Held;
int Jobs;
int Pumps;
int Collects;
int Reports;
int Violations;

#define FO_TRACE_ZONE(category)
#define FO_VERIFY_AND_CONTINUE(condition, ...) do { if (!(condition)) ++Violations; } while (false)
template<class F> void safe_call(F action) noexcept
{
    try {
        action();
    }
    catch (const std::exception&) {
        ++Violations;
    }
}

template<class... T> void ignore_unused(const T&...) { }
namespace logging
{
template<class... T> void write(const char*, const T&...) { }
}

MonoMethod* FindCoreScriptMethod(ManagedScriptBackend*, const char*, const char*, int)
{
    static MonoMethod method;
    return &method;
}

int32_t InvokeEntityWrapperTrackerCollect(ManagedScriptBackend*, MonoMethod*, int32_t passes)
{
    if (passes != 0) {
        ++Collects;
#if FO_WEB
        Jobs = Live > Held ? 1 : 0;
#else
        Live = Held;
#endif
    }

    return Live;
}

void mono_background_exec()
{
    ++Pumps;
    Live -= Jobs;
    Jobs = 0;
}

string InvokeEntityWrapperTrackerDump(ManagedScriptBackend*, MonoMethod*)
{
    ++Reports;
    return Live == 0 ? "" : "wrappers remain";
}
'''

SUFFIX = r'''
int main(int argc, char** argv)
{
    if (argc != 3) {
        return 2;
    }

    Live = std::atoi(argv[1]);
    Held = std::atoi(argv[2]);
    ManagedScriptBackend backend;
    backend.FinalizeManagedObjects();
    std::cout << Live << " " << Pumps << " " << Collects << " "
              << Reports << " " << Violations << "\n";
}
'''


@pytest.fixture(scope="module", params=[0, 1], ids=["native", "browser"])
def finalization_probe(tmp_path_factory, request):
    compiler = shutil.which("clang++") or shutil.which("c++")
    if compiler is None:
        pytest.skip("A C++20 compiler is required for the finalization-loop probe")

    source = BACKEND.read_text(encoding="utf-8")
    start = source.index("void ManagedScriptBackend::FinalizeManagedObjects() noexcept\n{")
    end = source.index("\n}\n", start) + len("\n}\n")
    output = tmp_path_factory.mktemp("managed-finalization-" + str(request.param))
    path = output / "Probe.cpp"
    path.write_text(PREFIX + source[start:end] + SUFFIX, encoding="utf-8")
    executable = output / "Probe.exe"
    result = subprocess.run(
        [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
         "-DFO_WEB=" + str(request.param), str(path), "-o", str(executable)],
        capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return executable, bool(request.param)


@pytest.mark.parametrize("live,held", [(0, 0), (3, 0), (8, 0), (10, 0), (1, 1)])
def test_finalization_drains_jobs_before_reading_count(finalization_probe, live, held):
    executable, browser = finalization_probe
    result = subprocess.run([str(executable), str(live), str(held)],
                            capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr
    remaining, pumps, collects, reports, violations = map(int, result.stdout.split())
    expected_remaining = max(held, live - 8) if browser else held
    assert remaining == expected_remaining
    assert reports == 1
    assert violations == int(expected_remaining != 0)

    if browser:
        assert 1 <= pumps <= 8
        assert collects == pumps
        if live > 0 and held == 0:
            assert pumps == min(live, 8)
    else:
        assert pumps == 0
        assert collects == 1
