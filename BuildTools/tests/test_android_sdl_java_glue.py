"""The Android template carries SDL's Java glue; it must be the glue of the SDL the engine links."""

from __future__ import annotations

from pathlib import Path


ENGINE = Path(__file__).resolve().parents[2]
SDL_JAVA = ENGINE / "ThirdParty/SDL/android-project/app/src/main/java/org/libsdl/app"
TEMPLATE_JAVA = ENGINE / "BuildTools/android-project/app/src/main/java/org/libsdl/app"
# The one local edit: class-level suppressions that keep the Gradle build free of deprecation warnings
LOCAL_ANNOTATION = '@SuppressWarnings("deprecation")'


def java_lines(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8").replace("\r\n", "\n")
    return [line for line in text.split("\n") if line.strip() != LOCAL_ANNOTATION]


def test_template_sdl_java_glue_matches_the_linked_sdl() -> None:
    # The native library registers its JNI methods by exact signature; a stale Java copy aborts the app inside
    # System.loadLibrary, which is how the SDL update of 2026-04-28 broke every Android client at startup
    sdl_files = sorted(path.name for path in SDL_JAVA.glob("*.java"))
    assert sdl_files, SDL_JAVA
    assert sorted(path.name for path in TEMPLATE_JAVA.glob("*.java")) == sdl_files

    stale = [name for name in sdl_files if java_lines(TEMPLATE_JAVA / name) != java_lines(SDL_JAVA / name)]
    assert not stale, f"Copy these from {SDL_JAVA} and re-add the class-level {LOCAL_ANNOTATION}: {stale}"
