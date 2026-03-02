/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
/*
 * activateFosDebug.ts contains the shared extension code that can be executed both in node.js and the browser.
 */

'use strict';

import * as vscode from 'vscode';
import { WorkspaceFolder, DebugConfiguration, ProviderResult, CancellationToken } from 'vscode';
import { FileAccessor } from './fosRuntime';
import { discoverTargets, AttachTargetInfo } from './attachDiscovery';

export function activateFosDebug(context: vscode.ExtensionContext, factory?: vscode.DebugAdapterDescriptorFactory) {

	context.subscriptions.push(
		vscode.commands.registerCommand('extension.fos-debug.toggleFormatting', (variable) => {
			const ds = vscode.debug.activeDebugSession;
			if (ds) {
				ds.customRequest('toggleFormatting');
			}
		})
	);

	context.subscriptions.push(vscode.commands.registerCommand('extension.fos-debug.getProgramName', config => {
		return vscode.window.showInputBox({
			placeHolder: "Please enter the name of a markdown file in the workspace folder",
			value: "readme.md"
		});
	}));

	// register a configuration provider for 'fos' debug type
	const provider = new FosConfigurationProvider();
	context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider('fos', provider));

	// register a dynamic configuration provider for 'fos' debug type
	context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider('fos', {
		provideDebugConfigurations(folder: WorkspaceFolder | undefined): ProviderResult<DebugConfiguration[]> {
			return [
				{
					name: "Dynamic Launch",
					request: "launch",
					type: "fos",
					program: "${file}"
				},
				{
					name: "Another Dynamic Launch",
					request: "launch",
					type: "fos",
					program: "${file}"
				},
				{
					name: "FOS Launch",
					request: "launch",
					type: "fos",
					program: "${file}"
				}
			];
		}
	}, vscode.DebugConfigurationProviderTriggerKind.Dynamic));

	if (factory) {
		context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory('fos', factory));
		if ('dispose' in factory) {
			//context.subscriptions.push(factory);
		}
	}

	// override VS Code's default implementation of the debug hover
	// here we match variables that are words starting with an '$'
	context.subscriptions.push(vscode.languages.registerEvaluatableExpressionProvider('fos', {
		provideEvaluatableExpression(document: vscode.TextDocument, position: vscode.Position): vscode.ProviderResult<vscode.EvaluatableExpression> {

			const VARIABLE_REGEXP = /\$[a-z][a-z0-9]*/ig;
			const line = document.lineAt(position.line).text;

			let m: RegExpExecArray | null;
			while (m = VARIABLE_REGEXP.exec(line)) {
				const varRange = new vscode.Range(position.line, m.index, position.line, m.index + m[0].length);

				if (varRange.contains(position)) {
					return new vscode.EvaluatableExpression(varRange);
				}
			}
			return undefined;
		}
	}));

	// override VS Code's default implementation of the "inline values" feature"
	context.subscriptions.push(vscode.languages.registerInlineValuesProvider('fos', {

		provideInlineValues(document: vscode.TextDocument, viewport: vscode.Range, context: vscode.InlineValueContext): vscode.ProviderResult<vscode.InlineValue[]> {

			const allValues: vscode.InlineValue[] = [];

			for (let l = viewport.start.line; l <= context.stoppedLocation.end.line; l++) {
				const line = document.lineAt(l);
				var regExp = /\$([a-z][a-z0-9]*)/ig;	// variables are words starting with '$'
				do {
					var m = regExp.exec(line.text);
					if (m) {
						const varName = m[1];
						const varRange = new vscode.Range(l, m.index, l, m.index + varName.length);

						// some literal text
						//allValues.push(new vscode.InlineValueText(varRange, `${varName}: ${viewport.start.line}`));

						// value found via variable lookup
						allValues.push(new vscode.InlineValueVariableLookup(varRange, varName, false));

						// value determined via expression evaluation
						//allValues.push(new vscode.InlineValueEvaluatableExpression(varRange, varName));
					}
				} while (m);
			}

			return allValues;
		}
	}));
}

