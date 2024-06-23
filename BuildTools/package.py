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

parser = argparse.ArgumentParser(description='FOnline packager')
parser.add_argument('-buildhash', dest='buildhash', required=True, help='build hash')
parser.add_argument('-devname', dest='devname', required=True, help='Dev game name')
parser.add_argument('-nicename', dest='nicename', required=True, help='Representable game name')
parser.add_argument('-authorname', dest='authorname', required=True, help='Author name')
parser.add_argument('-gameversion', dest='gameversion', required=True, help='Game version')
parser.add_argument('-target', dest='target', required=True, choices=['Server', 'Client', 'Single', 'Editor', 'Mapper'], help='package target type')
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
parser.add_argument('-respack', dest='respack', required=True, action='append', default=[], help='resource pack entry')
parser.add_argument('-config', dest='config', required=True, help='config name')
parser.add_argument('-angelscript', dest='angelscript', action='store_true', help='attach angelscript scripts')
parser.add_argument('-mono', dest='mono', action='store_true', help='attach mono scripts')
parser.add_argument('-input', dest='input', required=True, action='append', default=[], help='input dir (from FO_OUTPUT_PATH)')
parser.add_argument('-output', dest='output', required=True, help='output dir')
parser.add_argument('-compresslevel', dest='compresslevel', required=True, help='compress level 0-9')
args = parser.parse_args()

def log(*text):
	print('[Package]', *text, flush=True)

log(f'Make {args.target} ({args.config}) for {args.platform}')

packArgs = args.pack.split('+')
outputPath = (args.output if args.output else os.getcwd()).rstrip('\\/')
buildToolsPath = os.path.dirname(os.path.realpath(__file__))
resourcesDir = 'Resources'

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
	file = file[0:pos] + data + padding + file[pos + maxSize:]
	with open(filePath, 'wb') as f:
		f.write(file)
	assert fileSize == os.path.getsize(filePath)

def patchFile(filePath, textFrom, textTo):
	with open(filePath, 'rb') as f:
		content = f.read()
	content = content.replace(textFrom.encode('utf-8'), textTo.encode('utf-8'))
	with open(filePath, 'wb') as f:
		f.write(content)

