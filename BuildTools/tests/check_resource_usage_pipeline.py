"""Exercise the real Baker, usage selection, archive writer and native archive reader."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
import package


FORCED = "Data/Forced asset.json"
FONT_PAGE = "Fonts/Atlas.tga"
ROOT_IMAGE = "Props/Visible.fofrm"
EXPECTED = {ROOT_IMAGE, "Fonts/Test.fofnt", FONT_PAGE, FORCED, "SpriteInfo/Art.foinfo"}
TGA_HEADER = bytes([0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 32, 8])


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def write(path: Path, data: str | bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data if isinstance(data, bytes) else data.encode("utf-8"))


def bake(baker: Path, config: Path, work: Path, label: str) -> None:
    result = subprocess.run(
        [str(baker), "-ApplyConfig", str(config), "-ApplySubConfig", "NONE"],
        cwd=work, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=120,
    )
    output = result.stdout + result.stderr
    write(work / f"{label}.log", output)
    require(result.returncode == 0, f"{label} failed ({result.returncode}):\n{output[-12000:]}")


def create_fixture(work: Path) -> tuple[Path, Path, Path]:
    art = work / "inputs/art"
    definitions = work / "inputs/definitions"
    baked = work / "baked"
    red_pixel = TGA_HEADER + bytes([0, 0, 255, 255])
    write(art / "Props/Plate source.tga", red_pixel)
    write(art / ROOT_IMAGE, "Frame = Plate source.tga\nSize = 2 3\nOffset = 4 -5\nMotion = 1 -2\n")
    write(art / FONT_PAGE, red_pixel)
    write(art / "Fonts/Test.fofnt", "Image Atlas.tga*\n")
    write(art / FORCED, '{"forced":true}\n')
    write(art / "Data/Unused.json", '{"unused":true}\n')
    write(definitions / "Definitions.cs", "///@ FixedType Common ResourceSet\n"
          "///@ Property ResourceSet Common hstring[] Files Resource\n")
    write(definitions / "Resources.fopro", "[ProtoItem]\n$Name = Visible\n"
          f"PicMap = {ROOT_IMAGE}\n[ResourceSet]\n$Name = Forced\n"
          f'Files = Fonts/Test.fofnt "{FORCED}"\n')

    # Pack paths use space-separated tokens, so keep them relative to the config.
    # Metadata.fometa-* is the native metadata registration entry point
    config = work / "Producer.fomain"
    write(config, f"Baking.BakeOutput = {baked.as_posix()}\n"
          "Baking.SingleThreadBaking = True\n"
          "Baking.RawCopyFileExtensions = json fofnt foinfo fofrm tga\n"
          "[ResourcePack]\nName = Metadata\nInputDirs = inputs/definitions\nBakers = Metadata Proto\n"
          "[ResourcePack]\nName = Art\nInputDirs = inputs/art\nBakers = Image RawCopy\n")
    return config, art, baked


def make_packager(baked: Path) -> package.Packager:
    consumer = package.Packager.__new__(package.Packager)
    consumer.baking_path = str(baked)
    consumer.args = SimpleNamespace(buildhash=(baked / "Resources.build-hash").read_text().strip())
    consumer.resource_pack_compress_level = 6
    consumer.resource_pack_min_compress_gain = 5
    consumer.resource_pack_hash_library = None
    consumer.resource_pack_hasher = None
    return consumer


def check_selection(consumer: package.Packager, baked: Path, expected: set[str]) -> list[str]:
    # The expected paths are fixture constants, not values copied out of the manifest
    manifest = json.loads((baked / "Resources.usage.json").read_text(encoding="utf-8"))
    actual = set(manifest["Packs"]["Art"])
    require(actual == expected, f"Producer selected {sorted(actual)}, expected {sorted(expected)}")
    require(manifest["BuildHash"] == consumer.args.buildhash, "Producer build identity differs")
    config_hash = (baked / "Resources.baking-config-hash").read_text().strip()
    require(manifest["BakingConfigHash"] == config_hash, "Producer configuration identity differs")

    files = consumer.collect_resource_files("Art", "Client")
    selected = {Path(path).relative_to(baked / "Art").as_posix() for path in files}
    require(selected == expected, f"Consumer selected {sorted(selected)}, expected {sorted(expected)}")
    require((baked / "Art/Data/Unused.json").is_file(), "Full bake discarded an unused output")
    return files


def check_native_readback(baker: Path, work: Path, baked: Path, archive: Path,
                          expected: set[str], label: str) -> None:
    output = work / label
    config = work / "Reader.fomain"
    write(config, f"Baking.BakeOutput = {output.as_posix()}\n"
          "Baking.SingleThreadBaking = True\n"
          "Baking.RawCopyFileExtensions = json fofnt foinfo fofrm tga\n"
          f"[ResourcePack]\nName = Readback\nInputFiles = {archive.name}\nBakers = RawCopy\n")
    bake(baker, config, work, label)
    actual = {path.relative_to(output / "Readback").as_posix(): path.read_bytes()
              for path in (output / "Readback").rglob("*") if path.is_file()}
    require(set(actual) == expected, f"Native reader returned {sorted(actual)}, expected {sorted(expected)}")

    for name, data in actual.items():
        require(data == (baked / "Art" / name).read_bytes(), f"Native readback changed {name}")


def check_negative_controls(consumer: package.Packager, work: Path, baked: Path,
                            files: list[str], expected: set[str]) -> None:
    art = baked / "Art"
    wrong_selections = (
        sorted(str(path) for path in art.rglob("*") if path.is_file()),
        [path for path in files if Path(path).relative_to(art).as_posix() != FONT_PAGE],
    )

    for index, wrong_files in enumerate(wrong_selections):
        archive = work / f"WrongSelection{index}.fores"
        consumer.write_resource_pack_files(str(archive), str(art), wrong_files)

        try:
            package.validate_resource_pack(archive, sorted(expected))
        except AssertionError:
            continue

        raise AssertionError(f"Package oracle accepted wrong selection {index}")


def run_pipeline(baker: Path, work: Path) -> None:
    config, art, baked = create_fixture(work)
    bake(baker, config, work, "producer")
    consumer = make_packager(baked)
    files = check_selection(consumer, baked, EXPECTED)
    archive = work / "Selected.fores"
    consumer.write_resource_pack_files(str(archive), str(baked / "Art"), files)
    package.validate_resource_pack(archive, sorted(EXPECTED))
    check_native_readback(baker, work, baked, archive, EXPECTED, "readback")

    previous_root = (baked / "Art" / ROOT_IMAGE).read_bytes()
    registry = work / "inputs/definitions/Resources.fopro"
    write(registry, registry.read_text(encoding="utf-8").replace(f' "{FORCED}"', ""))
    plate = art / "Props/Plate source.tga"
    source_stat = plate.stat()
    write(plate, TGA_HEADER + bytes([0, 255, 0, 255]))
    os.utime(plate, ns=(source_stat.st_atime_ns, source_stat.st_mtime_ns))
    bake(baker, config, work, "producer-incremental")

    expected = EXPECTED - {FORCED}
    require((baked / "Art" / ROOT_IMAGE).read_bytes() != previous_root, "Same-stat source change was missed")
    require((baked / "Art" / FORCED).is_file(), "Full bake discarded a former explicit inclusion")
    files = check_selection(consumer, baked, expected)
    consumer.write_resource_pack_files(str(archive), str(baked / "Art"), files)
    package.validate_resource_pack(archive, sorted(expected))
    check_native_readback(baker, work, baked, archive, expected, "readback-incremental")
    check_negative_controls(consumer, work, baked, files, expected)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baker", type=Path, required=True, help="real native Baker executable")
    parser.add_argument("--workdir", type=Path, help="parent directory for temporary fixture files")
    args = parser.parse_args()
    baker = args.baker.resolve()

    if not baker.is_file():
        parser.error(f"Baker executable does not exist: {baker}")

    if args.workdir:
        args.workdir.mkdir(parents=True, exist_ok=True)

    # External archive caches are unrelated to this isolated producer/consumer contract
    os.environ.pop(package.RESOURCE_ARCHIVE_CACHE_HELPER_ENV, None)

    with tempfile.TemporaryDirectory(prefix="resource usage pipeline-", dir=args.workdir) as directory:
        run_pipeline(baker, Path(directory))

    print("Resource usage pipeline passed: full/incremental bake, exact selection, native readback, 2 negative controls")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
