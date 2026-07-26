from __future__ import annotations

import copy
import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ENGINE_ROOT / "BuildTools"))

import docs_native_extension  # noqa: E402


def _manifest() -> dict[str, object]:
    return json.loads((ENGINE_ROOT / docs_native_extension.DEFAULT_MANIFEST).read_text(encoding="utf-8"))


def _write_fixture(root: Path, manifest: dict[str, object]) -> None:
    manifest_path = root / docs_native_extension.DEFAULT_MANIFEST
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    project_interface_path = root / docs_native_extension.DEFAULT_PROJECT_INTERFACE
    project_interface_path.parent.mkdir(parents=True, exist_ok=True)
    project_interface_path.write_text(
        (ENGINE_ROOT / docs_native_extension.DEFAULT_PROJECT_INTERFACE).read_text(encoding="utf-8"),
        encoding="utf-8",
    )
    codegen_path = root / manifest["registration"]["codegen_parser"]
    codegen_path.parent.mkdir(parents=True, exist_ok=True)
    codegen_path.write_text("# fixture codegen\n", encoding="utf-8")

    call_site_hooks: dict[str, list[str]] = {}
    for hook in manifest["hooks"]:
        for call_site in hook["call_sites"]:
            call_site_hooks.setdefault(call_site, []).append(hook["name"])
    for call_site, hook_names in call_site_hooks.items():
        path = root / call_site
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("\n".join(hook_names) + "\n", encoding="utf-8")


class DocumentationNativeExtensionTests(unittest.TestCase):
    def test_current_model_matches_runtime_roles_hooks_and_sources(self) -> None:
        model = docs_native_extension.generate_native_extension_model(ENGINE_ROOT)

        self.assertEqual(model["schema_version"], 1)
        self.assertEqual(model["generated_by"], "BuildTools/docs_native_extension.py")
        self.assertEqual(model["scope"]["stability"], "experimental")
        self.assertEqual(model["summary"], {
            "role_count": 5,
            "hook_count": 8,
            "binding_rule_count": 6,
            "compatibility_hashed_hook_count": 7,
            "hooks_by_role": {"BAKER": 1, "CLIENT": 1, "COMMON": 3, "SERVER": 3},
        })
        self.assertEqual([role["name"] for role in model["roles"]], ["COMMON", "SERVER", "CLIENT", "MAPPER", "BAKER"])
        self.assertEqual(model["hooks"][0]["id"], "native-extension.hook.ApplicationInitHook")
        self.assertEqual(model["hooks"][-1]["name"], "CheckItemVisibilityHook")

        identities = [
            entry["id"]
            for key in ("roles", "hooks", "binding_rules")
            for entry in model[key]
        ]
        self.assertEqual(len(identities), len(set(identities)))

    def test_manifest_validation_rejects_duplicate_role_and_unknown_hook_role(self) -> None:
        cases = []
        duplicate_role = _manifest()
        duplicate_role["roles"][1]["name"] = duplicate_role["roles"][0]["name"]
        cases.append((duplicate_role, "duplicate native extension role identity"))

        unknown_hook_role = _manifest()
        unknown_hook_role["hooks"][0]["role"] = "EDITOR"
        cases.append((unknown_hook_role, "unknown role EDITOR"))

        for manifest, expected in cases:
            with self.subTest(expected=expected), tempfile.TemporaryDirectory() as temp_dir:
                root = Path(temp_dir)
                _write_fixture(root, manifest)
                with self.assertRaisesRegex(ValueError, expected):
                    docs_native_extension.generate_native_extension_model(root)

    def test_manifest_validation_rejects_project_role_and_call_site_drift(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            manifest = _manifest()
            _write_fixture(root, manifest)

            project_interface_path = root / docs_native_extension.DEFAULT_PROJECT_INTERFACE
            project_interface = json.loads(project_interface_path.read_text(encoding="utf-8"))
            helper = next(entry for entry in project_interface["helpers"] if entry["name"] == "AddEngineSources")
            helper["allowed_roles"].pop()
            project_interface_path.write_text(json.dumps(project_interface), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "roles differ from ProjectInterface"):
                docs_native_extension.generate_native_extension_model(root)

            _write_fixture(root, manifest)
            first_call_site = root / manifest["hooks"][0]["call_sites"][0]
            first_call_site.write_text("missing hook\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "call site does not reference"):
                docs_native_extension.generate_native_extension_model(root)

    def test_reference_pages_are_deterministic_escaped_and_cover_every_id(self) -> None:
        model = docs_native_extension.generate_native_extension_model(ENGINE_ROOT)
        pages = docs_native_extension.generate_reference_pages(model)

        self.assertEqual(tuple(sorted(pages)), tuple(sorted(docs_native_extension.OUTPUT_PATHS)))
        self.assertEqual(pages, docs_native_extension.generate_reference_pages(copy.deepcopy(model)))
        combined = "\n".join(pages.values())
        for key in ("roles", "hooks", "binding_rules"):
            for entry in model[key]:
                self.assertIn(entry["id"], combined)

        escaped = copy.deepcopy(model)
        escaped["binding_rules"][0]["requirement"] = "A | B {shape} <unsafe>"
        bindings = docs_native_extension.generate_reference_pages(escaped)[
            f"{docs_native_extension.DEFAULT_OUTPUT_DIR}/bindings.md"
        ]
        self.assertIn("A &#124; B &#123;shape&#125; &lt;unsafe&gt;", bindings)

    def test_write_check_and_stale_detection(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            _write_fixture(root, _manifest())
            output = io.StringIO()
            with redirect_stdout(output), redirect_stderr(output):
                self.assertEqual(docs_native_extension.main(["--root", str(root), "--write"]), 0)
                self.assertEqual(docs_native_extension.main(["--root", str(root), "--check"]), 0)
                stale_path = root / docs_native_extension.OUTPUT_PATHS[0]
                stale_path.write_text(stale_path.read_text(encoding="utf-8") + "stale\n", encoding="utf-8")
                self.assertEqual(docs_native_extension.main(["--root", str(root), "--check"]), 1)
            self.assertIn("missing or stale", output.getvalue())


if __name__ == "__main__":
    unittest.main()
