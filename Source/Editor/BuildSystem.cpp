#include "BuildSystem.h"
#include "EffectBaker.h"
#include "FileUtils.h"
#include "ImageBaker.h"
#include "Log.h"
#include "ModelBaker.h"
#include "Settings.h"
#include "StringUtils.h"

#include "minizip/zip.h"

class BuildSystemImpl : public IBuildSystem
{
public:
    BuildSystemImpl();
    ~BuildSystemImpl() override;
    bool BuildAll() override;

private:
    bool GenerateResources(StrVec* resource_names);
    bool BuildTarget(string target);
    bool BuildWindows();
};

BuildSystem IBuildSystem::Create()
{
    return std::make_shared<BuildSystemImpl>();
}

BuildSystemImpl::BuildSystemImpl()
{
}

BuildSystemImpl ::~BuildSystemImpl()
{
}

bool BuildSystemImpl::BuildAll()
{
    try
    {
        return true;
    }
    catch (const fo_exception&)
    {
        return false;
    }
}

bool BuildSystemImpl::GenerateResources(StrVec* resource_names)
{
    // Generate resources
    bool something_changed = false;
    StrSet update_file_names;

    for (const string& project_path : ProjectFiles)
    {
        StrVec dummy_vec;
        StrVec check_dirs;
        File::GetFolderFileNames(project_path, true, "", dummy_vec, nullptr, &check_dirs);
        check_dirs.insert(check_dirs.begin(), "");

        for (const string& check_dir : check_dirs)
        {
            if (!_str(project_path + check_dir).extractLastDir().compareIgnoreCase("Resources"))
                continue;

            string resources_root = project_path + check_dir;
            FindDataVec resources_dirs;
            File::GetFolderFileNames(resources_root, false, "", dummy_vec, nullptr, nullptr, &resources_dirs);
            for (const FindData& resource_dir : resources_dirs)
            {
                const string& res_name = resource_dir.FileName;
                FileCollection resources("", resources_root + res_name + "/");
                if (!resources.GetFilesCount())
                    continue;

                if (res_name.find("_Raw") == string::npos)
                {
                    string res_name_zip = (string)res_name + ".zip";
                    string zip_path = (string) "Update/" + res_name_zip;
                    bool skip_making_zip = true;

                    File::CreateDirectoryTree(zip_path);

                    // Check if file available
                    File zip_file;
                    if (!zip_file.LoadFile(zip_path, true))
                        skip_making_zip = false;

                    // Test consistency
                    if (skip_making_zip)
                    {
                        zipFile zip = zipOpen(zip_path.c_str(), APPEND_STATUS_ADDINZIP);
                        if (!zip)
                            skip_making_zip = false;
                        else
                            zipClose(zip, nullptr);
                    }

                    // Check timestamps of inner resources
                    while (resources.IsNextFile())
                    {
                        string relative_path;
                        File& file = resources.GetNextFile(nullptr, nullptr, &relative_path, true);
                        if (resource_names)
                            resource_names->push_back(relative_path);

                        if (skip_making_zip && file.GetWriteTime() > zip_file.GetWriteTime())
                            skip_making_zip = false;
                    }

                    // Make zip
                    if (!skip_making_zip)
                    {
                        WriteLog("Pack resource '{}', files {}...\n", res_name, resources.GetFilesCount());

                        zipFile zip = zipOpen((zip_path + ".tmp").c_str(), APPEND_STATUS_CREATE);
                        if (zip)
                        {
                            resources.ResetCounter();

                            map<string, UCharVec> baked_files;
                            {
                                ImageBaker image_baker(resources);
                                ModelBaker model_baker(resources);
                                EffectBaker effect_baker(resources);
                                image_baker.AutoBakeImages();
                                model_baker.AutoBakeModels();
                                effect_baker.AutoBakeEffects();
                                image_baker.FillBakedFiles(baked_files);
                                model_baker.FillBakedFiles(baked_files);
                                effect_baker.FillBakedFiles(baked_files);
                            }

                            // Fill other files
                            resources.ResetCounter();
                            while (resources.IsNextFile())
                            {
                                string relative_path;
                                resources.GetNextFile(nullptr, nullptr, &relative_path, true);
                                if (baked_files.count(relative_path))
                                    continue;

                                File& file = resources.GetCurFile();
                                UCharVec data;
                                DataWriter writer {data};
                                writer.WritePtr(file.GetBuf(), file.GetFsize());
                                baked_files.emplace(relative_path, std::move(data));
                            }

                            // Write to zip
                            for (const auto& kv : baked_files)
                            {
                                zip_fileinfo zfi;
                                memzero(&zfi, sizeof(zfi));
                                if (zipOpenNewFileInZip(zip, kv.first.c_str(), &zfi, nullptr, 0, nullptr, 0, nullptr,
                                        Z_DEFLATED, Z_BEST_SPEED) == ZIP_OK)
                                {
                                    if (zipWriteInFileInZip(zip, &kv.second[0], static_cast<uint>(kv.second.size())))
                                        throw fo_exception("Can't write file in zip file", kv.first, zip_path);

                                    zipCloseFileInZip(zip);
                                }
                                else
                                {
                                    throw fo_exception("Can't open file in zip file", kv.first, zip_path);
                                }
                            }

                            zipClose(zip, nullptr);

                            File::DeleteFile(zip_path);
                            if (!File::RenameFile(zip_path + ".tmp", zip_path))
                                throw fo_exception("Can't rename file", zip_path + ".tmp", zip_path);

                            something_changed = true;
                        }
                        else
                        {
                            WriteLog("Can't open zip file '{}'.\n", zip_path);
                        }
                    }

                    update_file_names.insert(res_name_zip);
                }
                else
                {
                    bool log_shown = false;
                    while (resources.IsNextFile())
                    {
                        string path, relative_path;
                        File& file = resources.GetNextFile(nullptr, &path, &relative_path);
                        string fname = "Update/" + relative_path;
                        File update_file;
                        if (!update_file.LoadFile(fname, true) || file.GetWriteTime() > update_file.GetWriteTime())
                        {
                            if (!log_shown)
                            {
                                log_shown = true;
                                WriteLog("Copy resource '{}', files {}...\n", res_name, resources.GetFilesCount());
                            }

                            File* converted_file = nullptr; // Convert(fname, file);
                            if (!converted_file)
                            {
                                WriteLog("File '{}' conversation error.\n", fname);
                                continue;
                            }
                            converted_file->SaveFile(fname);
                            if (converted_file != &file)
                                delete converted_file;

                            something_changed = true;
                        }

                        if (resource_names)
                        {
                            string ext = _str(fname).getFileExtension();
                            if (ext == "zip" || ext == "bos" || ext == "dat")
                            {
                                DataFile* inner = DataFile::TryLoad(path);
                                if (inner)
                                {
                                    StrVec inner_files;
                                    inner->GetFileNames("", true, "", inner_files);
                                    resource_names->insert(
                                        resource_names->end(), inner_files.begin(), inner_files.end());
                                }
                                else
                                {
                                    WriteLog("Can't read data file '{}'.\n", path);
                                }
                            }
                        }

                        update_file_names.insert(relative_path);
                    }
                }
            }
        }
    }

    // Delete unnecessary update files
    FileCollection update_files("", "Update/");
    while (update_files.IsNextFile())
    {
        string path, relative_path;
        update_files.GetNextFile(nullptr, &path, &relative_path, true);
        if (!update_file_names.count(relative_path))
        {
            File::DeleteFile(path);
            something_changed = true;
        }
    }

    return something_changed;
}

