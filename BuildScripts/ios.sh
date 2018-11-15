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

pwd

/Applications/CMake.app/Contents/bin/cmake -G Xcode -C "$SOURCE_FULL_PATH/BuildScripts/ios.cache.cmake" "$SOURCE_FULL_PATH/Source"
#/Applications/CMake.app/Contents/bin/cmake --build . --config Release --target FOnline

#xcodebuild -project "${PROJECT}/${TARGET}_${PLATFORM}/Build/xcode/Unity-iPhone.xcodeproj" -configuration Release -scheme "Unity-iPhone" clean archive -archivePath "${PROJECT}/${TARGET}_${PLATFORM}/Build/xcode/build/Unity-iPhone"
#xcodebuild -exportArchive -archivePath "${PROJECT}/${TARGET}_${PLATFORM}/Build/xcode/build/Unity-iPhone.xcarchive" -exportFormat ipa -exportPath "${PROJECT}/${TARGET}_${PLATFORM}/Build/Unity-iPhone.ipa" -exportProvisioningProfile "Warfair"
