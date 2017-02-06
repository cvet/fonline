#!/bin/bash

export SOURCE_FULL_PATH=$(cd $FO_SOURCE; pwd)

mkdir $FO_BUILD_DEST
cd $FO_BUILD_DEST
mkdir ios
cd ios

/Applications/CMake.app/Contents/bin/cmake -G Xcode -C "$SOURCE_FULL_PATH/BuildScripts/ios.cache.cmake" $SOURCE_FULL_PATH/Source
/Applications/CMake.app/Contents/bin/cmake --build . --config RelWithDebInfo --target FOnline

#xcodebuild -project "${PROJECT}/${TARGET}_${PLATFORM}/Build/xcode/Unity-iPhone.xcodeproj" -configuration Release -scheme "Unity-iPhone" clean archive -archivePath "${PROJECT}/${TARGET}_${PLATFORM}/Build/xcode/build/Unity-iPhone"
#xcodebuild -exportArchive -archivePath "${PROJECT}/${TARGET}_${PLATFORM}/Build/xcode/build/Unity-iPhone.xcarchive" -exportFormat ipa -exportPath "${PROJECT}/${TARGET}_${PLATFORM}/Build/Unity-iPhone.ipa" -exportProvisioningProfile "Warfair"
