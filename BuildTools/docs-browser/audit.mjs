#!/usr/bin/env node

import { createServer } from "node:http";
import { mkdir, readFile, stat, writeFile } from "node:fs/promises";
import { dirname, extname, isAbsolute, join, relative, resolve, sep } from "node:path";
import process from "node:process";
import { fileURLToPath } from "node:url";

const SCHEMA_VERSION = 1;
const GENERATED_BY = "BuildTools/docs-browser/audit.mjs";
const DEFAULT_SITE_DIR = "_site";
const DEFAULT_REPORT = "Workspace/docs-browser-audit-report.json";
const DEFAULT_SCREENSHOTS = "Workspace/docs-browser-screenshots";
const WCAG_TAGS = [
  "wcag2a",
  "wcag2aa",
  "wcag21a",
  "wcag21aa",
  "wcag22a",
  "wcag22aa"
];
const PROFILES = [
  {
    id: "desktop",
    viewport: { width: 1440, height: 1000 },
    colorScheme: "light",
    isMobile: false
  },
  {
    id: "mobile",
    viewport: { width: 390, height: 844 },
    colorScheme: "light",
    isMobile: true,
    hasTouch: true
  },
  {
    id: "zoom-200",
    viewport: { width: 640, height: 512 },
    physicalViewport: { width: 1280, height: 1024 },
    deviceScaleFactor: 2,
    zoomPercent: 200,
    compactNavigation: true,
    colorScheme: "light",
    isMobile: false
  }
];
const MIME_TYPES = new Map([
  [".css", "text/css; charset=utf-8"],
  [".gif", "image/gif"],
  [".html", "text/html; charset=utf-8"],
  [".ico", "image/x-icon"],
  [".jpeg", "image/jpeg"],
  [".jpg", "image/jpeg"],
  [".js", "text/javascript; charset=utf-8"],
  [".json", "application/json; charset=utf-8"],
  [".png", "image/png"],
  [".svg", "image/svg+xml"],
  [".txt", "text/plain; charset=utf-8"],
  [".webp", "image/webp"]
]);
const ENGINE_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");

function usage() {
  return [
    "Usage: npm --prefix BuildTools/docs-browser run audit -- [options]",
    "",
    "Options:",
    `  --site-dir PATH       Rendered Jekyll tree (default: ${DEFAULT_SITE_DIR})`,
    `  --report PATH         JSON report (default: ${DEFAULT_REPORT})`,
    `  --screenshots PATH    Screenshot directory (default: ${DEFAULT_SCREENSHOTS})`,
    "  --route-limit N       Audit only the first N routes (local diagnosis only)",
    "  --help                Show this help"
  ].join("\n");
}

function parseArguments(argv) {
  const options = {
    siteDir: DEFAULT_SITE_DIR,
    report: DEFAULT_REPORT,
    screenshots: DEFAULT_SCREENSHOTS,
    routeLimit: null,
    help: false
  };
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    if (argument === "--help" || argument === "-h") {
      options.help = true;
      continue;
    }
    const value = argv[index + 1];
    if (!value || value.startsWith("--")) {
      throw new Error(`${argument} requires a value`);
    }
    if (argument === "--site-dir") {
      options.siteDir = value;
    } else if (argument === "--report") {
      options.report = value;
    } else if (argument === "--screenshots") {
      options.screenshots = value;
    } else if (argument === "--route-limit") {
      const parsed = Number.parseInt(value, 10);
      if (!Number.isSafeInteger(parsed) || parsed < 1) {
        throw new Error("--route-limit must be a positive integer");
      }
      options.routeLimit = parsed;
    } else {
      throw new Error(`unknown argument: ${argument}`);
    }
    index += 1;
  }
  return options;
}

function resolveFromRoot(root, value) {
  return isAbsolute(value) ? resolve(value) : resolve(root, value);
}

function displayPath(root, value) {
  const local = relative(root, value);
  return local && !local.startsWith(`..${sep}`) && local !== ".."
    ? local.replaceAll("\\", "/")
    : value.replaceAll("\\", "/");
}

async function loadRoutes(siteDir) {
  const catalogPath = join(siteDir, "Docs", "generated", "document-routes.json");
  const catalog = JSON.parse(await readFile(catalogPath, "utf8"));
  const routes = [];
  const seen = new Set();
  for (const document of catalog.routes || []) {
    const candidates = [
      {
        id: document.id,
        documentId: document.id,
        locale: catalog.canonical_locale || "en",
        path: document.current_path
      },
      ...(document.locale_routes || [])
        .filter((route) => route.availability === "available")
        .map((route) => ({
          id: `${document.id}:${route.locale}`,
          documentId: document.id,
          locale: route.locale,
          path: route.path
        }))
    ];
    for (const route of candidates) {
      if (!route.path || seen.has(route.path)) {
        continue;
      }
      seen.add(route.path);
      routes.push(route);
    }
  }
  routes.sort((left, right) => left.path.localeCompare(right.path));
  if (!routes.length) {
    throw new Error("rendered route catalog contains no auditable routes");
  }
  return routes;
}

function requestPath(siteDir, requestUrl) {
  const url = new URL(requestUrl || "/", "http://127.0.0.1");
  let pathname = decodeURIComponent(url.pathname);
  if (pathname.endsWith("/")) {
    pathname += "index.html";
  }
  const candidate = resolve(siteDir, `.${pathname}`);
  const prefix = `${siteDir}${sep}`;
  if (candidate !== siteDir && !candidate.startsWith(prefix)) {
    return null;
  }
  return candidate;
}

