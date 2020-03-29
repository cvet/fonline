#!/bin/bash -e

if [ "$1" = "" ]; then
    echo "Provide at least one argument"
    exit 1
fi

echo "Prepare workspace"

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh
source $CUR_DIR/tools.sh

function install_common_packages()
{
    echo "Install common packages"
    sudo apt-get -qq -y update

    echo "Install clang-9"
    sudo apt-get -qq -y install clang-9
    echo "Install clang-format-9"
    sudo apt-get -qq -y install clang-format-9
    echo "Install build-essential"
    sudo apt-get -qq -y install build-essential
    echo "Install git"
    sudo apt-get -qq -y install git
    echo "Install cmake"
    sudo apt-get -qq -y install cmake
    echo "Install python"
    sudo apt-get -qq -y install python
    echo "Install wget"
    sudo apt-get -qq -y install wget
    echo "Install unzip"
    sudo apt-get -qq -y install unzip

    echo "Set clang-9 as default clang compiler"
    sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-9 100
    sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-9 100
}

function install_linux_packages()
{
    echo "Install Linux packages"
    sudo apt-get -qq -y update

    echo "Install libc++-dev"
    sudo apt-get -qq -y install libc++-dev
    echo "Install libc++abi-dev"
    sudo apt-get -qq -y install libc++abi-dev
    echo "Install binutils-dev"
    sudo apt-get -qq -y install binutils-dev
    echo "Install libx11-dev"
    sudo apt-get -qq -y install libx11-dev
    echo "Install freeglut3-dev"
    sudo apt-get -qq -y install freeglut3-dev
    echo "Install libssl-dev"
    sudo apt-get -qq -y install libssl-dev
    echo "Install libevent-dev"
    sudo apt-get -qq -y install libevent-dev
    echo "Install libxi-dev"
    sudo apt-get -qq -y install libxi-dev
}

function install_web_packages()
{
    echo "Install Web packages"
    sudo apt-get -qq -y update

    echo "Install nodejs"
    sudo apt-get -qq -y install nodejs
    echo "Install default-jre"
    sudo apt-get -qq -y install default-jre
}

function install_android_packages()
{
    echo "Install Android packages"
    sudo apt-get -qq -y update

    echo "Install android-sdk"
    sudo apt-get -qq -y install android-sdk
    echo "Install openjdk-8-jdk"
    sudo apt-get -qq -y install openjdk-8-jdk
    echo "Install ant"
    sudo apt-get -qq -y install ant
}

function install_osxcross_packages()
{
    echo "Install OSXCross packages"
    sudo apt-get -qq -y update

    echo "Install patch"
    sudo apt-get -qq -y install patch
    echo "Install lzma-dev"
    sudo apt-get -qq -y install lzma-dev
    echo "Install libxml2-dev"
    sudo apt-get -qq -y install libxml2-dev
    echo "Install llvm-dev"
    sudo apt-get -qq -y install llvm-dev
    echo "Install uuid-dev"
    sudo apt-get -qq -y install uuid-dev
}

function setup_osxcross()
{
    echo "Setup OSXCross"
    rm -rf osxcross
    git clone --depth 1 https://github.com/tpoechtrager/osxcross
    cd osxcross
    cp "$FO_ROOT/BuildTools/osxcross/MacOSX10.15.sdk.tar.bz2" "./tarballs"
    export UNATTENDED=1
    ./build.sh
}

function setup_ios_toolchain()
{
    echo "Setup iOS toolchain"
    rm -rf ios-toolchain
    mkdir ios-toolchain
    cd ios-toolchain
    git clone --depth 1 https://github.com/tpoechtrager/cctools-port
    # ./cctools-port/usage_examples/ios_toolchain/build.sh "$FO_ROOT/BuildTools/osxcross/iPhone.sdk.tar.bz2"
}

function setup_emscripten()
{
    echo "Setup Emscripten"
    rm -rf emsdk
    mkdir emsdk
    cp -r "$FO_ROOT/BuildTools/emsdk" "./"
    cd emsdk
    chmod +x ./emsdk
    ./emsdk update
    ./emsdk list
    ./emsdk install --build=Release --shallow $EMSCRIPTEN_VERSION
    ./emsdk activate --build=Release --embedded $EMSCRIPTEN_VERSION
}

