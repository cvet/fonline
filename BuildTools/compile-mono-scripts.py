#!/usr/bin/python3

import os
import sys
import argparse
import subprocess

parser = argparse.ArgumentParser(description='FOnline scripts generation')
parser.add_argument('-scripts', dest='scripts', required=True, help='path to scripts directory')
parser.add_argument('-assembly', dest='assembly', action='append', default=[], help='assembly name')
args = parser.parse_args()

msbuild = 'msbuild'
#if os.name == 'nt':
#    if os.path.isdir('C:/Program Files/Mono/bin'):
#        msbuild = 'C:/Program Files/Mono/bin/msbuild'
#    elif os.path.isdir('C:/Program Files (x86)/Mono/bin'):
#        msbuild = 'C:/Program Files (x86)/Mono/bin/msbuild'

# Compile
def compileAssembly(target, name):
    subprocess.check_call([msbuild, '-noLogo', '-m', '-target:Build', '-p:Configuration=Release',
            '-p:OutputPath=' + target + 'Assemblies', name + '.' + target + '.csproj'], shell = True)

for target in ['Server', 'Client', 'Mapper']:
    for assembly in args.assembly:
        compileAssembly(target, assembly)

# Link
# Todo:
def linkAssembly(target, name):
    pass

for target in ['Server', 'Client', 'Mapper']:
    for assembly in args.assembly:
        linkAssembly(target, assembly)

# AOT
# Todo:
def aotAssembly(target, name, platform):
    pass

for target in ['Server', 'Client', 'Mapper']:
    for platform in ['x86', 'amd64', 'arm', 'arm64', 'wasm']:
        for assembly in args.assembly:
            aotAssembly('Client', assembly, platform)

for platform in ['x86', 'amd64', 'arm', 'arm64', 'wasm']:
    for a in args.assembly:
        aotAssembly('Client', a, platform)
for platform in ['x86', 'amd64', 'wasm']:
    for a in args.assembly:
        aotAssembly('Mapper', a, platform)
