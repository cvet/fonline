import * as vscode from 'vscode';

const COMMAND_ID = 'fonline-tools.showTools';
const STATUS_BAR_TEXT = '$(tools) FOnline Tools';

type ToolPick = vscode.QuickPickItem & {
    run(): PromiseLike<unknown>;
};

type GroupedTaskName = {
    groupLabel: string;
    itemLabel: string;
};

type HostPlatform = 'windows' | 'linux' | 'macos' | 'unknown';

type WorkspaceTaskDefinition = {
    label?: string;
    windows?: unknown;
    linux?: unknown;
    osx?: unknown;
};

type WorkspaceLaunchDefinition = {
    name?: string;
    type?: string;
    windows?: unknown;
    linux?: unknown;
    osx?: unknown;
};

type WorkspaceLaunchCompound = {
    name?: string;
    configurations?: string[];
    windows?: unknown;
    linux?: unknown;
    osx?: unknown;
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

const TASK_GROUP_SEPARATOR = '::';
const PLATFORM_TAG_PATTERN = /\[(windows|linux|macos|win32|win64|osx|darwin)\]/gi;
const PLATFORM_DEBUG_TYPES: Record<string, HostPlatform[]> = {
    cppvsdbg: ['windows'],
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

function getHostPlatform(): HostPlatform {
    const platform = getNodePlatform();
    if (platform === 'win32') {
        return 'windows';
    }
    if (platform === 'linux') {
        return 'linux';
    }
    if (platform === 'darwin') {
        return 'macos';
    }
    return 'unknown';
}

function getExplicitPlatforms(value: {windows?: unknown; linux?: unknown; osx?: unknown}): HostPlatform[] {
    const platforms: HostPlatform[] = [];
    if (value.windows !== undefined) {
        platforms.push('windows');
    }
    if (value.linux !== undefined) {
        platforms.push('linux');
    }
    if (value.osx !== undefined) {
        platforms.push('macos');
    }
    return platforms;
}

function normalizeTaggedPlatform(rawPlatform: string): HostPlatform | undefined {
    const platform = rawPlatform.trim().toLowerCase();
    if (platform === 'windows' || platform === 'win32' || platform === 'win64') {
        return 'windows';
    }
    if (platform === 'linux') {
        return 'linux';
    }
    if (platform === 'macos' || platform === 'osx' || platform === 'darwin') {
        return 'macos';
    }
    return undefined;
}

function extractPlatformTags(text: string): HostPlatform[] {
    const result = new Set<HostPlatform>();

    for (const match of text.matchAll(PLATFORM_TAG_PATTERN)) {
        const platform = match[1] ? normalizeTaggedPlatform(match[1]) : undefined;
        if (platform) {
            result.add(platform);
        }
    }

    return Array.from(result);
}

function stripPlatformTags(text: string): string {
    return text.replace(PLATFORM_TAG_PATTERN, '').replace(/\s{2,}/g, ' ').trim();
}

function extractPlatformQualifiers(text: string): HostPlatform[] {
    const result = new Set<HostPlatform>();
    const matches = text.matchAll(/\(([^)]+)\)/g);

    for (const match of matches) {
        const rawQualifier = match[1]?.trim().toLowerCase();
        if (!rawQualifier) {
            continue;
        }

        const firstWord = rawQualifier.split(/\s+/)[0];
        if (firstWord === 'windows' || firstWord === 'win32' || firstWord === 'win64') {
            result.add('windows');
        }
        else if (firstWord === 'linux') {
            result.add('linux');
        }
        else if (firstWord === 'macos' || firstWord === 'osx' || firstWord === 'darwin') {
            result.add('macos');
        }
    }

    return Array.from(result);
}

function matchesCurrentPlatform(taggedPlatforms: HostPlatform[], explicitPlatforms: HostPlatform[], qualifiedPlatforms: HostPlatform[]): boolean {
    const hostPlatform = getHostPlatform();

    if (taggedPlatforms.length !== 0 && hostPlatform !== 'unknown') {
        return taggedPlatforms.includes(hostPlatform);
    }

    if (qualifiedPlatforms.length !== 0 && hostPlatform !== 'unknown') {
        return qualifiedPlatforms.includes(hostPlatform);
    }

    if (explicitPlatforms.length !== 0 && hostPlatform !== 'unknown') {
        return explicitPlatforms.includes(hostPlatform);
    }

    return true;
}

function isTaskDefinitionSupported(definition: WorkspaceTaskDefinition): boolean {
    return matchesCurrentPlatform(
        extractPlatformTags(definition.label ?? ''),
        getExplicitPlatforms(definition),
        extractPlatformQualifiers(definition.label ?? ''),
    );
}

function isLaunchDefinitionSupported(definition: WorkspaceLaunchDefinition): boolean {
    const debugTypePlatforms = PLATFORM_DEBUG_TYPES[definition.type ?? ''] ?? [];
    if (!matchesCurrentPlatform(
        extractPlatformTags(definition.name ?? ''),
        getExplicitPlatforms(definition),
        extractPlatformQualifiers(definition.name ?? ''),
    )) {
        return false;
    }

    return matchesCurrentPlatform([], debugTypePlatforms, []);
}

function isLaunchCompoundSupported(compound: WorkspaceLaunchCompound, allowedConfigNames: Set<string>): boolean {
    if (!matchesCurrentPlatform(
        extractPlatformTags(compound.name ?? ''),
        getExplicitPlatforms(compound),
        extractPlatformQualifiers(compound.name ?? ''),
    )) {
        return false;
    }

    const configurations = compound.configurations ?? [];
    return configurations.length !== 0 && configurations.every(configName => allowedConfigNames.has(configName));
}

async function readWorkspaceTaskLabels(): Promise<Set<string>> {
    const workspaceFolder = getWorkspaceFolder();
    if (!workspaceFolder) {
        return new Set();
    }

    const tasksUri = vscode.Uri.joinPath(workspaceFolder.uri, '.vscode', 'tasks.json');
    const parsed = await readJsonFile<{ tasks?: WorkspaceTaskDefinition[] }>(tasksUri);
    if (!parsed) {
        return new Set();
    }

    return new Set(
        (parsed.tasks ?? [])
            .filter(isTaskDefinitionSupported)
            .flatMap(task => task.label ? [task.label] : []),
    );
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

function parseGroupedTaskName(taskName: string): GroupedTaskName | undefined {
    const separatorIndex = taskName.indexOf(TASK_GROUP_SEPARATOR);
    if (separatorIndex <= 0) {
        return undefined;
    }

    const groupLabel = stripPlatformTags(taskName.slice(0, separatorIndex).trim());
    const itemLabel = stripPlatformTags(taskName.slice(separatorIndex + TASK_GROUP_SEPARATOR.length).trim());
    if (!groupLabel || !itemLabel) {
        return undefined;
    }

    return {groupLabel, itemLabel};
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

    const taskPicks = tasks
        .filter(task => allowedLabels.has(task.name) && isMainWorkspaceTask(task, workspaceFolder))
        .map(task => ({
            label: stripPlatformTags(task.name),
            description: 'Task',
            run: () => vscode.tasks.executeTask(task),
        }));

    const groupedTaskEntries = new Map<string, ToolPick[]>();
    const directTaskPicks: ToolPick[] = [];

    for (const pick of taskPicks) {
        const groupedName = parseGroupedTaskName(pick.label);
        if (!groupedName) {
            directTaskPicks.push(pick);
            continue;
        }

        const groupPicks = groupedTaskEntries.get(groupedName.groupLabel) ?? [];
        groupPicks.push({...pick, label: groupedName.itemLabel});
        groupedTaskEntries.set(groupedName.groupLabel, groupPicks);
    }

    const groupedTaskPicks = Array.from(groupedTaskEntries.entries()).map(([groupLabel, groupPicks]) => ({
        label: `${groupLabel} ...`,
        description: 'Task Group',
        run: () => showSubmenu(groupLabel, `Run a task from ${groupLabel}`, groupPicks),
    } satisfies ToolPick));

    return [...directTaskPicks, ...groupedTaskPicks];
}

function getLaunchPicks(): ToolPick[] {
    const workspaceFolder = getWorkspaceFolder();
    const launchConfig = vscode.workspace.getConfiguration('launch', workspaceFolder?.uri);
    const configurations = (launchConfig.get<WorkspaceLaunchDefinition[]>('configurations') ?? [])
        .filter(isLaunchDefinitionSupported);
    const allowedConfigNames = new Set<string>(configurations.flatMap(config => config.name ? [config.name] : []));
    const compounds = (launchConfig.get<WorkspaceLaunchCompound[]>('compounds') ?? [])
        .filter(compound => isLaunchCompoundSupported(compound, allowedConfigNames));

    const configPicks = configurations
        .filter(config => !!config.name)
        .map(config => ({
            label: stripPlatformTags(config.name!),
            description: 'Launch',
            run: () => vscode.debug.startDebugging(workspaceFolder, config.name!),
        } satisfies ToolPick));

    const compoundPicks = compounds
        .filter(compound => !!compound.name)
        .map(compound => ({
            label: stripPlatformTags(compound.name!),
            description: 'Launch Compound',
            run: () => vscode.debug.startDebugging(workspaceFolder, compound.name!),
        } satisfies ToolPick));

    const allPicks = [...configPicks, ...compoundPicks];

    const groupedEntries = new Map<string, ToolPick[]>();
    const directPicks: ToolPick[] = [];

    for (const pick of allPicks) {
        const groupedName = parseGroupedTaskName(pick.label);
        if (!groupedName) {
            directPicks.push(pick);
            continue;
        }

        const groupPicks = groupedEntries.get(groupedName.groupLabel) ?? [];
        groupPicks.push({...pick, label: groupedName.itemLabel});
        groupedEntries.set(groupedName.groupLabel, groupPicks);
    }

    const groupedPicks = Array.from(groupedEntries.entries()).map(([groupLabel, groupPicks]) => ({
        label: `${groupLabel} ...`,
        description: 'Launch Group',
        run: () => showSubmenu(groupLabel, `Run a launch configuration from ${groupLabel}`, groupPicks),
    } satisfies ToolPick));

    return [...directPicks, ...groupedPicks];
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
