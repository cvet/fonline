from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

import docs_ai_delivery
import docs_ai_eval
import docs_description_translations
import docs_diagrams
import docs_inventory
import docs_localization
import docs_screenshots
import docs_site
import docs_snippets
import documentation_metadata


OUTPUT_PATHS = (
    "docs-manifest.json",
    "llms.txt",
    "llms-full.txt",
    "Docs/Site/Data/docs-site.json",
    "Docs/Site/Assets/docs-search.json",
    "Docs/Site/Assets/docs-search.ru.json",
    "Docs/generated/source-inventory.json",
    "Docs/generated/diagrams.json",
    "Docs/generated/screenshots.json",
    "Docs/generated/snippets.json",
    "Docs/generated/description-translation-status.json",
    "Docs/generated/ai-evaluation-report.json",
    "Gemfile",
    ".ruby-version",
    "CNAME",
)


def _write_catalog(root: Path, path: str, content: str) -> None:
    output = documentation_metadata.output_path(root, path)
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.is_file() or output.read_bytes() != content.encode("utf-8"):
        output.write_text(content, encoding="utf-8", newline="\n")


def prepare(root: Path, *, external: bool = False) -> None:
    root_args = ["--root", str(root), "--write"]
    manifest = docs_ai_delivery._load_manifest(root, docs_ai_delivery.DEFAULT_MANIFEST)
    publishing = docs_ai_delivery._require_object(manifest, "publishing", "documentation publishing")
    pins = {key: docs_ai_delivery._require_string(publishing, key, f"documentation publishing {key}")
            for key in ("pages_gem_version", "ruby_version", "domain")}
    if not re.fullmatch(r"[0-9]+", pins["pages_gem_version"]) or not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", pins["ruby_version"]):
        raise ValueError("Documentation publishing must declare exact numeric Pages and Ruby versions")
    if not re.fullmatch(r"[A-Za-z0-9.-]+", pins["domain"]):
        raise ValueError("Documentation publishing must declare a DNS domain")
    _write_catalog(root, "Gemfile", 'source "https://rubygems.org"\n\n'
                   f'gem "github-pages", "= {pins["pages_gem_version"]}", group: :jekyll_plugins\n')
    _write_catalog(root, ".ruby-version", pins["ruby_version"] + "\n")
    _write_catalog(root, "CNAME", pins["domain"] + "\n")

    if docs_inventory.main(root_args):
        raise ValueError("Documentation inventory generation failed")
    _write_catalog(root, docs_diagrams.DEFAULT_CATALOG,
                   docs_diagrams.render_outputs(root)[docs_diagrams.DEFAULT_CATALOG])
    _write_catalog(root, docs_screenshots.DEFAULT_CATALOG,
                   docs_screenshots.render_outputs(root)[docs_screenshots.DEFAULT_CATALOG])

    snippet_args = root_args + (["--external"] if external else [])
    if docs_snippets.main(snippet_args):
        raise ValueError("Documentation snippet validation failed")
    if docs_localization.main(root_args + ["--enforce-complete"]):
        raise ValueError("Documentation translation validation failed")
    if docs_description_translations.main(root_args + ["--enforce-complete"]):
        raise ValueError("Generated-description translation validation failed")
    if docs_site.main(root_args):
        raise ValueError("Documentation navigation and search generation failed")
    if docs_ai_eval.main(root_args):
        raise ValueError("Documentation retrieval validation failed")
    if docs_ai_delivery.main(root_args):
        raise ValueError("Documentation delivery generation failed")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Prepare untracked FOnline documentation build outputs")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--external", action="store_true", help="also require real shell parsers for snippets")
    args = parser.parse_args(argv)
    try:
        prepare(args.root.resolve(), external=args.external)
    except (OSError, UnicodeError, ValueError) as exception:
        print(f"Unable to prepare documentation outputs: {exception}", file=sys.stderr)
        return 1
    print(f"Documentation build outputs prepared: {len(OUTPUT_PATHS)} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
