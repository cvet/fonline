from __future__ import annotations

import argparse
import json
import re
import sys
from collections.abc import Iterable
from pathlib import Path, PurePosixPath
from urllib.parse import unquote, urlsplit

import docs_api
import docs_api_diff
import docs_ai_control_protocol
import docs_ai_delivery
import docs_ai_eval
import docs_audio
import docs_cli
import docs_cmake
import docs_contract_diff
import docs_description_translations
import docs_diagrams
import docs_effect_format
import docs_examples
import docs_external_evidence
import docs_font_format
import docs_gui_runtime
import docs_helper_cli
import docs_image_format
import docs_inventory
import docs_localization
import docs_map_format
import docs_model_format
import docs_native_extension
import docs_package
import docs_particle_format
import docs_prototype_format
import docs_public_api
import docs_reference
import docs_screenshots
import docs_site
import docs_snippets
import docs_support_matrix
import docs_text_format
import docs_video


DEFAULT_MANIFEST = "Docs/documentation-manifest.json"
ALLOWED_DIATAXIS = {"none", "tutorial", "how-to", "reference", "explanation"}
ALLOWED_VISIBILITY = {"public", "internal"}
ALLOWED_TRANSLATION = {"required", "not-required"}
ALLOWED_STATES = {"current", "placeholder", "redirect", "historical", "active-plan"}
ALLOWED_DISPOSITIONS = {"retain", "route", "migrate", "replace", "move-meta"}
ALLOWED_AUDIENCES = {
    "game-developer",
    "content-author",
    "native-extension-developer",
    "engine-contributor",
    "tool-developer",
    "release-operator",
    "ai-agent",
}
ALLOWED_PAGES_SOURCE_STATES = {"pending-admin-verification", "verified"}
ALLOWED_PAGES_BUILD_TYPES = {"legacy", "workflow"}
ALLOWED_DNS_STATES = {"public-resolution-confirmed"}
ALLOWED_DNS_OWNERSHIP_STATES = {"not-observed", "verified"}
PAGES_BUILD_ACTION = "actions/jekyll-build-pages@v1"
SITE_ARTIFACT_ACTION = "actions/upload-artifact@v4"
PAGES_README_PERMALINKS = {
    "BuildTools/README.md": "/BuildTools/README.html",
    "Docs/README.md": "/Docs/README.html",
    "Examples/MinimalProject/README.md": "/Examples/MinimalProject/README.html",
    "Examples/MinimalMultiplayer/README.md": "/Examples/MinimalMultiplayer/README.html",
    "Examples/AiControlSample/README.md": "/Examples/AiControlSample/README.html",
    "Source/README.md": "/Source/README.html",
    "Source/Tests/README.md": "/Source/Tests/README.html",
}
PLACEHOLDER_ROUTE_MARKER = "> placeholder route."
LEGACY_ROUTE_MARKER = "> legacy route."
UNFINISHED_CONTENT_MARKERS = ("...generate content...", "...write about", "estimated finishing date")

INLINE_LINK_RE = re.compile(r"!?\[[^\]]*]\((?P<target>[^\r\n)]*)\)")
REFERENCE_LINK_RE = re.compile(r"^\s*\[[^\]]+]:\s*(?P<target>\S+)")
HTML_LINK_RE = re.compile(r"\b(?:href|src)=[\"'](?P<target>[^\"']+)[\"']", re.IGNORECASE)
FENCE_RE = re.compile(r"^\s*(`{3,}|~{3,})")
INLINE_CODE_RE = re.compile(r"(`+)(.+?)\1")
HEADING_RE = re.compile(r"^(?P<marks>#{1,6})\s+(?P<title>.+?)\s*#*\s*$")
EXPLICIT_ANCHOR_RE = re.compile(r"<(?:a|[^>]+)\s+(?:id|name)=[\"'](?P<anchor>[^\"']+)[\"']", re.IGNORECASE)


def _is_within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def _strip_inline_code(line: str) -> str:
    return INLINE_CODE_RE.sub(lambda match: " " * len(match.group(0)), line)


def _iter_prose_lines(text: str, *, strip_inline_code: bool = True) -> Iterable[tuple[int, str]]:
    active_fence: str | None = None

    for line_number, line in enumerate(text.splitlines(), start=1):
        fence = FENCE_RE.match(line)
        if fence:
            marker = fence.group(1)[0]
            if active_fence is None:
                active_fence = marker
            elif active_fence == marker:
                active_fence = None
            continue

        if active_fence is None:
            yield line_number, _strip_inline_code(line) if strip_inline_code else line


def _normalize_link_target(raw_target: str) -> str:
    target = raw_target.strip()
    if target.startswith("<") and ">" in target:
        return target[1 : target.index(">")]

    title_separator = re.search(r"\s+[\"']", target)
    if title_separator:
        target = target[: title_separator.start()]
    return target


def _iter_markdown_links(path: Path) -> Iterable[tuple[int, str]]:
    text = path.read_text(encoding="utf-8")
    for line_number, line in _iter_prose_lines(text):
        for pattern in (INLINE_LINK_RE, HTML_LINK_RE):
            for match in pattern.finditer(line):
                yield line_number, _normalize_link_target(match.group("target"))

        reference_match = REFERENCE_LINK_RE.match(line)
        if reference_match:
            yield line_number, _normalize_link_target(reference_match.group("target"))


def _heading_slug(title: str) -> str:
    title = re.sub(r"!?\[([^\]]+)]\([^)]*\)", r"\1", title)
    title = re.sub(r"<[^>]+>", "", title)
    title = title.replace("`", "").replace("*", "")
    title = title.strip().lower()
    title = re.sub(r"[^\w\- ]", "", title, flags=re.UNICODE)
    return re.sub(r"\s+", "-", title)


def _markdown_anchors(path: Path) -> set[str]:
    anchors: set[str] = set()
    slug_counts: dict[str, int] = {}
    text = path.read_text(encoding="utf-8")

    for _, line in _iter_prose_lines(text, strip_inline_code=False):
        for anchor_match in EXPLICIT_ANCHOR_RE.finditer(line):
            anchors.add(anchor_match.group("anchor"))

        heading_match = HEADING_RE.match(line)
        if not heading_match:
            continue

        base_slug = _heading_slug(heading_match.group("title"))
        duplicate_index = slug_counts.get(base_slug, 0)
        slug_counts[base_slug] = duplicate_index + 1
        anchors.add(base_slug if duplicate_index == 0 else f"{base_slug}-{duplicate_index}")

    return anchors


def _resolve_local_target(root: Path, document: Path, target: str) -> tuple[Path, str]:
    path_text, _, fragment = target.partition("#")
    path_text = unquote(path_text.split("?", maxsplit=1)[0])
    if path_text.startswith("/"):
        candidate = root / path_text.lstrip("/")
    elif path_text:
        candidate = document.parent / path_text
    else:
        candidate = document
    return candidate.resolve(), unquote(fragment)


def validate_links(root: Path, documents: Iterable[str]) -> list[str]:
    errors: list[str] = []
    anchor_cache: dict[Path, set[str]] = {}

    for document_path in sorted(documents):
        document = root / document_path
        for line_number, target in _iter_markdown_links(document):
            if not target or target.startswith("#"):
                candidate = document
                fragment = unquote(target.removeprefix("#"))
            else:
                parsed = urlsplit(target)
                if parsed.scheme or target.startswith("//"):
                    continue
                candidate, fragment = _resolve_local_target(root, document, target)

            location = f"{document_path}:{line_number}"
            if not _is_within(candidate, root):
                errors.append(f"{location}: local link escapes the engine root: {target}")
                continue
            if not candidate.exists():
                errors.append(f"{location}: local link target does not exist: {target}")
                continue

            if fragment and candidate.is_file() and candidate.suffix.lower() in {".md", ".markdown"}:
                anchors = anchor_cache.setdefault(candidate, _markdown_anchors(candidate))
                if fragment not in anchors:
                    errors.append(f"{location}: Markdown anchor does not exist: {target}")

    return errors


def _expand_inventory(root: Path, patterns: Iterable[str]) -> set[str]:
    paths: set[str] = set()
    for pattern in patterns:
        for match in root.glob(pattern):
            if match.is_file():
                paths.add(match.relative_to(root).as_posix())
    return paths


def _validate_relative_path(value: str, label: str, errors: list[str]) -> PurePosixPath | None:
    path = PurePosixPath(value)
    if path.is_absolute() or ".." in path.parts:
        errors.append(f"{label} must stay inside the engine root: {value}")
        return None
    return path


def _source_exists(root: Path, source: str) -> bool:
    if any(character in source for character in "*?["):
        return any(root.glob(source))
    return (root / source).exists()


def _top_level_yaml_scalar(text: str, key: str) -> str | None:
    match = re.search(rf"^{re.escape(key)}:\s*(.*?)\s*$", text, flags=re.MULTILINE)
    if not match:
        return None

    value = match.group(1)
    if len(value) >= 2 and value[0] == value[-1] and value[0] in {"\"", "'"}:
        return value[1:-1]
    return value