async function createSiteServer(siteDir) {
  const server = createServer(async (request, response) => {
    if (request.method !== "GET" && request.method !== "HEAD") {
      response.writeHead(405, { Allow: "GET, HEAD" });
      response.end();
      return;
    }
    const filePath = requestPath(siteDir, request.url);
    if (!filePath) {
      response.writeHead(403);
      response.end();
      return;
    }
    try {
      const fileStats = await stat(filePath);
      if (!fileStats.isFile()) {
        throw new Error("not a file");
      }
      const body = await readFile(filePath);
      response.writeHead(200, {
        "Cache-Control": "no-store",
        "Content-Length": body.length,
        "Content-Type": MIME_TYPES.get(extname(filePath).toLowerCase()) || "application/octet-stream"
      });
      response.end(request.method === "HEAD" ? undefined : body);
    } catch {
      response.writeHead(404);
      response.end();
    }
  });
  await new Promise((accept, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", accept);
  });
  const address = server.address();
  if (!address || typeof address === "string") {
    throw new Error("local documentation server did not expose a TCP port");
  }
  return {
    baseUrl: `http://127.0.0.1:${address.port}`,
    close: () => new Promise((accept, reject) => server.close((error) => error ? reject(error) : accept()))
  };
}

function compactAxeViolation(violation) {
  return {
    id: violation.id,
    impact: violation.impact || "unknown",
    help: violation.help,
    help_url: violation.helpUrl,
    nodes: violation.nodes.map((node) => ({
      target: node.target,
      failure_summary: node.failureSummary || "",
      html: (node.html || "").slice(0, 400)
    }))
  };
}

async function inspectLayout(page, profile) {
  return page.evaluate((compactNavigation) => {
    const documentElement = document.documentElement;
    const main = document.querySelector(".docs-main");
    const content = document.querySelector(".docs-content");
    const header = document.querySelector(".docs-header");
    const sidebar = document.querySelector(".docs-sidebar");
    const navToggle = document.querySelector("[data-nav-toggle]");
    const boxes = {
      main: main?.getBoundingClientRect() || null,
      content: content?.getBoundingClientRect() || null,
      header: header?.getBoundingClientRect() || null,
      sidebar: sidebar?.getBoundingClientRect() || null
    };
    const errors = [];
    if (!main || !content || !header || !sidebar || !navToggle) {
      errors.push("required documentation layout element is missing");
      return errors;
    }
    const initialScrollX = window.scrollX;
    window.scrollTo(documentElement.scrollWidth, window.scrollY);
    const reachableScrollX = window.scrollX;
    window.scrollTo(initialScrollX, window.scrollY);
    if (reachableScrollX > 1) {
      const offenders = Array.from(document.querySelectorAll("body *"))
        .filter((element) => {
          if (compactNavigation && element.closest(".docs-sidebar")) {
            return false;
          }
          const style = getComputedStyle(element);
          const rect = element.getBoundingClientRect();
          return style.position !== "fixed"
            && (rect.left < -1 || rect.right > window.innerWidth + 1);
        })
        .slice(0, 5)
        .map((element) => {
          const rect = element.getBoundingClientRect();
          const identity = element.id
            ? `#${element.id}`
            : `${element.tagName.toLowerCase()}${element.classList.length ? `.${Array.from(element.classList).join(".")}` : ""}`;
          const text = (element.textContent || "").replace(/\s+/g, " ").trim().slice(0, 80);
          const ancestors = [];
          let ancestor = element.parentElement;
          while (ancestor && ancestors.length < 4) {
            const ancestorRect = ancestor.getBoundingClientRect();
            const ancestorStyle = getComputedStyle(ancestor);
            ancestors.push(
              `${ancestor.tagName.toLowerCase()}[${ancestorRect.left.toFixed(1)},${ancestorRect.right.toFixed(1)};`
                + `c${ancestor.clientWidth}/s${ancestor.scrollWidth};${ancestorStyle.overflowX}]`
            );
            ancestor = ancestor.parentElement;
          }
          return `${identity}[${rect.left.toFixed(1)},${rect.right.toFixed(1)}]${text ? ` "${text}"` : ""}`
            + ` via ${ancestors.join(">")}`;
        });
      errors.push(
        `page can scroll horizontally by ${reachableScrollX}px: `
          + `${documentElement.scrollWidth}px > ${documentElement.clientWidth}px`
          + `${offenders.length ? `; elements: ${offenders.join(", ")}` : ""}`
      );
    }
    if (boxes.content.width < 240 || boxes.content.left < -1 || boxes.content.right > window.innerWidth + 1) {
      errors.push("documentation content is clipped outside the viewport");
    }
    if (boxes.content.top < boxes.header.bottom - 1) {
      errors.push("fixed header overlaps documentation content");
    }
    for (const image of content.querySelectorAll("img")) {
      if (!image.complete || image.naturalWidth < 1 || image.naturalHeight < 1) {
        errors.push(`documentation image did not decode: ${image.getAttribute("src") || "(missing src)"}`);
      }
    }
    if (compactNavigation) {
      if (getComputedStyle(navToggle).display === "none") {
        errors.push("mobile navigation toggle is not visible");
      }
      if (!sidebar.inert || sidebar.getAttribute("aria-hidden") !== "true") {
        errors.push("closed mobile navigation is not hidden from keyboard and accessibility trees");
      }
    } else {
      if (getComputedStyle(navToggle).display !== "none") {
        errors.push("desktop navigation toggle must be hidden");
      }
      if (sidebar.inert || sidebar.hasAttribute("aria-hidden")) {
        errors.push("desktop navigation must remain available to keyboard and accessibility trees");
      }
      if (boxes.sidebar.right > boxes.main.left + 1) {
        errors.push("desktop sidebar overlaps documentation content");
      }
    }
    return errors;
  }, profile.compactNavigation ?? profile.isMobile);
}

