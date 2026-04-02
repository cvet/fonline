"use strict";
var __create = Object.create;
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __getProtoOf = Object.getPrototypeOf;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __export = (target, all) => {
  for (var name in all)
    __defProp(target, name, { get: all[name], enumerable: true });
};
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toESM = (mod, isNodeMode, target) => (target = mod != null ? __create(__getProtoOf(mod)) : {}, __copyProps(
  // If the importer is in node compatibility mode or this is not an ESM
  // file that has been converted to a CommonJS file using a Babel-
  // compatible transform (i.e. "__esModule" has not been set), then set
  // "default" to the CommonJS "module.exports" for node compatibility.
  isNodeMode || !mod || !mod.__esModule ? __defProp(target, "default", { value: mod, enumerable: true }) : target,
  mod
));
var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);

// src/extension.ts
var extension_exports = {};
__export(extension_exports, {
  activate: () => activate,
  deactivate: () => deactivate
});
module.exports = __toCommonJS(extension_exports);
var vscode = __toESM(require("vscode"));
var COMMAND_ID = "fonline-tools.showTools";
var STATUS_BAR_TEXT = "$(tools) FOnline Tools";
var TASK_GROUP_SEPARATOR = "::";
var PLATFORM_TAG_PATTERN = /\[(windows|linux|macos|win32|win64|osx|darwin)\]/gi;
var HIDDEN_TAG_PATTERN = /\[hidden\]/gi;
var SORT_TAG_PATTERN = /\[sort:(\d+)\]/gi;
var PLATFORM_DEBUG_TYPES = {
  cppvsdbg: ["windows"]
};
function decodeUtf8(bytes) {
  let encoded = "";
  for (const byte of bytes) {
    encoded += `%${byte.toString(16).padStart(2, "0")}`;
  }
  return decodeURIComponent(encoded);
}
function getWorkspaceFolder() {
  return vscode.workspace.workspaceFolders?.[0];
}
async function readJsonFile(fileUri) {
  try {
    const raw = await vscode.workspace.fs.readFile(fileUri);
    return JSON.parse(decodeUtf8(raw));
  } catch {
    return void 0;
  }
}
function getHostPlatform() {
  const platform = getNodePlatform();
  if (platform === "win32") {
    return "windows";
  }
  if (platform === "linux") {
    return "linux";
  }
  if (platform === "darwin") {
    return "macos";
  }
  return "unknown";
}
function getExplicitPlatforms(value) {
  const platforms = [];
  if (value.windows !== void 0) {
    platforms.push("windows");
  }
  if (value.linux !== void 0) {
    platforms.push("linux");
  }
  if (value.osx !== void 0) {
    platforms.push("macos");
  }
  return platforms;
}
function normalizeTaggedPlatform(rawPlatform) {
  const platform = rawPlatform.trim().toLowerCase();
  if (platform === "windows" || platform === "win32" || platform === "win64") {
    return "windows";
  }
  if (platform === "linux") {
    return "linux";
  }
  if (platform === "macos" || platform === "osx" || platform === "darwin") {
    return "macos";
  }
  return void 0;
}
function extractPlatformTags(text) {
  const result = /* @__PURE__ */ new Set();
  for (const match of text.matchAll(PLATFORM_TAG_PATTERN)) {
    const platform = match[1] ? normalizeTaggedPlatform(match[1]) : void 0;
    if (platform) {
      result.add(platform);
    }
  }
  return Array.from(result);
}
function stripPlatformTags(text) {
  return text.replace(PLATFORM_TAG_PATTERN, "").replace(HIDDEN_TAG_PATTERN, "").replace(SORT_TAG_PATTERN, "").replace(/\s{2,}/g, " ").trim();
}
function hasHiddenTag(text) {
  return /\[hidden\]/i.test(text);
}
function extractSortPriority(text) {
  const match = /\[sort:(\d+)\]/i.exec(text);
  return match ? parseInt(match[1], 10) : void 0;
}
function extractPlatformQualifiers(text) {
  const result = /* @__PURE__ */ new Set();
  const matches = text.matchAll(/\(([^)]+)\)/g);
  for (const match of matches) {
    const rawQualifier = match[1]?.trim().toLowerCase();
    if (!rawQualifier) {
      continue;
    }
    const firstWord = rawQualifier.split(/\s+/)[0];
    if (firstWord === "windows" || firstWord === "win32" || firstWord === "win64") {
      result.add("windows");
    } else if (firstWord === "linux") {
      result.add("linux");
    } else if (firstWord === "macos" || firstWord === "osx" || firstWord === "darwin") {
      result.add("macos");
    }
  }
  return Array.from(result);
}
function matchesCurrentPlatform(taggedPlatforms, explicitPlatforms, qualifiedPlatforms) {
  const hostPlatform = getHostPlatform();
  if (taggedPlatforms.length !== 0 && hostPlatform !== "unknown") {
    return taggedPlatforms.includes(hostPlatform);
  }
  if (qualifiedPlatforms.length !== 0 && hostPlatform !== "unknown") {
    return qualifiedPlatforms.includes(hostPlatform);
  }
  if (explicitPlatforms.length !== 0 && hostPlatform !== "unknown") {
    return explicitPlatforms.includes(hostPlatform);
  }
  return true;
}
function isTaskDefinitionSupported(definition) {
  return matchesCurrentPlatform(
    extractPlatformTags(definition.label ?? ""),
    getExplicitPlatforms(definition),
    extractPlatformQualifiers(definition.label ?? "")
  );
}
function isLaunchDefinitionSupported(definition) {
  const debugTypePlatforms = PLATFORM_DEBUG_TYPES[definition.type ?? ""] ?? [];
  if (!matchesCurrentPlatform(
    extractPlatformTags(definition.name ?? ""),
    getExplicitPlatforms(definition),
    extractPlatformQualifiers(definition.name ?? "")
  )) {
    return false;
  }
  return matchesCurrentPlatform([], debugTypePlatforms, []);
}
function isLaunchCompoundSupported(compound, allowedConfigNames) {
  if (!matchesCurrentPlatform(
    extractPlatformTags(compound.name ?? ""),
    getExplicitPlatforms(compound),
    extractPlatformQualifiers(compound.name ?? "")
  )) {
    return false;
  }
  const configurations = compound.configurations ?? [];
  return configurations.length !== 0 && configurations.every((configName) => allowedConfigNames.has(configName));
}
async function readWorkspaceTaskLabels() {
  const workspaceFolder = getWorkspaceFolder();
  if (!workspaceFolder) {
    return /* @__PURE__ */ new Set();
  }
  const tasksUri = vscode.Uri.joinPath(workspaceFolder.uri, ".vscode", "tasks.json");
  const parsed = await readJsonFile(tasksUri);
  if (!parsed) {
    return /* @__PURE__ */ new Set();
  }
  return new Set(
    (parsed.tasks ?? []).filter(isTaskDefinitionSupported).flatMap((task) => task.label ? [task.label] : [])
  );
}
function isMainWorkspaceTask(task, workspaceFolder) {
  const scope = task.scope;
  if (scope === vscode.TaskScope.Workspace) {
    return true;
  }
  return !!scope && "uri" in scope && scope.uri.toString() === workspaceFolder.uri.toString();
}
function getNodePlatform() {
  const nodeGlobal = globalThis;
  return nodeGlobal.process?.platform ?? "";
}
var GROUP_DESCRIPTIONS = /* @__PURE__ */ new Set(["Task Group", "Launch Group"]);
function sortPicks(picks) {
  return [...picks].sort((left, right) => {
    const leftIsGroup = GROUP_DESCRIPTIONS.has(left.description ?? "") ? 1 : 0;
    const rightIsGroup = GROUP_DESCRIPTIONS.has(right.description ?? "") ? 1 : 0;
    if (leftIsGroup !== rightIsGroup) {
      return leftIsGroup - rightIsGroup;
    }
    const leftSort = left.sortPriority ?? Infinity;
    const rightSort = right.sortPriority ?? Infinity;
    if (leftSort !== rightSort) {
      return leftSort - rightSort;
    }
    return left.label.localeCompare(right.label);
  });
}
function parseGroupedTaskName(taskName) {
  const separatorIndex = taskName.indexOf(TASK_GROUP_SEPARATOR);
  if (separatorIndex <= 0) {
    return void 0;
  }
  const groupLabel = stripPlatformTags(taskName.slice(0, separatorIndex).trim());
  const itemLabel = stripPlatformTags(taskName.slice(separatorIndex + TASK_GROUP_SEPARATOR.length).trim());
  if (!groupLabel || !itemLabel) {
    return void 0;
  }
  return { groupLabel, itemLabel };
}
async function showSubmenu(title, placeHolder, picks) {
  if (picks.length === 0) {
    void vscode.window.showInformationMessage(`No ${title.toLowerCase()} entries found.`);
    return;
  }
  const selected = await vscode.window.showQuickPick(sortPicks(picks), {
    title: `FOnline Tools: ${title}`,
    placeHolder,
    matchOnDescription: true,
    matchOnDetail: true
  });
  if (selected) {
    await selected.run();
  }
}
async function getTaskPicks() {
  const workspaceFolder = getWorkspaceFolder();
  if (!workspaceFolder) {
    return [];
  }
  const allowedLabels = await readWorkspaceTaskLabels();
  if (allowedLabels.size === 0) {
    return [];
  }
  const tasks2 = await vscode.tasks.fetchTasks();
  const taskPicks = tasks2.filter((task) => allowedLabels.has(task.name) && isMainWorkspaceTask(task, workspaceFolder) && !hasHiddenTag(task.name)).map((task) => ({
    label: stripPlatformTags(task.name),
    description: "Task",
    sortPriority: extractSortPriority(task.name),
    run: () => vscode.tasks.executeTask(task)
  }));
  const groupedTaskEntries = /* @__PURE__ */ new Map();
  const directTaskPicks = [];
  for (const pick of taskPicks) {
    const groupedName = parseGroupedTaskName(pick.label);
    if (!groupedName) {
      directTaskPicks.push(pick);
      continue;
    }
    const groupPicks = groupedTaskEntries.get(groupedName.groupLabel) ?? [];
    groupPicks.push({ ...pick, label: groupedName.itemLabel });
    groupedTaskEntries.set(groupedName.groupLabel, groupPicks);
  }
  const groupedTaskPicks = Array.from(groupedTaskEntries.entries()).map(([groupLabel, groupPicks]) => ({
    label: `${groupLabel} ...`,
    description: "Task Group",
    run: () => showSubmenu(groupLabel, `Run a task from ${groupLabel}`, groupPicks)
  }));
  return [...directTaskPicks, ...groupedTaskPicks];
}
function getLaunchPicks() {
  const workspaceFolder = getWorkspaceFolder();
  const launchConfig = vscode.workspace.getConfiguration("launch", workspaceFolder?.uri);
  const configurations = (launchConfig.get("configurations") ?? []).filter(isLaunchDefinitionSupported);
  const allowedConfigNames = new Set(configurations.flatMap((config) => config.name ? [config.name] : []));
  const compounds = (launchConfig.get("compounds") ?? []).filter((compound) => isLaunchCompoundSupported(compound, allowedConfigNames));
  const configPicks = configurations.filter((config) => !!config.name && !hasHiddenTag(config.name)).map((config) => ({
    label: stripPlatformTags(config.name),
    description: "Launch",
    sortPriority: extractSortPriority(config.name),
    run: () => vscode.debug.startDebugging(workspaceFolder, config.name)
  }));
  const compoundPicks = compounds.filter((compound) => !!compound.name && !hasHiddenTag(compound.name)).map((compound) => ({
    label: stripPlatformTags(compound.name),
    description: "Launch Compound",
    sortPriority: extractSortPriority(compound.name),
    run: () => vscode.debug.startDebugging(workspaceFolder, compound.name)
  }));
  const allPicks = [...configPicks, ...compoundPicks];
  const groupedEntries = /* @__PURE__ */ new Map();
  const directPicks = [];
  for (const pick of allPicks) {
    const groupedName = parseGroupedTaskName(pick.label);
    if (!groupedName) {
      directPicks.push(pick);
      continue;
    }
    const groupPicks = groupedEntries.get(groupedName.groupLabel) ?? [];
    groupPicks.push({ ...pick, label: groupedName.itemLabel });
    groupedEntries.set(groupedName.groupLabel, groupPicks);
  }
  const groupedPicks = Array.from(groupedEntries.entries()).map(([groupLabel, groupPicks]) => ({
    label: `${groupLabel} ...`,
    description: "Launch Group",
    run: () => showSubmenu(groupLabel, `Run a launch configuration from ${groupLabel}`, groupPicks)
  }));
  return [...directPicks, ...groupedPicks];
}
var MAX_SORT_HOTKEYS = 9;
var cachedSortedPicks = [];
async function refreshSortedPicks() {
  const allPicks = [...await getTaskPicks(), ...getLaunchPicks()];
  cachedSortedPicks = allPicks.filter((p) => p.sortPriority !== void 0).sort((a, b) => (a.sortPriority ?? Infinity) - (b.sortPriority ?? Infinity));
  return cachedSortedPicks;
}
async function showToolsPicker() {
  const picks = sortPicks([...await getTaskPicks(), ...getLaunchPicks()]);
  if (picks.length === 0) {
    void vscode.window.showInformationMessage("No FOnline tasks or launch configurations found.");
    return;
  }
  cachedSortedPicks = picks.filter((p) => p.sortPriority !== void 0).sort((a, b) => (a.sortPriority ?? Infinity) - (b.sortPriority ?? Infinity));
  const selected = await vscode.window.showQuickPick(picks, {
    title: "FOnline Tools",
    placeHolder: "Run a task or launch configuration",
    matchOnDescription: true,
    matchOnDetail: true
  });
  if (selected) {
    await selected.run();
  }
}
function activate(context) {
  const showToolsCommand = vscode.commands.registerCommand(COMMAND_ID, async () => {
    await showToolsPicker();
  });
  for (let i = 1; i <= MAX_SORT_HOTKEYS; i++) {
    const command = vscode.commands.registerCommand(`fonline-tools.runSorted${i}`, async () => {
      if (cachedSortedPicks.length === 0) {
        await refreshSortedPicks();
      }
      const pick = cachedSortedPicks.find((p) => p.sortPriority === i);
      if (pick) {
        await pick.run();
      } else {
        void vscode.window.showWarningMessage(`No FOnline item with [sort:${i}] found.`);
      }
    });
    context.subscriptions.push(command);
  }
  const statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 90);
  statusBarItem.name = "FOnline Tools";
  statusBarItem.text = STATUS_BAR_TEXT;
  statusBarItem.tooltip = "Show FOnline tasks and launch configurations";
  statusBarItem.command = COMMAND_ID;
  statusBarItem.show();
  context.subscriptions.push(showToolsCommand, statusBarItem);
}
function deactivate() {
}
// Annotate the CommonJS export names for ESM import in node:
0 && (module.exports = {
  activate,
  deactivate
});
//# sourceMappingURL=extension.js.map
