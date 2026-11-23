from pathlib import Path
import re


ENGINE_ROOT = Path(__file__).resolve().parents[2]
TARGETS = ("windows", "linux", "browser", "android", "osx", "ios", "iossimulator")
CORE_ARCHIVES = (
    "monosgen-2.0", "mono-component-debugger-stub-static",
    "mono-component-diagnostics_tracing-stub-static", "mono-component-hot_reload-stub-static",
    "mono-component-marshal-ilgen-stub-static", "minipal",
)


def published_runtime_files(target: str) -> tuple[str, ...]:
    headers = {
        f"include/mono-2.0/{name}"
        for source in ("ManagedScriptBackend.cpp", "ManagedPInvokeTable.cpp")
        for name in re.findall(r"^#include <(mono/[^>]+)>",
                               (ENGINE_ROOT / "Source/Scripting/Managed" / source).read_text(encoding="utf-8"), re.MULTILINE)
    }
    archives = list(CORE_ARCHIVES)
    if target != "windows":
        archives.append("System.Native")
        if target != "browser":
            archives.append("System.Globalization.Native")
        if target == "linux":
            archives.append("System.Security.Cryptography.Native.OpenSsl")
        if target == "browser":
            archives.extend(("mono-ee-interp", "mono-icall-table", "mono-wasm-eh-js", "mono-wasm-simd"))
    prefix, suffix = ("", ".lib") if target == "windows" else ("lib", ".a")
    files = sorted(headers) + [f"lib/{prefix}{name}{suffix}" for name in archives]
    files.extend(("lib/netcoreapp/System.Private.CoreLib.dll", "lib/netcoreapp/System.Runtime.dll"))
    if target == "browser":
        files.append("lib/es6/dotnet.es6.lib.js")
    return tuple(files)


def write_published_runtime(tree: Path, target: str, content: str = "runtime") -> None:
    for relative in published_runtime_files(target):
        path = tree / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