def _validate_publishing(
    root: Path,
    publishing: object,
    documents: object,
    errors: list[str],
) -> None:
    if not isinstance(publishing, dict):
        errors.append("documentation manifest must contain a publishing object")
        return

    expected_values = {
        "provider": "github-pages",
        "generator": "jekyll",
        "content_format": "markdown",
        "cname": "CNAME",
        "config": "_config.yml",
        "gemfile": "Gemfile",
        "ruby_version_file": ".ruby-version",
        "workflow": ".github/workflows/validate.yml",
        "site_artifact": "_site",
    }
    for key, expected in expected_values.items():
        if publishing.get(key) != expected:
            errors.append(f"documentation publishing {key} must be {expected}")

    domain = publishing.get("domain")
    if not isinstance(domain, str) or not domain.strip():
        errors.append("documentation publishing domain must be a non-empty string")
        return

    production_url = publishing.get("production_url")
    if production_url != f"https://{domain}":
        errors.append(f"documentation publishing production_url must be https://{domain}")

    repository = publishing.get("repository")
    if not isinstance(repository, str) or not re.fullmatch(r"[^/\s]+/[^/\s]+", repository):
        errors.append("documentation publishing repository must use owner/name form")

    theme = publishing.get("theme")
    if not isinstance(theme, str) or not theme.strip():
        errors.append("documentation publishing theme must be a non-empty string")

    ruby_version = publishing.get("ruby_version")
    if not isinstance(ruby_version, str) or not re.fullmatch(r"\d+\.\d+\.\d+", ruby_version):
        errors.append("documentation publishing ruby_version must use major.minor.patch form")

    pages_gem_version = publishing.get("pages_gem_version")
    if not isinstance(pages_gem_version, str) or not pages_gem_version.isdigit():
        errors.append("documentation publishing pages_gem_version must be a numeric string")

    source = publishing.get("source")
    if not isinstance(source, dict):
        errors.append("documentation publishing source must be an object")
    else:
        source_status = source.get("status")
        if source_status not in ALLOWED_PAGES_SOURCE_STATES:
            errors.append(f"documentation publishing source has invalid status: {source_status}")
        elif source_status == "verified":
            if not isinstance(source.get("branch"), str) or not source["branch"].strip():
                errors.append("verified documentation publishing source must name a branch")
            if not isinstance(source.get("folder"), str) or not source["folder"].strip():
                errors.append("verified documentation publishing source must name a folder")
            if source.get("build_type") not in ALLOWED_PAGES_BUILD_TYPES:
                errors.append("verified documentation publishing source has invalid build type")
            if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", str(source.get("verified_on", ""))):
                errors.append("verified documentation publishing source must name its audit date")
        elif source.get("branch") is not None or source.get("folder") is not None:
            errors.append("pending documentation publishing source branch/folder must remain null")

    dns = publishing.get("dns")
    if not isinstance(dns, dict) or not isinstance(dns.get("owner"), str) or not dns["owner"].strip():
        errors.append("documentation publishing dns must name an operational owner")
    elif dns.get("status") not in ALLOWED_DNS_STATES:
        errors.append("documentation publishing dns has invalid resolution status")
    elif dns.get("ownership_verification") not in ALLOWED_DNS_OWNERSHIP_STATES:
        errors.append("documentation publishing dns has invalid ownership-verification status")
    elif not re.fullmatch(r"\d{4}-\d{2}-\d{2}", str(dns.get("verified_on", ""))):
        errors.append("documentation publishing dns must name its audit date")
    elif dns["ownership_verification"] == "not-observed" and (
        not isinstance(dns.get("challenge_record"), str) or not dns["challenge_record"].strip()
    ):
        errors.append("unverified documentation publishing dns must name its challenge record")

    cname_path = root / "CNAME"
    config_path = root / "_config.yml"
    if not cname_path.is_file():
        errors.append("GitHub Pages CNAME file is missing")
    elif cname_path.read_text(encoding="utf-8").strip() != domain:
        errors.append(f"GitHub Pages CNAME does not match manifest domain: {domain}")
    if not config_path.is_file():
        errors.append("GitHub Pages _config.yml file is missing")
    else:
        config_text = config_path.read_text(encoding="utf-8")
        config_values = {
            "url": production_url,
            "baseurl": "",
            "repository": repository,
            "theme": theme,
            "strict_front_matter": "true",
        }
        for key, expected in config_values.items():
            if _top_level_yaml_scalar(config_text, key) != expected:
                errors.append(f"GitHub Pages _config.yml {key} must match the publishing manifest")
        if not re.search(r"^\s*-\s+jekyll-relative-links\s*$", config_text, flags=re.MULTILINE):
            errors.append("GitHub Pages _config.yml must enable jekyll-relative-links")
        manifest_documents = documents if isinstance(documents, dict) else {}
        for relative_path, permalink in PAGES_README_PERMALINKS.items():
            if relative_path not in manifest_documents:
                continue
            readme_path = root / relative_path
            if not readme_path.is_file():
                continue
            readme_text = readme_path.read_text(encoding="utf-8-sig")
            front_matter = re.match(r"\A---\s*\n(?P<body>.*?)\n---\s*\n", readme_text, re.DOTALL)
            if front_matter is None or not re.search(
                rf"(?m)^permalink:\s*{re.escape(permalink)}\s*$",
                front_matter.group("body") if front_matter else "",
            ):
                errors.append(
                    f"GitHub Pages public README must pin permalink {permalink}: "
                    f"{relative_path}"
                )

    ruby_version_path = root / ".ruby-version"
    if not ruby_version_path.is_file():
        errors.append("GitHub Pages .ruby-version file is missing")
    elif isinstance(ruby_version, str) and ruby_version_path.read_text(encoding="utf-8").strip() != ruby_version:
        errors.append("GitHub Pages .ruby-version does not match the publishing manifest")

    gemfile_path = root / "Gemfile"
    if not gemfile_path.is_file():
        errors.append("GitHub Pages Gemfile is missing")
    elif isinstance(pages_gem_version, str):
        gemfile_text = gemfile_path.read_text(encoding="utf-8")
        pages_pin = re.compile(
            rf"^\s*gem\s+[\"']github-pages[\"']\s*,\s*[\"']=\s*{re.escape(pages_gem_version)}[\"']",
            flags=re.MULTILINE,
        )
        if not pages_pin.search(gemfile_text):
            errors.append("GitHub Pages Gemfile pin does not match the publishing manifest")

    workflow_path = root / ".github/workflows/validate.yml"
    if not workflow_path.is_file():
        errors.append("GitHub Pages validation workflow is missing")
    else:
        workflow_text = workflow_path.read_text(encoding="utf-8")
        required_workflow_markers = (
            f"uses: {PAGES_BUILD_ACTION}",
            "destination: ./_site",
            "BuildTools/docs_site_artifact.py",
            "--site-dir _site",
            "Workspace/docs-site-artifact-report.json",
            f"uses: {SITE_ARTIFACT_ACTION}",
            "path: _site/",
        )
        for marker in required_workflow_markers:
            if marker not in workflow_text:
                errors.append(f"GitHub Pages validation workflow is missing: {marker}")
        lifecycle_test = "BuildTools/tests/test_docs_script_lifecycle.py"
        if lifecycle_test not in workflow_text:
            errors.append(f"documentation script lifecycle workflow is missing: {lifecycle_test}")
        model_animation_test = "BuildTools/tests/test_docs_model_animation.py"
        if model_animation_test not in workflow_text:
            errors.append(f"documentation model animation workflow is missing: {model_animation_test}")
        sprite_root_motion_test = "BuildTools/tests/test_docs_sprite_root_motion.py"
        if sprite_root_motion_test not in workflow_text:
            errors.append(f"documentation sprite root motion workflow is missing: {sprite_root_motion_test}")
        web_debugging_test = "BuildTools/tests/test_docs_web_debugging.py"
        if web_debugging_test not in workflow_text:
            errors.append(f"documentation Web debugging workflow is missing: {web_debugging_test}")
        android_debugging_test = "BuildTools/tests/test_docs_android_debugging.py"
        if android_debugging_test not in workflow_text:
            errors.append(f"documentation Android debugging workflow is missing: {android_debugging_test}")
        debugging_test = "BuildTools/tests/test_docs_debugging.py"
        if debugging_test not in workflow_text:
            errors.append(f"documentation native and AngelScript debugging workflow is missing: {debugging_test}")
        angelscript_style_test = "BuildTools/tests/test_docs_angelscript_style.py"
        if angelscript_style_test not in workflow_text:
            errors.append(f"documentation AngelScript style workflow is missing: {angelscript_style_test}")
        viewer_tools_test = "BuildTools/tests/test_docs_viewer_tools.py"
        if viewer_tools_test not in workflow_text:
            errors.append(f"documentation viewer tools workflow is missing: {viewer_tools_test}")
        mapper_tools_test = "BuildTools/tests/test_docs_mapper_tools.py"
        if mapper_tools_test not in workflow_text:
            errors.append(f"documentation Mapper tools workflow is missing: {mapper_tools_test}")


def _validate_delivery_policy(manifest: dict[str, object], errors: list[str]) -> None:
    try:
        docs_ai_delivery._versioning_config(manifest)
    except ValueError as exception:
        errors.append(f"invalid documentation versioning policy: {exception}")
    try:
        docs_ai_delivery._localization_config(manifest)
    except ValueError as exception:
        errors.append(f"invalid documentation localization policy: {exception}")


