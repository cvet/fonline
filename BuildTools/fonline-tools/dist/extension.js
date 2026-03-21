"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.deactivate = exports.activate = void 0;
const vscode = require("vscode");
const COMMAND_ID = 'fonline-tools.showTools';
const STATUS_BAR_TEXT = '$(tools) FOnline Tools';
function getWorkspaceFolder() {
    var _a;
    return (_a = vscode.workspace.workspaceFolders) === null || _a === void 0 ? void 0 : _a[0];
}
async function readJsonFile(fileUri) {
    try {
        const raw = await vscode.workspace.fs.readFile(fileUri);
        return JSON.parse(decodeUtf8(raw));
    }
    catch {
        return undefined;
    }
}
function decodeUtf8(bytes) {
    let encoded = '';
    for (const byte of bytes) {
        encoded += `%${byte.toString(16).padStart(2, '0')}`;
    }
    return decodeURIComponent(encoded);
}
async function readWorkspaceTaskLabels() {
    const workspaceFolder = getWorkspaceFolder();
    if (!workspaceFolder) {
        return new Set();
    }
    const tasksUri = vscode.Uri.joinPath(workspaceFolder.uri, '.vscode', 'tasks.json');
    const parsed = await readJsonFile(tasksUri);
    if (!parsed) {
        return new Set();
    }
    return new Set(((parsed.tasks !== null && parsed.tasks !== void 0 ? parsed.tasks : []).flatMap(task => task.label ? [task.label] : [])));
}
function isMainWorkspaceTask(task, workspaceFolder) {
    const scope = task.scope;
    if (scope === vscode.TaskScope.Workspace) {
        return true;
    }
    return !!scope && 'uri' in scope && scope.uri.toString() === workspaceFolder.uri.toString();
}
function normalizeBuildPresetInheritance(inherits) {
    if (!inherits) {
        return [];
    }
    return Array.isArray(inherits) ? inherits : [inherits];
}
function getNodePlatform() {
    var _a, _b;
    return (_b = (_a = globalThis.process) === null || _a === void 0 ? void 0 : _a.platform) !== null && _b !== void 0 ? _b : '';
}
function getHostBuildPresetName() {
    return getNodePlatform() === 'win32' ? 'base-msvc' : 'base-ninja';
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
    for (const target of preset.targets !== null && preset.targets !== void 0 ? preset.targets : []) {
        if (!result.includes(target)) {
            result.push(target);
        }
    }
}
async function getCMakeBuildTargetPicks() {
    var _a;
    const workspaceFolder = getWorkspaceFolder();
    if (!workspaceFolder) {
        return [];
    }
    const presetsUri = vscode.Uri.joinPath(workspaceFolder.uri, 'CMakePresets.json');
    const presets = await readJsonFile(presetsUri);
    if (!((_a = presets === null || presets === void 0 ? void 0 : presets.buildPresets) === null || _a === void 0 ? void 0 : _a.length)) {
        return [];
    }
    const buildPresets = new Map(presets.buildPresets
        .filter(preset => !!preset.name)
        .map(preset => [preset.name, preset]));
    const targets = [];
    collectBuildTargets(buildPresets, getHostBuildPresetName(), new Set(), targets);
    return targets.map(target => ({
        label: target,
        description: 'CMake Build Target',
        run: () => vscode.commands.executeCommand('cmake.buildWithTarget', target),
    }));
}
function normalizeExecutableTargets(value) {
    if (!Array.isArray(value)) {
        return [];
    }
    return value.filter(target => typeof target === 'object' && target !== null);
}
async function getCMakeRunTargetPicks() {
    try {
        const executableTargets = normalizeExecutableTargets(await vscode.commands.executeCommand('cmake.executableTargets'));
        return executableTargets
            .filter(target => !!target.name)
            .map(target => ({
            label: target.name,
            description: 'CMake Run Target',
            detail: target.path,
            run: async () => {
                await vscode.commands.executeCommand('cmake.selectLaunchTarget', target.name);
                await vscode.commands.executeCommand('cmake.launchTarget');
            },
        }));
    }
    catch {
        return [];
    }
}
async function getCMakeDebugTargetPicks() {
    try {
        const executableTargets = normalizeExecutableTargets(await vscode.commands.executeCommand('cmake.executableTargets'));
        return executableTargets
            .filter(target => !!target.name)
            .map(target => ({
            label: target.name,
            description: 'CMake Debug Target',
            detail: target.path,
            run: async () => {
                await vscode.commands.executeCommand('cmake.selectLaunchTarget', target.name);
                await vscode.commands.executeCommand('cmake.debugTarget');
            },
        }));
    }
    catch {
        return [];
    }
}
function sortPicks(picks) {
    return [...picks].sort((left, right) => {
        var _a, _b;
        const descriptionCompare = ((_a = left.description) !== null && _a !== void 0 ? _a : '').localeCompare((_b = right.description) !== null && _b !== void 0 ? _b : '');
        if (descriptionCompare !== 0) {
            return descriptionCompare;
        }
        return left.label.localeCompare(right.label);
    });
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
        matchOnDetail: true,
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
            label: 'Build Target',
            description: 'Menu',
            run: () => showSubmenu('Build Target', 'Build a CMake target', buildTargets),
        },
        {
            label: 'Run Target',
            description: 'Menu',
            run: () => showSubmenu('Run Target', 'Run a CMake target', runTargets),
        },
        {
            label: 'Debug Target',
            description: 'Menu',
            run: () => showSubmenu('Debug Target', 'Debug a CMake target', debugTargets),
        },
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
    const tasks = await vscode.tasks.fetchTasks();
    return tasks
        .filter(task => allowedLabels.has(task.name) && isMainWorkspaceTask(task, workspaceFolder))
        .map(task => ({
        label: task.name,
        description: 'Task',
        run: () => vscode.tasks.executeTask(task),
    }));
}
function getLaunchPicks() {
    var _a, _b;
    const workspaceFolder = getWorkspaceFolder();
    const launchConfig = vscode.workspace.getConfiguration('launch', workspaceFolder === null || workspaceFolder === void 0 ? void 0 : workspaceFolder.uri);
    const configurations = (_a = launchConfig.get('configurations')) !== null && _a !== void 0 ? _a : [];
    const compounds = (_b = launchConfig.get('compounds')) !== null && _b !== void 0 ? _b : [];
    const configPicks = configurations
        .filter(config => !!config.name)
        .map(config => ({
        label: config.name,
        description: 'Launch',
        run: () => vscode.debug.startDebugging(workspaceFolder, config.name),
    }));
    const compoundPicks = compounds
        .filter(compound => !!compound.name)
        .map(compound => ({
        label: compound.name,
        description: 'Launch Compound',
        run: () => vscode.debug.startDebugging(workspaceFolder, compound.name),
    }));
    return [...configPicks, ...compoundPicks];
}
async function showToolsPicker() {
    const directPicks = sortPicks([...await getTaskPicks(), ...getLaunchPicks()]);
    const picks = [...directPicks, ...await getMenuPicks()];
    if (picks.length === 0) {
        void vscode.window.showInformationMessage('No FOnline tasks or launch configurations found.');
        return;
    }
    const selected = await vscode.window.showQuickPick(picks, {
        title: 'FOnline Tools',
        placeHolder: 'Run a task or launch configuration',
        matchOnDescription: true,
        matchOnDetail: true,
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
    statusBarItem.name = 'FOnline Tools';
    statusBarItem.text = STATUS_BAR_TEXT;
    statusBarItem.tooltip = 'Show FOnline tasks and launch configurations';
    statusBarItem.command = COMMAND_ID;
    statusBarItem.show();
    context.subscriptions.push(showToolsCommand, statusBarItem);
}
exports.activate = activate;
function deactivate() {
}
exports.deactivate = deactivate;
