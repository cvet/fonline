#!/usr/bin/python3

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path


TARGETS = ('Server', 'Client', 'Mapper')
CLIENT_AOT_PLATFORMS = ('x86', 'amd64', 'arm', 'arm64', 'wasm')
MAPPER_AOT_PLATFORMS = ('x86', 'amd64', 'wasm')


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description='FOnline scripts generation')
    parser.add_argument('-scripts', dest='scripts', required=True, help='path to scripts directory')
    parser.add_argument('-assembly', dest='assembly', action='append', default=[], help='assembly name')
    return parser


def compile_assembly(msbuild_path: str, scripts_path: Path, target: str, name: str) -> None:
    subprocess.check_call([
        msbuild_path,
        '-noLogo',
        '-m',
        '-target:Build',
        '-p:Configuration=Release',
        '-p:OutputPath=' + target + 'Assemblies',
        name + '.' + target + '.csproj',
    ], cwd=scripts_path)


def link_assembly(target: str, name: str) -> None:
    _ = target, name


def aot_assembly(target: str, name: str, platform: str) -> None:
    _ = target, name, platform


def main() -> None:
    args = create_parser().parse_args()
    msbuild_path = 'msbuild'
    scripts_path = Path(args.scripts).resolve()

    for target in TARGETS:
        for assembly in args.assembly:
            compile_assembly(msbuild_path, scripts_path, target, assembly)

    for target in TARGETS:
        for assembly in args.assembly:
            link_assembly(target, assembly)

    for platform in CLIENT_AOT_PLATFORMS:
        for assembly in args.assembly:
            aot_assembly('Client', assembly, platform)

    for platform in MAPPER_AOT_PLATFORMS:
        for assembly in args.assembly:
            aot_assembly('Mapper', assembly, platform)


if __name__ == '__main__':
    main()