async function decodeContentImages(page) {
  await page.locator(".docs-content img").evaluateAll(async (images) => {
    for (const image of images) {
      image.loading = "eager";
    }
    await Promise.all(images.map(async (image) => {
      try {
        await image.decode();
      } catch {
        // inspectLayout reports the exact source after all decode attempts settle.
      }
    }));
  });
}

async function runAxe(page, axeSource) {
  await page.addScriptTag({ content: axeSource });
  return page.evaluate(async (tags) => {
    function parseCssColor(value) {
      const match = value.match(/^rgba?\(([^)]+)\)$/i);
      if (!match) {
        return null;
      }
      const parts = match[1].replaceAll(",", " ").replace("/", " ").split(/\s+/).filter(Boolean);
      if (parts.length < 3) {
        return null;
      }
      const channels = parts.slice(0, 3).map((part) => (
        part.endsWith("%") ? Number.parseFloat(part) * 2.55 : Number.parseFloat(part)
      ));
      const alpha = parts.length > 3
        ? (parts[3].endsWith("%") ? Number.parseFloat(parts[3]) / 100 : Number.parseFloat(parts[3]))
        : 1;
      if ([...channels, alpha].some((part) => !Number.isFinite(part))) {
        return null;
      }
      return { red: channels[0], green: channels[1], blue: channels[2], alpha };
    }

    function compositeColor(top, bottom) {
      const alpha = top.alpha + bottom.alpha * (1 - top.alpha);
      if (alpha === 0) {
        return { red: 255, green: 255, blue: 255, alpha: 1 };
      }
      return {
        red: (top.red * top.alpha + bottom.red * bottom.alpha * (1 - top.alpha)) / alpha,
        green: (top.green * top.alpha + bottom.green * bottom.alpha * (1 - top.alpha)) / alpha,
        blue: (top.blue * top.alpha + bottom.blue * bottom.alpha * (1 - top.alpha)) / alpha,
        alpha
      };
    }

    function effectiveBackground(element) {
      const layers = [];
      let current = element;
      while (current) {
        const background = parseCssColor(getComputedStyle(current).backgroundColor);
        if (!background) {
          return null;
        }
        if (background.alpha > 0) {
          layers.push(background);
        }
        current = current.parentElement;
      }
      let result = { red: 255, green: 255, blue: 255, alpha: 1 };
      for (let index = layers.length - 1; index >= 0; index -= 1) {
        result = compositeColor(layers[index], result);
      }
      return result;
    }

    function relativeLuminance(color) {
      const channels = [color.red, color.green, color.blue].map((channel) => {
        const normalized = channel / 255;
        return normalized <= 0.04045
          ? normalized / 12.92
          : ((normalized + 0.055) / 1.055) ** 2.4;
      });
      return channels[0] * 0.2126 + channels[1] * 0.7152 + channels[2] * 0.0722;
    }

    function contrastFallback(node) {
      const selector = node.target?.[0];
      let element = null;
      try {
        element = selector ? document.querySelector(selector) : null;
      } catch {
        return { status: "unresolved", target: node.target, reason: "invalid axe target selector" };
      }
      if (!element) {
        return { status: "unresolved", target: node.target, reason: "axe target is not queryable" };
      }
      const style = getComputedStyle(element);
      const foreground = parseCssColor(style.color);
      const background = effectiveBackground(element);
      if (!foreground || !background) {
        return { status: "unresolved", target: node.target, reason: "computed color is not RGB/RGBA" };
      }
      const compositedForeground = compositeColor(foreground, background);
      const lighter = Math.max(
        relativeLuminance(compositedForeground),
        relativeLuminance(background)
      );
      const darker = Math.min(
        relativeLuminance(compositedForeground),
        relativeLuminance(background)
      );
      const ratio = (lighter + 0.05) / (darker + 0.05);
      const fontSize = Number.parseFloat(style.fontSize);
      const fontWeight = Number.parseInt(style.fontWeight, 10) || 400;
      const largeText = fontSize >= 24 || (fontSize >= 18.66 && fontWeight >= 700);
      const requiredRatio = largeText ? 3 : 4.5;
      return {
        status: ratio + 0.001 >= requiredRatio ? "passed" : "failed",
        target: node.target,
        ratio: Number(ratio.toFixed(3)),
        required_ratio: requiredRatio,
        color: style.color,
        background: `rgb(${background.red.toFixed(1)}, ${background.green.toFixed(1)}, ${background.blue.toFixed(1)})`,
        font_size: style.fontSize,
        font_weight: style.fontWeight
      };
    }

    const results = await window.axe.run(document, {
      runOnly: { type: "tag", values: tags },
      resultTypes: ["violations", "incomplete"]
    });
    return {
      violations: results.violations,
      incomplete: results.incomplete.map((item) => ({
        id: item.id,
        impact: item.impact || "unknown",
        help: item.help,
        help_url: item.helpUrl,
        node_count: item.nodes.length,
        example_targets: item.nodes.slice(0, 3).map((node) => node.target),
        fallback: item.id === "color-contrast"
          ? item.nodes.map(contrastFallback)
          : item.nodes.map((node) => ({
            status: "unresolved",
            target: node.target,
            reason: "no fallback is defined for this axe rule"
          }))
      }))
    };
  }, WCAG_TAGS);
}

