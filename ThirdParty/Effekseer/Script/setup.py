import shutil
import os
import platform
import argparse

os.chdir(os.path.dirname(os.path.abspath(__file__)))

parser = argparse.ArgumentParser()
parser.add_argument('--output', default='../Dev/release')
args = parser.parse_args()
output = os.path.abspath(args.output)

shutil.copytree('../ResourceData/tool/resources/fonts', os.path.join(output, 'resources/fonts'), dirs_exist_ok=True)
shutil.copytree('../ResourceData/tool/resources/icons', os.path.join(output, 'resources/icons'), dirs_exist_ok=True)

pf = platform.system()
if pf == 'Windows':
    pass

if pf == 'Darwin':
    pass

if pf == 'Linux':
    pass
