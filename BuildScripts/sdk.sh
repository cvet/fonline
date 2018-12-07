#!/bin/bash -ex

[ "$FO_ROOT" ] || { echo "FO_ROOT variable is not set"; exit 1; }
[ "$FO_BUILD_DEST" ] || { echo "FO_BUILD_DEST variable is not set"; exit 1; }

export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)

export MONO_ZIP="mono-5.16.0.220-x64-0.zip"
export VSCODE_ZIP="vscode-1.29.1.zip"
export VSCODE_EXTENSIONS_ZIP="vscode-extensions-1.zip"
export MONO_URL="https://www.dropbox.com/s/zqkqfynqs5q0nds/$MONO_ZIP"
export VSCODE_URL="https://www.dropbox.com/s/eheuybsdkll6nhp/$VSCODE_ZIP"
export VSCODE_EXTENSIONS_URL="https://www.dropbox.com/s/w049fgcpa0dk7jj/$VSCODE_EXTENSIONS_ZIP"

if [[ -z "$FO_INSTALL_PACKAGES" ]]; then
	sudo apt-get -y update || true
	sudo apt-get -y install unzip
	sudo apt-get -y install wput
fi

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST
mkdir -p sdk
cd sdk

wget "$MONO_URL"
wget "$VSCODE_URL"
wget "$VSCODE_EXTENSIONS_URL"

unzip "$MONO_ZIP" -d "./Binaries/"
unzip "$VSCODE_ZIP" -d "./Binaries/VSCode/"
unzip "$VSCODE_EXTENSIONS_ZIP" -d "./Binaries/VSCode/VSCode-win32-x64/data/extensions/"
cp -r "./Binaries/VSCode/VSCode-win32-x64/data/extensions" "./Binaries/VSCode/VSCode-linux-x64/data/extensions"
#cp -r "./Binaries/VSCode/fonline-vscode-extension" "./Binaries/VSCode/VSCode-win32-x64/data/extensions/"
#cp -r "./Binaries/VSCode/fonline-vscode-extension" "./Binaries/VSCode/VSCode-linux-x64/data/extensions/"
cp -r "./Binaries/Mono/lib/mono/4.5/*.dll" "./Modules/Core/Resources/Mono/Assemblies"
cp -r "./Binaries/Mono/lib/mono/4.5/Facades/*.dll" "./Modules/Core/Resources/Mono/Assemblies"

#rm -rf "./Binaries/VSCode/fonline-vscode-extension" 
rm -rf "$MONO_ZIP"
rm -rf "$VSCODE_ZIP"
rm -rf "$VSCODE_EXTENSIONS_ZIP"

cd ../
cd ../