def makeZip(name, path):
	zip = zipfile.ZipFile(name, 'w', zipfile.ZIP_DEFLATED, compresslevel=int(args.compresslevel))
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
	# Make packs
	def getTargetEntries(target):
		resourceEntries = []
		if target == 'Server':
			resourceEntries += [['Embedded', '**']]
			resourceEntries += [['ServerProtos', '*.foprob']]
			resourceEntries += [['ServerAngelScript', '*.fosb']]
			resourceEntries += [['Dialogs', '*.fodlg']]
			resourceEntries += [['Maps', '*.fomapb*']]
			for pack in set(args.respack):
				if 'Server' in pack:
					resourceEntries += [[pack, '**']]
		elif target == 'Client' or target == 'Mapper':
			resourceEntries += [['Embedded', '**']]
			resourceEntries += [['Core', '**']]
			resourceEntries += [['EngineData', 'RestoreInfo.fobin']]
			resourceEntries += [['ClientProtos', 'ClientProtos.foprob']]
			resourceEntries += [['ClientAngelScript', 'ClientRootModule.fosb']]
			resourceEntries += [['StaticMaps', '*.fomapb2']]
			resourceEntries += [['Texts', '*.fotxtb']]
			if target == 'Mapper':
				resourceEntries += [['FullProtos', '*.foprob']]
				resourceEntries += [['MapperAngelScript', '*.fosb']]
			for pack in set(args.respack):
				if not ([1 for t in ['Server', 'Client', 'Single', 'Editor', 'Mapper'] if t in pack] and target not in pack):
					if pack not in ['Embedded', 'Core']:
						assert pack not in [p[0] for p in resourceEntries] and pack != 'Configs', 'Used reserved pack name'
						resourceEntries += [[pack, '**']]
		elif target == 'Editor':
			resourceEntries += [['Embedded', '**']]
		else:
			assert False
		return resourceEntries
		
	bakeringPath = getInput('Bakering', 'Resources')
	log('Bakering input', bakeringPath)
	
	if args.target != 'Editor':
		os.makedirs(os.path.join(targetOutputPath, resourcesDir))
	
	for packName, fileMask in getTargetEntries(args.target):
		files = [f for f in glob.glob(os.path.join(bakeringPath, packName, fileMask), recursive=True) if os.path.isfile(f)]
		assert len(files), 'No files in pack ' + packName
		if packName == 'Embedded':
			log('Make pack', packName + '/' + fileMask, '=>', 'embed to executable', '(' + str(len(files)) + ')')
			embeddedData = io.BytesIO()
			with zipfile.ZipFile(embeddedData, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=int(args.compresslevel)) as zip:
				for file in files:
					zip.write(file, os.path.relpath(file, os.path.join(bakeringPath, packName)))
			embeddedData = embeddedData.getvalue()
			embeddedData = struct.pack("I", len(embeddedData)) + embeddedData
		elif packName == 'Raw':
			log('Make pack', packName + '/' + fileMask, '=>', 'raw copy', '(' + str(len(files)) + ')')
			for file in files:
				shutil.copy(file, os.path.join(targetOutputPath, resourcesDir, os.path.relpath(file, os.path.join(bakeringPath, packName))))
		else:
			log('Make pack', packName + '/' + fileMask, '=>', packName + '.zip', '(' + str(len(files)) + ')')
			with zipfile.ZipFile(os.path.join(targetOutputPath, resourcesDir, packName + '.zip'), 'w', zipfile.ZIP_DEFLATED, compresslevel=int(args.compresslevel)) as zip:
				for file in files:
					zip.write(file, os.path.relpath(file, os.path.join(bakeringPath, packName)))
	
	if args.target == 'Server':
		os.makedirs(os.path.join(targetOutputPath, 'Client' + resourcesDir))
		
		for packName, fileMask in getTargetEntries('Client'):
			files = [f for f in glob.glob(os.path.join(bakeringPath, packName, fileMask), recursive=True) if os.path.isfile(f)]
			assert len(files), 'No files in pack ' + packName
			if packName == 'Raw':
				log('Make client pack', packName + '/' + fileMask, '=>', 'raw copy', '(' + str(len(files)) + ')')
				for file in files:
					shutil.copy(file, os.path.join(targetOutputPath, 'Client' + resourcesDir, os.path.relpath(file, os.path.join(bakeringPath, packName))))
			elif packName != 'Embedded':
				log('Make client pack', packName + '/' + fileMask, '=>', packName + '.zip', '(' + str(len(files)) + ')')
				with zipfile.ZipFile(os.path.join(targetOutputPath, 'Client' + resourcesDir, packName + '.zip'), 'w', zipfile.ZIP_DEFLATED, compresslevel=int(args.compresslevel)) as zip:
					for file in files:
						zip.write(file, os.path.relpath(file, os.path.join(bakeringPath, packName)))
	
	def patchEmbedded(filePath):
		patchData(filePath, bytearray([(i + 42) % 200 for i in range(1200000)]), embeddedData, 1200000)
	log('Embedded data length', len(embeddedData))
	
	# Evaluate config
	configName = args.config if args.target != 'Client' else 'Client_' + args.config
	log('Config', configName)
	
	assert os.path.isfile(os.path.join(bakeringPath, 'Configs', configName + '.focfg')), 'Config file not found'
	with open(os.path.join(bakeringPath, 'Configs', configName + '.focfg'), 'r', encoding='utf-8-sig') as f:		
		configData = str.encode(f.read())
	
	def patchConfig(filePath, additionalConfigData=None):
		resultData = configData + (str.encode('\n' + additionalConfigData) if additionalConfigData else b'')
		patchData(filePath, b'###InternalConfig###', resultData, 5022)
	log('Embedded config length', len(configData))
	
	if args.platform == 'Windows':
		# Binaries
		for arch in args.arch.split('+'):
			for binType in [''] + \
					(['Headless'] if 'Headless' in packArgs else []) + \
					(['Service'] if 'Service' in packArgs else []) + \
					(['TotalProfiling'] if 'TotalProfiling' in packArgs else []) + \
					(['OnDemandProfiling'] if 'OnDemandProfiling' in packArgs else []) + \
					(['OGL'] if 'OGL' in packArgs else []):
				binName = args.devname + '_' + args.target + (binType if binType in ['Headless', 'Service'] else '')
				binOutName = (binName if args.target != 'Client' else args.nicename) + \
						('_Profiling' if binType in ['TotalProfiling', 'OnDemandProfiling'] else '') + \
						('_OpenGL' if binType == 'OGL' else '')
				log('Setup', arch, binName, binType)
				binEntry = args.target + '-' + args.platform + '-' + arch + \
						('-Profiling_Total' if binType == 'TotalProfiling' else '') + \
						('-Profiling_OnDemand' if binType == 'OnDemandProfiling' else '') + \
						('-Debug' if 'Debug' in packArgs else '')
				binPath = getInput(os.path.join('Binaries', binEntry), binName)
				log('Binary input', binPath)
				shutil.copy(os.path.join(binPath, binName + '.exe'), os.path.join(targetOutputPath, binOutName + '.exe'))
				if os.path.isfile(os.path.join(binPath, binName + '.pdb')):
					log('PDB file included')
					shutil.copy(os.path.join(binPath, binName + '.pdb'), os.path.join(targetOutputPath, binOutName + '.pdb'))
				else:
					log('PDB file NOT included')
				patchEmbedded(os.path.join(targetOutputPath, binOutName + '.exe'))
				patchConfig(os.path.join(targetOutputPath, binOutName + '.exe'), 'ForceOpenGL=1' if binType == 'OGL' else None)
		
		# Zip
		if 'Zip' in packArgs:
			log('Create zipped archive')
			makeZip(targetOutputPath + '.zip', targetOutputPath)
		
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
		
		# Cleanup raw files if not requested
		if 'Raw' not in packArgs:
			shutil.rmtree(targetOutputPath, True)

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
				st = os.stat(os.path.join(targetOutputPath, binOutName))
				os.chmod(os.path.join(targetOutputPath, binOutName), st.st_mode | stat.S_IEXEC)
		
		# Tar
		if 'Tar' in packArgs:
			log('Create tar archive')
			makeTar(targetOutputPath + '.tar', targetOutputPath, 'w')
		
		# TarGz
		if 'TarGz' in packArgs:
			log('Create tar.gz archive')
			makeTar(targetOutputPath + '.tar.gz', targetOutputPath, 'w:gz')
		
		# Zip
		if 'Zip' in packArgs:
			log('Create zipped archive')
			makeZip(targetOutputPath + '.zip', targetOutputPath)
		
		# AppImage
		if 'Zip' in packArgs:
			# Todo: AppImage
			pass
		
		# Cleanup raw files if not requested
		if 'Raw' not in packArgs:
			shutil.rmtree(targetOutputPath, True)
		
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
		
		shutil.copy(os.path.join(curPath, 'web', 'default-index.html'), os.path.join(targetOutputPath, 'index.html'))
		
		if 'WebServer' in packArgs:
			shutil.copy(os.path.join(curPath, 'web', 'simple-web-server.py'), os.path.join(targetOutputPath, 'web-server.py'))
		
		# Generate resources
		assert 'EMSDK' in os.environ and os.environ['EMSDK'], 'No EMSDK provided'
		filePackagerPath = os.path.join(os.environ['EMSDK'], 'upstream', 'emscripten', 'tools', 'file_packager.py')
		assert os.path.isfile(filePackagerPath), 'No emscripten tools/file_packager.py found'
		
		packagerArgs = ['python3', filePackagerPath, os.path.join(targetOutputPath, 'Resources.data').replace('\\', '/'),
				'--preload', os.path.join(targetOutputPath, resourcesDir).replace('\\', '/') + '@Resources/',
				'--js-output=' + os.path.join(targetOutputPath, 'Resources.js').replace('\\', '/')]
		log('Call emscripten packager:')
		for arg in packagerArgs:
			log('-', arg)
		
		r = subprocess.call(packagerArgs)
		assert r == 0, 'Emscripten tools/file_packager.py failed'
		
		shutil.rmtree(os.path.join(targetOutputPath, resourcesDir), True)
		
		# Patch *.html
		patchFile(os.path.join(targetOutputPath, 'index.html'), '$TITLE$', args.nicename)
		patchFile(os.path.join(targetOutputPath, 'index.html'), '$LOADING$', args.nicename)
		patchFile(os.path.join(targetOutputPath, 'index.html'), '$RESOURCESJS$', 'Resources.js')
		patchFile(os.path.join(targetOutputPath, 'index.html'), '$MAINJS$', binOutName + '.js')
		
		# Favicon
		#logo = getLogo()
		#logo.save(os.path.join(gameOutputPath, 'favicon.ico'), 'ico')
		
		# Zip
		if 'Zip' in packArgs:
			log('Create zipped archive')
			makeZip(targetOutputPath + '.zip', targetOutputPath)
		
	else:
		assert False, 'Unknown build target'

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
