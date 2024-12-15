#!/bin/bash -e

echo "Setup environment"

[ "$FO_PROJECT_ROOT" ] || export FO_PROJECT_ROOT="$(pwd)"
[ "$FO_ENGINE_ROOT" ] || export FO_ENGINE_ROOT="$(cd $(dirname ${BASH_SOURCE[0]})/../ && pwd)"
[ "$FO_WORKSPACE" ] || export FO_WORKSPACE=$PWD/Workspace
[ "$FO_OUTPUT" ] || export FO_OUTPUT=$FO_WORKSPACE/output

export FO_PROJECT_ROOT=$(cd $FO_PROJECT_ROOT; pwd)
export FO_ENGINE_ROOT=$(cd $FO_ENGINE_ROOT; pwd)
export FO_WORKSPACE=$(mkdir -p $FO_WORKSPACE; cd $FO_WORKSPACE; pwd)
export FO_OUTPUT=$(mkdir -p $FO_OUTPUT; cd $FO_OUTPUT; pwd)
export EMSCRIPTEN_VERSION="3.1.67"
export ANDROID_NDK_VERSION="android-ndk-r27b"
export ANDROID_SDK_VERSION="tools_r25.2.3"
export ANDROID_NATIVE_API_LEVEL_NUMBER=23

if test -d "/usr/lib/android-sdk"; then
    export ANDROID_HOME="/usr/lib/android-sdk"
    export ANDROID_SDK_ROOT="/usr/lib/android-sdk"
fi
if test -d "$FO_WORKSPACE/android-ndk"; then
    export ANDROID_NDK_ROOT="$FO_WORKSPACE/android-ndk"
fi
if test -d "$FO_WORKSPACE/dotnet"; then
    export FO_DOTNET_RUNTIME="$FO_WORKSPACE/dotnet"
fi

echo "- FO_PROJECT_ROOT=$FO_PROJECT_ROOT"
echo "- FO_ENGINE_ROOT=$FO_ENGINE_ROOT"
echo "- FO_WORKSPACE=$FO_WORKSPACE"
echo "- FO_OUTPUT=$FO_OUTPUT"
echo "- EMSCRIPTEN_VERSION=$EMSCRIPTEN_VERSION"
echo "- ANDROID_HOME=$ANDROID_HOME"
echo "- ANDROID_SDK_ROOT=$ANDROID_SDK_ROOT"
echo "- ANDROID_NDK_VERSION=$ANDROID_NDK_VERSION"
echo "- ANDROID_SDK_VERSION=$ANDROID_SDK_VERSION"
echo "- ANDROID_NATIVE_API_LEVEL_NUMBER=$ANDROID_NATIVE_API_LEVEL_NUMBER"
echo "- ANDROID_NDK_ROOT=$ANDROID_NDK_ROOT"
echo "- FO_DOTNET_RUNTIME=$FO_DOTNET_RUNTIME"

cd $FO_WORKSPACE
