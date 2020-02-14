#!/bin/bash -e

echo "Setup environment"

[ "$FO_ROOT" ] || { [[ -e CMakeLists.txt ]] && { export FO_ROOT=. || true ;} ;} || export FO_ROOT=../
[ "$FO_WORKSPACE" ] || export FO_WORKSPACE=Workspace
[ "$FO_INSTALL_PACKAGES" ] || export FO_INSTALL_PACKAGES=1

export FO_WORKSPACE_VERSION="11.02.2020"
export FO_ROOT=$(cd $FO_ROOT; pwd)
export FO_WORKSPACE=$(mkdir -p $FO_WORKSPACE; cd $FO_WORKSPACE; pwd)
export FO_OUTPUT="$FO_WORKSPACE/output"
export EMSCRIPTEN_VERSION="1.39.4"
export ANDROID_HOME="/usr/lib/android-sdk"
export ANDROID_NDK_VERSION="android-ndk-r18b"
export ANDROID_SDK_VERSION="tools_r25.2.3"
export ANDROID_NATIVE_API_LEVEL_NUMBER=21

echo "FOnline related environment variables:"
echo "- FO_WORKSPACE_VERSION=$FO_WORKSPACE_VERSION"
echo "- FO_ROOT=$FO_ROOT"
echo "- FO_WORKSPACE=$FO_WORKSPACE"
echo "- FO_OUTPUT=$FO_OUTPUT"
echo "- FO_INSTALL_PACKAGES=$FO_INSTALL_PACKAGES"
echo "- EMSCRIPTEN_VERSION=$EMSCRIPTEN_VERSION"
echo "- ANDROID_HOME=$ANDROID_HOME"
echo "- ANDROID_NDK_VERSION=$ANDROID_NDK_VERSION"
echo "- ANDROID_SDK_VERSION=$ANDROID_SDK_VERSION"
echo "- ANDROID_NATIVE_API_LEVEL_NUMBER=$ANDROID_NATIVE_API_LEVEL_NUMBER"