async function auditProfile(browser, profile, routes, baseUrl, axeSource) {
  const context = await browser.newContext({
    viewport: profile.viewport,
    deviceScaleFactor: profile.deviceScaleFactor || 1,
    colorScheme: profile.colorScheme,
    isMobile: profile.isMobile,
    hasTouch: profile.hasTouch || false,
    reducedMotion: "reduce"
  });
  const page = await context.newPage();
  const report = {
    id: profile.id,
    viewport: profile.viewport,
    physical_viewport: profile.physicalViewport || profile.viewport,
    device_scale_factor: profile.deviceScaleFactor || 1,
    zoom_percent: profile.zoomPercent || 100,
    route_count: routes.length,
    passed_route_count: 0,
    axe_violation_count: 0,
    axe_incomplete_node_count: 0,
    axe_incomplete_fallback_passed_count: 0,
    axe_incomplete_fallback_failed_count: 0,
    axe_incomplete_unresolved_count: 0,
    axe_incomplete: [],
    errors: []
  };
  const incompleteRules = new Map();
  let activeRoute = null;
  let runtimeErrors = [];
  page.on("console", (message) => {
    if (message.type() === "error") {
      runtimeErrors.push(`console error: ${message.text()}`);
    }
  });
  page.on("pageerror", (error) => {
    runtimeErrors.push(`page error: ${error.message}`);
  });
  page.on("requestfailed", (request) => {
    runtimeErrors.push(`request failed: ${request.method()} ${request.url()} (${request.failure()?.errorText || "unknown"})`);
  });
  page.on("response", (response) => {
    if (response.status() >= 400) {
      runtimeErrors.push(`resource returned HTTP ${response.status()}: ${response.url()}`);
    }
  });

  for (const route of routes) {
    activeRoute = route;
    runtimeErrors = [];
    const routeErrors = [];
    try {
      await page.goto(`${baseUrl}${route.path}`, { waitUntil: "load", timeout: 15000 });
      await page.evaluate(() => document.fonts?.ready || Promise.resolve());
      await decodeContentImages(page);
      routeErrors.push(...await inspectLayout(page, profile));
      const axeResults = await runAxe(page, axeSource);
      for (const incomplete of axeResults.incomplete) {
        report.axe_incomplete_node_count += incomplete.node_count;
        let record = incompleteRules.get(incomplete.id);
        if (!record) {
          record = {
            id: incomplete.id,
            impact: incomplete.impact,
            help: incomplete.help,
            help_url: incomplete.help_url,
            route_count: 0,
            node_count: 0,
            fallback_passed_count: 0,
            fallback_failed_count: 0,
            unresolved_count: 0,
            minimum_contrast_ratio: null,
            example_paths: [],
            example_targets: [],
            fallback_findings: []
          };
          incompleteRules.set(incomplete.id, record);
        }
        record.route_count += 1;
        record.node_count += incomplete.node_count;
        for (const fallback of incomplete.fallback) {
          if (fallback.status === "passed") {
            report.axe_incomplete_fallback_passed_count += 1;
            record.fallback_passed_count += 1;
            if (
              fallback.ratio !== undefined
              && (record.minimum_contrast_ratio === null || fallback.ratio < record.minimum_contrast_ratio)
            ) {
              record.minimum_contrast_ratio = fallback.ratio;
            }
          } else {
            const field = fallback.status === "failed"
              ? "fallback_failed_count"
              : "unresolved_count";
            const reportField = fallback.status === "failed"
              ? "axe_incomplete_fallback_failed_count"
              : "axe_incomplete_unresolved_count";
            report[reportField] += 1;
            record[field] += 1;
            if (record.fallback_findings.length < 8) {
              record.fallback_findings.push({ path: route.path, ...fallback });
            }
          }
        }
        if (record.example_paths.length < 8) {
          record.example_paths.push(route.path);
        }
        for (const target of incomplete.example_targets) {
          if (record.example_targets.length >= 8) {
            break;
          }
          record.example_targets.push({ path: route.path, target });
        }
        if (incomplete.fallback.some((fallback) => fallback.status !== "passed")) {
          routeErrors.push({
            kind: "axe-incomplete-fallback",
            rule: incomplete.id,
            findings: incomplete.fallback.filter((fallback) => fallback.status !== "passed")
          });
        }
      }
      for (const violation of axeResults.violations) {
        report.axe_violation_count += violation.nodes.length;
        routeErrors.push({
          kind: "axe",
          violation: compactAxeViolation(violation)
        });
      }
    } catch (error) {
      routeErrors.push(`browser audit failed: ${error.message}`);
    }
    routeErrors.push(...runtimeErrors);
    if (routeErrors.length) {
      report.errors.push({
        id: activeRoute.id,
        locale: activeRoute.locale,
        path: activeRoute.path,
        findings: routeErrors
      });
    } else {
      report.passed_route_count += 1;
    }
  }
  report.axe_incomplete = Array.from(incompleteRules.values())
    .sort((left, right) => left.id.localeCompare(right.id));
  await context.close();
  return report;
}

async function waitForSearch(page) {
  await page.waitForFunction(() => {
    const status = document.querySelector("[data-search-status]");
    const results = document.querySelectorAll("[data-search-results] a");
    return results.length > 0
      && /(?:\d+\s+results?|Найдено:\s*\d+)/i.test(status?.textContent || "");
  });
}

