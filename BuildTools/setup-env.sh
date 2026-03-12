#!/bin/bash -e

CUR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_BIN="$(command -v python3 || command -v python || true)"

if [[ -z "$PYTHON_BIN" ]]; then
	echo "Python not found" >&2
	exit 1
fi

eval "$($PYTHON_BIN "$CUR_DIR/shared_buildtools.py" env --shell bash)"

echo "Setup environment"
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
echo "- FO_DOTNET_RUNTIME_ROOT=$FO_DOTNET_RUNTIME_ROOT"
echo "- FO_IOS_SDK=$FO_IOS_SDK"
