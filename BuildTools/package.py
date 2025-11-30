#!/usr/bin/python3

import os
import argparse
import sys
import shutil
import zipfile
import subprocess
import tarfile
import glob
import io
import struct
import stat
import foconfig

parser = argparse.ArgumentParser(description='FOnline packager')
parser.add_argument('-maincfg', dest='maincfg', required=True, help='Main config path')
parser.add_argument('-buildhash', dest='buildhash', required=True, help='build hash')
parser.add_argument('-devname', dest='devname', required=True, help='Dev game name')
parser.add_argument('-nicename', dest='nicename', required=True, help='Representable game name')
parser.add_argument('-target', dest='target', required=True, choices=['Server', 'Client', 'Single', 'Editor', 'Mapper', 'Baker'], help='package target type')
parser.add_argument('-platform', dest='platform', required=True, choices=['Windows', 'Linux', 'Android', 'macOS', 'iOS', 'Web'], help='platform type')
parser.add_argument('-arch', dest='arch', required=True, help='architectures to include (divided by +)')
# Windows: win32 win64
# Linux: x64
# Android: arm arm64 x86
# macOS: x64
# iOS: arm64
# Web: wasm
parser.add_argument('-pack', dest='pack', required=True, help='package type')
# Windows: Raw Zip Wix Headless Service
# Linux: Raw Tar TarGz Zip AppImage
# Android: Raw Apk
# macOS: Raw Bundle Zip
# iOS: Raw Bundle
# Web: Raw Zip
parser.add_argument('-config', dest='config', required=True, help='config name')
parser.add_argument('-angelscript', dest='angelscript', action='store_true', help='attach angelscript scripts')
parser.add_argument('-mono', dest='mono', action='store_true', help='attach mono scripts')
parser.add_argument('-input', dest='input', required=True, action='append', default=[], help='input dir (from FO_OUTPUT_PATH)')
parser.add_argument('-output', dest='output', required=True, help='output dir')
args = parser.parse_args()

fomain = foconfig.ConfigParser()
fomain.loadFromFile(args.maincfg)

def log(*text):
	print('[Package]', *text, flush=True)

log(f'Make {args.target} ({args.config}) for {args.platform}')

packArgs = args.pack.split('+')
outputPath = os.path.realpath(args.output if args.output else os.getcwd()).rstrip('\\/')
buildToolsPath = os.path.dirname(os.path.realpath(__file__))
serverResDir = fomain.mainSection().getStr('ServerResources')
clientResDir = fomain.mainSection().getStr('ClientResources')
embeddedBufSize = fomain.mainSection().getInt('EmbeddedBufSize')
zipCompressLevel = fomain.mainSection().getInt('ZipCompressLevel')

curPath = os.path.dirname(sys.argv[0])

# Find files
def getInput(subdir, inputType):
	for i in args.input:
		absDir = os.path.abspath(i)
		absDir = os.path.join(absDir, subdir)
		if os.path.isdir(absDir):
			buildHashPath = os.path.join(absDir, inputType + '.build-hash')
			if os.path.isfile(buildHashPath):
				with open(buildHashPath, 'r', encoding='utf-8-sig') as f:
					buildHash = f.read()
				
				if buildHash == args.buildhash:
					return absDir
				
				else:
					assert False, 'Build hash file ' + buildHashPath + ' has wrong hash'
			else:
				assert False, 'Build hash file ' + buildHashPath + ' not found'
	else:
		assert False, 'Input dir ' + subdir + ' not found for ' + inputType

def getLogo():
	logoPath = os.path.join(outputPath, 'Logo.png')
	if not os.path.isfile(logoPath):
		logoPath = os.path.join(binariesPath, 'DefaultLogo.png')
	import PIL
	return PIL.Image.open(logoPath)

