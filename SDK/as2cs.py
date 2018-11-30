import fnmatch
import os
from tokenizer import tokenize, TOK

inputDirs = ['Modules']

asScripts = []
for inDir in inputDirs:
	for root, dirnames, filenames in os.walk(inDir):
		for filename in fnmatch.filter(filenames, '*.fos'):
			asScripts.append(os.path.abspath(os.path.join(root, filename)))

for filePath in asScripts:
	with open(filePath, 'rb') as f:
		text = f.read()
	
	print filePath
	for token in tokenize(text):
		print "{0}: '{1}' {2}".format(
				TOK.descr[token.kind],
				token.txt or "-",
				token.val or "")
	
	break
