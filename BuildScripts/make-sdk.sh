#!/bin/bash -e

[ "$FO_ROOT" ] || { [[ -e CMakeLists.txt ]] && { export FO_ROOT=. || true ;} ;} || export FO_ROOT=../
[ "$FO_BUILD_DEST" ] || export FO_BUILD_DEST=Build

export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)

if [ "$GITHUB_SHA" ]; then
	FO_VERSION=${GITHUB_SHA:0:7}
elif [ -d "$FO_BUILD_DEST" ]; then
	if [ ! -f "${FO_BUILD_DEST}/FOnlineVersion.txt" ]; then
		echo "File '${FO_BUILD_DEST}/FOnlineVersion.txt' not exists"
		exit 1
	fi
	FO_VERSION=`cat ${FO_BUILD_DEST}/FOnlineVersion.txt`
else
	echo "Path '$FO_BUILD_DEST' not exists"
	exit 1
fi

echo "Make FOnline SDK version: ${FO_VERSION}"
FO_SDK_PATH="FOnlineSDK"

rm -rf "$FO_SDK_PATH"
mkdir "$FO_SDK_PATH"
mkdir "$FO_SDK_PATH/FOnlineData"
mkdir "$FO_SDK_PATH/FOnlineTemp"
cp -r "$ROOT_FULL_PATH/SdkPlaceholder/." "$FO_SDK_PATH/FOnlineData"

if [ "$GITHUB_SHA" ]; then
	cp -r "$ROOT_FULL_PATH/binaries-windows32/." "$FO_SDK_PATH/FOnlineData"
	cp -r "$ROOT_FULL_PATH/binaries-windows64/." "$FO_SDK_PATH/FOnlineData"
	cp -r "$ROOT_FULL_PATH/binaries-linux/." "$FO_SDK_PATH/FOnlineData"
	cp -r "$ROOT_FULL_PATH/binaries-android/." "$FO_SDK_PATH/FOnlineData"
	cp -r "$ROOT_FULL_PATH/binaries-web/." "$FO_SDK_PATH/FOnlineData"
	cp -r "$ROOT_FULL_PATH/binaries-mac/." "$FO_SDK_PATH/FOnlineData"
	cp -r "$ROOT_FULL_PATH/binaries-ios/." "$FO_SDK_PATH/FOnlineData"
else
	cp -r "$FO_BUILD_DEST/Binaries/." "$FO_SDK_PATH/FOnlineData/Binaries"
fi

cp -r "$FO_SDK_PATH/FOnlineData/Binaries/Editor/." "$FO_SDK_PATH"
rm -rf "$FO_SDK_PATH/FOnlineData/Binaries/Editor"
echo "$FO_VERSION" > "$FO_SDK_PATH/FOnlineData/FOnlineVersion.txt"
echo "$FO_VERSION" > "$FO_SDK_PATH/FOnlineTemp/FOnlineVersion.txt"

echo "FOnline SDK ready!"