async function auditDesktopInteractions(browser, baseUrl, routePath, screenshotDir, screenshotPaths) {
  const context = await browser.newContext({
    viewport: PROFILES[0].viewport,
    colorScheme: "light",
    reducedMotion: "reduce",
    permissions: ["clipboard-read", "clipboard-write"]
  });
  const page = await context.newPage();
  const errors = [];
  try {
    await page.goto(`${baseUrl}${routePath}`, { waitUntil: "load" });
    await page.keyboard.press("Tab");
    if (!await page.locator(".skip-link").evaluate((element) => element === document.activeElement)) {
      errors.push("first desktop Tab does not focus the skip link");
    }
    await page.keyboard.press("Enter");
    if (!await page.locator("#main-content").evaluate((element) => element === document.activeElement)) {
      errors.push("activating the skip link does not focus main content");
    }

    await page.keyboard.press("Control+K");
    try {
      await page.waitForFunction(() => {
        const dialog = document.querySelector("[data-search-dialog]");
        const input = document.querySelector("[data-search-input]");
        return dialog?.open && input === document.activeElement;
      }, null, { timeout: 2000 });
    } catch {
      errors.push("Ctrl+K does not open search and focus the query input");
    }
    await page.locator("[data-search-input]").fill("Game.Sync");
    await waitForSearch(page);
    const firstResult = page.locator("[data-search-results] a").first();
    if (!await firstResult.getAttribute("href")) {
      errors.push("technical-identifier search has no linked result");
    }
    await page.keyboard.press("Escape");
    if (await page.locator("[data-search-dialog]").evaluate((element) => element.open)) {
      errors.push("Escape does not close search");
    }

    const initialTheme = await page.locator("html").getAttribute("data-theme");
    await page.locator("[data-theme-toggle]").click();
    const changedTheme = await page.locator("html").getAttribute("data-theme");
    if (initialTheme === changedTheme || changedTheme !== "dark") {
      errors.push("theme toggle does not switch from light to dark");
    }
    await page.reload({ waitUntil: "load" });
    if (await page.locator("html").getAttribute("data-theme") !== "dark") {
      errors.push("theme choice does not persist across reload");
    }
    if (await page.locator("[data-theme-toggle]").getAttribute("aria-label") !== "Use light theme") {
      errors.push("theme toggle accessible name does not track the active theme");
    }

    const copyButton = page.locator(".copy-code").first();
    if (await copyButton.count()) {
      await copyButton.click();
      try {
        await page.waitForFunction(
          () => document.querySelector(".copy-code")?.textContent === "Copied",
          null,
          { timeout: 2000 }
        );
      } catch {
        errors.push("code copy control does not expose successful state");
      }
    }

    await page.evaluate(() => window.scrollTo(0, 0));
    const screenshotPath = join(screenshotDir, "desktop-documentation.png");
    await page.screenshot({ path: screenshotPath, fullPage: true });
    screenshotPaths.push("desktop-documentation.png");
  } catch (error) {
    errors.push(`desktop interaction audit failed: ${error.message}`);
  }
  await context.close();
  return {
    id: "desktop-keyboard-search-theme-copy",
    path: routePath,
    passed: errors.length === 0,
    errors
  };
}

async function auditMobileInteractions(browser, baseUrl, routePath, screenshotDir, screenshotPaths) {
  const context = await browser.newContext({
    viewport: PROFILES[1].viewport,
    colorScheme: "light",
    isMobile: true,
    hasTouch: true,
    reducedMotion: "reduce"
  });
  const page = await context.newPage();
  const errors = [];
  try {
    await page.goto(`${baseUrl}${routePath}`, { waitUntil: "load" });
    const toggle = page.locator("[data-nav-toggle]");
    const sidebar = page.locator(".docs-sidebar");
    await toggle.click();
    await page.waitForFunction(
      () => Math.abs(document.querySelector(".docs-sidebar")?.getBoundingClientRect().left || 0) < 1
    );
    if (await toggle.getAttribute("aria-expanded") !== "true") {
      errors.push("mobile menu toggle does not expose its open state");
    }
    if (await sidebar.getAttribute("aria-hidden") === "true" || await sidebar.getAttribute("inert") !== null) {
      errors.push("open mobile navigation remains hidden from keyboard or accessibility trees");
    }
    if (!await sidebar.evaluate((element) => element.contains(document.activeElement))) {
      errors.push("opening mobile navigation does not move focus into it");
    }
    await page.keyboard.press("Shift+Tab");
    if (!await toggle.evaluate((element) => element === document.activeElement)) {
      errors.push("mobile navigation focus cycle does not include its toggle");
    }
    await page.keyboard.press("Shift+Tab");
    if (!await sidebar.evaluate((element) => element.contains(document.activeElement))) {
      errors.push("mobile navigation focus escapes into the obscured page");
    }

    const navigationScreenshot = join(screenshotDir, "mobile-navigation.png");
    await page.screenshot({ path: navigationScreenshot, fullPage: false });
    screenshotPaths.push("mobile-navigation.png");

    await page.keyboard.press("Escape");
    if (await toggle.getAttribute("aria-expanded") !== "false") {
      errors.push("Escape does not close mobile navigation");
    }
    if (!await toggle.evaluate((element) => element === document.activeElement)) {
      errors.push("closing mobile navigation does not restore toggle focus");
    }
    if (await sidebar.getAttribute("aria-hidden") !== "true" || await sidebar.getAttribute("inert") === null) {
      errors.push("closed mobile navigation remains keyboard-accessible");
    }

    await toggle.click();
    await page.locator(".sidebar-search").click();
    try {
      await page.waitForFunction(() => {
        const dialog = document.querySelector("[data-search-dialog]");
        const input = document.querySelector("[data-search-input]");
        return dialog?.open && input === document.activeElement;
      }, null, { timeout: 2000 });
    } catch {
      errors.push("mobile navigation search control does not open search and focus its input");
    }
    await page.locator("[data-search-input]").fill("map baking");
    await waitForSearch(page);
    await page.keyboard.press("Escape");

    await page.evaluate(() => window.scrollTo(0, 0));
    const screenshotPath = join(screenshotDir, "mobile-documentation.png");
    await page.screenshot({ path: screenshotPath, fullPage: true });
    screenshotPaths.push("mobile-documentation.png");
  } catch (error) {
    errors.push(`mobile interaction audit failed: ${error.message}`);
  }
  await context.close();
  return {
    id: "mobile-navigation-keyboard-search",
    path: routePath,
    passed: errors.length === 0,
    errors
  };
}

