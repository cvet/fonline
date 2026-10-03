import fs from "node:fs";
import path from "node:path";
import { createRequire } from "node:module";

const args = process.argv.slice(2);
if (args.length !== 5) {
  console.error("usage: node capture_showcase_web.mjs <url> <output.png> <report.json> <playwright-root> <contract.json>");
  process.exit(2);
}

const [url, outputPath, reportPath, playwrightRoot, contractPath] = args;
const contract = JSON.parse(fs.readFileSync(contractPath, "utf8"));
const requireFromPlaywright = createRequire(path.join(path.resolve(playwrightRoot), "package.json"));
const { chromium } = requireFromPlaywright("playwright");
const consoleEntries = [];
const runtimeErrors = [];
const requestFailures = [];
const responses = new Map();
const startedAt = Date.now();
let browser;

function hasMarker(marker) {
  return consoleEntries.some((entry) => entry.text.includes(marker));
}

try {
  browser = await chromium.launch({ headless: true });
  const context = await browser.newContext({
    viewport: contract.browser_viewport,
    deviceScaleFactor: 1,
  });
  const page = await context.newPage();
  page.on("console", (message) => {
    const entry = { type: message.type(), text: message.text() };
    consoleEntries.push(entry);
    if (entry.type === "error") {
      runtimeErrors.push(`console: ${entry.text}`);
    }
  });
  page.on("pageerror", (error) => runtimeErrors.push(`pageerror: ${error.stack || error.message}`));
  page.on("requestfailed", (request) => {
    requestFailures.push({
      url: request.url(),
      error: request.failure()?.errorText || "unknown",
    });
  });
  page.on("response", (response) => {
    const pathname = new URL(response.url()).pathname;
    responses.set(pathname, {
      status: response.status(),
      contentType: response.headers()["content-type"] || "",
    });
  });

  await page.goto(url, { waitUntil: "domcontentloaded", timeout: contract.timeout_seconds * 1000 });
  const deadline = Date.now() + contract.timeout_seconds * 1000;
  while (Date.now() < deadline && !contract.required_client_markers.every(hasMarker)) {
    await page.waitForTimeout(250);
  }
  const missingMarkers = contract.required_client_markers.filter((marker) => !hasMarker(marker));
  if (missingMarkers.length !== 0) {
    throw new Error(`missing client markers: ${missingMarkers.join(", ")}`);
  }

  await page.waitForTimeout(1500);
  const canvas = page.locator("#canvas");
  await canvas.waitFor({ state: "visible", timeout: 10000 });
  const evidence = await page.evaluate(() => {
    const canvas = document.getElementById("canvas");
    const gl = canvas?.getContext("webgl2");
    if (!canvas || !gl) {
      throw new Error("WebGL 2 canvas is unavailable");
    }
    const width = gl.drawingBufferWidth;
    const height = gl.drawingBufferHeight;
    return {
      width,
      height,
      renderer: gl.getParameter(gl.RENDERER),
      vendor: gl.getParameter(gl.VENDOR),
      version: gl.getParameter(gl.VERSION),
      shading_language_version: gl.getParameter(gl.SHADING_LANGUAGE_VERSION),
    };
  });

  if (evidence.width !== contract.capture.width || evidence.height !== contract.capture.height) {
    throw new Error(`drawing buffer is ${evidence.width}x${evidence.height}, expected ${contract.capture.width}x${contract.capture.height}`);
  }
  const requiredResponses = {};
  for (const name of contract.required_responses) {
    const entry = [...responses.entries()].find(([pathname]) =>
      pathname.endsWith(`/${name}`) || pathname === `/${name}` || (name === "index.html" && pathname === "/")
    );
    if (!entry || entry[1].status !== 200) {
      throw new Error(`required response did not return 200: ${name}`);
    }
    requiredResponses[name] = entry[1];
  }
  if (!requiredResponses["FOCS_Client.wasm"].contentType.includes("application/wasm")) {
    throw new Error(`unexpected wasm content type: ${requiredResponses["FOCS_Client.wasm"].contentType}`);
  }
  const forbidden = contract.forbidden_markers.filter((marker) => hasMarker(marker));
  if (forbidden.length !== 0) {
    throw new Error(`forbidden browser markers: ${forbidden.join(", ")}`);
  }
  const fatalRequestFailures = requestFailures.filter((failure) => {
    const pathname = new URL(failure.url).pathname;
    const response = responses.get(pathname);
    const isRequired = contract.required_responses.some((name) =>
      pathname.endsWith(`/${name}`) || pathname === `/${name}` || (name === "index.html" && pathname === "/")
    );
    return failure.error !== "net::ERR_ABORTED" || !isRequired || response?.status !== 200;
  });
  runtimeErrors.push(
    ...fatalRequestFailures.map((failure) => `requestfailed: ${failure.url} (${failure.error})`)
  );
  if (runtimeErrors.length !== 0) {
    throw new Error(runtimeErrors.join("\n"));
  }

  fs.mkdirSync(path.dirname(outputPath), { recursive: true });
  await canvas.screenshot({ path: outputPath, type: "png" });
  const report = {
    schema_version: 1,
    url,
    elapsed_ms: Date.now() - startedAt,
    browser_version: browser.version(),
    ...evidence,
    required_responses: requiredResponses,
    request_failures: requestFailures,
    tolerated_request_failures: requestFailures.filter((failure) => !fatalRequestFailures.includes(failure)),
    client_markers: Object.fromEntries(contract.required_client_markers.map((marker) => [marker, true])),
    error_count: 0,
    errors: [],
  };
  fs.mkdirSync(path.dirname(reportPath), { recursive: true });
  fs.writeFileSync(reportPath, `${JSON.stringify(report, null, 2)}\n`);
  console.log(`Content Showcase WebGL browser pass: ${evidence.width}x${evidence.height}, ${evidence.renderer}`);
} catch (error) {
  const report = {
    schema_version: 1,
    url,
    elapsed_ms: Date.now() - startedAt,
    console: consoleEntries,
    request_failures: requestFailures,
    error_count: 1,
    errors: [error.stack || String(error)],
  };
  fs.mkdirSync(path.dirname(reportPath), { recursive: true });
  fs.writeFileSync(reportPath, `${JSON.stringify(report, null, 2)}\n`);
  console.error(error.stack || String(error));
  process.exitCode = 1;
} finally {
  if (browser) await browser.close();
}
