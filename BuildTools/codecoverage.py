#!/usr/bin/python3

from __future__ import annotations

import argparse
import contextlib
import html
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable
import xml.etree.ElementTree as ET


EXCLUDED_SOURCE_FRAGMENTS = (
    "/ThirdParty/",
    "/GeneratedSource/",
    "/Applications/",
)

SOURCE_FILE_EXTENSIONS = {
    ".c",
    ".cc",
    ".cpp",
    ".h",
    ".hpp",
    ".inl",
    ".ipp",
    ".m",
    ".mm",
}


@dataclass
class FileCoverage:
    path: Path
    hits_by_line: dict[int, int] = field(default_factory=dict)
    total_lines_override: int | None = None

    @property
    def total_lines(self) -> int:
        measured_lines = len(self.hits_by_line)
        if self.total_lines_override is None:
            return measured_lines
        return max(measured_lines, self.total_lines_override)

    @property
    def covered_lines(self) -> int:
        return sum(1 for hits in self.hits_by_line.values() if hits > 0)

    @property
    def uncovered_lines(self) -> int:
        return self.total_lines - self.covered_lines

    @property
    def line_rate(self) -> float:
        if self.total_lines == 0:
            return 0.0
        return self.covered_lines / self.total_lines


def log(*parts: object) -> None:
    print("[CodeCoverage]", *parts, flush=True)


def run(command: list[str], *, cwd: Path | None = None, env: dict[str, str] | None = None) -> None:
    log("Run:", " ".join(shell_quote(part) for part in command))
    subprocess.run(command, cwd=cwd, env=env, check=True)


def shell_quote(value: str) -> str:
    if re.fullmatch(r"[A-Za-z0-9_./:=+-]+", value):
        return value
    return '"' + value.replace('"', '\\"') + '"'


def normalize_path(path: str) -> str:
    return path.replace("\\", "/")


def repo_relative_path(path: Path, workspace_root: Path) -> str | None:
    resolved = path.resolve()
    try:
        return normalize_path(str(resolved.relative_to(workspace_root.resolve())))
    except ValueError:
        return None


def resolve_source_path(source_path: str, workspace_root: Path, source_roots: Iterable[Path] = ()) -> Path | None:
    source_candidate = Path(source_path)
    candidates: list[Path] = []

    if source_candidate.is_absolute():
        candidates.append(source_candidate)
    else:
        candidates.append(workspace_root / source_candidate)
        for source_root in source_roots:
            candidates.append(source_root / source_candidate)

    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()

    if source_candidate.is_absolute():
        return source_candidate.resolve()

    return (workspace_root / source_candidate).resolve()


def is_relevant_source(path: Path, workspace_root: Path) -> bool:
    relative_path = repo_relative_path(path, workspace_root)

    if relative_path is None:
        return False

    relative_path = "/" + relative_path

    if not relative_path.startswith("/Engine/Source/"):
        return False

    return all(fragment not in relative_path for fragment in EXCLUDED_SOURCE_FRAGMENTS)


