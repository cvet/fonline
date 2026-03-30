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
  return text.replace(PLATFORM_TAG_PATTERN, "").replace(/\s{2,}/g, " ").trim();
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
function normalizeBuildPresetInheritance(inherits) {
  if (!inherits) {
    return [];
  }
  return Array.isArray(inherits) ? inherits : [inherits];
}
function getNodePlatform() {
  const nodeGlobal = globalThis;
  return nodeGlobal.process?.platform ?? "";
}
function getHostBuildPresetName() {
  return getNodePlatform() === "win32" ? "base-msvc" : "base-ninja";
}
function collectBuildTargets(buildPresets, presetName, visited, result) {
  if (visited.has(presetName)) {
    return;
  }
  visited.add(presetName);
  const preset = buildPresets.get(presetName);
  if (!preset) {
    return;
  }
  for (const inheritedPreset of normalizeBuildPresetInheritance(preset.inherits)) {
    collectBuildTargets(buildPresets, inheritedPreset, visited, result);
  }
  for (const target of preset.targets ?? []) {
    if (!result.includes(target)) {
      result.push(target);
    }
  }
}
async function getCMakeBuildTargetPicks() {
  const workspaceFolder = getWorkspaceFolder();
  if (!workspaceFolder) {
    return [];
  }
  const presetsUri = vscode.Uri.joinPath(workspaceFolder.uri, "CMakePresets.json");
  const presets = await readJsonFile(presetsUri);
  if (!presets?.buildPresets?.length) {
    return [];
  }
  const buildPresets = new Map(
    presets.buildPresets.filter((preset) => !!preset.name).map((preset) => [preset.name, preset])
  );
  const targets = [];
  collectBuildTargets(buildPresets, getHostBuildPresetName(), /* @__PURE__ */ new Set(), targets);
  return targets.map((target) => ({
    label: target,
    description: "CMake Build Target",
    run: () => vscode.commands.executeCommand("cmake.buildWithTarget", target)
  }));
}
function normalizeExecutableTargets(value) {
  if (!Array.isArray(value)) {
    return [];
  }
  return value.filter((target) => typeof target === "object" && target !== null);
}
async function getCMakeRunTargetPicks() {
  try {
    const executableTargets = normalizeExecutableTargets(await vscode.commands.executeCommand("cmake.executableTargets"));
    return executableTargets.filter((target) => !!target.name).map((target) => ({
      label: target.name,
      description: "CMake Run Target",
      detail: target.path,
      run: async () => {
        await vscode.commands.executeCommand("cmake.selectLaunchTarget", target.name);
        await vscode.commands.executeCommand("cmake.launchTarget");
      }
    }));
  } catch {
    return [];
  }
}
async function getCMakeDebugTargetPicks() {
  try {
    const executableTargets = normalizeExecutableTargets(await vscode.commands.executeCommand("cmake.executableTargets"));
    return executableTargets.filter((target) => !!target.name).map((target) => ({
      label: target.name,
      description: "CMake Debug Target",
      detail: target.path,
      run: async () => {
        await vscode.commands.executeCommand("cmake.selectLaunchTarget", target.name);
        await vscode.commands.executeCommand("cmake.debugTarget");
      }
    }));
  } catch {
    return [];
  }
}
function sortPicks(picks) {
  return [...picks].sort((left, right) => {
    const descriptionCompare = (left.description ?? "").localeCompare(right.description ?? "");
    if (descriptionCompare !== 0) {
      return descriptionCompare;
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
async function getMenuPicks() {
  const buildTargets = await getCMakeBuildTargetPicks();
  const runTargets = await getCMakeRunTargetPicks();
  const debugTargets = await getCMakeDebugTargetPicks();
  return [
    {
      label: "Build Target",
      description: "Menu",
      run: () => showSubmenu("Build Target", "Build a CMake target", buildTargets)
    },
    {
      label: "Run Target",
      description: "Menu",
      run: () => showSubmenu("Run Target", "Run a CMake target", runTargets)
    },
    {
      label: "Debug Target",
      description: "Menu",
      run: () => showSubmenu("Debug Target", "Debug a CMake target", debugTargets)
    }
  ];
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
  const taskPicks = tasks2.filter((task) => allowedLabels.has(task.name) && isMainWorkspaceTask(task, workspaceFolder)).map((task) => ({
    label: stripPlatformTags(task.name),
    description: "Task",
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
  const configPicks = configurations.filter((config) => !!config.name).map((config) => ({
    label: stripPlatformTags(config.name),
    description: "Launch",
    run: () => vscode.debug.startDebugging(workspaceFolder, config.name)
  }));
  const compoundPicks = compounds.filter((compound) => !!compound.name).map((compound) => ({
    label: stripPlatformTags(compound.name),
    description: "Launch Compound",
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
async function showToolsPicker() {
  const directPicks = sortPicks([...await getTaskPicks(), ...getLaunchPicks()]);
  const picks = [...directPicks, ...await getMenuPicks()];
  if (picks.length === 0) {
    void vscode.window.showInformationMessage("No FOnline tasks or launch configurations found.");
    return;
  }
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
