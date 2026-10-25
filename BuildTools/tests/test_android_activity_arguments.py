"""Exercise the generated Activity's actual arguments at the packaged native boundary."""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
import package as package_tool


@pytest.mark.parametrize("server_host", [None, "", "   ", " 127.0.0.1 "])
def test_packaged_activity_passes_device_paths_and_typed_host_without_subconfig(tmp_path: Path, server_host: str | None) -> None:
    compiler = shutil.which("javac")
    java = shutil.which("java")

    if compiler is None or java is None:
        pytest.skip("A Java compiler and runtime are required to execute the Activity argument probe")

    package_root = tmp_path / "package"
    template = package_root / "app/src/main/java-template"
    template.mkdir(parents=True)
    shutil.copy(BUILDTOOLS / "android-project/app/src/main/java-template/FOnlineActivity.java", template)
    packager = package_tool.Packager.__new__(package_tool.Packager)
    packager.target_output_path = str(package_root)
    packager.client_res_dir = "Content/Resource Packs"
    packager.args = SimpleNamespace(config="ResolvedVariant")
    packager.patch_android_activity("com.fonline.activityprobe")
    activity = package_root / "app/src/main/java/com/fonline/activityprobe/FOnlineActivity.java"
    stub = tmp_path / "SDLActivity.java"
    stub.write_text(
        """package org.libsdl.app;
import java.io.File;
public class SDLActivity {
    protected String[] getLibraries() { return new String[0]; }
    protected String[] getArguments() { return new String[0]; }
    public File getFilesDir() { return new File(System.getProperty("fixture.root")); }
    public AppInfo getApplicationInfo() { return new AppInfo(); }
    public Intent getIntent() { return new Intent(); }
    public static class AppInfo { public String sourceDir = "/fixture/client.apk"; }
    public static class Intent {
        public String getStringExtra(String name) {
            return name.equals("server_host") ? System.getProperty("fixture.serverHost") : null;
        }
    }
}
""",
        encoding="utf-8",
    )
    probe = tmp_path / "Probe.java"
    probe.write_text(
        """package com.fonline.activityprobe;
public final class Probe {
    public static void main(String[] ignored) {
        for (String value : new FOnlineActivity().getArguments()) {
            System.out.println(value);
        }
    }
}
""",
        encoding="utf-8",
    )
    classes = tmp_path / "classes"
    subprocess.run([compiler, "-encoding", "UTF-8", "-d", str(classes), str(stub), str(activity), str(probe)], check=True, capture_output=True)
    writable = tmp_path / "private data"
    command = [java, "-cp", str(classes), "-Dfixture.root=" + str(writable)]

    if server_host is not None:
        command.append("-Dfixture.serverHost=" + server_host)

    command.append("com.fonline.activityprobe.Probe")
    result = subprocess.run(command, check=True, capture_output=True, text=True, encoding="utf-8")
    arguments = result.stdout.splitlines()

    # ApplicationInit.cpp rejects this option in packaged builds, whose selected config is already embedded.
    assert "--ApplySubConfig" not in arguments
    assert len(arguments) % 2 == 0
    expected = {
        "--Common.UserWritablePath": str(writable),
        "--Baking.ClientResources": "/fixture/client.apk!/assets/Content/Resource Packs",
        "--Baking.CacheResources": str(writable / "Cache"),
    }

    if server_host is not None and server_host.strip():
        expected["--ClientNetwork.ServerHost"] = server_host.strip()

    assert dict(zip(arguments[::2], arguments[1::2])) == expected
    assert len(arguments) == len(expected) * 2
    assert (writable / "Cache").is_dir()
