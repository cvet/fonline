PUSHD %CD%\%FO_SOURCE%
SET SOURCE_FULL_PATH=%CD%
POPD

MKDIR %FO_BUILD_DEST%
PUSHD %FO_BUILD_DEST%
MKDIR windows

MKDIR windows\x86
PUSHD windows\x86
cmake -G "Visual Studio 14 2015" "%SOURCE_FULL_PATH%\Source"
POPD

MKDIR windows\x64
PUSHD windows\x64
cmake -G "Visual Studio 14 2015 Win64" "%SOURCE_FULL_PATH%\Source"
POPD

cmake --build windows\x64 --config RelWithDebInfo --target FOnlineServer
cmake --build windows\x64 --config RelWithDebInfo --target Mapper
cmake --build windows\x64 --config RelWithDebInfo --target ASCompiler
cmake --build windows\x64 --config RelWithDebInfo --target FOnline
cmake --build windows\x86 --config RelWithDebInfo --target FOnlineServer
cmake --build windows\x86 --config RelWithDebInfo --target Mapper
cmake --build windows\x86 --config RelWithDebInfo --target ASCompiler
cmake --build windows\x86 --config RelWithDebInfo --target FOnline

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
