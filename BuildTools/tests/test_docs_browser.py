from __future__ import annotations

import json
import shutil
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TOOL_DIR = ROOT / "BuildTools/docs-browser"
PLAYWRIGHT_VERSION = "1.62.0"
AXE_VERSION = "4.12.1"
NODE_VERSION = "24.16.0"


class DocumentationBrowserAuditTests(unittest.TestCase):
    def test_browser_dependencies_are_exact_and_locked(self) -> None:
        package = json.loads((TOOL_DIR / "package.json").read_text(encoding="utf-8"))
        lock = json.loads((TOOL_DIR / "package-lock.json").read_text(encoding="utf-8"))

        self.assertTrue(package["private"])
        self.assertEqual(package["dependencies"]["playwright"], PLAYWRIGHT_VERSION)
        self.assertEqual(package["dependencies"]["axe-core"], AXE_VERSION)
        self.assertEqual(lock["lockfileVersion"], 3)
        self.assertEqual(
            lock["packages"]["node_modules/playwright"]["version"],
            PLAYWRIGHT_VERSION,
        )
        self.assertEqual(
            lock["packages"]["node_modules/playwright-core"]["version"],
            PLAYWRIGHT_VERSION,
        )
        self.assertEqual(
            lock["packages"]["node_modules/axe-core"]["version"],
            AXE_VERSION,
        )

    def test_harness_declares_complete_routes_profiles_and_interactions(self) -> None:
        script = (TOOL_DIR / "audit.mjs").read_text(encoding="utf-8")

        for marker in (
            '"document-routes.json"',
            'id: "desktop"',
            'id: "mobile"',
            'id: "zoom-200"',
            "deviceScaleFactor: 2",
            "zoomPercent: 200",
            "const allRoutes = await loadRoutes(siteDir)",
            "allRoutes.slice(0, options.routeLimit)",
            "WCAG 2.2 Level AA automated axe-core subset",
            '"wcag22aa"',
            "auditDesktopInteractions",
            "auditMobileInteractions",
            "auditZoomInteractions",
            "auditLocaleInteractions",
            "auditDiagramRendering",
            "decodeContentImages",
            "first desktop Tab does not focus the skip link",
            "Ctrl+K does not open search and focus the query input",
            "mobile navigation focus escapes into the obscured page",
            "Russian search does not return a Russian documentation route",
            "English language switch does not open the paired English route",
            "page can scroll horizontally",
            "axe_incomplete",
            "example_paths",
            "relativeLuminance",
            "required_ratio",
            "axe_incomplete_unresolved_count",
            "console error:",
            "request failed:",
            "documentation image did not decode:",
            "desktop-documentation.png",
            "mobile-navigation.png",
            "mobile-documentation.png",
            "zoom-200-russian-documentation.png",
            "sidebarBox.right <= 1",
            "russian-documentation.png",
            "russian-entrypoint-${entrypointId}.png",
            "locale-switch-russian-entrypoint-${entrypointId}",
            "translated Russian entrypoint has no paired English route",
            "translated Russian entrypoint pair is absent from the generated route catalog",
            'route.documentId === "engine-architecture"',
            "`architecture-diagram-${profile.id}.png`",
        ):
            self.assertIn(marker, script)

        node = shutil.which("node")
        if node:
            subprocess.run(
                [node, "--check", str(TOOL_DIR / "audit.mjs")],
                cwd=ROOT,
                check=True,
                capture_output=True,
                text=True,
            )

    def test_manifest_owns_browser_policy_and_outputs(self) -> None:
        manifest = json.loads(
            (ROOT / "Docs/documentation-manifest.json").read_text(encoding="utf-8")
        )
        policy = manifest["site_delivery"]["browser_validation"]
        generated = manifest["generated_artifacts"]["site_delivery"]

        self.assertEqual(policy["schema_version"], 1)
        self.assertEqual(policy["node"], NODE_VERSION)
        self.assertEqual(policy["playwright"], PLAYWRIGHT_VERSION)
        self.assertEqual(policy["axe_core"], AXE_VERSION)
        self.assertEqual(
            [profile["id"] for profile in policy["profiles"]],
            ["desktop", "mobile", "zoom-200"],
        )
        self.assertEqual(
            policy["profiles"][2],
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
        )
        self.assertEqual(
            generated["browser_validator"],
            "BuildTools/docs-browser/audit.mjs",
        )
        self.assertEqual(
            generated["browser_report"],
            "Workspace/docs-browser-audit-report.json",
        )
        self.assertEqual(
            generated["browser_screenshots"],
            "Workspace/docs-browser-screenshots",
        )

    def test_layout_and_ci_enforce_the_accessible_interaction_contract(self) -> None:
        script = (ROOT / "assets/js/docs.js").read_text(encoding="utf-8")
        stylesheet = (ROOT / "assets/css/docs.css").read_text(encoding="utf-8")
        workflow = (ROOT / ".github/workflows/validate.yml").read_text(
            encoding="utf-8"
        )
        config = (ROOT / "_config.yml").read_text(encoding="utf-8")

        for marker in (
            "navigationSidebar.inert = hidden",
            'navigationSidebar.setAttribute("aria-hidden", "true")',
            "firstSidebarControl.focus()",
            "focusables[nextIndex].focus()",
            "searchDialog.close()",
            "updateScrollableTables()",
        ):
            self.assertIn(marker, script)
        self.assertIn("min-height: 24px", stylesheet)
        self.assertIn("overflow-x: clip", stylesheet)
        self.assertIn("BuildTools/docs-browser/", config)
        for marker in (
            "BuildTools/tests/test_docs_browser.py",
            "node-version: 24.16.0",
            "npm ci",
            "playwright install --with-deps chromium",
            "npm run audit",
            "Workspace/docs-browser-audit-report.json",
            "Workspace/docs-browser-screenshots/",
        ):
            self.assertIn(marker, workflow)


if __name__ == "__main__":
    unittest.main()