class FosConfigurationProvider implements vscode.DebugConfigurationProvider {

	/**
	 * Massage a debug configuration just before a debug session is being launched,
	 * e.g. add all missing attributes to the debug configuration.
	 */
	async resolveDebugConfiguration(folder: WorkspaceFolder | undefined, config: DebugConfiguration, token?: CancellationToken): Promise<DebugConfiguration | undefined> {

		// if launch.json is missing or empty
		if (!config.type && !config.request && !config.name) {
			const editor = vscode.window.activeTextEditor;
			if (editor && editor.document.languageId === 'fos') {
				config.type = 'fos';
				config.name = 'Launch';
				config.request = 'launch';
				config.program = '${file}';
				config.stopOnEntry = true;
			}
		}

		if (config.request === 'launch' && !config.program) {
			return vscode.window.showInformationMessage("Cannot find a program to debug").then(_ => {
				return undefined;	// abort launch
			});
		}

		if (config.request === 'attach' && !config.endpoint) {
			const discoveryPort = typeof config.discoveryPort === 'number' && config.discoveryPort > 0 ? config.discoveryPort : 43001;
			const discoveryTimeoutMs = typeof config.discoveryTimeoutMs === 'number' && config.discoveryTimeoutMs > 0 ? config.discoveryTimeoutMs : 800;

			let targets: AttachTargetInfo[];

			try {
				targets = await discoverTargets(discoveryPort, discoveryTimeoutMs);
			}
			catch (error) {
				const message = (error as Error).message;
				const endpoint = await vscode.window.showInputBox({
					title: 'Choose endpoint to attach:',
					prompt: `Attach discovery unavailable (${message}). Enter endpoint manually (e.g. tcp://127.0.0.1:43042).`,
					placeHolder: 'tcp://127.0.0.1:43042',
					ignoreFocusOut: true,
				});

				if (!endpoint || endpoint.trim().length === 0) {
					return undefined;
				}

				config.endpoint = endpoint.trim();
				return config;
			}

			if (targets.length === 0) {
				await vscode.window.showWarningMessage(`No debugger targets discovered on UDP port ${discoveryPort}.`);
				return undefined;
			}

			targets.sort((left, right) => left.endpoint.localeCompare(right.endpoint));

			type AttachPickItem = {
				label: string;
				description?: string;
				detail?: string;
				target?: AttachTargetInfo;
				cancel?: boolean;
			};

			const pickItems: AttachPickItem[] = targets.map(target => {
				const role = target.targetName ? target.targetName.charAt(0).toUpperCase() + target.targetName.slice(1) : 'Runtime';
				return {
					label: `${role}: ${target.endpoint}`,
					target,
				};
			});

			pickItems.push({
				label: 'Cancel',
				cancel: true,
			});

			const picked = await vscode.window.showQuickPick(pickItems, {
				title: 'Choose endpoint to attach:',
				placeHolder: 'Choose endpoint to attach:',
				ignoreFocusOut: true,
			});

			if (!picked || picked.cancel || !picked.target) {
				return undefined;
			}

			config.endpoint = picked.target.endpoint;
			config.processId = picked.target.processId;
		}

		return config;
	}
}

export const workspaceFileAccessor: FileAccessor = {
	isWindows: typeof process !== 'undefined' && process.platform === 'win32',
	async readFile(path: string): Promise<Uint8Array> {
		let uri: vscode.Uri;
		try {
			uri = pathToUri(path);
		} catch (e) {
			return new TextEncoder().encode(`cannot read '${path}'`);
		}

		return await vscode.workspace.fs.readFile(uri);
	},
	async writeFile(path: string, contents: Uint8Array) {
		await vscode.workspace.fs.writeFile(pathToUri(path), contents);
	}
};

function pathToUri(path: string) {
	try {
		return vscode.Uri.file(path);
	} catch (e) {
		return vscode.Uri.parse(path);
	}
}

