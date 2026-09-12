from __future__ import annotations

from pathlib import Path
import sys
from types import SimpleNamespace

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import package as _package  # noqa: E402


def make_packager(tmp_path: Path, arch: str, binary_output_postfix: str = "") -> _package.Packager:
    packager = _package.Packager.__new__(_package.Packager)
    packager.args = SimpleNamespace(
        output=str(tmp_path),
        devname="LF",
        target="Client",
        config="PublicGame",
        platform="Windows",
        arch=arch,
        binary_output_postfix=binary_output_postfix,
    )
    packager.output_path = str(tmp_path)
    packager.pack_args = set()

    return packager


def test_win32_win7_resolves_arch_and_uses_explicit_postfix(tmp_path: Path) -> None:
    packager = make_packager(tmp_path, "win32-win7", "Win7")

    assert packager.build_target_output_path() == str(tmp_path / "LF-Client-PublicGame-Win7")
    assert packager.build_binary_entry("win32-win7", _package.BinaryVariant()) == "Client-Windows-win32-Win7"


def test_win64_win7_resolves_arch_and_uses_explicit_postfix(tmp_path: Path) -> None:
    packager = make_packager(tmp_path, "win64-win7", "Win7")

    assert packager.build_target_output_path() == str(tmp_path / "LF-Client-PublicGame-Win7")
    assert packager.build_binary_entry("win64-win7", _package.BinaryVariant()) == "Client-Windows-win64-Win7"


@pytest.mark.parametrize(
    ("platform", "binary_arch"),
    [("win32", "win32"), ("win32-clang", "win32"), ("win64", "win64"), ("win64-clang", "win64")],
)
def test_windows_build_platform_uses_canonical_binary_arch(tmp_path: Path, platform: str, binary_arch: str) -> None:
    packager = make_packager(tmp_path, platform)

    assert packager.build_binary_entry(platform, _package.BinaryVariant()) == f"Client-Windows-{binary_arch}"


def test_windows7_arch_does_not_imply_postfix(tmp_path: Path) -> None:
    packager = make_packager(tmp_path, "win32-win7")

    assert packager.build_target_output_path() == str(tmp_path / "LF-Client-PublicGame")
    assert packager.build_binary_entry("win32-win7", _package.BinaryVariant()) == "Client-Windows-win32"


def test_postfix_matching_config_does_not_duplicate_output_name(tmp_path: Path) -> None:
    packager = make_packager(tmp_path, "win64", "Steam")
    packager.args.config = "Steam"

    assert packager.build_target_output_path() == str(tmp_path / "LF-Client-Steam")


def make_windows_client_package_fixture(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, *, include_headless: bool
) -> tuple[_package.Packager, Path, Path]:
    bin_path = tmp_path / "Binaries" / "Client-Windows-win64"
    output_path = tmp_path / "output" / "LF-Client-PublicGame"
    bin_path.mkdir(parents=True)
    output_path.mkdir(parents=True)

    for name in (
        "LF_Client.exe",
        "LF_Client.dll",
        "LF_ClientLib.dll",
        "LF_ClientHeadless.exe",
        "LF_ClientHeadless.dll",
        "LF_ClientLibHeadless.dll",
        "SDL3.dll",
    ):
        (bin_path / name).write_bytes(name.encode())

    packager = make_packager(tmp_path, "win64")
    packager.args.nicename = "LastFrontier"
    packager.pack_args = {"Raw"}
    if include_headless:
        packager.pack_args.add("Headless")
    packager.target_output_path = str(output_path)
    packager.resolve_binary_input_dir = lambda arch, variant, bin_name: str(bin_path)
    packager.patch_packaged_binary = lambda *args, **kwargs: None
    packager.copy_runtime_pdb = lambda *args, **kwargs: None
    packager.copy_pdb = lambda *args, **kwargs: None
    monkeypatch.setattr(_package, "patch_pe_pdb_path", lambda *args, **kwargs: True)

    return packager, bin_path, output_path


