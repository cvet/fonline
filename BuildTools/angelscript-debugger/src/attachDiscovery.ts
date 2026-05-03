export interface AttachArguments {
	processId?: string;
	endpoint?: string;
	discoveryPort?: number;
	discoveryTimeoutMs?: number;
}

export interface AttachTargetInfo {
	processId: string;
	endpoint: string;
	protocolVersion?: number;
	targetName?: string;
}

interface DiscoveryMessage {
	type?: string;
	processId?: string;
	endpoint?: string;
	protocolVersion?: number;
	targetName?: string;
}

const DISCOVERY_PROBE = 'fos-debug-discover-v1';
const DEFAULT_DISCOVERY_PORT = 43001;
const DEFAULT_DISCOVERY_TIMEOUT_MS = 800;

function getNodeDgramModule(): any {
	const localRequire = typeof require === 'function' ? require : undefined;
	if (localRequire) {
		try {
			return localRequire('dgram');
		}
		catch {
		}
	}

	const globalRequire = (globalThis as any)?.require;
	if (typeof globalRequire === 'function') {
		try {
			return globalRequire('dgram');
		}
		catch {
		}
	}

	const moduleRequire = (globalThis as any)?.module?.require;
	if (typeof moduleRequire === 'function') {
		try {
			return moduleRequire('dgram');
		}
		catch {
		}
	}

	try {
		const dynamicRequire = new Function('return require')();
		return dynamicRequire('dgram');
	}
	catch {
		return undefined;
	}
}

export async function discoverTargets(port: number, timeoutMs: number): Promise<AttachTargetInfo[]> {
	const dgram = getNodeDgramModule();
	if (!dgram) {
		throw new Error('Attach discovery requires Node.js dgram runtime. Use attach.endpoint (e.g. tcp://127.0.0.1:43042) or run the extension in desktop extension host.');
	}

	return await new Promise((resolve, reject) => {
		const socket = dgram.createSocket('udp4');
		const discovered = new Map<string, AttachTargetInfo>();
		let settled = false;

		const finish = (targets?: AttachTargetInfo[], error?: Error) => {
			if (settled) {
				return;
			}
			settled = true;
			try {
				socket.close();
			}
			catch {
			}

			if (error) {
				reject(error);
			}
			else {
				resolve(targets ?? []);
			}
		};

		socket.on('error', (error: Error) => finish(undefined, error));

		socket.on('message', (msg: Buffer) => {
			let parsed: DiscoveryMessage;
			try {
				parsed = JSON.parse(msg.toString('utf8')) as DiscoveryMessage;
			}
			catch {
				return;
			}

			if (parsed.type !== 'discovery' || !parsed.processId || !parsed.endpoint) {
				return;
			}

			discovered.set(parsed.endpoint, {
				processId: parsed.processId,
				endpoint: parsed.endpoint,
				protocolVersion: parsed.protocolVersion,
				targetName: parsed.targetName,
			});
		});

		socket.bind(0, () => {
			try {
				socket.setBroadcast(true);
				socket.send(Buffer.from(DISCOVERY_PROBE, 'utf8'), port, '255.255.255.255');
			}
			catch (error) {
				finish(undefined, error as Error);
				return;
			}

			setTimeout(() => finish([...discovered.values()]), timeoutMs);
		});
	});
}

export async function resolveAttachTarget(args: AttachArguments, _readTextFile: (path: string) => Promise<string>): Promise<AttachTargetInfo> {
	if (args.endpoint) {
		return {
			processId: args.processId ?? 'unknown',
			endpoint: args.endpoint
		};
	}

	const discoveryPort = typeof args.discoveryPort === 'number' && args.discoveryPort > 0 ? args.discoveryPort : DEFAULT_DISCOVERY_PORT;
	const discoveryTimeoutMs = typeof args.discoveryTimeoutMs === 'number' && args.discoveryTimeoutMs > 0 ? args.discoveryTimeoutMs : DEFAULT_DISCOVERY_TIMEOUT_MS;

	const targets = await discoverTargets(discoveryPort, discoveryTimeoutMs);
	if (targets.length === 0) {
		throw new Error(`No debugger targets discovered on UDP port ${discoveryPort}.`);
	}

	const processId = normalizeProcessId(args.processId);
	if (processId) {
		const matched = targets.find(t => t.processId === processId);
		if (!matched) {
			throw new Error(`No discovered debugger target with processId '${processId}'.`);
		}
		return matched;
	}

	return targets[0];
}

function normalizeProcessId(value?: string): string | undefined {
	if (!value) {
		return undefined;
	}

	const trimmed = value.trim();
	return trimmed.length > 0 ? trimmed : undefined;
}