def patchData(filePath, mark, data, maxSize):
	assert len(data) <= maxSize, 'Data size is to big ' + str(len(data)) + ' but maximum is ' + str(maxSize)
	with open(filePath, 'rb') as f:
		file = f.read()
	fileSize = os.path.getsize(filePath)
	pos = file.find(mark)
	assert pos != -1
	padding = b'#' * (maxSize - len(data))
	file = file[:pos] + data + padding + file[pos + maxSize:]
	with open(filePath, 'wb') as f:
		f.write(file)
	assert fileSize == os.path.getsize(filePath)

def patchFile(filePath, textFrom, textTo):
	with open(filePath, 'rb') as f:
		content = f.read()
	content = content.replace(textFrom.encode('utf-8'), textTo.encode('utf-8'))
	with open(filePath, 'wb') as f:
		f.write(content)

def makeZip(name, path, mode='w'):
	zip = zipfile.ZipFile(name, mode, zipfile.ZIP_DEFLATED, compresslevel=zipCompressLevel)
	for root, dirs, files in os.walk(path):
		for file in files:
			zip.write(os.path.join(root, file), os.path.join(os.path.relpath(root, path), file))
	zip.close()

def makeTar(name, path, mode):
	def filter(tarinfo):
		#tarinfo.mode = 0777
		return tarinfo
	tar = tarfile.open(name, mode)
	dir = os.getcwd()
	newDir, folder = os.path.split(path)
	os.chdir(newDir)
	tar.add(folder, filter = filter)
	os.chdir(dir)
	tar.close()

