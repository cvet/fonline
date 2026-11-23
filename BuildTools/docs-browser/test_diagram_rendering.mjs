import assert from "node:assert/strict";
import { createServer } from "node:http";
import { mkdir, mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { chromium } from "playwright";
import { auditDiagramRendering } from "./audit.mjs";

test("architecture diagram validation waits for lazy images and rejects missing images", async (suite) => {
  const directory = await mkdtemp(join(tmpdir(), "fonline-diagram-audit-"));
  const svg = '<svg xmlns="http://www.w3.org/2000/svg" width="640" height="240"><rect width="640" height="240" fill="green"/></svg>';
  let missing = false;
  let imageRequests = 0;
  const server = createServer((request, response) => {
    if (request.url === "/diagram.svg") {
      imageRequests += 1;
      setTimeout(() => {
        response.writeHead(missing ? 404 : 200, { "Content-Type": "image/svg+xml" });
        response.end(missing ? "missing" : svg);
      }, 150);
      return;
    }
    response.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
    response.end(`<!doctype html><html lang="en"><head><title>Lazy diagram fixture</title><style>
      body { margin: 0; } .docs-header { position: fixed; top: 0; height: 60px; width: 100%; }
      .docs-content { margin-top: 96px; max-width: 760px; } figure { margin: 0; } img { width: 100%; }
      </style></head><body><header class="docs-header">Header</header><main class="docs-content">
      <div style="height: 20000px">The diagram starts outside the lazy-load threshold.</div>
      <figure class="docs-diagram"><img src="/diagram.svg" alt="Delayed diagram" loading="lazy"></figure>
      </main></body></html>`);
  });
  let browser;
  try {
    await new Promise((resolve, reject) => {
      server.once("error", reject);
      server.listen(0, "127.0.0.1", resolve);
    });
    const baseUrl = `http://127.0.0.1:${server.address().port}`;
    browser = await chromium.launch({ headless: true });
    for (const [label, missingImage] of [["delayed SVG", false], ["missing SVG", true]]) {
      await suite.test(label, async () => {
        missing = missingImage;
        imageRequests = 0;
        const screenshots = join(directory, missingImage ? "missing" : "delayed");
        await mkdir(screenshots);
        const names = [];
        const result = await auditDiagramRendering(browser, baseUrl, "/architecture/", screenshots, names);
        assert.equal(result.passed, !missingImage, JSON.stringify(result));
        assert.equal(imageRequests, 3);
        assert.equal(names.length, 3);
        if (missingImage) {
          assert.deepEqual(result.errors, [
            "desktop architecture diagram did not decode",
            "mobile architecture diagram did not decode",
            "zoom-200 architecture diagram did not decode"
          ]);
        } else {
          assert.deepEqual(result.errors, []);
        }
      });
    }
  } finally {
    await browser?.close();
    await new Promise((resolve) => server.close(resolve));
    await rm(directory, { recursive: true, force: true });
  }
});