async function auditZoomInteractions(browser, baseUrl, routePath, screenshotDir, screenshotPaths) {
  const profile = PROFILES.find((candidate) => candidate.id === "zoom-200");
  const context = await browser.newContext({
    viewport: profile.viewport,
    deviceScaleFactor: profile.deviceScaleFactor,
    colorScheme: profile.colorScheme,
    isMobile: profile.isMobile,
    hasTouch: false,
    reducedMotion: "reduce"
  });
  const page = await context.newPage();
  const errors = [];
  try {
    await page.goto(`${baseUrl}${routePath}`, { waitUntil: "load" });
    await page.evaluate(() => document.fonts?.ready || Promise.resolve());
    await decodeContentImages(page);
    errors.push(...await inspectLayout(page, profile));

    const state = await page.evaluate(() => {
      const heading = document.querySelector(".docs-content h1");
      const header = document.querySelector(".docs-header");
      const headingBox = heading?.getBoundingClientRect() || null;
      const headerBox = header?.getBoundingClientRect() || null;
      return {
        language: document.documentElement.lang,
        devicePixelRatio: window.devicePixelRatio,
        innerWidth: window.innerWidth,
        innerHeight: window.innerHeight,
        headingVisible: Boolean(
          headingBox
          && headerBox
          && headingBox.top >= headerBox.bottom - 1
          && headingBox.top < window.innerHeight
          && headingBox.right <= window.innerWidth + 1
        )
      };
    });
    if (state.language !== "ru") {
      errors.push("200 percent zoom evidence does not use a Russian documentation route");
    }
    if (Math.abs(state.devicePixelRatio - profile.deviceScaleFactor) > 0.01) {
      errors.push(`200 percent zoom device scale is ${state.devicePixelRatio}, expected ${profile.deviceScaleFactor}`);
    }
    if (state.innerWidth !== profile.viewport.width || state.innerHeight !== profile.viewport.height) {
      errors.push(
        `200 percent zoom CSS viewport is ${state.innerWidth}x${state.innerHeight}, `
          + `expected ${profile.viewport.width}x${profile.viewport.height}`
      );
    }
    if (!state.headingVisible) {
      errors.push("Russian heading is not visible in the 200 percent zoom viewport");
    }

    const toggle = page.locator("[data-nav-toggle]");
    const sidebar = page.locator(".docs-sidebar");
    await toggle.click();
    await page.waitForFunction(
      () => Math.abs(document.querySelector(".docs-sidebar")?.getBoundingClientRect().left || 0) < 1
    );
    if (await toggle.getAttribute("aria-expanded") !== "true") {
      errors.push("200 percent zoom navigation toggle does not expose its open state");
    }
    if (!await sidebar.evaluate((element) => element.contains(document.activeElement))) {
      errors.push("200 percent zoom navigation does not move focus into the drawer");
    }
    await page.keyboard.press("Escape");
    if (await toggle.getAttribute("aria-expanded") !== "false") {
      errors.push("Escape does not close 200 percent zoom navigation");
    }
    if (!await toggle.evaluate((element) => element === document.activeElement)) {
      errors.push("closing 200 percent zoom navigation does not restore toggle focus");
    }
    await page.waitForFunction(() => {
      const sidebarBox = document.querySelector(".docs-sidebar")?.getBoundingClientRect();
      return sidebarBox && sidebarBox.right <= 1;
    });

    await page.evaluate(() => window.scrollTo(0, 0));
    const screenshotName = "zoom-200-russian-documentation.png";
    await page.screenshot({ path: join(screenshotDir, screenshotName), fullPage: false });
    screenshotPaths.push(screenshotName);
  } catch (error) {
    errors.push(`200 percent zoom interaction audit failed: ${error.message}`);
  }
  await context.close();
  return {
    id: "zoom-200-russian-reflow",
    path: routePath,
    zoom_percent: profile.zoomPercent,
    css_viewport: profile.viewport,
    physical_viewport: profile.physicalViewport,
    passed: errors.length === 0,
    errors
  };
}

