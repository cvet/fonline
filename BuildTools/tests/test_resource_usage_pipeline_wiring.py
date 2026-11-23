from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import sys

import pytest


ENGINE = Path(__file__).resolve().parents[2]
HELPER = ENGINE / "BuildTools/cmake/helpers/ResourceUsageIntegration.cmake"


def configure(root: Path) -> Path:
    cmake = shutil.which("cmake")

    if not cmake:
        pytest.skip("CMake is required for the integration target wiring probe")

    build = root / "build"
    arguments = [cmake, "-S", str(root), "-B", str(build)]

    if shutil.which("ninja"):
        arguments.extend(["-G", "Ninja"])

    result = subprocess.run(arguments, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return build


def wiring_project(root: Path, *, unit_tests: bool = True, baker: bool = True,
                   cross: bool = False, angelscript: bool = False) -> Path:
    script = root / "Engine/BuildTools/tests/check_resource_usage_pipeline.py"
    script.parent.mkdir(parents=True)
    script.write_text("from pathlib import Path\nimport sys\n"
                      f"assert Path(sys.argv[sys.argv.index('--baker') + 1]) == Path({sys.executable!r})\n"
                      f"assert sys.argv.count('--angelscript') == {1 if angelscript else 0}\n"
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
        f'set(CMAKE_CROSSCOMPILING {"TRUE" if cross else "FALSE"})\n'
        f'set(FO_ANGELSCRIPT_SCRIPTING {"ON" if angelscript else "OFF"})\n'
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
    assert (build / "unit.marker").read_text() == "ran"


@pytest.mark.parametrize("angelscript", [False, True])
@pytest.mark.parametrize("settings", [{"unit_tests": False}, {"baker": False}, {"cross": True}])
def test_unavailable_native_pipeline_does_not_change_other_profiles(tmp_path: Path, settings: dict,
                                                                    angelscript: bool) -> None:
    build = wiring_project(tmp_path, angelscript=angelscript, **settings)
    assert "RunResourceUsageIntegrationTests" not in (build / "dependencies.txt").read_text()


def test_standard_ci_validation_project_enables_the_real_baker(tmp_path: Path) -> None:
    # Execute the actual profile; only compiler detection and stage side effects are disabled
    source = (ENGINE / "BuildTools/validation-project/CMakeLists.txt").read_text(encoding="utf-8")
    source = source.replace("project(FOnlineTest)", "project(FOnlineTest LANGUAGES NONE)")
    init = tmp_path / "Engine/BuildTools/Init.cmake"
    init.parent.mkdir(parents=True)
    stages = ("StartProjectGeneration", "RegisterProjectOptions", "AddThirdPartyLibraries",
              "AddEngineSources", "RegisterEngineSources", "SetupCodeGeneration", "BuildCoreLibraries",
              "BuildApplications", "SetupScriptsAndBaking", "BuildPackages", "FinalizeProjectGeneration")
    init.write_text('macro(SetOption name value)\nset(${name} "${value}")\nendmacro()\n'
                    'macro(SetOptionValues)\nendmacro()\n' +
                    "".join(f"macro({name})\nendmacro()\n" for name in stages), encoding="utf-8")
    (tmp_path / "CMakeLists.txt").write_text(source +
        '\nfile(WRITE "${CMAKE_CURRENT_BINARY_DIR}/baker-option.txt" "${FO_BUILD_BAKER}")\n', encoding="utf-8")
    build = configure(tmp_path)
    assert (build / "baker-option.txt").read_text() == "ON"
