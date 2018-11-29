import fnmatch
import os

inputDirs = ['Modules']

asScripts = []
for inDir in inputDirs:
	for root, dirnames, filenames in os.walk(inDir):
		for filename in fnmatch.filter(filenames, '*.fos'):
			asScripts.append(os.path.abspath(os.path.join(root, filename)))

for m in asScripts:
	print m
