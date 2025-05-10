#!/usr/bin/python3

import os
import argparse
import platform
import sys
import subprocess
import traceback
import foconfig

BIN_TYPES = ['Server', 'Client', 'Mapper', 'Editor', 'Baker', 'ASCompiler', 'Tests']
BIN_NEED_BACKED_RESOURCES = ['Server', 'Client', 'Mapper']

# Argument parser setup
parser = argparse.ArgumentParser(description='FOnline packager')
parser.add_argument('-maincfg', dest='maincfg', required=True, help='Main config path')
parser.add_argument('-devname', dest='devname', required=True, help='Dev game name')
parser.add_argument('-buildhash', dest='buildhash', required=True, help='Build hash')
parser.add_argument('-bininput', dest='bininput', required=True, action='append', default=[], help='Binary input directory')
parser.add_argument('-config', dest='config', action='append', default=[], help='Config option')
args = parser.parse_args()

assert platform.system() in ['Linux', 'Darwin', 'Windows'], 'Invalid OS'

fomain = foconfig.ConfigParser()
fomain.loadFromFile(args.maincfg)

def log(*text):
    print('[Starter]', *text, flush=True)

try:
    import easygui
    hasEasyGui = True
except ImportError:
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
        for index, choice in enumerate(choices, start=1):
            log(f'{index}) {choice}' + (' (default)' if index == 1 else ''))
        try:
            index = int(input(': ') or 1)
        except ValueError:
            index = 0
        return choices[index - 1] if 1 <= index <= len(choices) else None

def checkDirHash(directory, inputType):
    buildHashPath = os.path.join(directory, inputType + '.build-hash')
    if os.path.isfile(buildHashPath):
        with open(buildHashPath, 'r', encoding='utf-8-sig') as f:
            return f.read() == args.buildhash
    return False

try:
    log('Build hash', args.buildhash)
    bakeOutput = fomain.mainSection().getStr('BakeOutput')
    bakingEntry = os.path.realpath(bakeOutput)

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

    configs = [subConfig.getStr('Name') for subConfig in fomain.getSections('SubConfig')]
    choices = []
    choiceActions = []

    for binEntry in binaryEntries:
        nameParts = os.path.basename(binEntry).split('-')
        binType = nameParts[0]
        archType = nameParts[2]
        buildType = nameParts[3] if len(nameParts) > 3 else ''

        def action(binEntry=binEntry, binType=binType, buildType=buildType):
            config = None
            if binType in ['Server', 'Client', 'Mapper', 'Editor']:
                binConfigs = configs[:]
                if binType in binConfigs:
                    binConfigs.remove(binType)
                    binConfigs.insert(0, binType)
                config = choicebox('Select config to start', binConfigs)
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

            exeArgs = ['-ApplyConfig', os.path.relpath(args.maincfg)]
            if config is not None:
                exeArgs += ['-ApplySubConfig', config]
            if binType == 'Baker':
                exeArgs += ['-ForceBaking', 'True' if bakingType == 'Force' else 'False']
            for entry in args.config:
                exeArgs += ['-' + entry.split(',')[0], entry.split(',')[1]]

            needWait = binType in ['Baker', 'ASCompiler'] or 'San_' in buildType
            if not needWait and platform.system() in ['Linux', 'Darwin']:
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
