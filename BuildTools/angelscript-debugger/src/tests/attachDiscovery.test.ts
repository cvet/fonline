import assert = require('assert');
import {AttachTargetInfo, matchAttachTargets, normalizeProcessId, resolveAttachTarget} from '../attachDiscovery';

suite('Attach target selection', () => {

	const server: AttachTargetInfo = {processId: '1200:43200', endpoint: 'tcp://127.0.0.1:43200', targetName: 'server'};
	const client: AttachTargetInfo = {processId: '1300:43300', endpoint: 'tcp://127.0.0.1:43300', targetName: 'client'};
	const remoteServer: AttachTargetInfo = {processId: '1200:43200', endpoint: 'tcp://192.168.1.10:43200', targetName: 'server'};

	test('an unset or blank processId keeps every target', () => {
		assert.deepStrictEqual(matchAttachTargets([server, client], normalizeProcessId(undefined)), [server, client]);
		assert.deepStrictEqual(matchAttachTargets([server, client], normalizeProcessId('  ')), [server, client]);
	});

	test('a processId selects the target with exactly that advertised id', () => {
		assert.deepStrictEqual(matchAttachTargets([server, client], normalizeProcessId(' 1300:43300 ')), [client]);
	});

	test('a processId that is only a prefix or a bare pid selects nothing', () => {
		assert.deepStrictEqual(matchAttachTargets([server, client], normalizeProcessId('1300')), []);
	});

	test('the same id on several hosts keeps every match for the picker', () => {
		assert.deepStrictEqual(matchAttachTargets([server, client, remoteServer], normalizeProcessId('1200:43200')), [server, remoteServer]);
	});

	test('an explicit endpoint is used without discovery', async () => {
		const target = await resolveAttachTarget({endpoint: 'tcp://127.0.0.1:43042', processId: '7:43042'}, async () => '');
		assert.deepStrictEqual(target, {processId: '7:43042', endpoint: 'tcp://127.0.0.1:43042'});
	});
});
