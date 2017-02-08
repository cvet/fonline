#!/bin/bash -e -x

[ "$FO_SOURCE" ] || { echo "FO_SOURCE is empty"; exit 1; }
[ "$FO_BUILD_DEST" ] || { echo "FO_BUILD_DEST is empty"; exit 1; }

export SOURCE_FULL_PATH=$(cd $FO_SOURCE; pwd)

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST
mkdir -p mac
cd mac
mkdir -p Mac
rm -rf Mac/*

/Applications/CMake.app/Contents/bin/cmake -G Xcode "$SOURCE_FULL_PATH/Source"
/Applications/CMake.app/Contents/bin/cmake --build . --config RelWithDebInfo --target FOnline

if [ -n "$FO_FTP_DEST" ]; then
	curl -T Mac/FOnline -u $FO_FTP_USER --ftp-create-dirs ftp://$FO_FTP_DEST/Client/Mac/
fi

if [ -n "$FO_COPY_DEST" ]; then
	cp -r Mac "$FO_COPY_DEST/Client"
fi