def _validate_generated_artifacts(
    root: Path,
    generated_artifacts: object,
    site_delivery_config: object,
    documents: object,
    errors: list[str],
) -> None:
    if not isinstance(generated_artifacts, dict):
        errors.append("documentation manifest must contain a generated_artifacts object")
        return

    for artifact_id, artifact in generated_artifacts.items():
        if not isinstance(artifact, dict):
            continue
        visibility = artifact.get("visibility", "public")
        if visibility not in {"public", "internal"}:
            errors.append(
                f"documentation generated artifact {artifact_id} has invalid visibility: {visibility}"
            )

    source_inventory = generated_artifacts.get("source_inventory")
    if not isinstance(source_inventory, dict):
        errors.append("documentation manifest must declare generated_artifacts.source_inventory")
        return

    path = source_inventory.get("path")
    generator = source_inventory.get("generator")
    if path != docs_inventory.DEFAULT_OUTPUT:
        errors.append(f"documentation source inventory path must be {docs_inventory.DEFAULT_OUTPUT}")
        return
    if generator != "BuildTools/docs_inventory.py":
        errors.append("documentation source inventory generator must be BuildTools/docs_inventory.py")

    output_path = root / path
    if not output_path.is_file():
        errors.append(f"generated documentation inventory is missing: {path}")
    elif output_path.read_text(encoding="utf-8") != docs_inventory.render_inventory(root):
        errors.append("generated documentation inventory is stale; run python BuildTools/docs_inventory.py --write")

    api_model = generated_artifacts.get("api_model")
    if not isinstance(api_model, dict):
        errors.append("documentation manifest must declare generated_artifacts.api_model")
        return

    api_path = api_model.get("path")
    api_generator = api_model.get("generator")
    source_parser = api_model.get("source_parser")
    api_schema_version = api_model.get("schema_version")
    if api_path != docs_api.DEFAULT_OUTPUT:
        errors.append(f"documentation API model path must be {docs_api.DEFAULT_OUTPUT}")
        return
    if api_generator != "BuildTools/docs_api.py":
        errors.append("documentation API model generator must be BuildTools/docs_api.py")
    if source_parser != "BuildTools/codegen.py":
        errors.append("documentation API model source parser must be BuildTools/codegen.py")
    if api_schema_version != docs_api.SCHEMA_VERSION:
        errors.append(f"documentation API model schema version must be {docs_api.SCHEMA_VERSION}")

    api_output_path = root / api_path
    if not api_output_path.is_file():
        errors.append(f"generated documentation API model is missing: {api_path}")
    elif api_output_path.read_text(encoding="utf-8") != docs_api.render_api_model(root):
        errors.append("generated documentation API model is stale; run python BuildTools/docs_api.py --write")

    reference_pages = generated_artifacts.get("reference_pages")
    if not isinstance(reference_pages, dict):
        errors.append("documentation manifest must declare generated_artifacts.reference_pages")
        return

    reference_directory = reference_pages.get("directory")
    reference_generator = reference_pages.get("generator")
    reference_source_model = reference_pages.get("source_model")
    reference_paths = reference_pages.get("paths")
    if reference_directory != docs_reference.DEFAULT_OUTPUT_DIR:
        errors.append(f"documentation reference page directory must be {docs_reference.DEFAULT_OUTPUT_DIR}")
    if reference_generator != "BuildTools/docs_reference.py":
        errors.append("documentation reference page generator must be BuildTools/docs_reference.py")
    if reference_source_model != docs_reference.DEFAULT_MODEL:
        errors.append(f"documentation reference page source model must be {docs_reference.DEFAULT_MODEL}")
    if reference_paths != list(docs_reference.OUTPUT_PATHS):
        errors.append("documentation reference page paths must match BuildTools/docs_reference.py OUTPUT_PATHS")

    if not api_output_path.is_file():
        return
    try:
        rendered_pages = docs_reference.render_reference_pages(root, docs_reference.DEFAULT_MODEL)
    except (OSError, json.JSONDecodeError, ValueError) as exception:
        errors.append(f"unable to render generated documentation reference pages: {exception}")
        return

    for reference_path, rendered_page in rendered_pages.items():
        output_path = root / reference_path
        if not output_path.is_file():
            errors.append(f"generated documentation reference page is missing: {reference_path}")
        elif output_path.read_text(encoding="utf-8") != rendered_page:
            errors.append(
                f"generated documentation reference page is stale: {reference_path}; "
                "run python BuildTools/docs_reference.py --write"
            )

    cmake_reference = generated_artifacts.get("cmake_reference")
    if not isinstance(cmake_reference, dict):
        errors.append("documentation manifest must declare generated_artifacts.cmake_reference")
        return

    expected_cmake_reference = {
        "source_manifest": docs_cmake.DEFAULT_MANIFEST,
        "model": docs_cmake.DEFAULT_MODEL,
        "generator": "BuildTools/docs_cmake.py",
        "schema_version": docs_cmake.SCHEMA_VERSION,
        "directory": docs_cmake.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_cmake.OUTPUT_PATHS),
        "structural_test": "BuildTools/tests/validate_project_interface.cmake",
    }
    for field, expected in expected_cmake_reference.items():
        if cmake_reference.get(field) != expected:
            errors.append(f"documentation CMake reference {field} must be {expected}")

    cmake_manifest_path = root / docs_cmake.DEFAULT_MANIFEST
    if not cmake_manifest_path.is_file():
        errors.append(f"CMake project interface manifest is missing: {docs_cmake.DEFAULT_MANIFEST}")
    else:
        try:
            rendered_cmake_model = docs_cmake.render_cmake_model(root)
            rendered_cmake_pages = docs_cmake.render_reference_pages(root)
        except (OSError, json.JSONDecodeError, ValueError) as exception:
            errors.append(f"unable to render generated CMake project-interface documentation: {exception}")
        else:
            cmake_model_path = root / docs_cmake.DEFAULT_MODEL
            if not cmake_model_path.is_file():
                errors.append(f"generated documentation CMake model is missing: {docs_cmake.DEFAULT_MODEL}")
            elif cmake_model_path.read_text(encoding="utf-8") != rendered_cmake_model:
                errors.append("generated documentation CMake model is stale; run python BuildTools/docs_cmake.py --write")

            for cmake_page_path, rendered_cmake_page in rendered_cmake_pages.items():
                output_path = root / cmake_page_path
                if not output_path.is_file():
                    errors.append(f"generated documentation CMake page is missing: {cmake_page_path}")
                elif output_path.read_text(encoding="utf-8") != rendered_cmake_page:
                    errors.append(
                        f"generated documentation CMake page is stale: {cmake_page_path}; "
                        "run python BuildTools/docs_cmake.py --write"
                    )

    structural_test_path = root / expected_cmake_reference["structural_test"]
    if not structural_test_path.is_file():
        errors.append(f"CMake project interface structural test is missing: {expected_cmake_reference['structural_test']}")

    workflow_path = root / ".github/workflows/validate.yml"
    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_docs_cmake.py",
            "BuildTools/docs_cmake.py --check",
            "cmake -P BuildTools/tests/validate_project_interface.cmake",
        ):
            if marker not in workflow_text:
                errors.append(f"documentation CMake reference workflow is missing: {marker}")

    cli_reference = generated_artifacts.get("cli_reference")
    if not isinstance(cli_reference, dict):
        errors.append("documentation manifest must declare generated_artifacts.cli_reference")
        return

    expected_cli_reference = {
        "source_parser": docs_cli.DEFAULT_SOURCE,
        "model": docs_cli.DEFAULT_MODEL,
        "generator": "BuildTools/docs_cli.py",
        "schema_version": docs_cli.SCHEMA_VERSION,
        "directory": docs_cli.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_cli.OUTPUT_PATHS),
    }
    for field, expected in expected_cli_reference.items():
        if cli_reference.get(field) != expected:
            errors.append(f"documentation BuildTools CLI reference {field} must be {expected}")

    try:
        rendered_cli_model = docs_cli.render_cli_model(root)
        rendered_cli_pages = docs_cli.render_reference_pages(root)
    except (OSError, ImportError, ValueError) as exception:
        errors.append(f"unable to render generated BuildTools CLI documentation: {exception}")
    else:
        cli_model_path = root / docs_cli.DEFAULT_MODEL
        if not cli_model_path.is_file():
            errors.append(f"generated documentation BuildTools CLI model is missing: {docs_cli.DEFAULT_MODEL}")
        elif cli_model_path.read_text(encoding="utf-8") != rendered_cli_model:
            errors.append("generated documentation BuildTools CLI model is stale; run python BuildTools/docs_cli.py --write")

        for cli_page_path, rendered_cli_page in rendered_cli_pages.items():
            output_path = root / cli_page_path
            if not output_path.is_file():
                errors.append(f"generated documentation BuildTools CLI page is missing: {cli_page_path}")
            elif output_path.read_text(encoding="utf-8") != rendered_cli_page:
                errors.append(
                    f"generated documentation BuildTools CLI page is stale: {cli_page_path}; "
                    "run python BuildTools/docs_cli.py --write"
                )

    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in ("BuildTools/tests/test_docs_cli.py", "BuildTools/docs_cli.py --check"):
            if marker not in workflow_text:
                errors.append(f"documentation BuildTools CLI reference workflow is missing: {marker}")

    helper_cli_reference = generated_artifacts.get("helper_cli_reference")
    if not isinstance(helper_cli_reference, dict):
        errors.append("documentation manifest must declare generated_artifacts.helper_cli_reference")
        return

    expected_helper_cli_reference = {
        "source_manifest": docs_helper_cli.DEFAULT_MANIFEST,
        "model": docs_helper_cli.DEFAULT_MODEL,
        "generator": "BuildTools/docs_helper_cli.py",
        "schema_version": docs_helper_cli.SCHEMA_VERSION,
        "directory": docs_helper_cli.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_helper_cli.OUTPUT_PATHS),
    }
    for field, expected in expected_helper_cli_reference.items():
        if helper_cli_reference.get(field) != expected:
            errors.append(f"documentation helper CLI reference {field} must be {expected}")

    try:
        rendered_helper_cli_model = docs_helper_cli.render_helper_cli_model(root)
        helper_cli_model = json.loads(rendered_helper_cli_model)
        rendered_helper_cli_pages = docs_helper_cli.render_reference_pages(root)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        errors.append(f"unable to render generated helper CLI documentation: {exception}")
    else:
        helper_cli_model_path = root / docs_helper_cli.DEFAULT_MODEL
        if not helper_cli_model_path.is_file():
            errors.append(f"generated documentation helper CLI model is missing: {docs_helper_cli.DEFAULT_MODEL}")
        elif helper_cli_model_path.read_text(encoding="utf-8") != rendered_helper_cli_model:
            errors.append(
                "generated documentation helper CLI model is stale; "
                "run python BuildTools/docs_helper_cli.py --write"
            )

        for helper_cli_page_path, rendered_helper_cli_page in rendered_helper_cli_pages.items():
            output_path = root / helper_cli_page_path
            if not output_path.is_file():
                errors.append(f"generated documentation helper CLI page is missing: {helper_cli_page_path}")
            elif output_path.read_text(encoding="utf-8") != rendered_helper_cli_page:
                errors.append(
                    f"generated documentation helper CLI page is stale: {helper_cli_page_path}; "
                    "run python BuildTools/docs_helper_cli.py --write"
                )

    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_docs_helper_cli.py",
            "BuildTools/docs_helper_cli.py --check",
        ):
            if marker not in workflow_text:
                errors.append(f"documentation helper CLI reference workflow is missing: {marker}")

    native_extension_reference = generated_artifacts.get("native_extension_reference")
    if not isinstance(native_extension_reference, dict):
        errors.append("documentation manifest must declare generated_artifacts.native_extension_reference")
        return

    expected_native_extension_reference = {
        "source_manifest": docs_native_extension.DEFAULT_MANIFEST,
        "model": docs_native_extension.DEFAULT_MODEL,
        "generator": "BuildTools/docs_native_extension.py",
        "schema_version": docs_native_extension.SCHEMA_VERSION,
        "directory": docs_native_extension.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_native_extension.OUTPUT_PATHS),
        "structural_test": "BuildTools/tests/validate_native_extension_interface.cmake",
    }
    for field, expected in expected_native_extension_reference.items():
        if native_extension_reference.get(field) != expected:
            errors.append(f"documentation native extension reference {field} must be {expected}")

    try:
        rendered_native_extension_model = docs_native_extension.render_native_extension_model(root)
        native_extension_model = json.loads(rendered_native_extension_model)
        rendered_native_extension_pages = docs_native_extension.render_reference_pages(root)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        errors.append(f"unable to render generated native extension documentation: {exception}")
    else:
        native_extension_model_path = root / docs_native_extension.DEFAULT_MODEL
        if not native_extension_model_path.is_file():
            errors.append(f"generated documentation native extension model is missing: {docs_native_extension.DEFAULT_MODEL}")
        elif native_extension_model_path.read_text(encoding="utf-8") != rendered_native_extension_model:
            errors.append(
                "generated documentation native extension model is stale; "
                "run python BuildTools/docs_native_extension.py --write"
            )
        for page_path, rendered_page in rendered_native_extension_pages.items():
            output_path = root / page_path
            if not output_path.is_file():
                errors.append(f"generated documentation native extension page is missing: {page_path}")
            elif output_path.read_text(encoding="utf-8") != rendered_page:
                errors.append(
                    f"generated documentation native extension page is stale: {page_path}; "
                    "run python BuildTools/docs_native_extension.py --write"
                )

    native_extension_structural_test = root / expected_native_extension_reference["structural_test"]
    if not native_extension_structural_test.is_file():
        errors.append(
            "native extension interface structural test is missing: "
            + expected_native_extension_reference["structural_test"]
        )

    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_docs_native_extension.py",
            "BuildTools/docs_native_extension.py --check",
            "cmake -P BuildTools/tests/validate_native_extension_interface.cmake",
        ):
            if marker not in workflow_text:
                errors.append(f"documentation native extension reference workflow is missing: {marker}")

    prototype_format_reference = generated_artifacts.get("prototype_format_reference")
    if not isinstance(prototype_format_reference, dict):
        errors.append("documentation manifest must declare generated_artifacts.prototype_format_reference")
        return

    expected_prototype_format_reference = {
        "source_manifest": docs_prototype_format.DEFAULT_MANIFEST,
        "model": docs_prototype_format.DEFAULT_MODEL,
        "generator": "BuildTools/docs_prototype_format.py",
        "schema_version": docs_prototype_format.SCHEMA_VERSION,
        "directory": docs_prototype_format.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_prototype_format.OUTPUT_PATHS),
    }
    for field, expected in expected_prototype_format_reference.items():
        if prototype_format_reference.get(field) != expected:
            errors.append(f"documentation prototype format reference {field} must be {expected}")

    try:
        rendered_prototype_format_model = docs_prototype_format.render_prototype_format_model(root)
        prototype_format_model = json.loads(rendered_prototype_format_model)
        rendered_prototype_format_pages = docs_prototype_format.render_reference_pages(root)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        errors.append(f"unable to render generated prototype format documentation: {exception}")
    else:
        prototype_format_model_path = root / docs_prototype_format.DEFAULT_MODEL
        if not prototype_format_model_path.is_file():
            errors.append(
                "generated documentation prototype format model is missing: "
                + docs_prototype_format.DEFAULT_MODEL
            )
        elif (
            prototype_format_model_path.read_text(encoding="utf-8")
            != rendered_prototype_format_model
        ):
            errors.append(
                "generated documentation prototype format model is stale; "
                "run python BuildTools/docs_prototype_format.py --write"
            )
        for page_path, rendered_page in rendered_prototype_format_pages.items():
            output_path = root / page_path
            if not output_path.is_file():
                errors.append(f"generated documentation prototype format page is missing: {page_path}")
            elif output_path.read_text(encoding="utf-8") != rendered_page:
                errors.append(
                    f"generated documentation prototype format page is stale: {page_path}; "
                    "run python BuildTools/docs_prototype_format.py --write"
                )

    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_docs_prototype_format.py",
            "BuildTools/docs_prototype_format.py --check",
        ):
            if marker not in workflow_text:
                errors.append(f"documentation prototype format workflow is missing: {marker}")

    map_format_reference = generated_artifacts.get("map_format_reference")
    if not isinstance(map_format_reference, dict):
        errors.append("documentation manifest must declare generated_artifacts.map_format_reference")
        return

    expected_map_format_reference = {
        "source_manifest": docs_map_format.DEFAULT_MANIFEST,
        "model": docs_map_format.DEFAULT_MODEL,
        "generator": "BuildTools/docs_map_format.py",
        "schema_version": docs_map_format.SCHEMA_VERSION,
        "directory": docs_map_format.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_map_format.OUTPUT_PATHS),
    }
    for field, expected in expected_map_format_reference.items():
        if map_format_reference.get(field) != expected:
            errors.append(f"documentation map format reference {field} must be {expected}")

    try:
        rendered_map_format_model = docs_map_format.render_map_format_model(root)
        map_format_model = json.loads(rendered_map_format_model)
        rendered_map_format_pages = docs_map_format.render_reference_pages(root)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        errors.append(f"unable to render generated map format documentation: {exception}")
    else:
        map_format_model_path = root / docs_map_format.DEFAULT_MODEL
        if not map_format_model_path.is_file():
            errors.append(
                "generated documentation map format model is missing: "
                + docs_map_format.DEFAULT_MODEL
            )
        elif map_format_model_path.read_text(encoding="utf-8") != rendered_map_format_model:
            errors.append(
                "generated documentation map format model is stale; "
                "run python BuildTools/docs_map_format.py --write"
            )
        for page_path, rendered_page in rendered_map_format_pages.items():
            output_path = root / page_path
            if not output_path.is_file():
                errors.append(f"generated documentation map format page is missing: {page_path}")
            elif output_path.read_text(encoding="utf-8") != rendered_page:
                errors.append(
                    f"generated documentation map format page is stale: {page_path}; "
                    "run python BuildTools/docs_map_format.py --write"
                )

    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_docs_map_format.py",
            "BuildTools/docs_map_format.py --check",
        ):
            if marker not in workflow_text:
                errors.append(f"documentation map format workflow is missing: {marker}")

    model_format_reference = generated_artifacts.get("model_format_reference")
    if not isinstance(model_format_reference, dict):
        errors.append("documentation manifest must declare generated_artifacts.model_format_reference")
        return

    expected_model_format_reference = {
        "source_manifest": docs_model_format.DEFAULT_MANIFEST,
        "model": docs_model_format.DEFAULT_MODEL,
        "generator": "BuildTools/docs_model_format.py",
        "schema_version": docs_model_format.SCHEMA_VERSION,
        "directory": docs_model_format.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_model_format.OUTPUT_PATHS),
    }
    for field, expected in expected_model_format_reference.items():
        if model_format_reference.get(field) != expected:
            errors.append(f"documentation model format reference {field} must be {expected}")

    try:
        rendered_model_format_model = docs_model_format.render_model_format_model(root)
        model_format_model = json.loads(rendered_model_format_model)
        rendered_model_format_pages = docs_model_format.render_reference_pages(root)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        errors.append(f"unable to render generated model format documentation: {exception}")
    else:
        model_format_model_path = root / docs_model_format.DEFAULT_MODEL
        if not model_format_model_path.is_file():
            errors.append(
                "generated documentation model format model is missing: "
                + docs_model_format.DEFAULT_MODEL
            )
        elif (
            model_format_model_path.read_text(encoding="utf-8")
            != rendered_model_format_model
        ):
            errors.append(
                "generated documentation model format model is stale; "
                "run python BuildTools/docs_model_format.py --write"
            )
        for page_path, rendered_page in rendered_model_format_pages.items():
            output_path = root / page_path
            if not output_path.is_file():
                errors.append(
                    f"generated documentation model format page is missing: {page_path}"
                )
            elif output_path.read_text(encoding="utf-8") != rendered_page:
                errors.append(
                    f"generated documentation model format page is stale: {page_path}; "
                    "run python BuildTools/docs_model_format.py --write"
                )

    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_docs_model_format.py",
            "BuildTools/docs_model_format.py --check",
        ):
            if marker not in workflow_text:
                errors.append(f"documentation model format workflow is missing: {marker}")

    text_format_reference = generated_artifacts.get("text_format_reference")
    if not isinstance(text_format_reference, dict):
        errors.append("documentation manifest must declare generated_artifacts.text_format_reference")
        return

    expected_text_format_reference = {
        "source_manifest": docs_text_format.DEFAULT_MANIFEST,
        "model": docs_text_format.DEFAULT_MODEL,
        "generator": "BuildTools/docs_text_format.py",
        "schema_version": docs_text_format.SCHEMA_VERSION,
        "directory": docs_text_format.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_text_format.OUTPUT_PATHS),
    }
    for field, expected in expected_text_format_reference.items():
        if text_format_reference.get(field) != expected:
            errors.append(f"documentation text format reference {field} must be {expected}")

    try:
        rendered_text_format_model = docs_text_format.render_text_format_model(root)
        rendered_text_format_pages = docs_text_format.render_reference_pages(root)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        errors.append(f"unable to render generated text format documentation: {exception}")
    else:
        text_format_model_path = root / docs_text_format.DEFAULT_MODEL
        if not text_format_model_path.is_file():
            errors.append(
                "generated documentation text format model is missing: "
                + docs_text_format.DEFAULT_MODEL
            )
        elif (
            text_format_model_path.read_text(encoding="utf-8")
            != rendered_text_format_model
        ):
            errors.append(
                "generated documentation text format model is stale; "
                "run python BuildTools/docs_text_format.py --write"
            )
        for page_path, rendered_page in rendered_text_format_pages.items():
            output_path = root / page_path
            if not output_path.is_file():
                errors.append(
                    f"generated documentation text format page is missing: {page_path}"
                )
            elif output_path.read_text(encoding="utf-8") != rendered_page:
                errors.append(
                    f"generated documentation text format page is stale: {page_path}; "
                    "run python BuildTools/docs_text_format.py --write"
                )

    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_docs_text_format.py",
            "BuildTools/docs_text_format.py --check",
        ):
            if marker not in workflow_text:
                errors.append(f"documentation text format workflow is missing: {marker}")

    effect_format_reference = generated_artifacts.get("effect_format_reference")
    if not isinstance(effect_format_reference, dict):
        errors.append("documentation manifest must declare generated_artifacts.effect_format_reference")
        return

    expected_effect_format_reference = {
        "source_manifest": docs_effect_format.DEFAULT_MANIFEST,
        "model": docs_effect_format.DEFAULT_MODEL,
        "generator": "BuildTools/docs_effect_format.py",
        "schema_version": docs_effect_format.SCHEMA_VERSION,
        "directory": docs_effect_format.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_effect_format.OUTPUT_PATHS),
    }
    for field, expected in expected_effect_format_reference.items():
        if effect_format_reference.get(field) != expected:
            errors.append(f"documentation effect format reference {field} must be {expected}")

    try:
        rendered_effect_format_model = docs_effect_format.render_effect_format_model(root)
        effect_format_model = json.loads(rendered_effect_format_model)
        rendered_effect_format_pages = docs_effect_format.render_reference_pages(root)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        errors.append(f"unable to render generated effect format documentation: {exception}")
    else:
        effect_format_model_path = root / docs_effect_format.DEFAULT_MODEL
        if not effect_format_model_path.is_file():
            errors.append(
                "generated documentation effect format model is missing: "
                + docs_effect_format.DEFAULT_MODEL
            )
        elif (
            effect_format_model_path.read_text(encoding="utf-8")
            != rendered_effect_format_model
        ):
            errors.append(
                "generated documentation effect format model is stale; "
                "run python BuildTools/docs_effect_format.py --write"
            )
        for page_path, rendered_page in rendered_effect_format_pages.items():
            output_path = root / page_path
            if not output_path.is_file():
                errors.append(
                    f"generated documentation effect format page is missing: {page_path}"
                )
            elif output_path.read_text(encoding="utf-8") != rendered_page:
                errors.append(
                    f"generated documentation effect format page is stale: {page_path}; "
                    "run python BuildTools/docs_effect_format.py --write"
                )

    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_docs_effect_format.py",
            "BuildTools/docs_effect_format.py --check",
        ):
            if marker not in workflow_text:
                errors.append(f"documentation effect format workflow is missing: {marker}")

    image_format_reference = generated_artifacts.get("image_format_reference")
    if not isinstance(image_format_reference, dict):
        errors.append("documentation manifest must declare generated_artifacts.image_format_reference")
        return

    expected_image_format_reference = {
        "source_manifest": docs_image_format.DEFAULT_MANIFEST,
        "model": docs_image_format.DEFAULT_MODEL,
        "generator": "BuildTools/docs_image_format.py",
        "schema_version": docs_image_format.SCHEMA_VERSION,
        "directory": docs_image_format.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_image_format.OUTPUT_PATHS),
    }
    for field, expected in expected_image_format_reference.items():
        if image_format_reference.get(field) != expected:
            errors.append(f"documentation image format reference {field} must be {expected}")

    try:
        rendered_image_format_model = docs_image_format.render_image_format_model(root)
        image_format_model = json.loads(rendered_image_format_model)
        rendered_image_format_pages = docs_image_format.render_reference_pages(root)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        errors.append(f"unable to render generated image format documentation: {exception}")
    else:
        image_format_model_path = root / docs_image_format.DEFAULT_MODEL
        if not image_format_model_path.is_file():
            errors.append(
                "generated documentation image format model is missing: "
                + docs_image_format.DEFAULT_MODEL
            )
        elif image_format_model_path.read_text(encoding="utf-8") != rendered_image_format_model:
            errors.append(
                "generated documentation image format model is stale; "
                "run python BuildTools/docs_image_format.py --write"
            )
        for page_path, rendered_page in rendered_image_format_pages.items():
            output_path = root / page_path
            if not output_path.is_file():
                errors.append(
                    f"generated documentation image format page is missing: {page_path}"
                )
            elif output_path.read_text(encoding="utf-8") != rendered_page:
                errors.append(
                    f"generated documentation image format page is stale: {page_path}; "
                    "run python BuildTools/docs_image_format.py --write"
                )

    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_docs_image_format.py",
            "BuildTools/docs_image_format.py --check",
        ):
            if marker not in workflow_text:
                errors.append(f"documentation image format workflow is missing: {marker}")

    particle_format_reference = generated_artifacts.get("particle_format_reference")
    if not isinstance(particle_format_reference, dict):
        errors.append("documentation manifest must declare generated_artifacts.particle_format_reference")
        return

    expected_particle_format_reference = {
        "source_manifest": docs_particle_format.DEFAULT_MANIFEST,
        "model": docs_particle_format.DEFAULT_MODEL,
        "generator": "BuildTools/docs_particle_format.py",
        "schema_version": docs_particle_format.SCHEMA_VERSION,
        "directory": docs_particle_format.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_particle_format.OUTPUT_PATHS),
    }
    for field, expected in expected_particle_format_reference.items():
        if particle_format_reference.get(field) != expected:
            errors.append(f"documentation particle format reference {field} must be {expected}")

    try:
        rendered_particle_format_model = docs_particle_format.render_particle_format_model(root)
        particle_format_model = json.loads(rendered_particle_format_model)
        rendered_particle_format_pages = docs_particle_format.render_reference_pages(root)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        errors.append(f"unable to render generated particle format documentation: {exception}")
    else:
        particle_format_model_path = root / docs_particle_format.DEFAULT_MODEL
        if not particle_format_model_path.is_file():
            errors.append(
                "generated documentation particle format model is missing: "
                + docs_particle_format.DEFAULT_MODEL
            )
        elif (
            particle_format_model_path.read_text(encoding="utf-8")
            != rendered_particle_format_model
        ):
            errors.append(
                "generated documentation particle format model is stale; "
                "run python BuildTools/docs_particle_format.py --write"
            )
        for page_path, rendered_page in rendered_particle_format_pages.items():
            output_path = root / page_path
            if not output_path.is_file():
                errors.append(
                    f"generated documentation particle format page is missing: {page_path}"
                )
            elif output_path.read_text(encoding="utf-8") != rendered_page:
                errors.append(
                    f"generated documentation particle format page is stale: {page_path}; "
                    "run python BuildTools/docs_particle_format.py --write"
                )

    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_docs_particle_format.py",
            "BuildTools/docs_particle_format.py --check",
        ):
            if marker not in workflow_text:
                errors.append(f"documentation particle format workflow is missing: {marker}")

    font_format_reference = generated_artifacts.get("font_format_reference")
    if not isinstance(font_format_reference, dict):
        errors.append("documentation manifest must declare generated_artifacts.font_format_reference")
        return

    expected_font_format_reference = {
        "source_manifest": docs_font_format.DEFAULT_MANIFEST,
        "model": docs_font_format.DEFAULT_MODEL,
        "generator": "BuildTools/docs_font_format.py",
        "schema_version": docs_font_format.SCHEMA_VERSION,
        "directory": docs_font_format.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_font_format.OUTPUT_PATHS),
    }
    for field, expected in expected_font_format_reference.items():
        if font_format_reference.get(field) != expected:
            errors.append(f"documentation font format reference {field} must be {expected}")

    try:
        rendered_font_format_model = docs_font_format.render_font_format_model(root)
        font_format_model = json.loads(rendered_font_format_model)
        rendered_font_format_pages = docs_font_format.render_reference_pages(root)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        errors.append(f"unable to render generated font format documentation: {exception}")
    else:
        font_format_model_path = root / docs_font_format.DEFAULT_MODEL
        if not font_format_model_path.is_file():
            errors.append(
                "generated documentation font format model is missing: "
                + docs_font_format.DEFAULT_MODEL
            )
        elif (
            font_format_model_path.read_text(encoding="utf-8")
            != rendered_font_format_model
        ):
            errors.append(
                "generated documentation font format model is stale; "
                "run python BuildTools/docs_font_format.py --write"
            )
        for page_path, rendered_page in rendered_font_format_pages.items():
            output_path = root / page_path
            if not output_path.is_file():
                errors.append(
                    f"generated documentation font format page is missing: {page_path}"
                )
            elif output_path.read_text(encoding="utf-8") != rendered_page:
                errors.append(
                    f"generated documentation font format page is stale: {page_path}; "
                    "run python BuildTools/docs_font_format.py --write"
                )

    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_docs_font_format.py",
            "BuildTools/docs_font_format.py --check",
        ):
            if marker not in workflow_text:
                errors.append(f"documentation font format workflow is missing: {marker}")

    audio_reference = generated_artifacts.get("audio_reference")
    if not isinstance(audio_reference, dict):
        errors.append("documentation manifest must declare generated_artifacts.audio_reference")
        return

    expected_audio_reference = {
        "source_manifest": docs_audio.DEFAULT_MANIFEST,
        "model": docs_audio.DEFAULT_MODEL,
        "generator": "BuildTools/docs_audio.py",
        "schema_version": docs_audio.SCHEMA_VERSION,
        "directory": docs_audio.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_audio.OUTPUT_PATHS),
    }
    for field, expected in expected_audio_reference.items():
        if audio_reference.get(field) != expected:
            errors.append(f"documentation audio reference {field} must be {expected}")

    try:
        rendered_audio_model = docs_audio.render_audio_model(root)
        rendered_audio_pages = docs_audio.render_reference_pages(root)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        errors.append(f"unable to render generated audio documentation: {exception}")
    else:
        audio_model_path = root / docs_audio.DEFAULT_MODEL
        if not audio_model_path.is_file():
            errors.append(
                "generated documentation audio model is missing: "
                + docs_audio.DEFAULT_MODEL
            )
        elif audio_model_path.read_text(encoding="utf-8") != rendered_audio_model:
            errors.append(
                "generated documentation audio model is stale; "
                "run python BuildTools/docs_audio.py --write"
            )
        for page_path, rendered_page in rendered_audio_pages.items():
            output_path = root / page_path
            if not output_path.is_file():
                errors.append(
                    f"generated documentation audio page is missing: {page_path}"
                )
            elif output_path.read_text(encoding="utf-8") != rendered_page:
                errors.append(
                    f"generated documentation audio page is stale: {page_path}; "
                    "run python BuildTools/docs_audio.py --write"
                )

    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_docs_audio.py",
            "BuildTools/docs_audio.py --check",
        ):
            if marker not in workflow_text:
                errors.append(f"documentation audio workflow is missing: {marker}")

    video_reference = generated_artifacts.get("video_reference")
    if not isinstance(video_reference, dict):
        errors.append(
            "documentation manifest must declare generated_artifacts.video_reference"
        )
        return

    expected_video_reference = {
        "source_manifest": docs_video.DEFAULT_MANIFEST,
        "model": docs_video.DEFAULT_MODEL,
        "generator": "BuildTools/docs_video.py",
        "schema_version": docs_video.SCHEMA_VERSION,
        "directory": docs_video.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_video.OUTPUT_PATHS),
    }
    for field, expected in expected_video_reference.items():
        if video_reference.get(field) != expected:
            errors.append(f"documentation video reference {field} must be {expected}")

    try:
        rendered_video_model = docs_video.render_video_model(root)
        rendered_video_pages = docs_video.render_reference_pages(root)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        errors.append(f"unable to render generated video documentation: {exception}")
    else:
        video_model_path = root / docs_video.DEFAULT_MODEL
        if not video_model_path.is_file():
            errors.append(
                "generated documentation video model is missing: "
                + docs_video.DEFAULT_MODEL
            )
        elif video_model_path.read_text(encoding="utf-8") != rendered_video_model:
            errors.append(
                "generated documentation video model is stale; "
                "run python BuildTools/docs_video.py --write"
            )
        for page_path, rendered_page in rendered_video_pages.items():
            output_path = root / page_path
            if not output_path.is_file():
                errors.append(
                    f"generated documentation video page is missing: {page_path}"
                )
            elif output_path.read_text(encoding="utf-8") != rendered_page:
                errors.append(
                    f"generated documentation video page is stale: {page_path}; "
                    "run python BuildTools/docs_video.py --write"
                )

    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_docs_video.py",
            "BuildTools/docs_video.py --check",
        ):
            if marker not in workflow_text:
                errors.append(f"documentation video workflow is missing: {marker}")

    gui_runtime_reference = generated_artifacts.get("gui_runtime_reference")
    if not isinstance(gui_runtime_reference, dict):
        errors.append(
            "documentation manifest must declare "
            "generated_artifacts.gui_runtime_reference"
        )
        return

    expected_gui_runtime_reference = {
        "source_manifest": docs_gui_runtime.DEFAULT_MANIFEST,
        "model": docs_gui_runtime.DEFAULT_MODEL,
        "generator": "BuildTools/docs_gui_runtime.py",
        "schema_version": docs_gui_runtime.SCHEMA_VERSION,
        "directory": docs_gui_runtime.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_gui_runtime.OUTPUT_PATHS),
    }
    for field, expected in expected_gui_runtime_reference.items():
        if gui_runtime_reference.get(field) != expected:
            errors.append(
                f"documentation GUI runtime reference {field} must be {expected}"
            )

    try:
        rendered_gui_runtime_model = docs_gui_runtime.render_gui_runtime_model(root)
        gui_runtime_model = json.loads(rendered_gui_runtime_model)
        rendered_gui_runtime_pages = docs_gui_runtime.render_reference_pages(root)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        errors.append(
            f"unable to render generated GUI runtime documentation: {exception}"
        )
    else:
        gui_runtime_model_path = root / docs_gui_runtime.DEFAULT_MODEL
        if not gui_runtime_model_path.is_file():
            errors.append(
                "generated documentation GUI runtime model is missing: "
                + docs_gui_runtime.DEFAULT_MODEL
            )
        elif (
            gui_runtime_model_path.read_text(encoding="utf-8")
            != rendered_gui_runtime_model
        ):
            errors.append(
                "generated documentation GUI runtime model is stale; "
                "run python BuildTools/docs_gui_runtime.py --write"
            )
        for page_path, rendered_page in rendered_gui_runtime_pages.items():
            output_path = root / page_path
            if not output_path.is_file():
                errors.append(
                    "generated documentation GUI runtime page is missing: "
                    + page_path
                )
            elif output_path.read_text(encoding="utf-8") != rendered_page:
                errors.append(
                    f"generated documentation GUI runtime page is stale: "
                    f"{page_path}; run python "
                    "BuildTools/docs_gui_runtime.py --write"
                )

    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_docs_gui_runtime.py",
            "BuildTools/docs_gui_runtime.py --check",
        ):
            if marker not in workflow_text:
                errors.append(
                    f"documentation GUI runtime workflow is missing: {marker}"
                )

    ai_control_protocol_reference = generated_artifacts.get(
        "ai_control_protocol_reference"
    )
    if not isinstance(ai_control_protocol_reference, dict):
        errors.append(
            "documentation manifest must declare "
            "generated_artifacts.ai_control_protocol_reference"
        )
        return

    expected_ai_control_protocol_reference = {
        "source_manifest": docs_ai_control_protocol.DEFAULT_MANIFEST,
        "model": docs_ai_control_protocol.DEFAULT_MODEL,
        "generator": "BuildTools/docs_ai_control_protocol.py",
        "schema_version": docs_ai_control_protocol.SCHEMA_VERSION,
        "directory": docs_ai_control_protocol.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_ai_control_protocol.OUTPUT_PATHS),
        "reference_client": "BuildTools/ai_control_client.py",
        "sample": "Examples/AiControlSample",
        "smoke": "Examples/AiControlSample/run_protocol_smoke.py",
    }
    for field, expected in expected_ai_control_protocol_reference.items():
        if ai_control_protocol_reference.get(field) != expected:
            errors.append(
                f"documentation AiControl protocol reference {field} must be {expected}"
            )

    try:
        rendered_ai_control_protocol_model = (
            docs_ai_control_protocol.render_ai_control_protocol_model(root)
        )
        ai_control_protocol_model = json.loads(rendered_ai_control_protocol_model)
        rendered_ai_control_protocol_pages = (
            docs_ai_control_protocol.render_reference_pages(root)
        )
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        errors.append(
            "unable to render generated AiControl protocol documentation: "
            f"{exception}"
        )
    else:
        ai_control_protocol_model_path = (
            root / docs_ai_control_protocol.DEFAULT_MODEL
        )
        if not ai_control_protocol_model_path.is_file():
            errors.append(
                "generated documentation AiControl protocol model is missing: "
                + docs_ai_control_protocol.DEFAULT_MODEL
            )
        elif (
            ai_control_protocol_model_path.read_text(encoding="utf-8")
            != rendered_ai_control_protocol_model
        ):
            errors.append(
                "generated documentation AiControl protocol model is stale; "
                "run python BuildTools/docs_ai_control_protocol.py --write"
            )
        for page_path, rendered_page in rendered_ai_control_protocol_pages.items():
            output_path = root / page_path
            if not output_path.is_file():
                errors.append(
                    "generated documentation AiControl protocol page is missing: "
                    + page_path
                )
            elif output_path.read_text(encoding="utf-8") != rendered_page:
                errors.append(
                    "generated documentation AiControl protocol page is stale: "
                    f"{page_path}; run python "
                    "BuildTools/docs_ai_control_protocol.py --write"
                )

    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_ai_control_protocol.py",
            "BuildTools/tests/test_docs_ai_control_protocol.py",
            "BuildTools/docs_ai_control_protocol.py --check",
        ):
            if marker not in workflow_text:
                errors.append(
                    "documentation AiControl protocol workflow is missing: "
                    + marker
                )

    package_reference = generated_artifacts.get("package_reference")
    if not isinstance(package_reference, dict):
        errors.append("documentation manifest must declare generated_artifacts.package_reference")
        return

    expected_package_reference = {
        "source_manifest": docs_package.DEFAULT_MANIFEST,
        "source_parser": docs_package.DEFAULT_SOURCE,
        "model": docs_package.DEFAULT_MODEL,
        "generator": "BuildTools/docs_package.py",
        "schema_version": docs_package.SCHEMA_VERSION,
        "directory": docs_package.DEFAULT_OUTPUT_DIR,
        "paths": list(docs_package.OUTPUT_PATHS),
        "structural_test": "BuildTools/tests/validate_package_interface.cmake",
    }
    for field, expected in expected_package_reference.items():
        if package_reference.get(field) != expected:
            errors.append(f"documentation package reference {field} must be {expected}")

    try:
        rendered_package_model = docs_package.render_package_model(root)
        rendered_package_pages = docs_package.render_reference_pages(root)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        errors.append(f"unable to render generated package interface documentation: {exception}")
    else:
        package_model_path = root / docs_package.DEFAULT_MODEL
        if not package_model_path.is_file():
            errors.append(f"generated documentation package model is missing: {docs_package.DEFAULT_MODEL}")
        elif package_model_path.read_text(encoding="utf-8") != rendered_package_model:
            errors.append("generated documentation package model is stale; run python BuildTools/docs_package.py --write")

        for package_page_path, rendered_package_page in rendered_package_pages.items():
            output_path = root / package_page_path
            if not output_path.is_file():
                errors.append(f"generated documentation package page is missing: {package_page_path}")
            elif output_path.read_text(encoding="utf-8") != rendered_package_page:
                errors.append(
                    f"generated documentation package page is stale: {package_page_path}; "
                    "run python BuildTools/docs_package.py --write"
                )

    package_structural_test = root / expected_package_reference["structural_test"]
    if not package_structural_test.is_file():
        errors.append(f"package interface structural test is missing: {expected_package_reference['structural_test']}")

    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_docs_package.py",
            "BuildTools/docs_package.py --check",
            "cmake -P BuildTools/tests/validate_package_interface.cmake",
        ):
            if marker not in workflow_text:
                errors.append(f"documentation package reference workflow is missing: {marker}")

    public_examples_reference = generated_artifacts.get("public_examples_reference")
    if not isinstance(public_examples_reference, dict):
        errors.append("documentation manifest must declare generated_artifacts.public_examples_reference")
    else:
        expected_public_examples_reference = {
            "source_manifest": docs_examples.DEFAULT_MANIFEST,
            "model": docs_examples.DEFAULT_MODEL,
            "generator": docs_examples.GENERATED_BY,
            "schema_version": docs_examples.SCHEMA_VERSION,
            "directory": str(PurePosixPath(docs_examples.DEFAULT_INDEX).parent),
            "paths": list(docs_examples.OUTPUT_PATHS),
        }
        for field, expected in expected_public_examples_reference.items():
            if public_examples_reference.get(field) != expected:
                errors.append(f"documentation public examples reference {field} must be {expected}")

        try:
            rendered_public_example_outputs = docs_examples.render_outputs(root)
        except (OSError, json.JSONDecodeError, ValueError) as exception:
            errors.append(f"unable to render public example repository documentation: {exception}")
        else:
            for relative_path, expected_content in rendered_public_example_outputs.items():
                output_path = root / relative_path
                if not output_path.is_file():
                    errors.append(f"generated public example repository artifact is missing: {relative_path}")
                elif output_path.read_text(encoding="utf-8") != expected_content:
                    errors.append(
                        f"generated public example repository artifact is stale: {relative_path}; "
                        "run python BuildTools/docs_examples.py --write"
                    )

        if workflow_path.is_file():
            workflow_text = workflow_path.read_text(encoding="utf-8")
            for marker in (
                "BuildTools/tests/test_docs_examples.py",
                "BuildTools/docs_examples.py --check",
            ):
                if marker not in workflow_text:
                    errors.append(f"documentation public examples workflow is missing: {marker}")

    support_matrix = generated_artifacts.get("support_matrix")
    if not isinstance(support_matrix, dict):
        errors.append("documentation manifest must declare generated_artifacts.support_matrix")
    else:
        expected_support_matrix = {
            "source_manifest": docs_support_matrix.DEFAULT_MANIFEST,
            "model": docs_support_matrix.DEFAULT_MODEL,
            "generator": docs_support_matrix.GENERATED_BY,
            "schema_version": docs_support_matrix.SCHEMA_VERSION,
            "directory": str(PurePosixPath(docs_support_matrix.DEFAULT_INDEX).parent),
            "paths": list(docs_support_matrix.OUTPUT_PATHS),
        }
        for field, expected in expected_support_matrix.items():
            if support_matrix.get(field) != expected:
                errors.append(f"documentation support matrix {field} must be {expected}")

        try:
            rendered_support_outputs = docs_support_matrix.render_outputs(root)
        except (OSError, json.JSONDecodeError, ValueError) as exception:
            errors.append(f"unable to render support matrix documentation: {exception}")
        else:
            for relative_path, expected_content in rendered_support_outputs.items():
                output_path = root / relative_path
                if not output_path.is_file():
                    errors.append(f"generated support matrix artifact is missing: {relative_path}")
                elif output_path.read_text(encoding="utf-8") != expected_content:
                    errors.append(
                        f"generated support matrix artifact is stale: {relative_path}; "
                        "run python BuildTools/docs_support_matrix.py --write"
                    )

        if workflow_path.is_file():
            workflow_text = workflow_path.read_text(encoding="utf-8")
            for marker in (
                "BuildTools/tests/test_docs_support_matrix.py",
                "BuildTools/docs_support_matrix.py --check",
            ):
                if marker not in workflow_text:
                    errors.append(f"documentation support matrix workflow is missing: {marker}")

    public_api_document = (
        documents.get(docs_public_api.DEFAULT_OUTPUT)
        if isinstance(documents, dict)
        else None
    )
    public_api_required = (
        isinstance(public_api_document, dict)
        and public_api_document.get("state") == "current"
    )
    public_api_contract = generated_artifacts.get("public_api_contract")
    if public_api_required and not isinstance(public_api_contract, dict):
        errors.append(
            "documentation manifest must declare "
            "generated_artifacts.public_api_contract for the current public contract index"
        )
    elif isinstance(public_api_contract, dict):
        expected_public_api_contract = {
            "path": docs_public_api.DEFAULT_OUTPUT,
            "paths": list(docs_public_api.OUTPUT_PATHS),
            "generator": docs_public_api.GENERATED_BY,
            "source_models": docs_public_api.SOURCE_MODELS,
            "source_references": docs_public_api.SOURCE_REFERENCES,
        }
        for field, expected in expected_public_api_contract.items():
            if public_api_contract.get(field) != expected:
                errors.append(
                    f"documentation public API contract {field} must be {expected}"
                )

        try:
            rendered_public_api_pages = docs_public_api.generate_public_api_pages(root)
        except (
            OSError,
            json.JSONDecodeError,
            ValueError,
            docs_contract_diff.ContractDiffError,
        ) as exception:
            errors.append(f"unable to render public API contract index: {exception}")
        else:
            for relative_path, rendered_page in rendered_public_api_pages.items():
                output_path = root / relative_path
                if not output_path.is_file():
                    errors.append(
                        f"generated public API contract page is missing: {relative_path}"
                    )
                elif output_path.read_text(encoding="utf-8") != rendered_page:
                    errors.append(
                        f"generated public API contract page is stale: {relative_path}; "
                        "run python BuildTools/docs_public_api.py --write"
                    )

        if workflow_path.is_file():
            workflow_text = workflow_path.read_text(encoding="utf-8")
            for marker in (
                "BuildTools/tests/test_docs_public_api.py",
                "BuildTools/docs_public_api.py --check",
            ):
                if marker not in workflow_text:
                    errors.append(
                        f"documentation public API workflow is missing: {marker}"
                    )

    external_evidence_document = (
        documents.get(docs_external_evidence.DEFAULT_INDEX)
        if isinstance(documents, dict)
        else None
    )
    external_evidence_required = (
        isinstance(external_evidence_document, dict)
        and external_evidence_document.get("state") == "current"
    )
    external_evidence = generated_artifacts.get("external_project_evidence")
    if external_evidence_required and not isinstance(external_evidence, dict):
        errors.append(
            "documentation manifest must declare "
            "generated_artifacts.external_project_evidence for the current audit"
        )
    elif isinstance(external_evidence, dict):
        expected_external_evidence = {
            "visibility": "internal",
            "source_manifest": docs_external_evidence.DEFAULT_MANIFEST,
            "model": docs_external_evidence.DEFAULT_MODEL,
            "generator": docs_external_evidence.GENERATED_BY,
            "schema_version": docs_external_evidence.SCHEMA_VERSION,
            "directory": str(
                PurePosixPath(docs_external_evidence.DEFAULT_INDEX).parent
            ),
            "paths": list(docs_external_evidence.OUTPUT_PATHS),
            "external_source_verification": (
                "python BuildTools/docs_external_evidence.py --check "
                "--verify-sources --last-frontier-root ../ "
                "--tla-root Workspace/fonline-tla-audit"
            ),
        }
        for field, expected in expected_external_evidence.items():
            if external_evidence.get(field) != expected:
                errors.append(
                    f"documentation external project evidence {field} must be "
                    f"{expected}"
                )

        try:
            rendered_external_evidence = docs_external_evidence.render_outputs(root)
        except (OSError, json.JSONDecodeError, ValueError) as exception:
            errors.append(
                f"unable to render external project evidence documentation: "
                f"{exception}"
            )
        else:
            for relative_path, expected_content in rendered_external_evidence.items():
                output_path = root / relative_path
                if not output_path.is_file():
                    errors.append(
                        f"generated external project evidence artifact is missing: "
                        f"{relative_path}"
                    )
                elif output_path.read_text(encoding="utf-8") != expected_content:
                    errors.append(
                        f"generated external project evidence artifact is stale: "
                        f"{relative_path}; run python "
                        "BuildTools/docs_external_evidence.py --write"
                    )

        if workflow_path.is_file():
            workflow_text = workflow_path.read_text(encoding="utf-8")
            for marker in (
                "BuildTools/tests/test_docs_external_evidence.py",
                "BuildTools/docs_external_evidence.py --check",
            ):
                if marker not in workflow_text:
                    errors.append(
                        f"documentation external project evidence workflow is "
                        f"missing: {marker}"
                    )

    documentation_diagrams = generated_artifacts.get("documentation_diagrams")
    if not isinstance(documentation_diagrams, dict):
        errors.append(
            "documentation manifest must declare "
            "generated_artifacts.documentation_diagrams"
        )
    else:
        expected_documentation_diagrams = {
            "source_manifest": docs_diagrams.DEFAULT_MANIFEST,
            "model": docs_diagrams.DEFAULT_CATALOG,
            "generator": docs_diagrams.GENERATED_BY,
            "schema_version": docs_diagrams.SCHEMA_VERSION,
            "directory": docs_diagrams.DEFAULT_OUTPUT_DIR,
            "paths": list(docs_diagrams.OUTPUT_PATHS),
        }
        for field, expected in expected_documentation_diagrams.items():
            if documentation_diagrams.get(field) != expected:
                errors.append(
                    f"documentation diagrams {field} must be {expected}"
                )

        try:
            rendered_diagram_outputs = docs_diagrams.render_outputs(root)
        except (OSError, json.JSONDecodeError, ValueError) as exception:
            errors.append(f"unable to render documentation diagrams: {exception}")
        else:
            for relative_path, expected_content in rendered_diagram_outputs.items():
                output_path = root / relative_path
                if not output_path.is_file():
                    errors.append(
                        f"generated documentation diagram artifact is missing: "
                        f"{relative_path}"
                    )
                elif output_path.read_text(encoding="utf-8") != expected_content:
                    errors.append(
                        f"generated documentation diagram artifact is stale: "
                        f"{relative_path}; run python "
                        "BuildTools/docs_diagrams.py --write"
                    )

        if workflow_path.is_file():
            workflow_text = workflow_path.read_text(encoding="utf-8")
            for marker in (
                "BuildTools/tests/test_docs_diagrams.py",
                "BuildTools/docs_diagrams.py --check",
            ):
                if marker not in workflow_text:
                    errors.append(
                        f"documentation diagram workflow is missing: {marker}"
                    )

    documentation_screenshots = generated_artifacts.get(
        "documentation_screenshots"
    )
    if not isinstance(documentation_screenshots, dict):
        errors.append(
            "documentation manifest must declare "
            "generated_artifacts.documentation_screenshots"
        )
    else:
        expected_documentation_screenshots = {
            "source_manifest": docs_screenshots.DEFAULT_MANIFEST,
            "model": docs_screenshots.DEFAULT_CATALOG,
            "generator": docs_screenshots.GENERATED_BY,
            "schema_version": docs_screenshots.SCHEMA_VERSION,
            "directory": docs_screenshots.DEFAULT_OUTPUT_DIR,
            "paths": list(docs_screenshots.MANIFEST_PATHS),
        }
        for field, expected in expected_documentation_screenshots.items():
            if documentation_screenshots.get(field) != expected:
                errors.append(
                    f"documentation screenshots {field} must be {expected}"
                )

        try:
            rendered_screenshot_outputs = docs_screenshots.render_outputs(root)
        except (OSError, json.JSONDecodeError, ValueError) as exception:
            errors.append(
                f"unable to validate documentation screenshots: {exception}"
            )
        else:
            for relative_path, expected_content in (
                rendered_screenshot_outputs.items()
            ):
                output_path = root / relative_path
                if not output_path.is_file():
                    errors.append(
                        "generated documentation screenshot artifact is "
                        f"missing: {relative_path}"
                    )
                elif output_path.read_text(
                    encoding="utf-8"
                ) != expected_content:
                    errors.append(
                        "generated documentation screenshot artifact is "
                        f"stale: {relative_path}; run python "
                        "BuildTools/docs_screenshots.py --write"
                    )

        if workflow_path.is_file():
            workflow_text = workflow_path.read_text(encoding="utf-8")
            for marker in (
                "BuildTools/tests/test_docs_screenshots.py",
                "BuildTools/docs_screenshots.py --check",
            ):
                if marker not in workflow_text:
                    errors.append(
                        f"documentation screenshot workflow is missing: {marker}"
                    )

    localization_status = generated_artifacts.get("localization_status")
    if not isinstance(localization_status, dict):
        errors.append(
            "documentation manifest must declare generated_artifacts.localization_status"
        )
    else:
        expected_localization_status = {
            "source_manifest": docs_localization.DEFAULT_MANIFEST,
            "glossary": docs_localization.DEFAULT_GLOSSARY,
            "path": docs_localization.DEFAULT_OUTPUT,
            "generator": docs_localization.GENERATED_BY,
            "schema_version": docs_localization.SCHEMA_VERSION,
        }
        for field, expected in expected_localization_status.items():
            if localization_status.get(field) != expected:
                errors.append(
                    f"documentation localization status {field} must be {expected}"
                )

        try:
            expected_localization_content = docs_localization.render_localization_status(
                root
            )
        except (OSError, json.JSONDecodeError, ValueError) as exception:
            errors.append(f"unable to render documentation localization status: {exception}")
        else:
            output_path = root / docs_localization.DEFAULT_OUTPUT
            if not output_path.is_file():
                errors.append(
                    "generated documentation localization status is missing: "
                    + docs_localization.DEFAULT_OUTPUT
                )
            elif output_path.read_text(encoding="utf-8") != expected_localization_content:
                errors.append(
                    "generated documentation localization status is stale; "
                    "run python BuildTools/docs_localization.py --write"
                )

        if workflow_path.is_file():
            workflow_text = workflow_path.read_text(encoding="utf-8")
            for marker in (
                "BuildTools/tests/test_docs_localization.py",
                "BuildTools/docs_localization.py --check",
            ):
                if marker not in workflow_text:
                    errors.append(
                        f"documentation localization workflow is missing: {marker}"
                    )

    description_translation_status = generated_artifacts.get(
        "description_translation_status"
    )
    if not isinstance(description_translation_status, dict):
        errors.append(
            "documentation manifest must declare "
            "generated_artifacts.description_translation_status"
        )
    else:
        expected_description_translation_status = {
            "source_catalog": docs_description_translations.DEFAULT_CATALOG,
            "models": docs_description_translations.MODEL_PATHS,
            "path": docs_description_translations.DEFAULT_OUTPUT,
            "generator": docs_description_translations.GENERATED_BY,
            "schema_version": docs_description_translations.SCHEMA_VERSION,
        }
        for field, expected in expected_description_translation_status.items():
            if description_translation_status.get(field) != expected:
                errors.append(
                    f"documentation generated-description translation status {field} "
                    f"must be {expected}"
                )

        try:
            expected_description_translation_content = (
                docs_description_translations.render_status(root)
            )
        except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as exception:
            errors.append(
                "unable to render documentation generated-description translation "
                f"status: {exception}"
            )
        else:
            output_path = root / docs_description_translations.DEFAULT_OUTPUT
            if not output_path.is_file():
                errors.append(
                    "generated documentation description translation status is missing: "
                    + docs_description_translations.DEFAULT_OUTPUT
                )
            elif (
                output_path.read_text(encoding="utf-8")
                != expected_description_translation_content
            ):
                errors.append(
                    "generated documentation description translation status is stale; "
                    "run python BuildTools/docs_description_translations.py --write"
                )

        if workflow_path.is_file():
            workflow_text = workflow_path.read_text(encoding="utf-8")
            for marker in (
                "BuildTools/tests/test_docs_description_translations.py",
                "BuildTools/docs_description_translations.py --check",
            ):
                if marker not in workflow_text:
                    errors.append(
                        "documentation generated-description translation workflow is "
                        f"missing: {marker}"
                    )

    ai_delivery = generated_artifacts.get("ai_delivery")
    if not isinstance(ai_delivery, dict):
        errors.append("documentation manifest must declare generated_artifacts.ai_delivery")
    else:
        expected_ai_delivery = {
            "source_manifest": docs_ai_delivery.DEFAULT_MANIFEST,
            "generator": docs_ai_delivery.GENERATED_BY,
            "schema_version": docs_ai_delivery.SCHEMA_VERSION,
            "paths": list(docs_ai_delivery.OUTPUT_PATHS),
        }
        for field, expected in expected_ai_delivery.items():
            if ai_delivery.get(field) != expected:
                errors.append(f"documentation AI delivery {field} must be {expected}")

        try:
            rendered_ai_outputs = docs_ai_delivery.render_outputs(root)
        except ValueError as exception:
            errors.append(f"invalid documentation AI delivery: {exception}")
        else:
            for relative_path, expected_content in rendered_ai_outputs.items():
                output_path = root / relative_path
                if not output_path.is_file():
                    errors.append(f"generated documentation AI artifact is missing: {relative_path}")
                elif output_path.read_text(encoding="utf-8") != expected_content:
                    errors.append(
                        f"generated documentation AI artifact is stale: {relative_path}; "
                        "run python BuildTools/docs_ai_delivery.py --write"
                    )

        if workflow_path.is_file():
            workflow_text = workflow_path.read_text(encoding="utf-8")
            for marker in (
                "BuildTools/tests/test_docs_ai_delivery.py",
                "BuildTools/docs_ai_delivery.py --check",
            ):
                if marker not in workflow_text:
                    errors.append(f"documentation AI delivery workflow is missing: {marker}")

    ai_evaluation = generated_artifacts.get("ai_evaluation")
    if not isinstance(ai_evaluation, dict):
        errors.append(
            "documentation manifest must declare generated_artifacts.ai_evaluation"
        )
    else:
        expected_ai_evaluation = {
            "source": docs_ai_eval.DEFAULT_SOURCE,
            "search": docs_ai_eval.DEFAULT_SEARCH,
            "path": docs_ai_eval.DEFAULT_OUTPUT,
            "generator": docs_ai_eval.GENERATED_BY,
            "schema_version": docs_ai_eval.SCHEMA_VERSION,
            "test": "BuildTools/tests/test_docs_ai_eval.py",
        }
        for field, expected in expected_ai_evaluation.items():
            if ai_evaluation.get(field) != expected:
                errors.append(
                    f"documentation AI evaluation {field} must be {expected}"
                )

        try:
            evaluation_report = docs_ai_eval.evaluate(root)
        except ValueError as exception:
            errors.append(f"invalid documentation AI evaluation: {exception}")
        else:
            for evaluation_error in evaluation_report["errors"]:
                errors.append(f"documentation AI evaluation: {evaluation_error}")
            expected_content = (
                json.dumps(evaluation_report, indent=2, ensure_ascii=True) + "\n"
            )
            output_path = root / docs_ai_eval.DEFAULT_OUTPUT
            if not output_path.is_file():
                errors.append(
                    "generated documentation AI evaluation report is missing: "
                    + docs_ai_eval.DEFAULT_OUTPUT
                )
            elif output_path.read_text(encoding="utf-8") != expected_content:
                errors.append(
                    "generated documentation AI evaluation report is stale; "
                    "run python BuildTools/docs_ai_eval.py --write"
                )

        if workflow_path.is_file():
            workflow_text = workflow_path.read_text(encoding="utf-8")
            for marker in (
                "BuildTools/tests/test_docs_ai_eval.py",
                "BuildTools/tests/test_docs_ai_model_eval.py",
                "BuildTools/tests/test_docs_ai_model_review.py",
                "BuildTools/docs_ai_eval.py --check",
            ):
                if marker not in workflow_text:
                    errors.append(
                        f"documentation AI evaluation workflow is missing: {marker}"
                    )

    snippet_validation = generated_artifacts.get("snippet_validation")
    if not isinstance(snippet_validation, dict):
        errors.append(
            "documentation manifest must declare generated_artifacts.snippet_validation"
        )
    else:
        expected_snippet_validation = {
            "source_policy": docs_snippets.DEFAULT_POLICY,
            "path": docs_snippets.DEFAULT_OUTPUT,
            "generator": docs_snippets.GENERATED_BY,
            "schema_version": docs_snippets.SCHEMA_VERSION,
            "test": "BuildTools/tests/test_docs_snippets.py",
            "external_check": "python3 BuildTools/docs_snippets.py --check --external",
        }
        for field, expected in expected_snippet_validation.items():
            if snippet_validation.get(field) != expected:
                errors.append(
                    f"documentation snippet validation {field} must be {expected}"
                )

        try:
            snippet_report = docs_snippets.evaluate(root)
        except ValueError as exception:
            errors.append(f"invalid documentation snippet validation: {exception}")
        else:
            for snippet_error in snippet_report["errors"]:
                errors.append(f"documentation snippet validation: {snippet_error}")
            expected_content = (
                json.dumps(snippet_report, indent=2, ensure_ascii=True) + "\n"
            )
            output_path = root / docs_snippets.DEFAULT_OUTPUT
            if not output_path.is_file():
                errors.append(
                    "generated documentation snippet report is missing: "
                    + docs_snippets.DEFAULT_OUTPUT
                )
            elif output_path.read_text(encoding="utf-8") != expected_content:
                errors.append(
                    "generated documentation snippet report is stale; "
                    "run python BuildTools/docs_snippets.py --write"
                )

        if workflow_path.is_file():
            workflow_text = workflow_path.read_text(encoding="utf-8")
            for marker in (
                "BuildTools/tests/test_docs_snippets.py",
                "BuildTools/docs_snippets.py --check",
                "BuildTools/docs_snippets.py --check --external",
            ):
                if marker not in workflow_text:
                    errors.append(
                        f"documentation snippet workflow is missing: {marker}"
                    )

    site_delivery = generated_artifacts.get("site_delivery")
    if not isinstance(site_delivery, dict):
        errors.append("documentation manifest must declare generated_artifacts.site_delivery")
    else:
        browser_policy = (
            site_delivery_config.get("browser_validation")
            if isinstance(site_delivery_config, dict)
            else None
        )
        expected_browser_policy = {
            "schema_version": 1,
            "node": "24.16.0",
            "playwright": "1.62.0",
            "axe_core": "4.12.1",
            "wcag_target": "WCAG 2.2 Level AA automated axe-core subset",
            "wcag_tags": [
                "wcag2a",
                "wcag2aa",
                "wcag21a",
                "wcag21aa",
                "wcag22a",
                "wcag22aa",
            ],
            "profiles": [
                {"id": "desktop", "width": 1440, "height": 1000},
                {"id": "mobile", "width": 390, "height": 844},
                {
                    "id": "zoom-200",
                    "width": 640,
                    "height": 512,
                    "physical_width": 1280,
                    "physical_height": 1024,
                    "device_scale_factor": 2,
                    "zoom_percent": 200,
                    "compact_navigation": True,
                },
            ],
        }
        if browser_policy != expected_browser_policy:
            errors.append(
                "documentation site browser validation policy must match the "
                "pinned desktop/mobile/200-percent-zoom WCAG 2.2 AA contract"
            )

        expected_site_delivery = {
            "source_manifest": docs_site.DEFAULT_MANIFEST,
            "generator": docs_site.GENERATED_BY,
            "schema_version": docs_site.SCHEMA_VERSION,
            "paths": list(docs_site.OUTPUT_PATHS),
            "layout": "_layouts/default.html",
            "assets": [
                "assets/css/docs.css",
                "assets/js/docs.js",
                "assets/images/fonline-mark.png",
            ],
            "artifact_validator": "BuildTools/docs_site_artifact.py",
            "artifact_report": "Workspace/docs-site-artifact-report.json",
            "browser_validator": "BuildTools/docs-browser/audit.mjs",
            "browser_package_lock": "BuildTools/docs-browser/package-lock.json",
            "browser_report": "Workspace/docs-browser-audit-report.json",
            "browser_screenshots": "Workspace/docs-browser-screenshots",
        }
        for field, expected in expected_site_delivery.items():
            if site_delivery.get(field) != expected:
                errors.append(f"documentation site delivery {field} must be {expected}")

        try:
            rendered_site_outputs = docs_site.render_outputs(root)
        except ValueError as exception:
            errors.append(f"invalid documentation site delivery: {exception}")
        else:
            for relative_path, expected_content in rendered_site_outputs.items():
                output_path = root / relative_path
                if not output_path.is_file():
                    errors.append(f"generated documentation site artifact is missing: {relative_path}")
                elif output_path.read_text(encoding="utf-8") != expected_content:
                    errors.append(
                        f"generated documentation site artifact is stale: {relative_path}; "
                        "run python BuildTools/docs_site.py --write"
                    )

        for relative_path in [expected_site_delivery["layout"], *expected_site_delivery["assets"]]:
            if not (root / relative_path).is_file():
                errors.append(f"documentation site rendering asset is missing: {relative_path}")

        if workflow_path.is_file():
            workflow_text = workflow_path.read_text(encoding="utf-8")
            for marker in (
                "BuildTools/tests/test_docs_site.py",
                "BuildTools/tests/test_docs_site_layout.py",
                "BuildTools/tests/test_docs_site_artifact.py",
                "BuildTools/tests/test_docs_browser.py",
                "BuildTools/docs_site.py --check",
                "BuildTools/docs_site_artifact.py",
                "playwright install --with-deps chromium",
                "npm run audit",
            ):
                if marker not in workflow_text:
                    errors.append(f"documentation site delivery workflow is missing: {marker}")

    contract_diff = generated_artifacts.get("contract_diff")
    if not isinstance(contract_diff, dict):
        errors.append("documentation manifest must declare generated_artifacts.contract_diff")
        return

    expected_contract_diff = {
        "generator": "BuildTools/docs_contract_diff.py",
        "domain_generators": {
            "api": "BuildTools/docs_api_diff.py",
            "cmake": "BuildTools/docs_contract_diff.py",
            "cli": "BuildTools/docs_contract_diff.py",
            "package": "BuildTools/docs_contract_diff.py",
            "helper-cli": "BuildTools/docs_contract_diff.py",
            "native-extension": "BuildTools/docs_contract_diff.py",
            "prototype-format": "BuildTools/docs_contract_diff.py",
            "map-format": "BuildTools/docs_contract_diff.py",
            "model-format": "BuildTools/docs_contract_diff.py",
            "text-format": "BuildTools/docs_contract_diff.py",
            "effect-format": "BuildTools/docs_contract_diff.py",
            "image-format": "BuildTools/docs_contract_diff.py",
            "particle-format": "BuildTools/docs_contract_diff.py",
            "font-format": "BuildTools/docs_contract_diff.py",
            "audio": "BuildTools/docs_contract_diff.py",
            "video": "BuildTools/docs_contract_diff.py",
            "gui-runtime": "BuildTools/docs_contract_diff.py",
            "ai-control-protocol": "BuildTools/docs_contract_diff.py",
        },
        "source_models": {
            "api": docs_api.DEFAULT_OUTPUT,
            "cmake": docs_cmake.DEFAULT_MODEL,
            "cli": docs_cli.DEFAULT_MODEL,
            "package": docs_package.DEFAULT_MODEL,
            "helper-cli": docs_helper_cli.DEFAULT_MODEL,
            "native-extension": docs_native_extension.DEFAULT_MODEL,
            "prototype-format": docs_prototype_format.DEFAULT_MODEL,
            "map-format": docs_map_format.DEFAULT_MODEL,
            "model-format": docs_model_format.DEFAULT_MODEL,
            "text-format": docs_text_format.DEFAULT_MODEL,
            "effect-format": docs_effect_format.DEFAULT_MODEL,
            "image-format": docs_image_format.DEFAULT_MODEL,
            "particle-format": docs_particle_format.DEFAULT_MODEL,
            "font-format": docs_font_format.DEFAULT_MODEL,
            "audio": docs_audio.DEFAULT_MODEL,
            "video": docs_video.DEFAULT_MODEL,
            "gui-runtime": docs_gui_runtime.DEFAULT_MODEL,
            "ai-control-protocol": docs_ai_control_protocol.DEFAULT_MODEL,
        },
        "dispositions": docs_contract_diff.DEFAULT_DISPOSITIONS,
        "schema_version": docs_contract_diff.SCHEMA_VERSION,
        "report_outputs": [
            docs_contract_diff.DEFAULT_JSON_OUTPUT,
            docs_contract_diff.DEFAULT_MARKDOWN_OUTPUT,
        ],
        "workflow": ".github/workflows/validate.yml",
    }
    for field, expected in expected_contract_diff.items():
        if contract_diff.get(field) != expected:
            errors.append(f"documentation contract diff {field} must be {expected}")

    dispositions_path = root / docs_contract_diff.DEFAULT_DISPOSITIONS
    if not dispositions_path.is_file():
        errors.append(
            f"documentation contract change dispositions are missing: {docs_contract_diff.DEFAULT_DISPOSITIONS}"
        )
    else:
        try:
            docs_api_diff.load_dispositions(dispositions_path)
        except (OSError, ValueError) as exception:
            errors.append(f"invalid documentation contract change dispositions: {exception}")

    workflow_path = root / ".github/workflows/validate.yml"
    if workflow_path.is_file():
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for marker in (
            "BuildTools/tests/test_docs_contract_diff.py",
            "BuildTools/docs_contract_diff.py",
            "--baseline-git-ref",
            "--enforce",
            "Docs/contract-change-dispositions.json",
            "fetch-depth: 0",
        ):
            if marker not in workflow_text:
                errors.append(f"documentation contract diff workflow is missing: {marker}")


