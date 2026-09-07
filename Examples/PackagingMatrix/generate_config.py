from __future__ import annotations

import argparse
import ast
import re
import sys
from dataclasses import dataclass
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent


def resolve_engine_root() -> Path:
    embedded = PROJECT_ROOT / "Engine"
    return embedded if embedded.is_dir() else PROJECT_ROOT.parents[1]


ENGINE_ROOT = resolve_engine_root()
OUTPUT_PATH = PROJECT_ROOT / "FOnlinePackagingMatrix.fomain"

OVERRIDES = {
    "Common.GameName": "FOnline Packaging Matrix",
    "Common.GameVersion": "0.1.0",
    "Network.ServerPort": "4020",
    "Network.WebSocketPort": "4021",
    "Client.Language": "engl",
    "ClientNetwork.ServerHost": "127.0.0.1",
    "Server.DbStorage": "Memory",
    "Server.WorkerThreads": "2",
    "Server.EntityStartId": "10000000001",
    "Server.EntityIdReserveBatch": "1000",
    "ServerNetwork.DisableNetworking": "True",
    "Baking.ForceBaking": "False",
    "Baking.SingleThreadBaking": "False",
    "Baking.IgnoreMissingBakerWarning": "False",
    "Baking.RawCopyFileExtensions": "fofnt bmfc fnt acm ogg wav ogv json ini",
    "Baking.ProtoFileExtensions": "fopro fomap",
    "Baking.BakeLanguages": "engl",
    "Baking.BakeOutput": "Baking",
    "Baking.ServerResources": "ServerResources",
    "Baking.ClientResources": "Resources",
    "Baking.PlatformBinaries": "PlatformBinaries",
    "Baking.CacheResources": "Cache",
    "Baking.ZipCompressLevel": "1",
    "Render.HeadlessWindow": "True",
    "Render.NullRenderer": "True",
    "Audio.DisableAudio": "True",
}

RESOURCE_SECTIONS = """
[SubConfig]
Name = PackageSmoke
PackageSmoke.Automation = True
ServerNetwork.DisableNetworking = False

[ResourcePack]
Name = Metadata
InputDirs = Scripts
IncludePatterns = *
Bakers = Metadata

[ResourcePack]
Name = Configs
Bakers = Config
ServerOnly = True

[ResourcePack]
Name = Scripts
InputDirs = Scripts
IncludePatterns = PackageSmoke.fos
Bakers = AngelScript

[ResourcePack]
Name = Embedded
InputDirs = Engine/Resources/Embedded
IncludePatterns = **
Bakers = Image Effect RawCopy

[ResourcePack]
Name = Core
InputDirs = Engine/Resources/Core Engine/Resources/Embedded
IncludePatterns = **
Bakers = Image Effect RawCopy
""".lstrip()


@dataclass(frozen=True)
class Setting:
    value_type: str
    name: str
    defaults: tuple[str, ...]


def split_macro_arguments(body: str) -> list[str]:
    arguments: list[str] = []
    token: list[str] = []
    quoted = False
    escaped = False
    for character in body:
        if escaped:
            token.append(character)
            escaped = False
        elif quoted and character == "\\":
            token.append(character)
            escaped = True
        elif character == '"':
            token.append(character)
            quoted = not quoted
        elif character == "," and not quoted:
            arguments.append("".join(token).strip())
            token.clear()
        else:
            token.append(character)
    if quoted:
        raise ValueError(f"Unterminated quoted setting macro: {body}")
    arguments.append("".join(token).strip())
    return arguments


def parse_settings(path: Path) -> list[Setting]:
    settings: list[Setting] = []
    for source_line in path.read_text(encoding="utf-8-sig").splitlines():
        line = source_line.strip()
        if not line.startswith(("FIXED_SETTING(", "VARIABLE_SETTING(")):
            continue
        end = line.find(");")
        if end == -1:
            raise ValueError(f"Malformed setting macro: {source_line}")
        arguments = split_macro_arguments(line[line.find("(") + 1 : end])
        if len(arguments) < 3:
            raise ValueError(f"Malformed setting arguments: {source_line}")
        settings.append(
            Setting(
                value_type=arguments[0],
                name=f"{arguments[1]}.{arguments[2]}",
                defaults=tuple(arguments[3:]),
            )
        )
    return settings


def parse_runtime_managed_settings(path: Path) -> set[str]:
    return set(
        re.findall(
            r'_appliedSettings\.emplace\("([^"]+)"\)',
            path.read_text(encoding="utf-8-sig"),
        )
    )


def decode_string(value: str) -> str:
    if value.startswith('"') and value.endswith('"'):
        decoded = ast.literal_eval(value)
        if not isinstance(decoded, str):
            raise ValueError(f"Expected string literal, got {value}")
        return decoded
    return value


def serialize_default(setting: Setting) -> str:
    values = [decode_string(value) for value in setting.defaults]
    if setting.value_type.startswith("vector<"):
        return " ".join(values)
    if values:
        value = values[0]
        if setting.value_type == "bool":
            return "True" if value.lower() == "true" else "False"
        return value
    if setting.value_type == "bool":
        return "False"
    if setting.value_type in {"string"} or setting.value_type.startswith("vector<"):
        return ""
    return "0"


def render_config(engine_root: Path = ENGINE_ROOT) -> str:
    settings = parse_settings(engine_root / "Source/Common/Settings.inc")
    runtime_managed = parse_runtime_managed_settings(engine_root / "Source/Common/Settings.cpp")
    unknown_overrides = sorted(set(OVERRIDES) - {setting.name for setting in settings})
    if unknown_overrides:
        raise ValueError(f"Overrides reference unknown settings: {', '.join(unknown_overrides)}")

    lines = [
        "# Generated by generate_config.py from Engine/Source/Common/Settings.inc.",
        "# Run `python generate_config.py --check` after changing Engine settings.",
        "",
    ]
    last_group = ""
    for setting in settings:
        if setting.name in runtime_managed:
            continue
        group = setting.name.split(".", 1)[0]
        if group != last_group:
            if last_group:
                lines.append("")
            lines.append(f"# {group}")
            last_group = group
        value = OVERRIDES.get(setting.name, serialize_default(setting))
        lines.append(f"{setting.name} = {value}" if value else f"{setting.name} =")

    lines.extend(["", "# Fixture-owned script setting", "PackageSmoke.Automation = False", ""])
    lines.append(RESOURCE_SECTIONS.rstrip())
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate the packaging-matrix full runtime config.")
    parser.add_argument("--check", action="store_true", help="Fail if the checked-in output is stale.")
    args = parser.parse_args(argv)
    rendered = render_config()
    current = OUTPUT_PATH.read_text(encoding="utf-8-sig") if OUTPUT_PATH.exists() else None
    if args.check:
        if current != rendered:
            print(f"Stale generated config: {OUTPUT_PATH}", file=sys.stderr)
            return 1
        print(f"Packaging matrix config is current: {OUTPUT_PATH}")
        return 0
    OUTPUT_PATH.write_text(rendered, encoding="utf-8", newline="\n")
    print(f"Wrote {OUTPUT_PATH}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
