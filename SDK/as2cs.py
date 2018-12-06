import fnmatch
import os
from tokenize import tokenize, untokenize, NUMBER, STRING, NAME, OP
from io import BytesIO

inputDirs = ['Modules']

def convert(filePath):
	print filePath
	
	with open(filePath, 'rb') as f:
		text = f.read()
	
	g = tokenize(BytesIO(text.encode('utf-8')).readline)
	#print g
	#for t in g:
	#	print '>', t
	"""
	for token in tokenize(text):
		print '{0}: <{1}> {2}'.format(
				TOK.descr[token.kind],
				token.txt or '-',
				token.val or '')
	"""

def covertAll():
	asScripts = []
	for inDir in inputDirs:
		for root, dirs, fileNames in os.walk(inDir):
			for fileName in fnmatch.filter(fileNames, '*.fos'):
				asScripts.append(os.path.abspath(os.path.join(root, fileName)))

	for filePath in asScripts:
		convert(filePath)

convert('Modules/Core/Tween.fos')