def build():
	# Pack resorces
	if 'NoRes' not in packArgs:
		bakeOutput = fomain.mainSection().getStr('BakeOutput')
		bakingPath = getInput(bakeOutput, 'Resources')
		log('Baking input', bakingPath)
		
		def getTargetResPacks(target):
			resourceEntries = []
			resPacks = fomain.getSections('ResourcePack')
			for resPack in resPacks:
				serverOnly = resPack.getBool('ServerOnly', False)
				clientOnly = resPack.getBool('ClientOnly', False)
				mapperOnly = resPack.getBool('MapperOnly', False)
				if target == 'Server' and not clientOnly and not mapperOnly:
					resourceEntries.append(resPack.getStr('Name'))
				if target == 'Client' and not serverOnly and not mapperOnly:
					resourceEntries.append(resPack.getStr('Name'))
			return resourceEntries
		
		def filterFile(target, f):
			if not os.path.isfile(f):
				return False
			if target == 'Server' and (f.endswith('-client') or f.endswith('-mapper')):
				return False
			if target == 'Client' and (f.endswith('-server') or f.endswith('-mapper')):
				return False
			if target == 'Mapper' and f.endswith('-server'):
				return False
			return True
		
		if args.target in ['Server']:
			os.makedirs(os.path.join(targetOutputPath, serverResDir))
			os.makedirs(os.path.join(targetOutputPath, clientResDir))
		if args.target in ['Client']:
			os.makedirs(os.path.join(targetOutputPath, clientResDir))
		
		for packName in getTargetResPacks(args.target):
			files = [f for f in glob.glob(os.path.join(bakingPath, packName, '**'), recursive=True) if filterFile(args.target, f)]
			assert len(files), 'No files in pack ' + packName
			if packName == 'Embedded':
				log('Make pack', packName, '=>', 'embed to executable', '(' + str(len(files)) + ')')
				embeddedData = io.BytesIO()
				with zipfile.ZipFile(embeddedData, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=zipCompressLevel) as zip:
					for file in files:
						zip.write(file, os.path.relpath(file, os.path.join(bakingPath, packName)))
				embeddedData = embeddedData.getvalue()
				embeddedData = struct.pack("I", len(embeddedData)) + embeddedData
			else:
				log('Make pack', packName, '=>', packName + '.zip', '(' + str(len(files)) + ')')
				baseResName = serverResDir if args.target in ['Server'] else clientResDir
				with zipfile.ZipFile(os.path.join(targetOutputPath, baseResName, packName + '.zip'), 'w', zipfile.ZIP_DEFLATED, compresslevel=zipCompressLevel) as zip:
					for file in files:
						zip.write(file, os.path.relpath(file, os.path.join(bakingPath, packName)))
		
		# Client resources for server
		if args.target == 'Server':
			for packName in getTargetResPacks('Client'):
				files = [f for f in glob.glob(os.path.join(bakingPath, packName, '**'), recursive=True) if filterFile('Client', f)]
				assert len(files), 'No files in pack ' + packName
				if packName != 'Embedded':
					log('Make client pack', packName, '=>', packName + '.zip', '(' + str(len(files)) + ')')
					with zipfile.ZipFile(os.path.join(targetOutputPath, clientResDir, packName + '.zip'), 'w', zipfile.ZIP_DEFLATED, compresslevel=zipCompressLevel) as zip:
						for file in files:
							zip.write(file, os.path.relpath(file, os.path.join(bakingPath, packName)))
	
		# Evaluate config
		configName = ('Server_' if args.target != 'Client' else 'Client_') + args.config
		log('Config', configName)
		
		assert os.path.isfile(os.path.join(bakingPath, 'Configs', configName + '.fomain-bin')), 'Config file not found'
		with open(os.path.join(bakingPath, 'Configs', configName + '.fomain-bin'), 'r', encoding='utf-8-sig') as f:		
			configData = str.encode(f.read())
		
		log('Embedded data length', len(embeddedData))
		log('Embedded config length', len(configData))
	
	def patchEmbedded(filePath):
		patchData(filePath, bytearray([(i + 42) % 200 for i in range(embeddedBufSize)]), embeddedData, embeddedBufSize)
	
	def patchConfig(filePath, additionalConfigData=None):
		resultData = configData + (str.encode('\n' + additionalConfigData) if additionalConfigData else b'')
		patchData(filePath, b'###InternalConfig###1234', resultData, 10048)
	
	def patchPackagedMark(filePath):
		patchData(filePath, b'###NOT_PACKAGED###', b'###XXXXXXXXXXXX###', 18)
	
	if args.platform == 'Windows':
		# Binaries
		for arch in args.arch.split('+'):
			for binType in [''] + \
					(['Headless'] if 'Headless' in packArgs else []) + \
					(['Service'] if 'Service' in packArgs else []) + \
					(['TotalProfiling'] if 'TotalProfiling' in packArgs else []) + \
					(['OnDemandProfiling'] if 'OnDemandProfiling' in packArgs else []) + \
					(['OGL'] if 'OGL' in packArgs else []):
				isLib = 'Lib' in packArgs
				binName = args.devname + '_' + args.target + (binType if binType in ['Headless', 'Service'] else '') + ('Lib' if isLib else '')
				log('Setup', arch, binName, binType)
				
				binOutName = (binName if args.target != 'Client' else args.nicename) + \
						('_Profiling' if binType in ['TotalProfiling', 'OnDemandProfiling'] else '') + \
						('_OpenGL' if binType == 'OGL' else '')
				binEntry = args.target + '-' + args.platform + '-' + arch + \
						('-Profiling_Total' if binType == 'TotalProfiling' else '') + \
						('-Profiling_OnDemand' if binType == 'OnDemandProfiling' else '') + \
						('-Debug' if 'Debug' in packArgs else '')
				binPath = getInput(os.path.join('Binaries', binEntry), binName)
				binExt = '.dll' if isLib else '.exe'
				log('Binary input', binPath)
				
				shutil.copy(os.path.join(binPath, binName + binExt), os.path.join(targetOutputPath, binOutName + binExt))
				
				if os.path.isfile(os.path.join(binPath, binName + '.pdb')):
					log('PDB file included')
					shutil.copy(os.path.join(binPath, binName + '.pdb'), os.path.join(targetOutputPath, binOutName + '.pdb'))
				else:
					log('PDB file NOT included')
				
				if 'NoRes' not in packArgs:
					patchEmbedded(os.path.join(targetOutputPath, binOutName + binExt))
					patchConfig(os.path.join(targetOutputPath, binOutName + binExt), 'ForceOpenGL=1' if binType == 'OGL' else None)
					patchPackagedMark(os.path.join(targetOutputPath, binOutName + binExt))
		
		"""
		# MSI Installer
		sys.path.insert(0, os.path.join(curPath, 'msicreator'))
		import createmsi
		import uuid

		msiConfig = "" " \
		{
			"upgrade_guid": "%s",
			"version": "%s",
			"product_name": "%s",
			"manufacturer": "%s",
			"name": "%s",
			"name_base": "%s",
			"comments": "%s",
			"installdir": "%s",
			"license_file": "%s",
			"need_msvcrt": %s,
			"arch": %s,
			"parts":
			[ {
					"id": "%s",
					"title": "%s",
					"description": "%s",
					"absent": "%s",
					"staged_dir": "%s"
			} ]
		}"" " % (uuid.uuid3(uuid.NAMESPACE_OID, gameName), '1.0.0', \
				gameName, 'Dream', gameName, gameName, 'The game', \
				gameName, 'License.rtf', 'false', 32, \
				gameName, gameName, 'MMORPG', 'disallow', gameName)

		try:
			cwd = os.getcwd()
			wixBinPath = os.path.abspath(os.path.join(curPath, 'wix'))
			wixTempPath = os.path.abspath(os.path.join(targetOutputPath, 'WixTemp'))
			os.environ['WIX_TEMP'] = wixTempPath
			os.makedirs(wixTempPath)

			licensePath = os.path.join(outputPath, 'MsiLicense.rtf')
			if not os.path.isfile(licensePath):
				licensePath = os.path.join(binariesPath, 'DefaultMsiLicense.rtf')
			shutil.copy(licensePath, os.path.join(targetOutputPath, 'License.rtf'))

			with open(os.path.join(targetOutputPath, 'MSI.json'), 'wt') as f:
				f.write(msiConfig)

			os.chdir(targetOutputPath)

			msi = createmsi.PackageGenerator('MSI.json')
			msi.generate_files()
			msi.final_output = gameName + '.msi'
			msi.args1 = ['-nologo', '-sw']
			msi.args2 = ['-sf', '-spdb', '-sw', '-nologo']
			msi.build_package(wixBinPath)

			shutil.rmtree(wixTempPath, True)
			os.remove('MSI.json')
			os.remove('License.rtf')
			os.remove(gameName + '.wixobj')
			os.remove(gameName + '.wxs')

		except Exception as e:
			print(str(e))
		finally:
			os.chdir(cwd)

		# Update binaries
		binPath = resourcesPath + '/../Binaries'
		if not os.path.exists(binPath):
			os.makedirs(binPath)
		shutil.copy(gameOutputPath + '/' + gameName + '.exe', binPath + '/' + gameName + '.exe')
		shutil.copy(gameOutputPath + '/' + gameName + '.pdb', binPath + '/' + gameName + '.pdb')
		"""

	elif args.platform == 'Linux':
		# Raw files
		for arch in args.arch.split('+'):
			for binType in [''] + \
					(['Headless'] if 'Headless' in packArgs else []) + \
					(['Daemon'] if 'Daemon' in packArgs else []) + \
					(['TotalProfiling'] if 'TotalProfiling' in packArgs else []) + \
					(['OnDemandProfiling'] if 'OnDemandProfiling' in packArgs else []):
				binName = args.devname + '_' + args.target + (binType if binType in ['Headless', 'Daemon'] else '')
				binOutName = (binName if args.target != 'Client' else args.nicename) + \
						('_Profiling' if binType in ['TotalProfiling', 'OnDemandProfiling'] else '')
				log('Setup', arch, binName, binType)
				binEntry = args.target + '-' + args.platform + '-' + arch + \
						('-Profiling_Total' if binType == 'TotalProfiling' else '') + \
						('-Profiling_OnDemand' if binType == 'OnDemandProfiling' else '') + \
						('-Debug' if 'Debug' in packArgs else '')
				binPath = getInput(os.path.join('Binaries', binEntry), binName)
				log('Binary input', binPath)
				shutil.copy(os.path.join(binPath, binName), os.path.join(targetOutputPath, binOutName))
				patchEmbedded(os.path.join(targetOutputPath, binOutName))
				patchConfig(os.path.join(targetOutputPath, binOutName))
				patchPackagedMark(os.path.join(targetOutputPath, binOutName))
				st = os.stat(os.path.join(targetOutputPath, binOutName))
				os.chmod(os.path.join(targetOutputPath, binOutName), st.st_mode | stat.S_IEXEC)
		
		# AppImage
		if 'AppImage' in packArgs:
			# Todo: AppImage
			pass
		
	elif args.platform == 'Mac':
		# Raw files
		os.makedirs(gameOutputPath)
		shutil.copytree(resourcesPath, gameOutputPath + '/Data')
		shutil.copy(binariesPath + '/Mac/FOnline', gameOutputPath + '/' + gameName)
		patchConfig(gameOutputPath + '/' + gameName)
		
		# Tar
		makeTar(targetOutputPath + '/' + gameName + '.tar', gameOutputPath, 'w')
		makeTar(targetOutputPath + '/' + gameName + '.tar.gz', gameOutputPath, 'w:gz')
	
	elif args.platform == 'Android':
		shutil.copytree(binariesPath + '/Android', gameOutputPath)
		patchConfig(gameOutputPath + '/libs/armeabi-v7a/libFOnline.so')
		# No x86 build
		# patchConfig(gameOutputPath + '/libs/x86/libFOnline.so')
		patchFile(gameOutputPath + '/res/values/strings.xml', 'FOnline', gameName)
		
		# Icons
		logo = getLogo()
		logo.resize((48, 48)).save(gameOutputPath + '/res/drawable-mdpi/ic_launcher.png', 'png')
		logo.resize((72, 72)).save(gameOutputPath + '/res/drawable-hdpi/ic_launcher.png', 'png')
		logo.resize((96, 96)).save(gameOutputPath + '/res/drawable-xhdpi/ic_launcher.png', 'png')
		logo.resize((144, 144)).save(gameOutputPath + '/res/drawable-xxhdpi/ic_launcher.png', 'png')
		
		# Bundle
		shutil.copytree(resourcesPath, gameOutputPath + '/assets')
		with open(gameOutputPath + '/assets/FilesTree.txt', 'wb') as f:
			f.write('\n'.join(os.listdir(resourcesPath)))
		
		# Pack
		antPath = os.path.abspath(os.path.join(curPath, 'ant', 'bin', 'ant.bat'))
		r = subprocess.call([antPath, '-f', gameOutputPath, 'debug'], shell = True)
		assert r == 0
		shutil.copy(gameOutputPath + '/bin/SDLActivity-debug.apk', targetOutputPath + '/' + gameName + '.apk')
	
	elif args.platform == 'Web':
		assert args.arch == 'wasm'
		
		binName = args.devname + '_' + args.target
		binOutName = binName
		log('Setup', binName)
		binEntry = args.target + '-' + args.platform + '-' + args.arch + ('-Debug' if 'Debug' in packArgs else '')
		binPath = getInput(os.path.join('Binaries', binEntry), binName)
		log('Binary input', binPath)
		shutil.copy(os.path.join(binPath, binName + '.js'), os.path.join(targetOutputPath, binOutName + '.js'))
		shutil.copy(os.path.join(binPath, binName + '.wasm'), os.path.join(targetOutputPath, binOutName + '.wasm'))
		
		patchEmbedded(os.path.join(targetOutputPath, binOutName + '.wasm'))
		patchConfig(os.path.join(targetOutputPath, binOutName + '.wasm'))
		patchPackagedMark(os.path.join(targetOutputPath, binOutName + '.wasm'))
		
		shutil.copy(os.path.join(curPath, 'web', 'default-index.html'), os.path.join(targetOutputPath, 'index.html'))
		
		if 'WebServer' in packArgs:
			shutil.copy(os.path.join(curPath, 'web', 'simple-web-server.py'), os.path.join(targetOutputPath, 'web-server.py'))
		
		# Generate resources
		assert 'EMSDK' in os.environ and os.environ['EMSDK'], 'No EMSDK provided'
		filePackagerPath = os.path.join(os.environ['EMSDK'], 'upstream', 'emscripten', 'tools', 'file_packager.py')
		assert os.path.isfile(filePackagerPath), 'No emscripten tools/file_packager.py found'
		
		packagerArgs = ['python3', filePackagerPath, os.path.join(targetOutputPath, 'Resources.data').replace('\\', '/'),
				'--preload', os.path.join(targetOutputPath, clientResDir).replace('\\', '/') + '@' + clientResDir,
				'--js-output=' + os.path.join(targetOutputPath, 'Resources.js').replace('\\', '/')]
		log('Call emscripten packager:')
		for arg in packagerArgs:
			log('-', arg)
		
		r = subprocess.call(packagerArgs)
		assert r == 0, 'Emscripten tools/file_packager.py failed'
		
		shutil.rmtree(os.path.join(targetOutputPath, clientResDir), True)
		
		# Patch *.html
		patchFile(os.path.join(targetOutputPath, 'index.html'), '$TITLE$', args.nicename)
		patchFile(os.path.join(targetOutputPath, 'index.html'), '$LOADING$', args.nicename)
		patchFile(os.path.join(targetOutputPath, 'index.html'), '$RESOURCESJS$', 'Resources.js')
		patchFile(os.path.join(targetOutputPath, 'index.html'), '$MAINJS$', binOutName + '.js')
		
		# Favicon
		#logo = getLogo()
		#logo.save(os.path.join(gameOutputPath, 'favicon.ico'), 'ico')
		
	else:
		assert False, 'Unknown build target'
	
	# Zip
	if 'Zip' in packArgs:
		log('Create zipped archive')
		makeZip(targetOutputPath + '.zip', targetOutputPath)
	
	# SingleZip
	if 'SingleZip' in packArgs:
		log('Add to single zip archive')
		singleZipPath = os.path.join(outputPath, os.path.basename(outputPath) + '.zip')
		makeZip(singleZipPath, targetOutputPath, 'a')
	
	# Tar
	if 'Tar' in packArgs:
		log('Create tar archive')
		makeTar(targetOutputPath + '.tar', targetOutputPath, 'w')
	
	# TarGz
	if 'TarGz' in packArgs:
		log('Create tar.gz archive')
		makeTar(targetOutputPath + '.tar.gz', targetOutputPath, 'w:gz')
	
	# Root raw
	if 'Root' in packArgs:
		shutil.copytree(targetOutputPath, outputPath, dirs_exist_ok=True)
	
	# Cleanup raw files if not requested
	if 'Raw' not in packArgs:
		shutil.rmtree(targetOutputPath, True)

try:
	targetOutputPath = os.path.join(outputPath, args.devname + '-' + args.target)
	if args.target != args.config:
		targetOutputPath += '-' + args.config
	if args.platform != 'Windows':
		targetOutputPath += '-' + args.platform
	
	log('Output to', targetOutputPath)
	
	shutil.rmtree(targetOutputPath, True)
	if os.path.isfile(targetOutputPath + '.zip'):
		os.remove(targetOutputPath + '.zip')
	os.makedirs(targetOutputPath)
	
	build()
	
	log('Complete!')
	
except:
	if targetOutputPath:
		shutil.rmtree(targetOutputPath, True)
	raise
