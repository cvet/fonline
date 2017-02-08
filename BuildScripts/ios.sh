#!/bin/bash -ex

[ "$FO_SOURCE" ] || { echo "FO_SOURCE is empty"; exit 1; }
[ "$FO_BUILD_DEST" ] || { echo "FO_BUILD_DEST is empty"; exit 1; }

export SOURCE_FULL_PATH=$(cd $FO_SOURCE; pwd)

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST
mkdir -p ios
cd ios
mkdir -p iOS
rm -rf iOS/*

/Applications/CMake.app/Contents/bin/cmake -G Xcode -C "$SOURCE_FULL_PATH/BuildScripts/ios.cache.cmake" "$SOURCE_FULL_PATH/Source"
/Applications/CMake.app/Contents/bin/cmake --build . --config Release --target FOnline

#xcodebuild -project "${PROJECT}/${TARGET}_${PLATFORM}/Build/xcode/Unity-iPhone.xcodeproj" -configuration Release -scheme "Unity-iPhone" clean archive -archivePath "${PROJECT}/${TARGET}_${PLATFORM}/Build/xcode/build/Unity-iPhone"
#xcodebuild -exportArchive -archivePath "${PROJECT}/${TARGET}_${PLATFORM}/Build/xcode/build/Unity-iPhone.xcarchive" -exportFormat ipa -exportPath "${PROJECT}/${TARGET}_${PLATFORM}/Build/Unity-iPhone.ipa" -exportProvisioningProfile "Warfair"

if [ -n "$FO_FTP_DEST" ]; then
	curl -T iOS/FOnline -u $FO_FTP_USER --ftp-create-dirs ftp://$FO_FTP_DEST/Client/iOS/
fi

if [ -n "$FO_COPY_DEST" ]; then
	cp -r iOS "$FO_COPY_DEST/Client"
fi