def validate_manifest(root: Path, manifest_path: Path) -> tuple[list[str], dict[str, object]]:
    errors: list[str] = []
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        return [f"Unable to read documentation manifest: {exception}"], {}

    if manifest.get("schema_version") != 1:
        errors.append("documentation manifest schema_version must be 1")

    try:
        docs_external_evidence._validate_owner_policy(manifest)
    except ValueError as exception:
        errors.append(f"invalid documentation owner review policy: {exception}")

    inventory = manifest.get("inventory")
    _validate_publishing(
        root,
        manifest.get("publishing"),
        manifest.get("documents"),
        errors,
    )
    _validate_delivery_policy(manifest, errors)
    _validate_generated_artifacts(
        root,
        manifest.get("generated_artifacts"),
        manifest.get("site_delivery"),
        manifest.get("documents"),
        errors,
    )
    owners = manifest.get("owners")
    documents = manifest.get("documents")
    if not isinstance(inventory, dict) or not isinstance(owners, dict) or not isinstance(documents, dict):
        errors.append("documentation manifest must contain inventory, owners, and documents objects")
        return errors, manifest

    include = inventory.get("include", [])
    exclude = inventory.get("exclude", [])
    if not isinstance(include, list) or not isinstance(exclude, list):
        errors.append("documentation manifest inventory include/exclude values must be arrays")
        return errors, manifest

    discovered = _expand_inventory(root, include) - _expand_inventory(root, exclude)
    declared = set(documents)
    localized_variants: set[str] = set()
    try:
        localization_status = docs_localization.generate_localization_status(root)
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError):
        # Generated-artifact validation above reports the owning localization error
        pass
    else:
        for record in localization_status["documents"]:
            if record["status"] == "current":
                localized_variants.add(str(record["russian_path"]))

    for missing_path in sorted(discovered - declared - localized_variants):
        errors.append(f"documentation inventory is missing a manifest entry: {missing_path}")
    for stale_path in sorted(declared - discovered):
        errors.append(f"documentation manifest path is outside the inventory or missing: {stale_path}")

    seen_ids: dict[str, str] = {}
    redirect_targets: dict[str, str] = {}
    required_fields = {
        "id",
        "title",
        "audiences",
        "classification",
        "owner",
        "state",
        "disposition",
        "target",
        "sources",
    }

    for path, document in documents.items():
        label = f"manifest entry {path}"
        _validate_relative_path(path, label, errors)
        if not isinstance(document, dict):
            errors.append(f"{label} must be an object")
            continue

        missing_fields = required_fields - set(document)
        if missing_fields:
            errors.append(f"{label} is missing fields: {', '.join(sorted(missing_fields))}")
            continue

        document_id = document["id"]
        if not isinstance(document_id, str) or not re.fullmatch(r"[a-z0-9][a-z0-9-]*", document_id):
            errors.append(f"{label} has an invalid stable id: {document_id}")
        elif document_id in seen_ids:
            errors.append(f"{label} repeats id {document_id} from {seen_ids[document_id]}")
        else:
            seen_ids[document_id] = path

        if not isinstance(document["title"], str) or not document["title"].strip():
            errors.append(f"{label} must have a non-empty title")
        if not isinstance(document["audiences"], list) or not document["audiences"]:
            errors.append(f"{label} must name at least one audience")
        elif any(audience not in ALLOWED_AUDIENCES for audience in document["audiences"]):
            errors.append(f"{label} names an unknown audience")
        if document["owner"] not in owners:
            errors.append(f"{label} names unknown owner: {document['owner']}")
        if document["state"] not in ALLOWED_STATES:
            errors.append(f"{label} has invalid state: {document['state']}")
        if document["disposition"] not in ALLOWED_DISPOSITIONS:
            errors.append(f"{label} has invalid disposition: {document['disposition']}")

        classification = document["classification"]
        if not isinstance(classification, dict):
            errors.append(f"{label} classification must be an object")
            classification = {}
        else:
            diataxis = classification.get("diataxis")
            visibility = classification.get("visibility")
            translation = classification.get("translation")
            human = classification.get("human")
            if diataxis not in ALLOWED_DIATAXIS:
                errors.append(f"{label} has invalid Diataxis kind: {diataxis}")
            if visibility not in ALLOWED_VISIBILITY:
                errors.append(f"{label} has invalid visibility: {visibility}")
            if translation not in ALLOWED_TRANSLATION:
                errors.append(f"{label} has invalid translation scope: {translation}")
            if not isinstance(human, bool):
                errors.append(f"{label} classification human must be boolean")
            if visibility == "public" and human is True and translation != "required":
                errors.append(f"{label} is public human documentation and must require translation")

        if document["state"] == "redirect":
            redirect_to = document.get("redirect_to")
            if not isinstance(redirect_to, str) or not redirect_to:
                errors.append(f"{label} redirect must name redirect_to")
            else:
                redirect_targets[path] = redirect_to
            if document["disposition"] != "replace":
                errors.append(f"{label} redirect disposition must be replace")
            if classification.get("visibility") != "public":
                errors.append(f"{label} redirect must remain public")
            if classification.get("human") is not False:
                errors.append(f"{label} redirect classification human must be false")
            if classification.get("translation") != "not-required":
                errors.append(f"{label} redirect translation scope must be not-required")
        elif "redirect_to" in document:
            errors.append(f"{label} redirect_to is only valid for redirect state")

        target = document["target"]
        if not isinstance(target, str) or _validate_relative_path(target, f"{label} target", errors) is None:
            pass
        elif document["disposition"] in {"migrate", "replace"} and classification.get("visibility") == "public":
            if not target.startswith("Docs/en/"):
                errors.append(f"{label} public migration target must be under Docs/en/: {target}")
        elif document["disposition"] == "move-meta" and not target.startswith("Docs/_meta/"):
            errors.append(f"{label} internal migration target must be under Docs/_meta/: {target}")

        sources = document["sources"]
        if not isinstance(sources, list) or not sources:
            errors.append(f"{label} must name at least one source path")
        else:
            for source in sources:
                if not isinstance(source, str):
                    errors.append(f"{label} source paths must be strings")
                    continue
                if _validate_relative_path(source, f"{label} source", errors) is None:
                    continue
                if not _source_exists(root, source):
                    errors.append(f"{label} source path does not exist: {source}")

        document_file = root / path
        if document_file.is_file():
            document_text = document_file.read_text(encoding="utf-8").lower()
            first_lines = "\n".join(document_text.splitlines()[:8])
            if document["state"] == "placeholder" and PLACEHOLDER_ROUTE_MARKER not in first_lines:
                errors.append(f"{label} placeholder must declare a visible '> Placeholder route.' notice")
            if document["state"] != "placeholder" and PLACEHOLDER_ROUTE_MARKER in first_lines:
                errors.append(f"{label} contains a placeholder route but is not classified as placeholder")
            if document["state"] == "redirect" and LEGACY_ROUTE_MARKER not in first_lines:
                errors.append(f"{label} redirect must declare a visible '> Legacy route.' notice")
            if document["state"] != "redirect" and LEGACY_ROUTE_MARKER in first_lines:
                errors.append(f"{label} contains a legacy route but is not classified as redirect")
            is_public = isinstance(classification, dict) and classification.get("visibility") == "public"
            if is_public and any(marker in document_text for marker in UNFINISHED_CONTENT_MARKERS):
                errors.append(f"{label} contains unfinished placeholder content")

    for path, redirect_to in redirect_targets.items():
        document = documents[path]
        target_path = seen_ids.get(redirect_to)
        if target_path is None:
            errors.append(f"manifest entry {path} redirect_to does not exist: {redirect_to}")
        elif target_path == path:
            errors.append(f"manifest entry {path} redirect_to must name another document")
        elif isinstance(document, dict) and document.get("target") != documents[target_path].get("target"):
            errors.append(
                f"manifest entry {path} redirect target must share the canonical target "
                f"of {redirect_to}"
            )
        else:
            redirect_path = root / path
            canonical_path = (root / target_path).resolve()
            links_to_canonical = False
            for _, target in _iter_markdown_links(redirect_path):
                parsed = urlsplit(target)
                if parsed.scheme or target.startswith("//"):
                    continue
                candidate, _ = _resolve_local_target(root, redirect_path, target)
                if candidate == canonical_path:
                    links_to_canonical = True
                    break
            if not links_to_canonical:
                errors.append(
                    f"manifest entry {path} redirect must link to canonical document "
                    f"{target_path}"
                )

    return errors, manifest


def validate_documentation(root: Path, manifest_relative_path: str = DEFAULT_MANIFEST) -> tuple[list[str], int]:
    manifest_path = root / manifest_relative_path
    errors, manifest = validate_manifest(root, manifest_path)
    documents = manifest.get("documents", {}) if isinstance(manifest, dict) else {}
    if isinstance(documents, dict):
        existing_documents = [path for path in documents if (root / path).is_file()]
        errors.extend(validate_links(root, existing_documents))
        return errors, len(existing_documents)
    return errors, 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate standalone FOnline engine documentation")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    args = parser.parse_args(argv)

    root = args.root.resolve()
    errors, document_count = validate_documentation(root, args.manifest)
    if errors:
        print(f"Documentation validation failed with {len(errors)} error(s):", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print(f"Documentation validation passed: {document_count} Markdown entries")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