def ensure_directory(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def iter_relevant_source_files(workspace_root: Path) -> Iterable[Path]:
    source_root = workspace_root / "Engine" / "Source"

    for path in sorted(source_root.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in SOURCE_FILE_EXTENSIONS:
            continue
        if is_relevant_source(path, workspace_root):
            yield path


def estimate_source_lines(path: Path) -> int:
    content = path.read_text(encoding="utf-8-sig", errors="ignore")
    total = 0
    in_block_comment = False

    for raw_line in content.splitlines():
        remaining = raw_line.strip()

        while remaining:
            if in_block_comment:
                block_end = remaining.find("*/")
                if block_end == -1:
                    remaining = ""
                    break
                remaining = remaining[block_end + 2 :].strip()
                in_block_comment = False
                continue

            if remaining.startswith("//"):
                remaining = ""
                break

            line_comment = remaining.find("//")
            block_start = remaining.find("/*")

            if line_comment != -1 and (block_start == -1 or line_comment < block_start):
                remaining = remaining[:line_comment].strip()
                break

            if block_start == -1:
                break

            block_end = remaining.find("*/", block_start + 2)
            if block_end == -1:
                remaining = remaining[:block_start].strip()
                in_block_comment = True
                break

            remaining = (remaining[:block_start] + " " + remaining[block_end + 2 :]).strip()

        if remaining:
            total += 1

    return total


def add_missing_source_files(coverage: dict[str, FileCoverage], workspace_root: Path) -> dict[str, FileCoverage]:
    for path in iter_relevant_source_files(workspace_root):
        relative_path = repo_relative_path(path, workspace_root)
        if relative_path is None or relative_path in coverage:
            continue

        coverage[relative_path] = FileCoverage(path=path, total_lines_override=estimate_source_lines(path))

    return coverage


def raw_directory(output_dir: Path) -> Path:
    return output_dir / "raw"


def reports_directory(output_dir: Path) -> Path:
    return output_dir / "report"


def remove_matching_files(root: Path, patterns: Iterable[str]) -> None:
    if not root.exists():
        return

    for pattern in patterns:
        for entry in root.rglob(pattern):
            if entry.is_file():
                entry.unlink()


def clean_outputs(args: argparse.Namespace) -> None:
    remove_matching_files(args.build_dir, ("*.gcda", "*.profraw", "*.profdata", "*.coverage"))

    if args.output_dir.exists():
        with contextlib.suppress(FileNotFoundError):
            shutil.rmtree(args.output_dir)

    ensure_directory(raw_directory(args.output_dir))
    ensure_directory(reports_directory(args.output_dir))


def find_required_tool(candidates: Iterable[str], description: str) -> str:
    for candidate in candidates:
        resolved = shutil.which(candidate)
        if resolved:
            return resolved

    raise SystemExit(f"Required tool not found for {description}")


def find_versioned_llvm_tool(tool_name: str) -> str:
    preferred_major: int | None = None
    clang_path = shutil.which("clang++") or shutil.which("clang")

    if clang_path:
        version_process = subprocess.run(
            [clang_path, "--version"],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        version_match = re.search(r"version\s+(\d+)", version_process.stdout)

        if version_match:
            preferred_major = int(version_match.group(1))

    tool_paths: list[Path] = []
    path_value = os.environ.get("PATH", "")

    for entry in path_value.split(os.pathsep):
        if not entry:
            continue

        directory = Path(entry)

        if not directory.is_dir():
            continue

        tool_paths.extend(sorted(directory.glob(f"{tool_name}-*")))

    versioned_candidates: list[tuple[tuple[int, ...], Path]] = []

    for candidate in tool_paths:
        suffix = candidate.name.removeprefix(f"{tool_name}-")

        if not suffix:
            continue

        parts = tuple(int(part) for part in re.findall(r"\d+", suffix))

        if not parts:
            continue

        versioned_candidates.append((parts, candidate))

    if preferred_major is not None:
        for parts, candidate in sorted(versioned_candidates, reverse=True):
            if parts[0] == preferred_major:
                return str(candidate)

    direct_match = shutil.which(tool_name)

    if direct_match:
        return direct_match

    if versioned_candidates:
        versioned_candidates.sort(reverse=True)
        return str(versioned_candidates[0][1])

    if clang_path:
        clang_directory = Path(clang_path).resolve().parent

        for candidate in sorted(clang_directory.glob(f"{tool_name}-*"), reverse=True):
            return str(candidate)

    raise SystemExit(f"Required tool not found for {tool_name}")


def find_microsoft_code_coverage_console() -> str:
    path_candidates = [
        "Microsoft.CodeCoverage.Console",
        "Microsoft.CodeCoverage.Console.exe",
    ]

    for candidate in path_candidates:
        resolved = shutil.which(candidate)
        if resolved:
            return resolved

    env_roots = [os.environ.get("VSINSTALLDIR"), os.environ.get("ProgramFiles"), os.environ.get("ProgramFiles(x86)"), os.environ.get("ProgramW6432")]

    search_roots = [Path(root) for root in env_roots if root]
    relative_parts = (
        Path("Common7/IDE/Extensions/Microsoft/CodeCoverage.Console/Microsoft.CodeCoverage.Console.exe"),
        Path("Microsoft Visual Studio/2022/Community/Common7/IDE/Extensions/Microsoft/CodeCoverage.Console/Microsoft.CodeCoverage.Console.exe"),
        Path("Microsoft Visual Studio/2022/Professional/Common7/IDE/Extensions/Microsoft/CodeCoverage.Console/Microsoft.CodeCoverage.Console.exe"),
        Path("Microsoft Visual Studio/2022/Enterprise/Common7/IDE/Extensions/Microsoft/CodeCoverage.Console/Microsoft.CodeCoverage.Console.exe"),
        Path("Microsoft Visual Studio/2022/BuildTools/Common7/IDE/Extensions/Microsoft/CodeCoverage.Console/Microsoft.CodeCoverage.Console.exe"),
    )

    for root in search_roots:
        for relative_part in relative_parts:
            candidate = root / relative_part
            if candidate.exists():
                return str(candidate)

    raise SystemExit("Microsoft.CodeCoverage.Console was not found. Run from a Visual Studio developer shell or add the tool to PATH.")


def find_dotnet_coverage() -> str:
    return find_required_tool(("dotnet-coverage", "dotnet-coverage.exe"), "dotnet-coverage conversion")


def generate_msvc_settings(path: Path, binary_dir: Path) -> None:
    settings = f'''<?xml version="1.0" encoding="utf-8"?>
<RunSettings>
  <DataCollectionRunSettings>
    <DataCollectors>
      <DataCollector friendlyName="Code Coverage" uri="datacollector://Microsoft/CodeCoverage/2.0" assemblyQualifiedName="Microsoft.VisualStudio.Coverage.DynamicCoverageDataCollector, Microsoft.VisualStudio.TraceCollector, Version=17.0.0.0, Culture=neutral, PublicKeyToken=b03f5f7f11d50a3a">
        <Configuration>
          <Format>coverage</Format>
          <IncludeTestAssembly>False</IncludeTestAssembly>
          <CodeCoverage>
            <ModulePaths>
              <Include>
                <ModulePath>.*\\.(dll|exe)$</ModulePath>
              </Include>
              <IncludeDirectories>
                <Directory Recursive="true">{xml_escape(str(binary_dir))}</Directory>
              </IncludeDirectories>
            </ModulePaths>
            <EnableStaticNativeInstrumentation>True</EnableStaticNativeInstrumentation>
            <EnableDynamicNativeInstrumentation>True</EnableDynamicNativeInstrumentation>
            <EnableStaticNativeInstrumentationRestore>True</EnableStaticNativeInstrumentationRestore>
            <CollectFromChildProcesses>False</CollectFromChildProcesses>
          </CodeCoverage>
        </Configuration>
      </DataCollector>
    </DataCollectors>
  </DataCollectionRunSettings>
</RunSettings>
'''
    path.write_text(settings, encoding="utf-8")


def xml_escape(value: str) -> str:
    return html.escape(value, quote=True)


def make_binary_command(args: argparse.Namespace) -> list[str]:
    command = [str(args.binary)]

    if not any(arg == "--order" or arg.startswith("--order=") for arg in args.binary_args):
        command.extend(("--order", "decl"))

    command.extend(args.binary_args)
    return command


def run_binary(args: argparse.Namespace) -> None:
    ensure_directory(raw_directory(args.output_dir))
    ensure_directory(reports_directory(args.output_dir))

    binary_command = make_binary_command(args)

    if args.backend == "gcc":
        remove_matching_files(args.build_dir, ("*.gcda",))
        run(binary_command, cwd=args.workspace_root)
        return

    if args.backend == "llvm":
        remove_matching_files(args.output_dir, ("*.profraw", "*.profdata"))
        env = os.environ.copy()
        env["LLVM_PROFILE_FILE"] = str(raw_directory(args.output_dir) / "lf-code-coverage-%p.profraw")
        run(binary_command, cwd=args.workspace_root, env=env)
        return

    if args.backend == "msvc":
        remove_matching_files(args.output_dir, ("*.coverage", "*.xml"))
        coverage_console = find_microsoft_code_coverage_console()
        settings_path = args.output_dir / "msvc-codecoverage.runsettings"
        coverage_path = raw_directory(args.output_dir) / "lf-code-coverage.coverage"
        generate_msvc_settings(settings_path, args.binary.parent)
        run([
            coverage_console,
            "collect",
            "--settings",
            str(settings_path),
            "--output",
            str(coverage_path),
            *binary_command,
        ], cwd=args.workspace_root)
        return

    raise SystemExit(f"Unsupported coverage backend: {args.backend}")


def parse_lcov(lcov_path: Path, workspace_root: Path) -> dict[str, FileCoverage]:
    results: dict[str, FileCoverage] = {}
    current_file: FileCoverage | None = None

    for raw_line in lcov_path.read_text(encoding="utf-8").splitlines():
        if raw_line.startswith("SF:"):
            resolved = resolve_source_path(raw_line[3:], workspace_root)

            if resolved is None or not is_relevant_source(resolved, workspace_root):
                current_file = None
                continue

            relative_path = repo_relative_path(resolved, workspace_root)
            assert relative_path is not None
            current_file = results.setdefault(relative_path, FileCoverage(path=resolved))
        elif raw_line.startswith("DA:") and current_file is not None:
            line_no_text, hits_text = raw_line[3:].split(",", 1)
            line_no = int(line_no_text)
            hits = int(hits_text.split(",", 1)[0])
            current_file.hits_by_line[line_no] = current_file.hits_by_line.get(line_no, 0) + hits
        elif raw_line == "end_of_record":
            current_file = None

    return results


def parse_cobertura(cobertura_path: Path, workspace_root: Path) -> dict[str, FileCoverage]:
    tree = ET.parse(cobertura_path)
    root = tree.getroot()
    source_roots = []

    for source in root.findall("./sources/source"):
        if source.text:
            source_roots.append(Path(source.text))

    results: dict[str, FileCoverage] = {}

    for class_element in root.findall(".//class"):
        filename = class_element.get("filename")

        if not filename:
            continue

        resolved = resolve_source_path(filename, workspace_root, source_roots)

        if resolved is None or not is_relevant_source(resolved, workspace_root):
            continue

        relative_path = repo_relative_path(resolved, workspace_root)

        if relative_path is None:
            continue

        file_coverage = results.setdefault(relative_path, FileCoverage(path=resolved))
        lines_element = class_element.find("lines")

        if lines_element is None:
            continue

        for line_element in lines_element.findall("line"):
            line_number_text = line_element.get("number")
            hits_text = line_element.get("hits")

            if not line_number_text or not hits_text:
                continue

            line_number = int(line_number_text)
            hits = int(hits_text)
            file_coverage.hits_by_line[line_number] = max(file_coverage.hits_by_line.get(line_number, 0), hits)

    return results


def collect_gcc_report(args: argparse.Namespace) -> dict[str, FileCoverage]:
    lcov = find_required_tool(("lcov",), "GCC coverage capture")
    raw_dir = raw_directory(args.output_dir)
    raw_info = raw_dir / "gcc-full.info"
    extracted_info = raw_dir / "gcc-engine.info"
    filtered_info = raw_dir / "coverage.lcov"

    run([lcov, "--capture", "--directory", str(args.build_dir), "--output-file", str(raw_info)], cwd=args.workspace_root)
    run([lcov, "--extract", str(raw_info), f"{args.workspace_root / 'Engine' / 'Source'}/*", "--output-file", str(extracted_info)], cwd=args.workspace_root)
    run([
        lcov,
        "--remove",
        str(extracted_info),
        "*/ThirdParty/*",
        "*/GeneratedSource/*",
        "--output-file",
        str(filtered_info),
    ], cwd=args.workspace_root)

    return parse_lcov(filtered_info, args.workspace_root)


def collect_llvm_report(args: argparse.Namespace) -> dict[str, FileCoverage]:
    llvm_profdata = find_versioned_llvm_tool("llvm-profdata")
    llvm_cov = find_versioned_llvm_tool("llvm-cov")
    raw_dir = raw_directory(args.output_dir)
    profraw_files = sorted(raw_dir.glob("*.profraw"))

    if not profraw_files:
        raise SystemExit("No .profraw files were produced. Run RunCodeCoverage first.")

    profdata_path = raw_dir / "coverage.profdata"
    lcov_path = raw_dir / "coverage.lcov"

    run([llvm_profdata, "merge", "-sparse", *[str(entry) for entry in profraw_files], "-o", str(profdata_path)], cwd=args.workspace_root)

    export_process = subprocess.run(
        [llvm_cov, "export", "-format=lcov", f"-instr-profile={profdata_path}", str(args.binary)],
        cwd=args.workspace_root,
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    )
    lcov_path.write_text(export_process.stdout, encoding="utf-8")

    return parse_lcov(lcov_path, args.workspace_root)


def collect_msvc_report(args: argparse.Namespace) -> dict[str, FileCoverage]:
    coverage_path = raw_directory(args.output_dir) / "lf-code-coverage.coverage"

    if not coverage_path.exists():
        raise SystemExit("No .coverage file was produced. Run RunCodeCoverage first.")

    dotnet_coverage = find_dotnet_coverage()
    cobertura_path = raw_directory(args.output_dir) / "coverage.cobertura.xml"
    run([dotnet_coverage, "merge", "-o", str(cobertura_path), "-f", "cobertura", str(coverage_path)], cwd=args.workspace_root)
    return parse_cobertura(cobertura_path, args.workspace_root)


def write_summary(summary_path: Path, coverage: dict[str, FileCoverage], untouched: list[str]) -> str:
    total_lines = sum(entry.total_lines for entry in coverage.values())
    covered_lines = sum(entry.covered_lines for entry in coverage.values())
    uncovered_lines = total_lines - covered_lines
    line_rate = 0.0 if total_lines == 0 else covered_lines / total_lines
    sorted_files = sorted(coverage.items(), key=lambda item: (item[1].line_rate, -item[1].uncovered_lines, item[0]))

    lines = [
        f"Total line coverage: {line_rate * 100:.2f}% ({covered_lines}/{total_lines})",
        f"Files analyzed: {len(coverage)}",
        f"Uncovered lines: {uncovered_lines}",
        "",
        "Files:",
        "  Coverage  Covered/Total  Uncovered  File",
    ]

    for relative_path, entry in sorted_files:
        lines.append(
            f"  {entry.line_rate * 100:7.2f}%  "
            f"{entry.covered_lines:5d}/{entry.total_lines:5d}  "
            f"{entry.uncovered_lines:9d}  "
            f"{relative_path}"
        )

    if untouched:
        lines.append("")
        lines.append(f"Source files not touched by coverage run ({len(untouched)}):")
        for path in untouched:
            lines.append(f"  {path}")

    summary_text = "\n".join(lines) + "\n"
    summary_path.write_text(summary_text, encoding="utf-8")
    return summary_text


def write_html_report(report_path: Path, coverage: dict[str, FileCoverage], untouched: list[str]) -> None:
    total_lines = sum(entry.total_lines for entry in coverage.values())
    covered_lines = sum(entry.covered_lines for entry in coverage.values())
    line_rate = 0.0 if total_lines == 0 else covered_lines / total_lines
    rows = []

    for relative_path, entry in sorted(coverage.items(), key=lambda item: (item[1].line_rate, -item[1].uncovered_lines, item[0])):
        rows.append(
            "<tr>"
            f"<td>{html.escape(relative_path)}</td>"
            f"<td>{entry.line_rate * 100:.2f}%</td>"
            f"<td>{entry.covered_lines}</td>"
            f"<td>{entry.total_lines}</td>"
            f"<td>{entry.uncovered_lines}</td>"
            "</tr>"
        )

    untouched_section = ""
    if untouched:
        untouched_rows = "\n".join(f"        <li>{html.escape(path)}</li>" for path in untouched)
        untouched_section = f'''
    <h2 style="margin-top:32px;font-size:20px;">Source files not touched by coverage run ({len(untouched)})</h2>
    <ul style="background:var(--panel);border:1px solid var(--border);border-radius:18px;padding:24px 24px 24px 40px;font-size:14px;">
{untouched_rows}
    </ul>'''

    report = f'''<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
    <title>Engine Coverage</title>
  <style>
    :root {{
      color-scheme: light;
      --bg: #f5f3ec;
      --panel: #fffdf8;
      --text: #1d1a16;
      --muted: #6a6258;
      --border: #d6cfc3;
      --accent: #805b36;
    }}
    body {{
      margin: 0;
      font-family: "Iosevka Etoile", "IBM Plex Sans", sans-serif;
      background: linear-gradient(180deg, #efe9de 0%, var(--bg) 100%);
      color: var(--text);
    }}
    main {{
      max-width: 1100px;
      margin: 0 auto;
      padding: 32px 24px 48px;
    }}
    .hero {{
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: 18px;
      padding: 24px;
      box-shadow: 0 18px 40px rgba(73, 52, 31, 0.08);
    }}
    .hero h1 {{
      margin: 0 0 8px;
      font-size: 32px;
    }}
    .hero p {{
      margin: 0;
      color: var(--muted);
    }}
    .stats {{
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
      gap: 16px;
      margin: 24px 0;
    }}
    .stat {{
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: 16px;
      padding: 16px;
    }}
    .stat strong {{
      display: block;
      font-size: 28px;
      margin-bottom: 6px;
      color: var(--accent);
    }}
    table {{
      width: 100%;
      border-collapse: collapse;
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: 18px;
      overflow: hidden;
    }}
    th, td {{
      padding: 12px 14px;
      border-bottom: 1px solid var(--border);
      text-align: left;
      font-size: 14px;
    }}
    th {{
      background: #f0e8db;
      position: sticky;
      top: 0;
    }}
    tr:last-child td {{
      border-bottom: 0;
    }}
  </style>
</head>
<body>
  <main>
    <section class="hero">
            <h1>Engine Coverage</h1>
      <p>Summary report generated by the internal coverage tool.</p>
    </section>
    <section class="stats">
      <div class="stat"><strong>{line_rate * 100:.2f}%</strong><span>Total line coverage</span></div>
      <div class="stat"><strong>{covered_lines}</strong><span>Covered lines</span></div>
      <div class="stat"><strong>{total_lines}</strong><span>Total instrumented lines</span></div>
      <div class="stat"><strong>{len(coverage)}</strong><span>Engine source files</span></div>
    </section>
    <table>
      <thead>
        <tr>
          <th>File</th>
          <th>Coverage</th>
          <th>Covered</th>
          <th>Total</th>
          <th>Uncovered</th>
        </tr>
      </thead>
      <tbody>
        {''.join(rows)}
      </tbody>
    </table>
    {untouched_section}
  </main>
</body>
</html>
'''
    report_path.write_text(report, encoding="utf-8")


def generate_report(args: argparse.Namespace) -> None:
    ensure_directory(raw_directory(args.output_dir))
    ensure_directory(reports_directory(args.output_dir))

    if args.backend == "gcc":
        coverage = collect_gcc_report(args)
    elif args.backend == "llvm":
        coverage = collect_llvm_report(args)
    elif args.backend == "msvc":
        coverage = collect_msvc_report(args)
    else:
        raise SystemExit(f"Unsupported coverage backend: {args.backend}")

    coverage = add_missing_source_files(coverage, args.workspace_root)

    if not coverage:
        raise SystemExit("Coverage data was collected, but no engine source files matched the current filters.")

    untouched = sorted(path for path, entry in coverage.items() if entry.total_lines_override is not None)
    for path in untouched:
        del coverage[path]

    report_dir = reports_directory(args.output_dir)
    summary_path = report_dir / "summary.txt"
    html_path = report_dir / "index.html"
    summary_text = write_summary(summary_path, coverage, untouched)
    write_html_report(html_path, coverage, untouched)
    print(summary_text, end="")
    log("HTML report:", html_path)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run and analyze engine code coverage")
    subparsers = parser.add_subparsers(dest="command", required=True)

    for command_name in ("clean", "run", "report", "full"):
        subparser = subparsers.add_parser(command_name)
        subparser.add_argument("--workspace-root", type=Path, required=True)
        subparser.add_argument("--build-dir", type=Path, required=True)
        subparser.add_argument("--binary", type=Path, required=True)
        subparser.add_argument("--backend", choices=("gcc", "llvm", "msvc"), required=True)
        subparser.add_argument("--output-dir", type=Path, required=True)
        subparser.add_argument("binary_args", nargs=argparse.REMAINDER)

    return parser.parse_args()


def main() -> int:
    args = parse_args()
    args.workspace_root = args.workspace_root.resolve()
    args.build_dir = args.build_dir.resolve()
    args.binary = args.binary.resolve()
    args.output_dir = args.output_dir.resolve()

    if args.binary_args and args.binary_args[0] == "--":
        args.binary_args = args.binary_args[1:]

    if args.command == "clean":
        clean_outputs(args)
    elif args.command == "run":
        run_binary(args)
    elif args.command == "report":
        generate_report(args)
    elif args.command == "full":
        clean_outputs(args)
        run_binary(args)
        generate_report(args)
    else:
        raise SystemExit(f"Unsupported command: {args.command}")

    return 0


if __name__ == "__main__":
    sys.exit(main())