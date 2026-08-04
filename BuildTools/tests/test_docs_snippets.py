from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import docs_snippets  # noqa: E402


class DocumentationSnippetTests(unittest.TestCase):
    def _create_fixture(
        self, markdown: str
    ) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary_directory = tempfile.TemporaryDirectory()
        root = Path(temporary_directory.name)
        (root / "Docs").mkdir()
        (root / "BuildTools").mkdir()
        (root / "Docs/Guide.md").write_text(markdown, encoding="utf-8")
        policy_source = BUILDTOOLS_DIR / "SnippetPolicy.json"
        (root / docs_snippets.DEFAULT_POLICY).write_text(
            policy_source.read_text(encoding="utf-8"),
            encoding="utf-8",
        )
        manifest = {
            "documents": {
                "Docs/Guide.md": {
                    "id": "guide",
                    "title": "Guide",
                    "state": "current",
                    "classification": {
                        "visibility": "public",
                        "human": True,
                    },
                },
                "Docs/Internal.md": {
                    "id": "internal",
                    "title": "Internal",
                    "state": "current",
                    "classification": {
                        "visibility": "internal",
                        "human": True,
                    },
                },
            }
        }
        (root / docs_snippets.DEFAULT_MANIFEST).write_text(
            json.dumps(manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        return temporary_directory, root

    def test_repository_corpus_has_complete_normative_coverage(self) -> None:
        report = docs_snippets.evaluate(BUILDTOOLS_DIR.parent)

        self.assertEqual(report["error_count"], 0)
        self.assertEqual(report["snippet_count"], 462)
        self.assertEqual(report["normative_count"], 305)
        self.assertEqual(report["normative_validated_count"], 305)
        self.assertEqual(report["normative_coverage"], 1.0)
        self.assertEqual(report["evidence_count"], 157)
        self.assertEqual(report["external_parser_required_count"], 182)

    def test_every_declared_harness_validates_a_fixture(self) -> None:
        markdown = """# Guide

```bash
cmake --build <build-dir>
```

```powershell
$value = 1
```

```cmake
SetOption(FO_MAIN_CONFIG "Game.fomain")
```

```cpp
void Run() { Call(); }
```

```angelscript
void Run() { verify(true, "ok"); }
```

```glsl
void main() { gl_Position = vec4(1.0); }
```

```ini
[ProtoItem]
$Name = Example
```

```json
{"status": "ok"}
```

```xml
<SPARK><System name="Example" /></SPARK>
```

```python
value = "ok"
```

```text
expected output
```
"""
        temporary_directory, root = self._create_fixture(markdown)
        self.addCleanup(temporary_directory.cleanup)

        report = docs_snippets.evaluate(root)

        self.assertEqual(report["error_count"], 0)
        self.assertEqual(report["snippet_count"], len(docs_snippets.EXPECTED_LANGUAGES))
        self.assertEqual(report["normative_coverage"], 1.0)
        self.assertEqual(set(report["language_counts"]), set(docs_snippets.EXPECTED_LANGUAGES))

    def test_malformed_fences_and_bodies_are_rejected(self) -> None:
        cases = {
            "untyped": "# Guide\n\n```\nvalue\n```\n",
            "unknown": "# Guide\n\n```toml\nvalue = 1\n```\n",
            "unclosed": "# Guide\n\n```bash\necho value\n",
            "cpp": "# Guide\n\n```cpp\nvoid Run( {\n```\n",
            "cmake": "# Guide\n\n```cmake\nSetOption(value\n```\n",
            "ini": "# Guide\n\n```ini\nnot an assignment\n```\n",
            "json": "# Guide\n\n```json\n{broken}\n```\n",
            "xml": "# Guide\n\n```xml\n<SPARK><System></SPARK>\n```\n",
            "python": "# Guide\n\n```python\nif:\n```\n",
            "empty": "# Guide\n\n```text\n```\n",
        }
        for label, markdown in cases.items():
            with self.subTest(label=label):
                temporary_directory, root = self._create_fixture(markdown)
                try:
                    report = docs_snippets.evaluate(root)
                    self.assertGreater(report["error_count"], 0)
                finally:
                    temporary_directory.cleanup()

    def test_write_check_and_stale_detection(self) -> None:
        temporary_directory, root = self._create_fixture(
            "# Guide\n\n```json\n{\"ok\": true}\n```\n"
        )
        self.addCleanup(temporary_directory.cleanup)

        self.assertEqual(
            docs_snippets.main(["--root", str(root), "--write"]),
            0,
        )
        self.assertEqual(
            docs_snippets.main(["--root", str(root), "--check"]),
            0,
        )
        (root / docs_snippets.DEFAULT_OUTPUT).write_text("stale\n", encoding="utf-8")
        self.assertEqual(
            docs_snippets.main(["--root", str(root), "--check"]),
            1,
        )

    def test_external_parsers_parse_without_executing(self) -> None:
        try:
            docs_snippets._find_bash()
            docs_snippets._find_powershell()
        except ValueError as exception:
            self.skipTest(str(exception))
        temporary_directory, root = self._create_fixture(
            """# Guide

```bash
touch docs-snippet-must-not-exist
```

```powershell
Set-Content docs-snippet-must-not-exist value
```
"""
        )
        self.addCleanup(temporary_directory.cleanup)

        self.assertEqual(
            docs_snippets.main(["--root", str(root), "--external"]),
            0,
        )
        self.assertFalse((root / "docs-snippet-must-not-exist").exists())

        (root / "Docs/Guide.md").write_text(
            "# Guide\n\n```bash\nif then\n```\n",
            encoding="utf-8",
        )
        self.assertEqual(
            docs_snippets.main(["--root", str(root), "--external"]),
            1,
        )


if __name__ == "__main__":
    unittest.main()
