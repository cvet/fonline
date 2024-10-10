#!/usr/bin/python3

import os
import argparse
import platform
import sys
import subprocess
import traceback

BIN_TYPES = ['Server', 'Client', 'Single', 'Mapper', 'Editor', 'Baker', 'ASCompiler', 'Tests']
BIN_NEED_BACKED_RESOURCES = ['Server', 'Client', 'Single', 'Mapper']

parser = argparse.ArgumentParser(description='FOnline packager')
parser.add_argument('-devname', dest='devname', required=True, help='Dev game name')
parser.add_argument('-buildhash', dest='buildhash', required=True, help='build hash')
parser.add_argument('-baking', dest='baking', required=True, help='baking dir')
parser.add_argument('-bininput', dest='bininput', required=True, action='append', default=[], help='binary input dir')
parser.add_argument('-defaultcfg', dest='defaultcfg', help='Game default config')
parser.add_argument('-mappercfg', dest='mappercfg', help='Mapper default config')
parser.add_argument('-content', dest='content', action='append', default=[], help='content file path')
parser.add_argument('-resource', dest='resource', action='append', default=[], help='resource file path')
args = parser.parse_args()

assert platform.system() in ['Linux', 'Darwin', 'Windows'], 'Invalid OS'

def log(*text):
	print('[Starter]', *text, flush=True)

try:
	import easygui
	hasEasyGui = True
except:
	hasEasyGui = False
	log('Please install easygui for better experience')
	log('Type in console: pip install easygui')

def error(message):
	log(message)
	if hasEasyGui:
		easygui.msgbox(message, args.devname + ' Starter')
	else:
		input('Press enter to continue')
	sys.exit(1)

def choicebox(message, choices):
	log(message)
	if hasEasyGui:
		return easygui.choicebox(message, args.devname + ' Starter', choices=choices)
	else:
		index = 1
		for choice in choices:
			log(f'{index}) {choice}' + (' (default)' if index == 1 else ''))
			index += 1
		try:
			index = int(input(': ') or 1)
		except:
			index = 0
		return choices[index - 1] if index >= 1 and index <= len(choices) else None

def checkDirHash(dir, inputType):
	buildHashPath = os.path.join(dir, inputType + '.build-hash')
	if os.path.isfile(buildHashPath):
		with open(buildHashPath, 'r', encoding='utf-8-sig') as f:
			return f.read() == args.buildhash
	return False

try:
	log('Build hash', args.buildhash)
	bakingEntry = os.path.realpath(args.baking)

	if not checkDirHash(bakingEntry, 'Resources'):
		log('Baked resources not found')
		bakingEntry = None

	binaryEntries = []
	
	for binInput in args.bininput:
		absInput = os.path.realpath(binInput)
		log('Scan for binaries', absInput)
		
		for entry in os.scandir(absInput):
			binDir = os.path.join(absInput, entry.name)
			def verifyBinaryHash():
				return checkDirHash(binDir, args.devname + '_' + entry.name.split('-')[0])
			if platform.system() == 'Windows' and '-Windows-' in entry.name and verifyBinaryHash():
				binaryEntries.append(binDir)
			elif platform.system() == 'Linux' and '-Linux-' in entry.name and verifyBinaryHash():
				binaryEntries.append(binDir)
			elif platform.system() == 'Darwin' and '-macOS-' in entry.name and verifyBinaryHash():
				binaryEntries.append(binDir)

	if not bakingEntry:
		binaryEntries = list(filter(lambda x: os.path.basename(x).split('-')[0] not in BIN_NEED_BACKED_RESOURCES, binaryEntries))
	binaryEntries = list(filter(lambda x: len(os.path.basename(x).split('-')) >= 3, binaryEntries))
	binaryEntries = list(filter(lambda x: os.path.basename(x).split('-')[0] in BIN_TYPES, binaryEntries))
	binaryEntries.sort(key=lambda x: (BIN_TYPES.index(os.path.basename(x).split('-')[0]), os.path.basename(x).split('-')[3:]))

	if not binaryEntries:
		error('Binaries not found or outdated')

	for binaryEntry in binaryEntries:
		log('Found binary entry', os.path.relpath(binaryEntry))

	configs = []

	if bakingEntry:
		for entry in os.listdir(os.path.join(bakingEntry, 'Configs')):
			if os.path.isfile(os.path.join(bakingEntry, 'Configs', entry)) and not entry.startswith('Client_') and os.path.splitext(entry)[1] == '.focfg':
				log('Found config', os.path.splitext(entry)[0])
				configs.append(os.path.splitext(entry)[0])
		if args.defaultcfg and args.defaultcfg in configs:
			del configs[configs.index(args.defaultcfg)]
			configs.insert(0, args.defaultcfg)

	choices = []
	choiceActions = []

	for binEntry in binaryEntries:
		nameParts = os.path.basename(binEntry).split('-')
		binType = nameParts[0]
		archType = nameParts[2]
		buildType = nameParts[3] if len(nameParts) > 3 else ''
		
		def action(binEntry=binEntry, binType=binType, buildType=buildType):
			config = None
			
			if binType == 'Mapper' and args.mappercfg:
				config = args.mappercfg
			elif binType in ['Server', 'Client', 'Single', 'Mapper']:
				config = choicebox('Select config to start', configs)
				if not config:
					sys.exit(0)
			elif binType == 'Baker':
				bakingType = choicebox('Select baking mode', ['Iterative', 'Force'])
				if not bakingType:
					sys.exit(0)

			exePath = os.path.join(binEntry, args.devname + '_' + binType)
			if platform.system() == 'Windows':
				exePath += '.exe'
			exePath = os.path.relpath(exePath)
			
			exeArgs = []
			if binType in ['Server', 'Client', 'Single', 'Mapper']:
				exeArgs += ['-ResourcesDir', os.path.relpath(bakingEntry)]
				exeArgs += ['-EmbeddedResources', os.path.relpath(os.path.join(bakingEntry, 'Embedded'))]
				exeArgs += ['-DataSynchronization', 'False']
			if config:
				exeArgs += ['-ExternalConfig', os.path.relpath(os.path.join(bakingEntry, 'Configs', f'{config}.focfg'))]
			if binType == 'Baker':
				exeArgs += ['-ForceBaking', 'True' if bakingType == 'Force' else 'False']
			if binType in ['Editor', 'Baker', 'ASCompiler', 'Mapper']:
				exeArgs += ['-BakeOutput', os.path.relpath(os.path.realpath(args.baking))]
				for c in args.content:
					exeArgs += ['-BakeContentEntries', f'+{c}']
				for r in args.resource:
					exeArgs += ['-BakeResourceEntries', f'+{r}']
			
			needWait = binType in ['Baker', 'ASCompiler'] or 'San_' in buildType
			
			if not needWait and (platform.system() == 'Linux' or platform.system() == 'Darwin'):
				exeArgs += ['--fork']
			
			log('Run and wait' if needWait else 'Run', exePath)
			
			proc = subprocess.Popen([exePath] + exeArgs)
			
			if needWait:
				proc.wait()
				if proc.returncode:
					error('Something went wrong\nVerify log messages')
				else:
					input('Press enter to continue')
		
		choices.append(binType + (' ' + buildType if buildType else '') + (' ' + archType if archType not in ['win64', 'x64'] else ''))
		choiceActions.append(action)

	reply = choicebox('Select binary to start', choices)
	if reply:
		choiceActions[choices.index(reply)]()

except Exception as ex:
	error(f'Unhandled exception: {ex}\n{traceback.format_exc()}')
