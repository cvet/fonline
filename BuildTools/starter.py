#!/usr/bin/python3

import os
import argparse
import platform
import easygui
import sys
import subprocess
import traceback

BIN_TYPES = ['Server', 'Client', 'Single', 'Mapper', 'Editor', 'Baker', 'ASCompiler', 'Tests']
BIN_NEED_BACKED_RESOURCES = ['Server', 'Client', 'Single', 'Mapper']

parser = argparse.ArgumentParser(description='FOnline packager')
parser.add_argument('-devname', dest='devname', required=True, help='Dev game name')
parser.add_argument('-buildhash', dest='buildhash', required=True, help='build hash')
parser.add_argument('-bakering', dest='bakering', required=True, help='bakering dir')
parser.add_argument('-bininput', dest='bininput', required=True, action='append', default=[], help='binary input dir')
parser.add_argument('-defaultcfg', dest='defaultcfg', help='Game default config')
parser.add_argument('-mappercfg', dest='mappercfg', help='Mapper default config')
parser.add_argument('-content', dest='content', action='append', default=[], help='content file path')
parser.add_argument('-resource', dest='resource', action='append', default=[], help='resource file path')
args = parser.parse_args()

assert platform.system() in ['Linux', 'Darwin', 'Windows'], 'Invalid OS'

def log(*text):
	print('[Starter]', *text, flush=True)

def error(message):
	log(message)
	easygui.msgbox(message, args.devname + ' Starter')
	sys.exit(1)

def choicebox(message, choises):
	return easygui.choicebox(message, args.devname + ' Starter', choices=choises)

def checkDirHash(dir, inputType):
	buildHashPath = os.path.join(dir, inputType + '.build-hash')
	if os.path.isfile(buildHashPath):
		with open(buildHashPath, 'r', encoding='utf-8-sig') as f:
			return f.read() == args.buildhash
	return False

try:
	bakeringEntry = os.path.realpath(args.bakering)

	if not checkDirHash(bakeringEntry, 'Resources'):
		log('Baked resources not found')
		bakeringEntry = None

	binaryEntries = []
	
	for input in args.bininput:
		absInput = os.path.realpath(input)
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

	if not bakeringEntry:
		binaryEntries = list(filter(lambda x: os.path.basename(x).split('-')[0] not in BIN_NEED_BACKED_RESOURCES, binaryEntries))
	binaryEntries = list(filter(lambda x: len(os.path.basename(x).split('-')) >= 3, binaryEntries))
	binaryEntries = list(filter(lambda x: os.path.basename(x).split('-')[0] in BIN_TYPES, binaryEntries))
	binaryEntries.sort(key=lambda x: BIN_TYPES.index(os.path.basename(x).split('-')[0]))

	if not binaryEntries:
		error('Binaries not found')

	for binaryEntry in binaryEntries:
		log('Found binary entry', os.path.relpath(binaryEntry))

	configs = []

	if bakeringEntry:
		for entry in os.listdir(os.path.join(bakeringEntry, 'Configs')):
			if os.path.isfile(os.path.join(bakeringEntry, 'Configs', entry)) and not entry.startswith('Client_') and os.path.splitext(entry)[1] == '.focfg':
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
		osType = nameParts[1]
		archType = nameParts[2]
		buildType = nameParts[3] if len(nameParts) > 3 else ''
		
		def action(binEntry=binEntry, binType=binType, osType=osType, archType=archType, buildType=buildType):
			config = None
			
			if binType == 'Mapper' and args.mappercfg:
				config = args.mappercfg
			elif binType in ['Server', 'Client', 'Single', 'Mapper']:
				log('Select config to start')
				config = choicebox('Select config to start', configs)
				if not config:
					sys.exit(0)
			elif binType == 'Baker':
				log('Select bakering mode')
				bakeringType = choicebox('Select bakering mode', ['Iterative', 'Force'])
				if not bakeringType:
					sys.exit(0)
			
			exePath = os.path.join(binEntry, args.devname + '_' + binType + ('-' + buildType if buildType else ''))
			if platform.system() == 'Windows':
				exePath += '.exe'
			exePath = os.path.relpath(exePath)
			
			exeArgs = []
			if binType in ['Server', 'Client', 'Single', 'Mapper']:
				exeArgs += ['-ResourcesDir', os.path.relpath(bakeringEntry)]
				exeArgs += ['-EmbeddedResources', os.path.relpath(os.path.join(bakeringEntry, 'Embedded'))]
				exeArgs += ['-DataSynchronization', 'False']
			if config:
				exeArgs += ['-ExternalConfig', os.path.relpath(os.path.join(bakeringEntry, 'Configs', f'{config}.focfg'))]
			if binType == 'Baker':
				exeArgs += ['-ForceBakering', 'True' if bakeringType == 'Force' else 'False']
			if binType in ['Editor', 'Baker', 'ASCompiler']:
				exeArgs += ['-BakeOutput', os.path.relpath(os.path.realpath(args.bakering))]
				for c in args.content:
					exeArgs += ['-BakeContentEntries', f'+{c}']
				for r in args.resource:
					exeArgs += ['-BakeResourceEntries', f'+{r}']
			if (platform.system() == 'Linux' or platform.system() == 'Darwin') and binType in ['Server', 'Client', 'Single', 'Editor', 'Mapper']:
				exeArgs =+ ['--fork']
			
			log('Run', exePath)
			for exeArg in exeArgs:
				log('-', exeArg)
			
			proc = subprocess.Popen([exePath] + exeArgs)
			
			if binType in ['Baker', 'ASCompiler']:
				proc.wait()
				if proc.returncode:
					error('Something went wrong\nVerify log messages')
		
		choices.append(str(len(choices) + 1) + ' ' + binType + (' ' + buildType if buildType else ''))
		choiceActions.append(action)

	log('Select binary to start')
	reply = choicebox('Select binary to start', choices)
	if reply:
		choiceActions[choices.index(reply)]()

except Exception as ex:
	error(f'Unhandled exception: {ex}\n{traceback.format_exc()}')
