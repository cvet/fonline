ECHO ON

IF [%FO_SOURCE%] == [] ECHO FO_SOURCE is empty & EXIT /B 1
IF [%FO_BUILD_DEST%] == [] ECHO FO_BUILD_DEST is empty & EXIT /B 1

PUSHD %CD%\%FO_SOURCE%
SET SOURCE_FULL_PATH=%CD%
POPD

MKDIR %FO_BUILD_DEST%
PUSHD %FO_BUILD_DEST%
IF EXIST windows\Client RD windows\Client /S /Q
IF EXIST windows\Server RD windows\Server /S /Q
IF EXIST windows\Mapper RD windows\Mapper /S /Q
IF EXIST windows\ASCompiler RD windows\ASCompiler /S /Q

MKDIR windows\x86
PUSHD windows\x86
cmake -G "Visual Studio 14 2015" "%SOURCE_FULL_PATH%\Source"
POPD

MKDIR windows\x64
PUSHD windows\x64
cmake -G "Visual Studio 14 2015 Win64" "%SOURCE_FULL_PATH%\Source"
POPD

cmake --build windows\x64 --config RelWithDebInfo --target FOnlineServer -DFO_AUTOBUILD=ON
cmake --build windows\x64 --config RelWithDebInfo --target Mapper -DFO_AUTOBUILD=ON
cmake --build windows\x64 --config RelWithDebInfo --target ASCompiler -DFO_AUTOBUILD=ON
cmake --build windows\x64 --config RelWithDebInfo --target FOnline -DFO_AUTOBUILD=ON
cmake --build windows\x86 --config RelWithDebInfo --target FOnlineServer -DFO_AUTOBUILD=ON
cmake --build windows\x86 --config RelWithDebInfo --target Mapper -DFO_AUTOBUILD=ON
cmake --build windows\x86 --config RelWithDebInfo --target ASCompiler -DFO_AUTOBUILD=ON
cmake --build windows\x86 --config RelWithDebInfo --target FOnline -DFO_AUTOBUILD=ON

IF DEFINED FO_FTP_DEST (
	PUSHD windows\Client
	"%SOURCE_FULL_PATH%\BuildScripts\wput\wput.exe" FOnline.exe ftp://%FO_FTP_USER%@%FO_FTP_DEST%/Client/Windows/
	"%SOURCE_FULL_PATH%\BuildScripts\wput\wput.exe" FOnline.pdb ftp://%FO_FTP_USER%@%FO_FTP_DEST%/Client/Windows/
	"%SOURCE_FULL_PATH%\BuildScripts\wput\wput.exe" FOnline64.exe ftp://%FO_FTP_USER%@%FO_FTP_DEST%/Client/Windows/
	"%SOURCE_FULL_PATH%\BuildScripts\wput\wput.exe" FOnline64.pdb ftp://%FO_FTP_USER%@%FO_FTP_DEST%/Client/Windows/
	POPD
	"%SOURCE_FULL_PATH%\BuildScripts\wput\wput.exe" windows/Server --basename=windows/ ftp://%FO_FTP_USER%@%FO_FTP_DEST%/
	"%SOURCE_FULL_PATH%\BuildScripts\wput\wput.exe" windows/Mapper --basename=windows/ ftp://%FO_FTP_USER%@%FO_FTP_DEST%/
	"%SOURCE_FULL_PATH%\BuildScripts\wput\wput.exe" windows/ASCompiler --basename=windows/ ftp://%FO_FTP_USER%@%FO_FTP_DEST%/
)

IF DEFINED FO_COPY_DEST (
	XCOPY /S /Y windows\Client "..\%FO_COPY_DEST%\Client\Windows\"
	XCOPY /S /Y windows\Server "..\%FO_COPY_DEST%\Server\"
	XCOPY /S /Y windows\Mapper "..\%FO_COPY_DEST%\Mapper\"
	XCOPY /S /Y windows\ASCompiler "..\%FO_COPY_DEST%\ASCompiler\"
)

POPD
