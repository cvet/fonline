from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
from types import ModuleType

import pytest


ENGINE = Path(__file__).resolve().parents[2]
BUILDTOOLS = ENGINE / "BuildTools"
HELPER = BUILDTOOLS / "cmake/helpers/ResourceUsageIntegration.cmake"
sys.path.insert(0, str(BUILDTOOLS))

import buildtools  # noqa: E402


def load_pipeline_module() -> ModuleType:
    path = ENGINE / "BuildTools/tests/check_resource_usage_pipeline.py"
    spec = importlib.util.spec_from_file_location("resource_usage_pipeline_consumer", path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def configure(root: Path, role_arguments: list[str] | None = None) -> Path:
    cmake = shutil.which("cmake")

    if not cmake:
        pytest.skip("CMake is required for the integration target wiring probe")

    build = root / "build"
    arguments = [cmake, "-S", str(root), "-B", str(build), *(role_arguments or [])]

    if shutil.which("ninja"):
        arguments.extend(["-G", "Ninja"])

    result = subprocess.run(arguments, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return build


def wiring_project(root: Path, *, unit_tests: bool = True, baker: bool = True,
                   cross: bool = False, angelscript: bool = False) -> Path:
    script = root / "Engine/BuildTools/tests/check_resource_usage_pipeline.py"
    script.parent.mkdir(parents=True)
    script.write_text("from pathlib import Path\nimport json\nimport sys\n"
                      f"assert Path(sys.argv[sys.argv.index('--baker') + 1]) == Path({sys.executable!r})\n"
                      "Path('pipeline-args.txt').write_text(json.dumps(sys.argv[1:]), encoding='utf-8')\n"
                      "Path('pipeline.marker').write_text('ran')\n", encoding="utf-8")
    (root / "unit.py").write_text("from pathlib import Path\n"
                                  "assert Path('pipeline.marker').read_text() == 'ran'\n"
                                  "Path('unit.marker').write_text('ran')\n", encoding="utf-8")
    imported = ('add_executable(Probe_Baker IMPORTED GLOBAL)\n'
                f'set_target_properties(Probe_Baker PROPERTIES IMPORTED_LOCATION "{Path(sys.executable).as_posix()}")\n'
                if baker else "")
    (root / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.22)\nproject(Probe LANGUAGES NONE)\n'
        f'set(FO_DEV_NAME Probe)\nset(FO_ENGINE_ROOT Engine)\nset(FO_UNIT_TESTS {"ON" if unit_tests else "OFF"})\n'
        f'set(FO_ANGELSCRIPT_SCRIPTING {"ON" if angelscript else "OFF"})\n'
        f'set(CMAKE_CROSSCOMPILING {"TRUE" if cross else "FALSE"})\n'
        f'set(Python3_EXECUTABLE "{Path(sys.executable).as_posix()}")\n'
        + imported +
        'add_custom_target(RunUnitTests COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unit.py")\n'
        f'include("{HELPER.as_posix()}")\nSetupResourceUsageIntegrationTests()\n'
        'get_property(dependencies TARGET RunUnitTests PROPERTY MANUALLY_ADDED_DEPENDENCIES)\n'
        'file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/dependencies.txt" "${dependencies}")\n', encoding="utf-8")
    return configure(root)


@pytest.mark.parametrize("angelscript", [False, True])
def test_unit_target_requires_and_executes_pipeline_first(tmp_path: Path, angelscript: bool) -> None:
    build = wiring_project(tmp_path, angelscript=angelscript)
    assert "RunResourceUsageIntegrationTests" in (build / "dependencies.txt").read_text()
    result = subprocess.run([shutil.which("cmake"), "--build", str(build), "--config", "Release",
                             "--target", "RunUnitTests"], capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    assert (build / "pipeline.marker").read_text() == "ran"
    # unit.py refuses to run until the pipeline marker exists, so both markers prove target ordering
    assert (build / "unit.marker").read_text() == "ran"

    arguments = json.loads((build / "pipeline-args.txt").read_text(encoding="utf-8"))
    assert arguments[0] == "--baker"
    assert arguments[2] == "--workdir"
    assert arguments.count("--angelscript") == (1 if angelscript else 0)


@pytest.mark.parametrize("angelscript", [False, True])
def test_producer_fixture_content_tracks_angelscript_mode(tmp_path: Path, angelscript: bool) -> None:
    pipeline = load_pipeline_module()
    config, _, _ = pipeline.create_fixture(tmp_path, angelscript=angelscript)
    fixture = tmp_path / "inputs/definitions/Fixture.fos"
    assert fixture.is_file() == angelscript

    if angelscript:
        assert fixture.read_text(encoding="utf-8") == "namespace SharedDiagnostics { int Value() { return 42; } }\n"

    config_text = config.read_text(encoding="utf-8")
    expected_bakers = "Metadata AngelScript Proto" if angelscript else "Metadata Proto"
    assert f"Bakers = {expected_bakers}\n" in config_text
    assert ("AngelScript" in config_text) == angelscript

    font = (tmp_path / "inputs/art/Fonts/Test.fofnt").read_text(encoding="utf-8")
    assert font == "Version 2\nImage Ignored.tga\nImage#page\nAtlas.tga*\nEnd\nImage Unreachable.tga\n"
    assert "Fonts/Ignored.tga" in pipeline.FULL
    assert "Fonts/Ignored.tga" not in pipeline.EXPECTED


@pytest.mark.parametrize("angelscript", [False, True])
@pytest.mark.parametrize("settings", [{"unit_tests": False}, {"baker": False}, {"cross": True}])
def test_unavailable_native_pipeline_does_not_change_other_profiles(tmp_path: Path, settings: dict, angelscript: bool) -> None:
    build = wiring_project(tmp_path, angelscript=angelscript, **settings)
    assert "RunResourceUsageIntegrationTests" not in (build / "dependencies.txt").read_text()


VALIDATION_STAGES = (
    "StartProjectGeneration", "RegisterProjectOptions", "AddThirdPartyLibraries",
    "AddEngineSources", "RegisterEngineSources", "SetupCodeGeneration", "BuildCoreLibraries",
    "BuildApplications", "SetupScriptsAndBaking", "BuildPackages", "FinalizeProjectGeneration",
)


def validation_profile_project(root: Path, role_arguments: list[str]) -> Path:
    source = (BUILDTOOLS / "validation-project/CMakeLists.txt").read_text(encoding="utf-8")
    assert source.count("project(FOnlineTest)") == 1
    source = source.replace("project(FOnlineTest)", "project(FOnlineTest LANGUAGES NONE)")

    # Execute the real Init.cmake and SetOption; only unrelated compiler/library stages are inert.
    init = root / "Engine/BuildTools/Init.cmake"
    init.parent.mkdir(parents=True)
    init.write_text(
        f'include("{(BUILDTOOLS / "Init.cmake").as_posix()}")\n'
        + "".join(f"macro({stage})\nendmacro()\n" for stage in VALIDATION_STAGES),
        encoding="utf-8",
    )
    pipeline_script = root / "Engine/BuildTools/tests/check_resource_usage_pipeline.py"
    pipeline_script.parent.mkdir(parents=True)
    pipeline_script.write_text(
        "from pathlib import Path\n"
        "Path('pipeline.marker').write_text('ran')\n",
        encoding="utf-8",
    )
    (root / "unit.py").write_text(
        "from pathlib import Path\n"
        "assert Path('pipeline.marker').read_text() == 'ran'\n"
        "Path('unit.marker').write_text('ran')\n",
        encoding="utf-8",
    )

    python_path = Path(sys.executable).as_posix()
    (root / "CMakeLists.txt").write_text(
        source + f'''
if(FO_BUILD_BAKER)
    add_executable(FOTEST_Baker IMPORTED GLOBAL)
    set_target_properties(FOTEST_Baker PROPERTIES IMPORTED_LOCATION "{python_path}")
endif()
set(Python3_EXECUTABLE "{python_path}")
set(FO_ENGINE_ROOT Engine)
add_custom_target(RunUnitTests COMMAND "${{Python3_EXECUTABLE}}" "${{CMAKE_CURRENT_SOURCE_DIR}}/unit.py"
    WORKING_DIRECTORY "${{CMAKE_CURRENT_BINARY_DIR}}")
include("{(BUILDTOOLS / "cmake/helpers/ResourceUsageIntegration.cmake").as_posix()}")
SetupResourceUsageIntegrationTests()
if(TARGET RunResourceUsageIntegrationTests)
    set(pipeline_observed ON)
else()
    set(pipeline_observed OFF)
endif()
file(WRITE "${{CMAKE_CURRENT_BINARY_DIR}}/observed-baker.txt" "${{FO_BUILD_BAKER}}")
file(WRITE "${{CMAKE_CURRENT_BINARY_DIR}}/observed-unit.txt" "${{FO_UNIT_TESTS}}")
file(WRITE "${{CMAKE_CURRENT_BINARY_DIR}}/observed-client.txt" "${{FO_BUILD_CLIENT}}")
file(WRITE "${{CMAKE_CURRENT_BINARY_DIR}}/observed-backend.txt" "${{FO_ANGELSCRIPT_SCRIPTING}}")
file(WRITE "${{CMAKE_CURRENT_BINARY_DIR}}/observed-ascompiler.txt" "${{FO_BUILD_ASCOMPILER}}")
file(WRITE "${{CMAKE_CURRENT_BINARY_DIR}}/observed-pipeline.txt" "${{pipeline_observed}}")
''',
        encoding="utf-8",
    )
    return configure(root, role_arguments)


def validation_profile_observations(build: Path) -> dict[str, bool]:
    def observed(name: str) -> bool:
        value = (build / f"observed-{name}.txt").read_text(encoding="utf-8").strip().upper()
        return value in {"1", "ON", "TRUE", "YES"}

    return {
        name: observed(name)
        for name in ("baker", "unit", "client", "backend", "ascompiler", "pipeline")
    }


def expected_profile_observations(**expected: bool) -> dict[str, bool]:
    observations = {
        "baker": False,
        "unit": False,
        "client": False,
        "backend": True,
        "ascompiler": False,
        "pipeline": False,
    }
    observations.update(expected)
    return observations


def build_unit_tests(build: Path) -> subprocess.CompletedProcess[str]:
    cmake = shutil.which("cmake")
    assert cmake is not None
    return subprocess.run(
        [cmake, "--build", str(build), "--config", "Release", "--target", "RunUnitTests"],
        capture_output=True, text=True, timeout=120,
    )


@pytest.mark.parametrize(
    ("platform_name", "config"),
    [("linux", "San_Thread"), ("win64", "Release")],
)
def test_real_unit_tests_profile_enables_baker_and_orders_pipeline_before_units(
    tmp_path: Path, platform_name: str, config: str,
) -> None:
    arguments = buildtools.make_platform_build_flag_args(platform_name, "unit-tests", config)
    assert "-DFO_BUILD_BAKER=1" in arguments
    assert "-DFO_UNIT_TESTS=1" in arguments

    build = validation_profile_project(tmp_path, arguments)

    assert validation_profile_observations(build) == expected_profile_observations(
        baker=True, unit=True, pipeline=True,
    )

    result = build_unit_tests(build)

    assert result.returncode == 0, result.stdout + result.stderr
    assert (build / "pipeline.marker").read_text() == "ran"
    assert (build / "unit.marker").read_text() == "ran"


def test_original_missing_baker_state_fails_the_unit_pipeline(tmp_path: Path) -> None:
    arguments = buildtools.make_platform_build_flag_args("linux", "unit-tests", "San_Thread")
    assert "-DFO_BUILD_BAKER=1" in arguments

    original_state = [
        "-DFO_BUILD_BAKER=0" if argument == "-DFO_BUILD_BAKER=1" else argument
        for argument in arguments
    ]
    assert original_state.count("-DFO_BUILD_BAKER=0") == 1

    build = validation_profile_project(tmp_path, original_state)

    assert validation_profile_observations(build) == expected_profile_observations(unit=True)

    result = build_unit_tests(build)

    assert result.returncode != 0
    assert not (build / "pipeline.marker").exists()
    assert not (build / "unit.marker").exists()


def test_real_setoption_keeps_a_late_explicit_baker_disable(tmp_path: Path) -> None:
    arguments = [
        *buildtools.make_platform_build_flag_args("linux", "unit-tests", "San_Thread"),
        "-DFO_BUILD_BAKER=0",
    ]

    build = validation_profile_project(tmp_path, arguments)

    assert validation_profile_observations(build) == expected_profile_observations(unit=True)


def test_real_profile_default_without_cli_still_enables_baker(tmp_path: Path) -> None:
    build = validation_profile_project(tmp_path, [])

    assert validation_profile_observations(build) == expected_profile_observations(
        baker=True, unit=True, pipeline=True,
    )


def test_unrelated_client_role_keeps_baker_and_pipeline_disabled(tmp_path: Path) -> None:
    arguments = buildtools.make_platform_build_flag_args("linux", "client", "Release")

    build = validation_profile_project(tmp_path, arguments)

    assert validation_profile_observations(build) == expected_profile_observations(client=True)


def test_unrelated_client_role_rejects_forced_baker_without_unit_tests(tmp_path: Path) -> None:
    arguments = buildtools.make_platform_build_flag_args("linux", "client", "Release")
    assert "-DFO_BUILD_BAKER=0" in arguments

    forced_baker = [
        "-DFO_BUILD_BAKER=1" if argument == "-DFO_BUILD_BAKER=0" else argument
        for argument in arguments
    ]

    build = validation_profile_project(tmp_path, forced_baker)

    assert validation_profile_observations(build) == expected_profile_observations(
        baker=True, client=True,
    )