bool BuildSystemImpl::BuildTarget(string target)
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
                    r = subprocess.call([resHackPath, gameOutputPath + '/' + gameName + '.exe', icoPath], shell =
     True) os.remove(icoPath) assert r == 0

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
                    shutil.copy(os.path.join(outputPath, 'WebIndex.html'), os.path.join(gameOutputPath,
     'index.html')) else: shutil.copy(os.path.join(binariesPath, 'Web', 'index.html'), os.path.join(gameOutputPath,
     'index.html')) shutil.copy(binariesPath + '/Web/FOnline.js', os.path.join(gameOutputPath, 'FOnline.js'))
            shutil.copy(binariesPath + '/Web/FOnline.wasm', os.path.join(gameOutputPath, 'FOnline.wasm'))
            shutil.copy(binariesPath + '/Web/SimpleWebServer.py', os.path.join(gameOutputPath,
     'SimpleWebServer.py')) patchConfig(gameOutputPath + '/FOnline.wasm')

     # Debug version
            shutil.copy(binariesPath + '/Web/index.html', gameOutputPath + '/debug.html')
            shutil.copy(binariesPath + '/Web/FOnline_Debug.js', gameOutputPath + '/FOnline_Debug.js')
            shutil.copy(binariesPath + '/Web/FOnline_Debug.js.mem', gameOutputPath + '/FOnline_Debug.js.mem')
            patchConfig(gameOutputPath + '/FOnline_Debug.js.mem')
            patchFile(gameOutputPath + '/debug.html', 'FOnline.js', 'FOnline_Debug.js')

     # Generate resources
            r = subprocess.call(['python', os.environ['EMSCRIPTEN'] + '/tools/file_packager.py', \
                            'Resources.data', '--preload', resourcesPath + '@/Data', '--js-output=Resources.js'],
     shell = True) assert r == 0 shutil.move('Resources.js', gameOutputPath + '/Resources.js')
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

    return false;
}

bool BuildSystemImpl::BuildWindows()
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
