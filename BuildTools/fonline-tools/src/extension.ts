import * as vscode from 'vscode';

const COMMAND_ID = 'fonline-tools.showTools';
const STATUS_BAR_TEXT = '$(tools) FOnline Tools';

type ToolPick = vscode.QuickPickItem & {
    run(): PromiseLike<unknown>;
};

type CMakeExecutableTarget = {
    name?: string;
    path?: string;
};

type CMakeBuildPreset = {
    name?: string;
    hidden?: boolean;
    inherits?: string | string[];
    targets?: string[];
};

type CMakePresetsFile = {
    buildPresets?: CMakeBuildPreset[];
};

function decodeUtf8(bytes: Uint8Array): string {
    let encoded = '';

    for (const byte of bytes) {
        encoded += `%${byte.toString(16).padStart(2, '0')}`;
    }

    return decodeURIComponent(encoded);
}

function getWorkspaceFolder(): vscode.WorkspaceFolder | undefined {
    return vscode.workspace.workspaceFolders?.[0];
}

async function readJsonFile<T>(fileUri: vscode.Uri): Promise<T | undefined> {
    try {
        const raw = await vscode.workspace.fs.readFile(fileUri);
        return JSON.parse(decodeUtf8(raw)) as T;
    }
    catch {
        return undefined;
    }
}

async function readWorkspaceTaskLabels(): Promise<Set<string>> {
    const workspaceFolder = getWorkspaceFolder();
    if (!workspaceFolder) {
        return new Set();
    }

    const tasksUri = vscode.Uri.joinPath(workspaceFolder.uri, '.vscode', 'tasks.json');
    const parsed = await readJsonFile<{ tasks?: Array<{ label?: string }> }>(tasksUri);
    if (!parsed) {
        return new Set();
    }

    return new Set((parsed.tasks ?? []).flatMap(task => task.label ? [task.label] : []));
}

function isMainWorkspaceTask(task: vscode.Task, workspaceFolder: vscode.WorkspaceFolder): boolean {
    const scope = task.scope;
    if (scope === vscode.TaskScope.Workspace) {
        return true;
    }
    return !!scope && 'uri' in scope && scope.uri.toString() === workspaceFolder.uri.toString();
}

function normalizeBuildPresetInheritance(inherits: string | string[] | undefined): string[] {
    if (!inherits) {
        return [];
    }

    return Array.isArray(inherits) ? inherits : [inherits];
}

function getNodePlatform(): string {
    const nodeGlobal = globalThis as typeof globalThis & { process?: { platform?: string } };
    return nodeGlobal.process?.platform ?? '';
}

function getHostBuildPresetName(): string {
    return getNodePlatform() === 'win32' ? 'base-msvc' : 'base-ninja';
}

function collectBuildTargets(buildPresets: Map<string, CMakeBuildPreset>, presetName: string, visited: Set<string>, result: string[]): void {
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

async function getCMakeBuildTargetPicks(): Promise<ToolPick[]> {
    const workspaceFolder = getWorkspaceFolder();
    if (!workspaceFolder) {
        return [];
    }

    const presetsUri = vscode.Uri.joinPath(workspaceFolder.uri, 'CMakePresets.json');
    const presets = await readJsonFile<CMakePresetsFile>(presetsUri);
    if (!presets?.buildPresets?.length) {
        return [];
    }

    const buildPresets = new Map(
        presets.buildPresets
            .filter(preset => !!preset.name)
            .map(preset => [preset.name!, preset] satisfies [string, CMakeBuildPreset]),
    );

    const targets: string[] = [];
    collectBuildTargets(buildPresets, getHostBuildPresetName(), new Set(), targets);

    return targets.map(target => ({
        label: target,
        description: 'CMake Build Target',
        run: () => vscode.commands.executeCommand('cmake.buildWithTarget', target),
    }));
}

function normalizeExecutableTargets(value: unknown): CMakeExecutableTarget[] {
    if (!Array.isArray(value)) {
        return [];
    }

    return value.filter(target => typeof target === 'object' && target !== null) as CMakeExecutableTarget[];
}

async function getCMakeRunTargetPicks(): Promise<ToolPick[]> {
    try {
        const executableTargets = normalizeExecutableTargets(await vscode.commands.executeCommand('cmake.executableTargets'));

        return executableTargets
            .filter(target => !!target.name)
            .map(target => ({
                label: target.name!,
                description: 'CMake Run Target',
                detail: target.path,
                run: async () => {
                    await vscode.commands.executeCommand('cmake.selectLaunchTarget', target.name);
                    await vscode.commands.executeCommand('cmake.launchTarget');
                },
            } satisfies ToolPick));
    }
    catch {
        return [];
    }
}

async function getCMakeDebugTargetPicks(): Promise<ToolPick[]> {
    try {
        const executableTargets = normalizeExecutableTargets(await vscode.commands.executeCommand('cmake.executableTargets'));

        return executableTargets
            .filter(target => !!target.name)
            .map(target => ({
                label: target.name!,
                description: 'CMake Debug Target',
                detail: target.path,
                run: async () => {
                    await vscode.commands.executeCommand('cmake.selectLaunchTarget', target.name);
                    await vscode.commands.executeCommand('cmake.debugTarget');
                },
            } satisfies ToolPick));
    }
    catch {
        return [];
    }
}

function sortPicks(picks: ToolPick[]): ToolPick[] {
    return [...picks].sort((left, right) => {
        const descriptionCompare = (left.description ?? '').localeCompare(right.description ?? '');
        if (descriptionCompare !== 0) {
            return descriptionCompare;
        }
        return left.label.localeCompare(right.label);
    });
}

async function showSubmenu(title: string, placeHolder: string, picks: ToolPick[]): Promise<void> {
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

async function getMenuPicks(): Promise<ToolPick[]> {
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

async function getTaskPicks(): Promise<ToolPick[]> {
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

function getLaunchPicks(): ToolPick[] {
    const workspaceFolder = getWorkspaceFolder();
    const launchConfig = vscode.workspace.getConfiguration('launch', workspaceFolder?.uri);
    const configurations = launchConfig.get<Array<{ name?: string }>>('configurations') ?? [];
    const compounds = launchConfig.get<Array<{ name?: string }>>('compounds') ?? [];

    const configPicks = configurations
        .filter(config => !!config.name)
        .map(config => ({
            label: config.name!,
            description: 'Launch',
            run: () => vscode.debug.startDebugging(workspaceFolder, config.name!),
        } satisfies ToolPick));

    const compoundPicks = compounds
        .filter(compound => !!compound.name)
        .map(compound => ({
            label: compound.name!,
            description: 'Launch Compound',
            run: () => vscode.debug.startDebugging(workspaceFolder, compound.name!),
        } satisfies ToolPick));

    return [...configPicks, ...compoundPicks];
}

async function showToolsPicker(): Promise<void> {
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

export function activate(context: vscode.ExtensionContext): void {
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

export function deactivate(): void {
    // Nothing to dispose manually.
}
