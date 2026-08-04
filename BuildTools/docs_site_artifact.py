from __future__ import annotations

import argparse
import json
import posixpath
import sys
from html.parser import HTMLParser
from pathlib import Path, PurePosixPath
from urllib.parse import unquote, urljoin, urlsplit


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "Docs/documentation-manifest.json"
DEFAULT_SITE_DIR = "_site"
DEFAULT_REPORT = "Workspace/docs-site-artifact-report.json"
MAX_CONSOLE_ERRORS = 50
GENERATED_BY = "BuildTools/docs_site_artifact.py"
PUBLISHABLE_SUFFIXES = {
    "",
    ".css",
    ".gif",
    ".html",
    ".ico",
    ".jpeg",
    ".jpg",
    ".js",
    ".json",
    ".md",
    ".png",
    ".svg",
    ".txt",
    ".webp",
    ".xml",
}


def _required_object(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be an object")
    return value


def _required_string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return value


def _repository_path(value: object, label: str) -> str:
    text = _required_string(value, label)
    path = PurePosixPath(text)
    if path.is_absolute() or ".." in path.parts or "\\" in text:
        raise ValueError(f"{label} must be a repository-relative forward-slash path")
    return text


def _load_json(path: Path, label: str) -> dict[str, object]:
    value = json.loads(path.read_text(encoding="utf-8"))
    return _required_object(value, label)


def _site_file(site_dir: Path, url_path: str) -> Path:
    decoded = unquote(url_path)
    pure_path = PurePosixPath(decoded.lstrip("/"))
    if ".." in pure_path.parts:
        raise ValueError(f"site URL escapes the artifact root: {url_path}")
    relative_path = Path(*pure_path.parts)
    if decoded.endswith("/") or decoded == "":
        relative_path /= "index.html"
    return site_dir / relative_path


def _candidate_site_files(site_dir: Path, url_path: str) -> list[Path]:
    direct = _site_file(site_dir, url_path)
    candidates = [direct]
    if not url_path.endswith("/") and not PurePosixPath(url_path).suffix:
        candidates.append(_site_file(site_dir, url_path + ".html"))
        candidates.append(_site_file(site_dir, url_path + "/"))
    return candidates


class RenderedPageParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.doctype = False
        self.html_lang: str | None = None
        self.title_parts: list[str] = []
        self._in_title = False
        self.h1_count = 0
        self.main_content_count = 0
        self.skip_link = False
        self.viewport = False
        self.canonical: str | None = None
        self.ids: set[str] = set()
        self.duplicate_ids: set[str] = set()
        self.hrefs: list[str] = []
        self.resources: list[str] = []
        self.images_missing_alt = 0
        self.buttons: list[tuple[str | None, str]] = []
        self._button_depth = 0
        self._button_label: str | None = None
        self._button_text: list[str] = []

    def handle_decl(self, declaration: str) -> None:
        if declaration.strip().casefold() == "doctype html":
            self.doctype = True

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        attributes = dict(attrs)
        element_id = attributes.get("id")
        if element_id:
            if element_id in self.ids:
                self.duplicate_ids.add(element_id)
            self.ids.add(element_id)

        if tag == "html":
            self.html_lang = attributes.get("lang")
        elif tag == "title":
            self._in_title = True
        elif tag == "h1":
            self.h1_count += 1
        elif tag == "main" and attributes.get("id") == "main-content":
            self.main_content_count += 1
        elif tag == "meta" and attributes.get("name") == "viewport":
            self.viewport = bool(attributes.get("content"))
        elif tag == "link":
            relation = (attributes.get("rel") or "").casefold().split()
            href = attributes.get("href")
            if "canonical" in relation:
                self.canonical = href
            if href:
                self.resources.append(href)
        elif tag == "script":
            source = attributes.get("src")
            if source:
                self.resources.append(source)
        elif tag == "img":
            if "alt" not in attributes:
                self.images_missing_alt += 1
            source = attributes.get("src")
            if source:
                self.resources.append(source)
        elif tag == "a":
            href = attributes.get("href")
            if href:
                self.hrefs.append(href)
            if "skip-link" in (attributes.get("class") or "").split() and href == "#main-content":
                self.skip_link = True
        elif tag == "button":
            self._button_depth = 1
            self._button_label = attributes.get("aria-label")
            self._button_text = []
        elif self._button_depth:
            self._button_depth += 1

    def handle_startendtag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        self.handle_starttag(tag, attrs)
        if tag == "button":
            self.handle_endtag(tag)

    def handle_endtag(self, tag: str) -> None:
        if tag == "title":
            self._in_title = False
        if self._button_depth:
            self._button_depth -= 1
            if tag == "button" and self._button_depth == 0:
                self.buttons.append((self._button_label, " ".join(self._button_text).strip()))
                self._button_label = None
                self._button_text = []

    def handle_data(self, data: str) -> None:
        if self._in_title:
            self.title_parts.append(data)
        if self._button_depth:
            stripped = data.strip()
            if stripped:
                self._button_text.append(stripped)

    @property
    def title(self) -> str:
        return " ".join(part.strip() for part in self.title_parts if part.strip())


def _expected_static_paths(manifest: dict[str, object]) -> list[str]:
    paths = {"CNAME", "assets/css/docs.css", "assets/js/docs.js", "assets/images/fonline-mark.png"}
    ai_delivery = _required_object(manifest.get("ai_delivery"), "documentation manifest ai_delivery")
    for field in ("llms", "full_context", "public_manifest"):
        config = _required_object(ai_delivery.get(field), f"documentation manifest ai_delivery.{field}")
        paths.add(_repository_path(config.get("path"), f"documentation manifest ai_delivery.{field}.path"))

    site_delivery = _required_object(manifest.get("site_delivery"), "documentation manifest site_delivery")
    search = _required_object(site_delivery.get("search"), "documentation manifest site_delivery.search")
    routing = _required_object(site_delivery.get("routing"), "documentation manifest site_delivery.routing")
    paths.add(_repository_path(search.get("path"), "documentation manifest site_delivery.search.path"))
    locale_paths = _required_object(
        search.get("locale_paths"),
        "documentation manifest site_delivery.search.locale_paths",
    )
    for locale, value in locale_paths.items():
        paths.add(
            _repository_path(
                value,
                f"documentation manifest site_delivery.search.locale_paths.{locale}",
            )
        )
    paths.add(_repository_path(routing.get("path"), "documentation manifest site_delivery.routing.path"))

    generated = _required_object(
        manifest.get("generated_artifacts"),
        "documentation manifest generated_artifacts",
    )
    for artifact_id, artifact in generated.items():
        if not isinstance(artifact, dict):
            continue
        if artifact.get("visibility", "public") == "internal":
            continue
        for field in ("model", "path"):
            value = artifact.get(field)
            if isinstance(value, str) and PurePosixPath(value).suffix in {".json", ".txt"}:
                paths.add(_repository_path(value, f"generated_artifacts.{artifact_id}.{field}"))
        artifact_paths = artifact.get("paths")
        if artifact_id == "documentation_diagrams" and isinstance(
            artifact_paths, list
        ):
            for index, value in enumerate(artifact_paths):
                if not isinstance(value, str):
                    continue
                suffix = PurePosixPath(value).suffix
                if suffix in {
                    ".gif",
                    ".jpeg",
                    ".jpg",
                    ".json",
                    ".png",
                    ".svg",
                    ".txt",
                    ".webp",
                    ".xml",
                }:
                    paths.add(
                        _repository_path(
                            value,
                            f"generated_artifacts.{artifact_id}.paths[{index}]",
                        )
                    )
    return sorted(paths)


def _internal_publication_paths(manifest: dict[str, object]) -> list[str]:
    paths: set[str] = set()
    documents = manifest.get("documents")
    if isinstance(documents, dict):
        for source_path, document in documents.items():
            if not isinstance(source_path, str) or not isinstance(document, dict):
                continue
            classification = document.get("classification")
            if isinstance(classification, dict) and classification.get("visibility") == "internal":
                paths.add(_repository_path(source_path, f"internal document {source_path}"))

    generated = manifest.get("generated_artifacts")
    if isinstance(generated, dict):
        for artifact_id, artifact in generated.items():
            if not isinstance(artifact, dict) or artifact.get("visibility", "public") != "internal":
                continue
            for field in ("model", "path"):
                value = artifact.get(field)
                if isinstance(value, str):
                    paths.add(_repository_path(value, f"generated_artifacts.{artifact_id}.{field}"))
            artifact_paths = artifact.get("paths")
            if isinstance(artifact_paths, list):
                for index, value in enumerate(artifact_paths):
                    if isinstance(value, str):
                        paths.add(
                            _repository_path(
                                value,
                                f"generated_artifacts.{artifact_id}.paths[{index}]",
                            )
                        )
    return sorted(paths)


def _site_publication_variants(site_dir: Path, relative_path: str) -> set[Path]:
    repository_path = Path(*PurePosixPath(relative_path).parts)
    variants = {site_dir / repository_path}
    if repository_path.suffix.casefold() == ".md":
        variants.add(site_dir / repository_path.with_suffix(".html"))
        if repository_path.name.casefold() == "readme.md":
            variants.add(site_dir / repository_path.parent / "index.html")
    return variants


def _canonical_url(base_url: str, current_path: str) -> str:
    if current_path == "/":
        return base_url.rstrip("/") + "/"
    return base_url.rstrip("/") + "/" + current_path.lstrip("/")


def _local_url_path(reference: str, current_path: str, canonical_host: str) -> tuple[str, str] | None:
    split = urlsplit(reference)
    if split.scheme or split.netloc:
        if split.scheme not in {"http", "https"} or split.netloc.casefold() != canonical_host:
            return None
        path = split.path or "/"
    elif reference.startswith("//"):
        return None
    elif split.path:
        path = urljoin(current_path, split.path)
    else:
        path = current_path
    normalized = posixpath.normpath(path)
    if path.endswith("/") and normalized != "/":
        normalized += "/"
    if not normalized.startswith("/"):
        normalized = "/" + normalized
    return normalized, unquote(split.fragment)


def audit_site(
    root: Path,
    site_dir: Path,
    manifest_path: str = DEFAULT_MANIFEST,
) -> dict[str, object]:
    root = root.resolve()
    site_dir = site_dir.resolve()
    manifest = _load_json(root / manifest_path, "documentation manifest")
    routing = _required_object(
        _required_object(manifest.get("site_delivery"), "documentation manifest site_delivery").get(
            "routing"
        ),
        "documentation manifest site_delivery.routing",
    )
    route_model_path = _repository_path(
        routing.get("path"),
        "documentation manifest site_delivery.routing.path",
    )
    route_model = _load_json(root / route_model_path, "documentation route model")
    base_url = _required_string(route_model.get("canonical_base_url"), "route model canonical_base_url")
    canonical_host = urlsplit(base_url).netloc.casefold()
    routes = route_model.get("routes")
    if not isinstance(routes, list):
        raise ValueError("documentation route model routes must be an array")

    errors: list[str] = []
    expected_route_files: dict[Path, dict[str, object]] = {}

    def add_expected_route(
        *,
        route_id: str,
        current_path: str,
        canonical_url: str,
        locale: str,
    ) -> None:
        expected_path = _site_file(site_dir, current_path)
        previous = expected_route_files.get(expected_path)
        if previous is not None:
            if (
                previous["locale"] != locale
                or previous["canonical_url"] != canonical_url
            ):
                errors.append(f"duplicate rendered route target: {current_path}")
            return
        expected_route_files[expected_path] = {
            "id": route_id,
            "current_path": current_path,
            "canonical_url": canonical_url,
            "locale": locale,
        }
        if not expected_path.is_file():
            errors.append(f"rendered route is missing: {route_id} -> {current_path}")

    for index, route in enumerate(routes):
        if not isinstance(route, dict):
            errors.append(f"route[{index}] must be an object")
            continue
        route_id = _required_string(route.get("id"), f"route[{index}].id")
        current_path = _required_string(route.get("current_path"), f"route[{index}].current_path")
        current_url = _required_string(route.get("current_url"), f"route[{index}].current_url")
        add_expected_route(
            route_id=route_id,
            current_path=current_path,
            canonical_url=current_url,
            locale="en",
        )
        locale_routes = route.get("locale_routes", [])
        if not isinstance(locale_routes, list):
            errors.append(f"route[{index}].locale_routes must be an array")
            continue
        for locale_index, locale_route in enumerate(locale_routes):
            if not isinstance(locale_route, dict):
                errors.append(f"route[{index}].locale_routes[{locale_index}] must be an object")
                continue
            if locale_route.get("availability") != "available":
                continue
            locale = _required_string(
                locale_route.get("locale"),
                f"route[{index}].locale_routes[{locale_index}].locale",
            )
            locale_path = _required_string(
                locale_route.get("path"),
                f"route[{index}].locale_routes[{locale_index}].path",
            )
            locale_url = _required_string(
                locale_route.get("url"),
                f"route[{index}].locale_routes[{locale_index}].url",
            )
            add_expected_route(
                route_id=f"{route_id}:{locale}",
                current_path=locale_path,
                canonical_url=locale_url,
                locale=locale,
            )

    endpoint_paths = _expected_static_paths(manifest)
    for relative_path in endpoint_paths:
        source_path = root / relative_path
        rendered_path = site_dir / Path(*PurePosixPath(relative_path).parts)
        if not source_path.is_file():
            errors.append(f"source endpoint is missing: {relative_path}")
        elif not rendered_path.is_file():
            errors.append(f"rendered endpoint is missing: /{relative_path}")
        elif source_path.read_bytes() != rendered_path.read_bytes():
            errors.append(f"rendered endpoint differs from source: /{relative_path}")

    internal_paths = _internal_publication_paths(manifest)
    published_internal_paths: set[str] = set()
    for relative_path in internal_paths:
        for candidate in _site_publication_variants(site_dir, relative_path):
            if candidate.exists():
                published_path = "/" + candidate.relative_to(site_dir).as_posix()
                published_internal_paths.add(published_path)
                errors.append(f"internal documentation artifact is published: {published_path}")

    parsed_pages: dict[Path, RenderedPageParser] = {}
    for html_path, route in sorted(expected_route_files.items(), key=lambda item: str(item[0])):
        if not html_path.is_file():
            continue
        parser = RenderedPageParser()
        parser.feed(html_path.read_text(encoding="utf-8"))
        parsed_pages[html_path] = parser
        route_id = str(route["id"])
        current_path = str(route["current_path"])
        if not parser.doctype:
            errors.append(f"{route_id}: rendered page must start with an HTML5 doctype")
        expected_locale = str(route["locale"])
        if parser.html_lang != expected_locale:
            errors.append(f"{route_id}: rendered html lang must be {expected_locale}")
        if not parser.title:
            errors.append(f"{route_id}: rendered page title is empty")
        if not parser.viewport:
            errors.append(f"{route_id}: rendered page lacks a viewport meta tag")
        if parser.main_content_count != 1:
            errors.append(f"{route_id}: rendered page must contain one main#main-content")
        if parser.h1_count != 1:
            errors.append(f"{route_id}: rendered page must contain exactly one h1")
        if not parser.skip_link:
            errors.append(f"{route_id}: rendered page lacks a skip link to #main-content")
        expected_canonical = str(route["canonical_url"])
        if expected_canonical != _canonical_url(base_url, current_path):
            errors.append(
                f"{route_id}: route model canonical URL does not match its path: "
                f"{expected_canonical}"
            )
        if parser.canonical != expected_canonical:
            errors.append(
                f"{route_id}: canonical URL must be {expected_canonical}, got {parser.canonical}"
            )
        if parser.duplicate_ids:
            errors.append(
                f"{route_id}: duplicate HTML ids: {', '.join(sorted(parser.duplicate_ids))}"
            )
        if parser.images_missing_alt:
            errors.append(f"{route_id}: {parser.images_missing_alt} image(s) lack alt attributes")
        unnamed_buttons = sum(
            1 for aria_label, text in parser.buttons if not (aria_label or "").strip() and not text
        )
        if unnamed_buttons:
            errors.append(f"{route_id}: {unnamed_buttons} button(s) lack accessible names")

    checked_references = 0
    published_route_paths = {
        str(route["current_path"]) for route in expected_route_files.values()
    }
    published_static_paths = {"/" + path.lstrip("/") for path in endpoint_paths}
    for html_path, parser in parsed_pages.items():
        route = expected_route_files[html_path]
        route_id = str(route["id"])
        current_path = str(route["current_path"])
        for reference, is_resource in [
            *((resource, True) for resource in parser.resources),
            *((href, False) for href in parser.hrefs),
        ]:
            local = _local_url_path(reference, current_path, canonical_host)
            if local is None:
                continue
            target_path, fragment = local
            suffix = PurePosixPath(target_path).suffix.casefold()
            should_validate = (
                is_resource
                or target_path in published_route_paths
                or target_path in published_static_paths
                or suffix in {".html", ".md"}
                or (target_path == current_path and bool(fragment))
            )
            if not should_validate or suffix not in PUBLISHABLE_SUFFIXES:
                continue
            checked_references += 1
            candidates = _candidate_site_files(site_dir, target_path)
            target_file = next((candidate for candidate in candidates if candidate.is_file()), None)
            if target_file is None:
                errors.append(f"{route_id}: local published target is missing: {reference}")
                continue
            if fragment and target_file.suffix.casefold() == ".html":
                target_parser = parsed_pages.get(target_file)
                if target_parser is not None and fragment not in target_parser.ids:
                    errors.append(f"{route_id}: local fragment is missing: {reference}")

    search_config = _required_object(
        _required_object(manifest.get("site_delivery"), "documentation manifest site_delivery").get(
            "search"
        ),
        "documentation manifest site_delivery.search",
    )
    locale_paths = _required_object(
        search_config.get("locale_paths"),
        "site_delivery.search.locale_paths",
    )
    for locale, value in sorted(locale_paths.items()):
        search_path = _repository_path(value, f"site_delivery.search.locale_paths.{locale}")
        rendered_search_path = site_dir / Path(*PurePosixPath(search_path).parts)
        if not rendered_search_path.is_file():
            continue
        search_model = _load_json(rendered_search_path, f"rendered {locale} search model")
        if search_model.get("locale") != locale:
            errors.append(f"rendered {locale} search model locale must be {locale}")
        documents = search_model.get("documents")
        if not isinstance(documents, list):
            errors.append(f"rendered {locale} search model documents must be an array")
        else:
            for index, document in enumerate(documents):
                if not isinstance(document, dict) or not isinstance(document.get("url"), str):
                    errors.append(f"rendered {locale} search document[{index}] must have a URL")
                    continue
                if document.get("locale") != locale:
                    errors.append(
                        f"rendered {locale} search document[{index}] locale must be {locale}"
                    )
                if not any(
                    candidate.is_file()
                    for candidate in _candidate_site_files(site_dir, document["url"])
                ):
                    errors.append(
                        f"rendered {locale} search document target is missing: "
                        f"{document.get('id')} -> "
                        f"{document['url']}"
                    )

    return {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "site_directory": site_dir.name,
        "canonical_base_url": base_url,
        "route_count": len(routes),
        "rendered_route_count": len(parsed_pages),
        "static_endpoint_count": len(endpoint_paths),
        "internal_path_count": len(internal_paths),
        "published_internal_path_count": len(published_internal_paths),
        "checked_local_reference_count": checked_references,
        "error_count": len(errors),
        "errors": sorted(set(errors)),
    }


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Validate the rendered GitHub Pages/Jekyll documentation artifact."
    )
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--site-dir", type=Path, default=Path(DEFAULT_SITE_DIR))
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    parser.add_argument("--json-output", type=Path, default=Path(DEFAULT_REPORT))
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    root = args.root.resolve()
    site_dir = args.site_dir if args.site_dir.is_absolute() else root / args.site_dir
    report_path = args.json_output if args.json_output.is_absolute() else root / args.json_output
    try:
        report = audit_site(root, site_dir, args.manifest)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(
            json.dumps(report, ensure_ascii=True, indent=2) + "\n",
            encoding="utf-8",
        )
        if report["errors"]:
            for error in report["errors"][:MAX_CONSOLE_ERRORS]:
                print(f"ERROR: {error}", file=sys.stderr)
            hidden_error_count = report["error_count"] - MAX_CONSOLE_ERRORS
            if hidden_error_count > 0:
                print(
                    f"ERROR: {hidden_error_count} additional error(s); see {report_path}",
                    file=sys.stderr,
                )
            print(
                f"Rendered documentation validation failed with {report['error_count']} error(s)",
                file=sys.stderr,
            )
            return 1
        print(
            "Rendered documentation validation passed: "
            f"{report['rendered_route_count']} routes, "
            f"{report['static_endpoint_count']} static endpoints, "
            f"{report['checked_local_reference_count']} local references"
        )
        return 0
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as exception:
        print(f"Rendered documentation validation failed: {exception}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
