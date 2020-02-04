#!/bin/bash -e

[ "$FO_ROOT" ] || { [[ -e CMakeLists.txt ]] && { export FO_ROOT=. || true ;} ;} || export FO_ROOT=../
[ "$FO_BUILD_DEST" ] || export FO_BUILD_DEST=Build

export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)

if [ ! -f "$FO_BUILD_DEST/FOnlineVersion.txt" ]; then
    echo "File '$FO_BUILD_DEST/FOnlineVersion.txt' not exists"
    exit 0
fi
FO_VERSION=`cat $FO_BUILD_DEST/FOnlineVersion.txt`

echo "Make FOnline SDK version: $FO_VERSION"
FO_SDK_PATH="FOnlineSDK"

echo "Make structure"
rm -rf "$FO_SDK_PATH"
mkdir "$FO_SDK_PATH"
mkdir "$FO_SDK_PATH/FOnlineData"
mkdir "$FO_SDK_PATH/FOnlineTemp"

echo "Copy placeholder"
cp -r "$ROOT_FULL_PATH/SdkPlaceholder/." "$FO_SDK_PATH/FOnlineData"

echo "Copy binaries"
cp -r "$FO_BUILD_DEST/Binaries/." "$FO_SDK_PATH/FOnlineData/Binaries"

echo "Create symlincs for editor at sdk root"
ln -s -r "$FO_SDK_PATH/FOnlineData/Binaries/Editor/FOnlineEditor.exe" "$FO_SDK_PATH/FOnlineEditor.exe.lnk"
ln -s -r "$FO_SDK_PATH/FOnlineData/Binaries/Editor/FOnlineEditor" "$FO_SDK_PATH/FOnlineEditor.lnk"

echo "Generate sdk config file"
echo "$FO_VERSION" > "$FO_SDK_PATH/FOnlineData/FOnlineVersion.txt"
echo "$FO_VERSION" > "$FO_SDK_PATH/FOnlineTemp/FOnlineVersion.txt"
rm -rf "$FO_SDK_PATH/FOnlineData/Binaries/FOnlineVersion.txt"

echo "FOnline SDK ready!"
