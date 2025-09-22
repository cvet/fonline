#!/bin/bash -e

echo "Setup environment"

[[ ! -z "$FO_PROJECT_ROOT" ]] || export FO_PROJECT_ROOT="$PWD"
[[ ! -z "$FO_ENGINE_ROOT" ]] || export FO_ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../ && pwd)"
[[ ! -z "$FO_WORKSPACE" ]] || export FO_WORKSPACE="$PWD/Workspace"
[[ ! -z "$FO_OUTPUT" ]] || export FO_OUTPUT="$FO_WORKSPACE/output"

export FO_PROJECT_ROOT="$(cd "$FO_PROJECT_ROOT" && pwd)"
export FO_ENGINE_ROOT="$(cd "$FO_ENGINE_ROOT" && pwd)"
export EMSCRIPTEN_VERSION="$(head -n 1 "$FO_ENGINE_ROOT/ThirdParty/emscripten")"
export ANDROID_NDK_VERSION="$(head -n 1 "$FO_ENGINE_ROOT/ThirdParty/android-ndk")"
export ANDROID_SDK_VERSION="$(head -n 1 "$FO_ENGINE_ROOT/ThirdParty/android-sdk")"
export ANDROID_NATIVE_API_LEVEL_NUMBER="$(head -n 1 "$FO_ENGINE_ROOT/ThirdParty/android-api")"
export FO_DOTNET_RUNTIME="$(head -n 1 "$FO_ENGINE_ROOT/ThirdParty/dotnet-runtime")"
export FO_IOS_SDK="$(head -n 1 "$FO_ENGINE_ROOT/ThirdParty/iOS-sdk")"

if [[ -d "/usr/lib/android-sdk" ]]; then
    export ANDROID_HOME="/usr/lib/android-sdk"
    export ANDROID_SDK_ROOT="/usr/lib/android-sdk"
fi
if [[ -d "$FO_WORKSPACE/android-ndk" ]]; then
    export ANDROID_NDK_ROOT="$FO_WORKSPACE/android-ndk"
fi
if [[ -d "$FO_WORKSPACE/dotnet/runtime" ]]; then
    export FO_DOTNET_RUNTIME_ROOT="$FO_WORKSPACE/dotnet/runtime"
fi

echo "- FO_PROJECT_ROOT=$FO_PROJECT_ROOT"
echo "- FO_ENGINE_ROOT=$FO_ENGINE_ROOT"
echo "- FO_WORKSPACE=$FO_WORKSPACE"
echo "- FO_OUTPUT=$FO_OUTPUT"
echo "- EMSCRIPTEN_VERSION=$EMSCRIPTEN_VERSION"
echo "- ANDROID_HOME/ANDROID_SDK_ROOT=$ANDROID_HOME"
echo "- ANDROID_NDK_VERSION=$ANDROID_NDK_VERSION"
echo "- ANDROID_SDK_VERSION=$ANDROID_SDK_VERSION"
echo "- ANDROID_NATIVE_API_LEVEL_NUMBER=$ANDROID_NATIVE_API_LEVEL_NUMBER"
echo "- ANDROID_NDK_ROOT=$ANDROID_NDK_ROOT"
echo "- FO_DOTNET_RUNTIME=$FO_DOTNET_RUNTIME"
echo "- FO_DOTNET_RUNTIME_ROOT=$FO_DOTNET_RUNTIME_ROOT"
echo "- FO_IOS_SDK=$FO_IOS_SDK"