async function auditLocaleInteractions(
  browser,
  baseUrl,
  englishPath,
  russianPath,
  screenshotDir,
  screenshotPaths,
  options = {}
) {
  const interactionId = options.id || "locale-switch-russian-search";
  const screenshotName = options.screenshot || "russian-documentation.png";
  const verifySearch = options.verifySearch !== false;
  const context = await browser.newContext({
    viewport: PROFILES[0].viewport,
    colorScheme: "light",
    reducedMotion: "reduce"
  });
  const page = await context.newPage();
  const errors = [];
  try {
    await page.goto(`${baseUrl}${russianPath}`, { waitUntil: "load" });
    if (await page.locator("html").getAttribute("lang") !== "ru") {
      errors.push("Russian documentation route does not render with html lang=ru");
    }
    const localeSwitch = page.locator(".locale-switch");
    if (await localeSwitch.count() !== 1) {
      errors.push("translated documentation route does not expose one language switch");
    } else {
      const russianOption = localeSwitch.locator('[hreflang="ru"]');
      const englishOption = localeSwitch.locator('[hreflang="en"]');
      if (await russianOption.getAttribute("aria-current") !== "page") {
        errors.push("language switch does not mark Russian as the active locale");
      }

      if (verifySearch) {
        await page.keyboard.press("Control+K");
        await page.locator("[data-search-input]").fill("игровой клиент");
        await waitForSearch(page);
        const firstResult = page.locator("[data-search-results] a").first();
        const resultHref = await firstResult.getAttribute("href");
        if (!resultHref?.startsWith("/Docs/ru/")) {
          errors.push("Russian search does not return a Russian documentation route");
        }
        await page.keyboard.press("Escape");
      }

      await englishOption.click();
      await page.waitForLoadState("load");
      if (new URL(page.url()).pathname !== englishPath) {
        errors.push("English language switch does not open the paired English route");
      }
      if (await page.locator("html").getAttribute("lang") !== "en") {
        errors.push("English language switch target does not render with html lang=en");
      }
    }

    await page.goto(`${baseUrl}${russianPath}`, { waitUntil: "load" });
    const screenshotPath = join(screenshotDir, screenshotName);
    await page.screenshot({ path: screenshotPath, fullPage: true });
    screenshotPaths.push(screenshotName);
  } catch (error) {
    errors.push(`locale interaction audit failed: ${error.message}`);
  }
  await context.close();
  return {
    id: interactionId,
    path: russianPath,
    paired_path: englishPath,
    passed: errors.length === 0,
    errors
  };
}

async function auditDiagramRendering(browser, baseUrl, routePath, screenshotDir, screenshotPaths) {
  const errors = [];
  for (const profile of PROFILES) {
    const context = await browser.newContext({
      viewport: profile.viewport,
      deviceScaleFactor: profile.deviceScaleFactor || 1,
      colorScheme: profile.colorScheme,
      isMobile: profile.isMobile,
      hasTouch: profile.hasTouch || false,
      reducedMotion: "reduce"
    });
    const page = await context.newPage();
    try {
      await page.goto(`${baseUrl}${routePath}`, { waitUntil: "load" });
      const diagram = page.locator(".docs-diagram").first();
      if (await diagram.count() !== 1) {
        errors.push(`${profile.id} architecture page does not expose one primary documentation diagram`);
      } else {
        const image = diagram.locator("img");
        const decoded = await image.evaluate(
          (element) => element.complete && element.naturalWidth > 0 && element.naturalHeight > 0
        );
        if (!decoded) {
          errors.push(`${profile.id} architecture diagram did not decode`);
        }
        await diagram.evaluate((element) => {
          const documentTop = element.getBoundingClientRect().top + window.scrollY;
          window.scrollTo(0, Math.max(0, documentTop - 84));
        });
        const diagramBox = await diagram.boundingBox();
        const headerBox = await page.locator(".docs-header").boundingBox();
        if (diagramBox && headerBox && diagramBox.y < headerBox.y + headerBox.height + 8) {
          errors.push(`${profile.id} architecture diagram screenshot remains under the fixed header`);
        }
        await page.locator(".docs-header").evaluate((element) => {
          element.style.visibility = "hidden";
        });
        const screenshotName = `architecture-diagram-${profile.id}.png`;
        await diagram.screenshot({ path: join(screenshotDir, screenshotName) });
        screenshotPaths.push(screenshotName);
      }
    } catch (error) {
      errors.push(`${profile.id} diagram rendering audit failed: ${error.message}`);
    }
    await context.close();
  }
  return {
    id: "architecture-diagram-rendering",
    path: routePath,
    passed: errors.length === 0,
    errors
  };
}

