#include "BuildSystem.h"
#include "Log.h"
#include "FileUtils.h"

class DefaultBuildSystem: public IBuildSystem
{
    bool BuildTarget( string target )
    {
        /*gameOutputPath = targetOutputPath + '/' + gameName

           if buildTarget == 'Windows':
         # Raw files
                os.makedirs(gameOutputPath)
                shutil.copytree(resourcesPath, gameOutputPath + '/Data')
                shutil.copy(binariesPath + '/Windows/FOnline.exe', gameOutputPath + '/' + gameName + '.exe')
                shutil.copy(binariesPath + '/Windows/FOnline.pdb', gameOutputPath + '/' + gameName + '.pdb')
                patchConfig(gameOutputPath + '/' + gameName + '.exe')

         # Patch icon
                if os.name == 'nt':
                        icoPath = os.path.join(gameOutputPath, 'Windows.ico')
                        logo.save(icoPath, 'ico')
                        resHackPath = os.path.abspath(os.path.join(curPath, '_other', 'ReplaceVistaIcon.exe'))
                        r = subprocess.call([resHackPath, gameOutputPath + '/' + gameName + '.exe', icoPath], shell = True)
                        os.remove(icoPath)
                        assert r == 0

         # Zip
                makeZip(targetOutputPath + '/' + gameName + '.zip', gameOutputPath)

         # MSI Installer
                sys.path.insert(0, os.path.join(curPath, '_msicreator'))
                import createmsi
                import uuid

                msiConfig = """ \
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
                }""" % (uuid.uuid3(uuid.NAMESPACE_OID, gameName), '1.0.0', \
                                gameName, 'Dream', gameName, gameName, 'The game', \
                                gameName, 'License.rtf', 'false', 32, \
                                gameName, gameName, 'MMORPG', 'disallow', gameName)

                try:
                        cwd = os.getcwd()
                        wixBinPath = os.path.abspath(os.path.join(curPath, '_wix'))
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

                except Exception, e:
                        print str(e)
                finally:
                        os.chdir(cwd)

         # Update binaries
                binPath = resourcesPath + '/../Binaries'
                if not os.path.exists(binPath):
                        os.makedirs(binPath)
                shutil.copy(gameOutputPath + '/' + gameName + '.exe', binPath + '/' + gameName + '.exe')
                shutil.copy(gameOutputPath + '/' + gameName + '.pdb', binPath + '/' + gameName + '.pdb')

           elif buildTarget == 'Linux':
         # Raw files
                os.makedirs(gameOutputPath)
                shutil.copytree(resourcesPath, gameOutputPath + '/Data')
         # shutil.copy(binariesPath + '/Linux/FOnline32', gameOutputPath + '/' + gameName + '32')
                shutil.copy(binariesPath + '/Linux/FOnline64', gameOutputPath + '/' + gameName + '64')
         # patchConfig(gameOutputPath + '/' + gameName + '32')
                patchConfig(gameOutputPath + '/' + gameName + '64')

         # Tar
                makeTar(targetOutputPath + '/' + gameName + '.tar', gameOutputPath, 'w')
                makeTar(targetOutputPath + '/' + gameName + '.tar.gz', gameOutputPath, 'w:gz')

           elif buildTarget == 'Mac':
         # Raw files
                os.makedirs(gameOutputPath)
                shutil.copytree(resourcesPath, gameOutputPath + '/Data')
                shutil.copy(binariesPath + '/Mac/FOnline', gameOutputPath + '/' + gameName)
                patchConfig(gameOutputPath + '/' + gameName)

         # Tar
                makeTar(targetOutputPath + '/' + gameName + '.tar', gameOutputPath, 'w')
                makeTar(targetOutputPath + '/' + gameName + '.tar.gz', gameOutputPath, 'w:gz')

           elif buildTarget == 'Android':
                shutil.copytree(binariesPath + '/Android', gameOutputPath)
                patchConfig(gameOutputPath + '/libs/armeabi-v7a/libFOnline.so')
         # No x86 build
         # patchConfig(gameOutputPath + '/libs/x86/libFOnline.so')
                patchFile(gameOutputPath + '/res/values/strings.xml', 'FOnline', gameName)

         # Icons
                logo.resize((48, 48)).save(gameOutputPath + '/res/drawable-mdpi/ic_launcher.png', 'png')
                logo.resize((72, 72)).save(gameOutputPath + '/res/drawable-hdpi/ic_launcher.png', 'png')
                logo.resize((96, 96)).save(gameOutputPath + '/res/drawable-xhdpi/ic_launcher.png', 'png')
                logo.resize((144, 144)).save(gameOutputPath + '/res/drawable-xxhdpi/ic_launcher.png', 'png')

         # Bundle
                shutil.copytree(resourcesPath, gameOutputPath + '/assets')
                with open(gameOutputPath + '/assets/FilesTree.txt', 'wb') as f:
                        f.write('\n'.join(os.listdir(resourcesPath)))

         # Pack
                antPath = os.path.abspath(os.path.join(curPath, '_ant', 'bin', 'ant.bat'))
                r = subprocess.call([antPath, '-f', gameOutputPath, 'debug'], shell = True)
                assert r == 0
                shutil.copy(gameOutputPath + '/bin/SDLActivity-debug.apk', targetOutputPath + '/' + gameName + '.apk')

           elif buildTarget == 'Web':
         # Release version
                os.makedirs(gameOutputPath)

                if os.path.isfile(os.path.join(outputPath, 'WebIndex.html')):
                        shutil.copy(os.path.join(outputPath, 'WebIndex.html'), os.path.join(gameOutputPath, 'index.html'))
                else:
                        shutil.copy(os.path.join(binariesPath, 'Web', 'index.html'), os.path.join(gameOutputPath, 'index.html'))
                shutil.copy(binariesPath + '/Web/FOnline.js', os.path.join(gameOutputPath, 'FOnline.js'))
                shutil.copy(binariesPath + '/Web/FOnline.wasm', os.path.join(gameOutputPath, 'FOnline.wasm'))
                shutil.copy(binariesPath + '/Web/SimpleWebServer.py', os.path.join(gameOutputPath, 'SimpleWebServer.py'))
                patchConfig(gameOutputPath + '/FOnline.wasm')

         # Debug version
                shutil.copy(binariesPath + '/Web/index.html', gameOutputPath + '/debug.html')
                shutil.copy(binariesPath + '/Web/FOnline_Debug.js', gameOutputPath + '/FOnline_Debug.js')
                shutil.copy(binariesPath + '/Web/FOnline_Debug.js.mem', gameOutputPath + '/FOnline_Debug.js.mem')
                patchConfig(gameOutputPath + '/FOnline_Debug.js.mem')
                patchFile(gameOutputPath + '/debug.html', 'FOnline.js', 'FOnline_Debug.js')

         # Generate resources
                r = subprocess.call(['python', os.environ['EMSCRIPTEN'] + '/tools/file_packager.py', \
                                'Resources.data', '--preload', resourcesPath + '@/Data', '--js-output=Resources.js'], shell = True)
                assert r == 0
                shutil.move('Resources.js', gameOutputPath + '/Resources.js')
                shutil.move('Resources.data', gameOutputPath + '/Resources.data')

         # Patch *.html
                patchFile(gameOutputPath + '/index.html', '$TITLE$', gameName)
                patchFile(gameOutputPath + '/index.html', '$LOADING$', gameName)
                patchFile(gameOutputPath + '/debug.html', '$TITLE$', gameName + ' Debug')
                patchFile(gameOutputPath + '/debug.html', '$LOADING$', gameName + ' Debug')

         # Favicon
                logo.save(os.path.join(gameOutputPath, 'favicon.ico'), 'ico')

           else:
                assert False, 'Unknown build target'*/
    }

