import * as vscode from 'vscode';

const COMMAND_ID = 'fonline-tools.showTools';
const STATUS_BAR_TEXT = '$(tools) FOnline Tools';

type ToolPick = vscode.QuickPickItem & {
    run(): PromiseLike<unknown>;
    sortPriority?: number;
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

const TASK_GROUP_SEPARATOR = '::';
const PLATFORM_TAG_PATTERN = /\[(windows|linux|macos|win32|win64|osx|darwin)\]/gi;
const HIDDEN_TAG_PATTERN = /\[hidden\]/gi;
const SORT_TAG_PATTERN = /\[sort:(\d+)\]/gi;
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
    return text.replace(PLATFORM_TAG_PATTERN, '').replace(HIDDEN_TAG_PATTERN, '').replace(SORT_TAG_PATTERN, '').replace(/\s{2,}/g, ' ').trim();
}

function hasHiddenTag(text: string): boolean {
    return /\[hidden\]/i.test(text);
}

function extractSortPriority(text: string): number | undefined {
    const match = /\[sort:(\d+)\]/i.exec(text);
    return match ? parseInt(match[1], 10) : undefined;
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

function getNodePlatform(): string {
    const nodeGlobal = globalThis as typeof globalThis & { process?: { platform?: string } };
    return nodeGlobal.process?.platform ?? '';
}

function getHostBuildPresetName(): string {
    return getNodePlatform() === 'win32' ? 'base-msvc' : 'base-ninja';
}

const GROUP_DESCRIPTIONS = new Set(['Task Group', 'Launch Group']);

function sortPicks(picks: ToolPick[]): ToolPick[] {
    return [...picks].sort((left, right) => {
        const leftIsGroup = GROUP_DESCRIPTIONS.has(left.description ?? '') ? 1 : 0;
        const rightIsGroup = GROUP_DESCRIPTIONS.has(right.description ?? '') ? 1 : 0;
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
        .filter(task => allowedLabels.has(task.name) && isMainWorkspaceTask(task, workspaceFolder) && !hasHiddenTag(task.name))
        .map(task => ({
            label: stripPlatformTags(task.name),
            description: 'Task',
            sortPriority: extractSortPriority(task.name),
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
        .filter(config => !!config.name && !hasHiddenTag(config.name))
        .map(config => ({
            label: stripPlatformTags(config.name!),
            description: 'Launch',
            sortPriority: extractSortPriority(config.name!),
            run: () => vscode.debug.startDebugging(workspaceFolder, config.name!),
        } satisfies ToolPick));

    const compoundPicks = compounds
        .filter(compound => !!compound.name && !hasHiddenTag(compound.name))
        .map(compound => ({
            label: stripPlatformTags(compound.name!),
            description: 'Launch Compound',
            sortPriority: extractSortPriority(compound.name!),
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

const MAX_SORT_HOTKEYS = 9;

let cachedSortedPicks: ToolPick[] = [];

async function refreshSortedPicks(): Promise<ToolPick[]> {
    const allPicks = [...await getTaskPicks(), ...getLaunchPicks()];
    cachedSortedPicks = allPicks.filter(p => p.sortPriority !== undefined).sort((a, b) => (a.sortPriority ?? Infinity) - (b.sortPriority ?? Infinity));
    return cachedSortedPicks;
}

async function showToolsPicker(): Promise<void> {
    const picks = sortPicks([...await getTaskPicks(), ...getLaunchPicks()]);

    if (picks.length === 0) {
        void vscode.window.showInformationMessage('No FOnline tasks or launch configurations found.');
        return;
    }

    cachedSortedPicks = picks.filter(p => p.sortPriority !== undefined).sort((a, b) => (a.sortPriority ?? Infinity) - (b.sortPriority ?? Infinity));

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

    for (let i = 1; i <= MAX_SORT_HOTKEYS; i++) {
        const command = vscode.commands.registerCommand(`fonline-tools.runSorted${i}`, async () => {
            if (cachedSortedPicks.length === 0) {
                await refreshSortedPicks();
            }

            const pick = cachedSortedPicks.find(p => p.sortPriority === i);
            if (pick) {
                await pick.run();
            }
            else {
                void vscode.window.showWarningMessage(`No FOnline item with [sort:${i}] found.`);
            }
        });
        context.subscriptions.push(command);
    }

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
