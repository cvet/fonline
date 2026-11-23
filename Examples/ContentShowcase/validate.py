#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import platform
import subprocess
import sys
from pathlib import Path


def run(command: list[str], root: Path, env: dict[str, str] | None = None) -> int:
    print(f"[content-showcase] {' '.join(command)}")
    return subprocess.run(command, cwd=root, env=env, check=False).returncode


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate the FOnline Content Showcase.")
    web_mode = parser.add_mutually_exclusive_group()
    web_mode.add_argument("--web", action="store_true", help="Build the WebAssembly/WebGL compile lane.")
    web_mode.add_argument(
        "--web-package",
        action="store_true",
        help="Native-bake and build the complete Web delivery package.",
    )
    web_mode.add_argument(
        "--web-runtime",
        action="store_true",
        help="Build the Web package and exercise it in a real Chromium WebGL 2 session.",
    )
    parser.add_argument(
        "--playwright-root",
        type=Path,
        help="Directory containing the installed Playwright package (defaults to WebTests).",
    )
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parent

    if args.web or args.web_package or args.web_runtime:
        emsdk = os.environ.get("FO_EMSDK")
        if not emsdk:
            print("FO_EMSDK is required for Web validation", file=sys.stderr)
            return 2
        env = os.environ.copy()
        env["FO_EMSDK"] = str(Path(emsdk).resolve())
        if args.web_package or args.web_runtime:
            system = platform.system()
            if system == "Windows":
                host_preset = "web-package-host-windows"
            elif system == "Linux":
                host_preset = "web-package-host-linux"
            else:
                print(f"unsupported Web package host: {system}", file=sys.stderr)
                return 2
            commands = (
                ["cmake", "--preset", host_preset],
                ["cmake", "--build", "--preset", f"{host_preset}-bake"],
                ["cmake", "--preset", "web-package"],
                ["cmake", "--build", "--preset", "web-package-check"],
            )
        else:
            commands = (["cmake", "--preset", "web"], ["cmake", "--build", "--preset", "web-check"])
    else:
        system = platform.system()
        if system == "Windows":
            preset = "windows"
        elif system == "Linux":
            preset = "linux"
        else:
            print(f"unsupported showcase host: {system}", file=sys.stderr)
            return 2
        env = None
        commands = (["cmake", "--preset", preset], ["cmake", "--build", "--preset", f"{preset}-check"])

    for command in commands:
        result = run(command, root, env)
        if result != 0:
            return result

    if args.web_runtime:
        playwright_root = (args.playwright_root or root / "WebTests").resolve()
        if not (playwright_root / "node_modules/playwright/package.json").is_file():
            print(
                f"Playwright is not installed in {playwright_root}; run npm ci in that directory",
                file=sys.stderr,
            )
            return 2
        output_root = root / "Build/web-package-output"
        server = output_root / "Binaries"
        if system == "Windows":
            server /= "Server-Windows-win64/FOCS_ServerHeadless.exe"
        else:
            server /= "Server-Linux-x64/FOCS_ServerHeadless"
        package_dir = output_root / "FOCS-ShowcaseWeb/FOCS-Client-ShowcaseRelease-Web"
        result = run(
            [
                sys.executable,
                "capture_showcase_web.py",
                "--server",
                str(server),
                "--package-dir",
                str(package_dir),
                "--config",
                str(root / "FOnlineContentShowcase.fomain"),
                "--playwright-root",
                str(playwright_root),
            ],
            root,
            env,
        )
        if result != 0:
            return result
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
