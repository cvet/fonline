#!/usr/bin/env python3

# Copyright 2017-2018 Jussi Pakkanen et al
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os, sys, subprocess
import createmsi

def build_msvcrt():
    msvcrt_file = 'program.cpp'
    msvcrt_staging_dir = 'msvcrt/main'
    msvcrt_binary_out = 'main/program.exe'
    if not os.path.exists(msvcrt_staging_dir):
        os.mkdir(msvcrt_staging_dir)
    subprocess.check_call(['cl',
                           '/nologo',
                           '/O2',
                           '/MD',
                           '/EHsc',
                           msvcrt_file,
                           '/Fe' + msvcrt_binary_out],
                          cwd='msvcrt')

def build_iconexe():
    staging_dir = 'shortcuts/staging'
    if not os.path.exists(staging_dir):
        os.mkdir(staging_dir)
    subprocess.check_call(['rc', '/nologo', '/fomyres.res', 'myres.rc'],
                          cwd='shortcuts')
    subprocess.check_call(['cl',
                           '/nologo',
                           '/Fprog.obj',
                           '/O2',
                           '/MT',
                           '/c',
                           'prog.c'],
                          cwd='shortcuts')
    subprocess.check_call(['link',
                           '/OUT:staging/iconprog.exe',
                           'myres.res',
                           'prog.obj',
                           '/nologo',
                           '/SUBSYSTEM:WINDOWS',
                           'user32.lib',
                           ],
                          cwd='shortcuts')

def build_binaries():
    build_msvcrt()
    build_iconexe()

if __name__ == '__main__':
    testdirs = [('basictest', 'msidef.json'),
                ('two_items', 'two.json'),
                ('msvcrt', 'msvcrt.json'),
                ('icons', 'icons.json'),
                ('shortcuts', 'shortcuts.json'),
                ('registryentries', 'registryentry.json'),
                ('majorupgrade', 'majorupgrade.json')
    ]

    build_binaries()
    for d in testdirs:
        os.chdir(d[0])
        createmsi.run([d[1]])
        os.chdir('..')