async function main() {
  const root = ENGINE_ROOT;
  let options;
  try {
    options = parseArguments(process.argv.slice(2));
  } catch (error) {
    process.stderr.write(`${error.message}\n\n${usage()}\n`);
    return 2;
  }
  if (options.help) {
    process.stdout.write(`${usage()}\n`);
    return 0;
  }

  const siteDir = resolveFromRoot(root, options.siteDir);
  const reportPath = resolveFromRoot(root, options.report);
  const screenshotDir = resolveFromRoot(root, options.screenshots);
  await mkdir(reportPath.substring(0, reportPath.lastIndexOf(sep)), { recursive: true });
  await mkdir(screenshotDir, { recursive: true });

  let browser = null;
  let server = null;
  let report;
  try {
    const [{ chromium }, axe] = await Promise.all([
      import("playwright"),
      import("axe-core")
    ]);
    const allRoutes = await loadRoutes(siteDir);
    const routes = options.routeLimit === null
      ? allRoutes
      : allRoutes.slice(0, options.routeLimit);
    server = await createSiteServer(siteDir);
    browser = await chromium.launch({ headless: true });
    const profiles = [];
    for (const profile of PROFILES) {
      profiles.push(await auditProfile(browser, profile, routes, server.baseUrl, axe.default.source));
    }
    const representative = allRoutes.find((route) => route.path === "/Docs/README.html") || allRoutes[0];
    const architecture = allRoutes.find(
      (route) => route.locale === "en" && route.documentId === "engine-architecture"
    );
    const russian = allRoutes.find((route) => route.locale === "ru");
    const russianZoom = allRoutes.find(
      (route) => route.locale === "ru" && route.documentId === "documentation-site-publication"
    ) || russian;
    const englishPair = russian
      ? allRoutes.find((route) => route.locale === "en" && route.documentId === russian.documentId)
      : null;
    const screenshots = [];
    const russianEntrypoints = allRoutes.filter(
      (route) => route.locale === "ru" && !route.path.startsWith("/Docs/")
    );
    const entrypointInteractions = [];
    for (const russianEntrypoint of russianEntrypoints) {
      const englishEntrypointPair = allRoutes.find(
        (route) => route.locale === "en" && route.documentId === russianEntrypoint.documentId
      );
      const entrypointId = russianEntrypoint.documentId.replace(/[^a-z0-9-]/gi, "-");
      if (!englishEntrypointPair) {
        entrypointInteractions.push({
          id: `locale-switch-russian-entrypoint-${entrypointId}`,
          path: russianEntrypoint.path,
          passed: false,
          errors: ["translated Russian entrypoint has no paired English route"]
        });
        continue;
      }
      entrypointInteractions.push(
        await auditLocaleInteractions(
          browser,
          server.baseUrl,
          englishEntrypointPair.path,
          russianEntrypoint.path,
          screenshotDir,
          screenshots,
          {
            id: `locale-switch-russian-entrypoint-${entrypointId}`,
            screenshot: `russian-entrypoint-${entrypointId}.png`,
            verifySearch: false
          }
        )
      );
    }
    if (entrypointInteractions.length === 0) {
      entrypointInteractions.push({
        id: "locale-switch-russian-entrypoint",
        path: "/",
        passed: false,
        errors: ["translated Russian entrypoint pair is absent from the generated route catalog"]
      });
    }
    const interactions = [
      await auditDesktopInteractions(
        browser,
        server.baseUrl,
        representative.path,
        screenshotDir,
        screenshots
      ),
      await auditMobileInteractions(
        browser,
        server.baseUrl,
        representative.path,
        screenshotDir,
        screenshots
      ),
      ...(russianZoom
        ? [
          await auditZoomInteractions(
            browser,
            server.baseUrl,
            russianZoom.path,
            screenshotDir,
            screenshots
          )
        ]
        : [{
          id: "zoom-200-russian-reflow",
          path: "/Docs/ru/contributing/documentation/site-publication.html",
          passed: false,
          errors: ["Russian documentation route is absent from the generated route catalog"]
        }]),
      ...(russian && englishPair
        ? [
          await auditLocaleInteractions(
            browser,
            server.baseUrl,
            englishPair.path,
            russian.path,
            screenshotDir,
            screenshots
          )
        ]
        : [{
          id: "locale-switch-russian-search",
          path: russian?.path || "/Docs/ru/",
          passed: false,
          errors: ["translated EN/RU route pair is absent from the generated route catalog"]
        }]),
      ...entrypointInteractions,
      ...(architecture
        ? [
          await auditDiagramRendering(
            browser,
            server.baseUrl,
            architecture.path,
            screenshotDir,
            screenshots
          )
        ]
        : [{
          id: "architecture-diagram-rendering",
          path: "/Docs/en/explanation/architecture/",
          passed: false,
          errors: ["architecture route is absent from the generated route catalog"]
        }])
    ];
    const errors = [
      ...profiles.flatMap((profile) => profile.errors.map((error) => ({
        profile: profile.id,
        ...error
      }))),
      ...interactions
        .filter((interaction) => !interaction.passed)
        .map((interaction) => ({
          profile: interaction.id,
          path: interaction.path,
          findings: interaction.errors
        }))
    ];
    report = {
      schema_version: SCHEMA_VERSION,
      generated_by: GENERATED_BY,
      target: "rendered-jekyll-artifact",
      site_dir: displayPath(root, siteDir),
      wcag_target: "WCAG 2.2 Level AA automated axe-core subset",
      wcag_tags: WCAG_TAGS,
      runtime: {
        node: process.version,
        playwright: "1.62.0",
        chromium: browser.version(),
        axe_core: axe.default.version
      },
      audited_route_count: routes.length,
      audited_page_count: routes.length * PROFILES.length,
      profiles,
      interactions,
      screenshots: screenshots.sort(),
      error_count: errors.length,
      errors
    };
  } catch (error) {
    report = {
      schema_version: SCHEMA_VERSION,
      generated_by: GENERATED_BY,
      target: "rendered-jekyll-artifact",
      site_dir: displayPath(root, siteDir),
      error_count: 1,
      errors: [{ profile: "harness", findings: [error.stack || error.message] }]
    };
  } finally {
    if (browser) {
      await browser.close();
    }
    if (server) {
      await server.close();
    }
  }

  await writeFile(reportPath, `${JSON.stringify(report, null, 2)}\n`, "utf8");
  if (report.error_count) {
    process.stderr.write(
      `Documentation browser audit failed with ${report.error_count} route or interaction finding group(s).\n`
    );
    return 1;
  }
  process.stdout.write(
    `Documentation browser audit passed: ${report.audited_page_count} rendered page checks, `
      + `${report.interactions.length} interaction profiles, ${report.screenshots.length} screenshots.\n`
  );
  return 0;
}

process.exitCode = await main();