    bool BuildWindows()
    {
        // Raw files

        /*os.makedirs(gameOutputPath)
           shutil.copytree(resourcesPath, gameOutputPath + '/Data')
           shutil.copy(binariesPath + '/Windows/FOnline.exe', gameOutputPath + '/' + gameName + '.exe')
           shutil.copy(binariesPath + '/Windows/FOnline.pdb', gameOutputPath + '/' + gameName + '.pdb')
           patchConfig(gameOutputPath + '/' + gameName + '.exe')

         # Patch icon
           if os.name == 'nt':
                icoPath = os.path.join(gameOutputPath, 'Windows.ico')
                logo.save(icoPath, 'ico')
                resHackPath = os.path.abspath(os.path.join(curPath, '_other', 'ReplaceVistaIcon.exe'))
                r = subprocess.call([resHackPath, gameOutputPath + '/' + gameName + '.exe', icoPath], shell = True)
                os.remove(icoPath)
                assert r == 0

         # Zip
           makeZip(targetOutputPath + '/' + gameName + '.zip', gameOutputPath)

         # MSI Installer
           sys.path.insert(0, os.path.join(curPath, '_msicreator'))
           import createmsi
           import uuid

           msiConfig = """ \
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
           }""" % (uuid.uuid3(uuid.NAMESPACE_OID, gameName), '1.0.0', \
                        gameName, 'Dream', gameName, gameName, 'The game', \
                        gameName, 'License.rtf', 'false', 32, \
                        gameName, gameName, 'MMORPG', 'disallow', gameName)

           try:
                cwd = os.getcwd()
                wixBinPath = os.path.abspath(os.path.join(curPath, '_wix'))
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

           except Exception, e:
                print str(e)
           finally:
                os.chdir(cwd)

         # Update binaries
           binPath = resourcesPath + '/../Binaries'
           if not os.path.exists(binPath):
                os.makedirs(binPath)
           shutil.copy(gameOutputPath + '/' + gameName + '.exe', binPath + '/' + gameName + '.exe')
           shutil.copy(gameOutputPath + '/' + gameName + '.pdb', binPath + '/' + gameName + '.pdb')*/
        return false;
    }

public:
    bool BuildAll()
    {
        try
        {
            // ...
            return true;
        }
        catch( const fo_exception& ex )
        {
            // ...
            return false;
        }
    }
};

BuildSystem Fabric::CreateDefaultBuildSystem()
{
    return std::make_shared< DefaultBuildSystem >();
}