def test_windows_client_package_excludes_unrequested_headless_runtimes(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    packager, bin_path, output_path = make_windows_client_package_fixture(tmp_path, monkeypatch, include_headless=False)

    packager.package_windows()

    assert (output_path / "LastFrontier.exe").is_file()
    assert (output_path / "LastFrontier.dll").is_file()
    assert (output_path / "SDL3.dll").is_file()
    assert not (output_path / "LF_ClientHeadless.dll").exists()
    assert not (output_path / "LF_ClientLibHeadless.dll").exists()
    assert (bin_path / "LF_ClientHeadless.dll").is_file()
    assert (bin_path / "LF_ClientLibHeadless.dll").is_file()


def test_windows_headless_package_keeps_only_explicitly_packaged_runtime_names(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    packager, bin_path, output_path = make_windows_client_package_fixture(tmp_path, monkeypatch, include_headless=True)

    packager.package_windows()

    assert (output_path / "LastFrontier.exe").is_file()
    assert (output_path / "LastFrontier.dll").is_file()
    assert (output_path / "LastFrontier_Headless.exe").is_file()
    assert (output_path / "LastFrontier_Headless.dll").is_file()
    assert (output_path / "SDL3.dll").is_file()
    assert not (output_path / "LF_Client.dll").exists()
    assert not (output_path / "LF_ClientLib.dll").exists()
    assert not (output_path / "LF_ClientHeadless.dll").exists()
    assert not (output_path / "LF_ClientLibHeadless.dll").exists()
    assert (bin_path / "LF_ClientHeadless.dll").is_file()
    assert (bin_path / "LF_ClientLibHeadless.dll").is_file()


def make_server_expectation_packager(tmp_path: Path, expectations: list[str]) -> _package.Packager:
    packager = _package.Packager.__new__(_package.Packager)
    packager.args = SimpleNamespace(
        output=str(tmp_path),
        devname="LF",
        nicename="LastFrontier",
        target="Server",
        config="PublicGame",
        platform="Linux",
        arch="x64",
        binary_output_postfix="",
        buildhash="deadbeef",
        expect_client_runtime=expectations,
    )
    packager.output_path = str(tmp_path)
    packager.pack_args = set()

    return packager


def test_declared_win7_client_runtime_must_reach_the_server_package(tmp_path: Path) -> None:
    packager = make_server_expectation_packager(tmp_path, ["Windows:win32-win7:Win7", "Windows:win64:"])

    # The Win7 client is a postfix variant sharing the Windows-win32 update target, and its payload
    # is the only file a Win7 client will accept
    staged = {("Windows-win32", "LastFrontier_Win7"), ("Windows-win64", "LastFrontier")}
    packager.verify_expected_client_runtime_payloads(staged, set(), None, [])


def test_payload_for_another_arch_does_not_satisfy_expectation(tmp_path: Path) -> None:
    packager = make_server_expectation_packager(tmp_path, ["Windows:win64:"])

    with pytest.raises(AssertionError) as failure:
        packager.verify_expected_client_runtime_payloads({("Windows-win32", "LastFrontier")}, set(), None, [])

    message = str(failure.value)
    assert "LastFrontier" in message
    assert "PlatformBinaries/Windows-win64" in message
    assert "Windows-win32/LastFrontier" in message


def test_missing_win7_client_runtime_fails_the_server_package(tmp_path: Path) -> None:
    packager = make_server_expectation_packager(tmp_path, ["Windows:win32-win7:Win7"])

    # Exactly the shape that shipped a server distributing no Win7 payload: the plain win32 build is
    # present, so the target directory exists, yet no file carries the Win7 client's build name
    staged = {("Windows-win32", "LastFrontier")}

    with pytest.raises(AssertionError) as failure:
        packager.verify_expected_client_runtime_payloads(
            staged, set(), None, ["Client-Windows-win32-Win7: built from cafe, package is deadbeef"])

    message = str(failure.value)
    assert "LastFrontier_Win7" in message
    assert "PlatformBinaries/Windows-win32" in message
    assert "built from cafe" in message


def test_all_platforms_require_managed_resources_but_only_native_self_updaters_require_modules(tmp_path: Path) -> None:
    # Android/iOS/Web do not fetch native modules, but they still need a target-specific Scripts pack.
    packager = make_server_expectation_packager(
        tmp_path, ["Android:arm64:", "iOS:arm64:", "Web:wasm:", "macOS:x64:"]
    )

    managed_resources = {
        ("Android-arm64", "Scripts"),
        ("iOS-arm64", "Scripts"),
        ("Web-wasm", "Scripts"),
        ("macOS-x64", "Scripts"),
    }
    packager.verify_expected_client_runtime_payloads(
        {("macOS-x64", "LastFrontier")}, managed_resources, "Scripts", [])


def test_missing_macos_client_runtime_fails_the_server_package(tmp_path: Path) -> None:
    packager = make_server_expectation_packager(tmp_path, ["macOS:x64:"])

    with pytest.raises(AssertionError) as failure:
        packager.verify_expected_client_runtime_payloads(set(), set(), None, [])

    assert "macOS-x64" in str(failure.value)


def test_missing_web_managed_resource_payload_fails_the_server_package(tmp_path: Path) -> None:
    packager = make_server_expectation_packager(tmp_path, ["Web:wasm:"])

    with pytest.raises(AssertionError) as failure:
        packager.verify_expected_client_runtime_payloads(set(), set(), "Scripts", [])

    message = str(failure.value)
    assert "Scripts.zip" in message
    assert "PlatformBinaries/Web-wasm" in message
    assert "another platform" in message