function setup_android_ndk()
{
    echo "Setup Android NDK"
    rm -rf android-ndk
    rm -rf android-arm-toolchain
    rm -rf android-arm64-toolchain
    rm -rf android-x86-toolchain
    rm -rf "$ANDROID_NDK_VERSION-linux-x86_64.zip"
    rm -rf "$ANDROID_NDK_VERSION"

    echo "Download Android NDK"
    wget -qq "https://dl.google.com/android/repository/$ANDROID_NDK_VERSION-linux-x86_64.zip"
    echo "Unzip Android NDK"
    unzip -qq -o "$ANDROID_NDK_VERSION-linux-x86_64.zip" -d "./"
    mv "$ANDROID_NDK_VERSION" "android-ndk"
    rm -f "$ANDROID_NDK_VERSION-linux-x86_64.zip"

    echo "Generate Android toolchains"
    cd android-ndk/build/tools
    python make_standalone_toolchain.py --arch arm --api $ANDROID_NATIVE_API_LEVEL_NUMBER --install-dir ../../../android-arm-toolchain
    python make_standalone_toolchain.py --arch arm64 --api $ANDROID_NATIVE_API_LEVEL_NUMBER --install-dir ../../../android-arm64-toolchain
    python make_standalone_toolchain.py --arch x86 --api $ANDROID_NATIVE_API_LEVEL_NUMBER --install-dir ../../../android-x86-toolchain
}

function generate_compilation_env()
{
    echo "Generate compilation environment"
    FO_ROOT_WIN=`wsl_path_to_windows "$FO_ROOT"`
    rm -rf compilation-env
    mkdir compilation-env
    cd compilation-env
    cmake.exe -G "Visual Studio 16 2019" -A x64 -DFONLINE_BUILD_SERVER=1 -DFONLINE_BUILD_MAPPER=1 -DFONLINE_BUILD_BAKER=1 "$FO_ROOT_WIN"
    cmake.exe --build . --config Debug
}

function generate_vscode_js_toolset()
{
    echo "Generate VSCode js toolset"
    rm -rf vscode-js-toolset
    mkdir vscode-js-toolset
    cd vscode-js-toolset
    source $FO_WORKSPACE/emsdk/emsdk_env.sh
    cmake -G "Unix Makefiles" -C "$FO_ROOT/BuildTools/web.cache.cmake" -DFONLINE_BUILD_MAPPER=1 "$FO_ROOT"
    cmake --build . --config Release -- -j$(nproc)
}

function generate_vscode_native_toolset()
{
    echo "Generate VSCode native toolset"
    FO_ROOT_WIN=`wsl_path_to_windows "$FO_ROOT"`
    rm -rf vscode-native-toolset
    mkdir vscode-native-toolset
    cd vscode-native-toolset
    cmake.exe -G "Visual Studio 16 2019" -A x64 -DFONLINE_BUILD_CLIENT=0 -DFONLINE_BUILD_SERVER=1 -DFONLINE_BUILD_BAKER=1 "$FO_ROOT_WIN"
    cmake.exe --build . --config Release
}

function verify_workspace_part()
{
    if [ ! -f "$1-version.txt" ] || [ `cat $1-version.txt` != "$2" ]; then
        if [ ! -z `check_arg check` ]; then
            echo "Workspace is not ready"
            exit 10
        fi

        workspace_job()
        {
            eval $3
            cd $FO_WORKSPACE
            echo $2 > "$1-version.txt"
        }

        run_job "workspace_job $1 $2 $3"
    fi
}

verify_workspace_part common-packages 3 install_common_packages
wait_jobs
if [ ! -z `check_arg linux all` ]; then
    verify_workspace_part linux-packages 1 install_linux_packages
    wait_jobs
fi
if [ ! -z `check_arg web all` ]; then
    verify_workspace_part web-packages 1 install_web_packages
    wait_jobs
fi
if [ ! -z `check_arg android android-arm64 android-x86 all` ]; then
    verify_workspace_part android-packages 1 install_android_packages
    wait_jobs
fi
if [ ! -z `check_arg mac ios all` ]; then
    verify_workspace_part osxcross-packages 1 install_osxcross_packages
    wait_jobs
fi

if [ ! -z `check_arg mac all` ]; then
    verify_workspace_part osxcross 1 setup_osxcross
fi
if [ ! -z `check_arg ios all` ]; then
    verify_workspace_part ios-toolchain 1 setup_ios_toolchain
fi
if [ ! -z `check_arg web all` ]; then
    verify_workspace_part emscripten $EMSCRIPTEN_VERSION setup_emscripten
fi
if [ ! -z `check_arg android android-arm64 android-x86 all` ]; then
    verify_workspace_part android-ndk $ANDROID_NDK_VERSION setup_android_ndk
fi
wait_jobs

if [ ! -z `check_arg compilation all` ]; then
    verify_workspace_part compilation-env 1 generate_compilation_env
fi
if [ ! -z `check_arg vscode all` ]; then
    verify_workspace_part vscode-js-toolset 1 generate_vscode_js_toolset
    verify_workspace_part vscode-native-toolset 1 generate_vscode_native_toolset
fi
wait_jobs

echo "Workspace is ready!"
