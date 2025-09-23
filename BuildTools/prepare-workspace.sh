#!/bin/bash -e

if [[ -z $1 ]]; then
    echo "Provide at least one argument"
    exit 1
fi

echo "Prepare workspace"

CUR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$CUR_DIR/setup-env.sh"
source "$CUR_DIR/internal-tools.sh"

mkdir -p "$FO_WORKSPACE"
mkdir -p "$FO_OUTPUT"
pushd "$FO_WORKSPACE"

# All packages:
# clang clang-format build-essential git cmake python3 wget unzip binutils-dev libc++-dev libc++abi-dev libx11-dev freeglut3-dev libssl-dev libevent-dev libxi-dev nodejs default-jre android-sdk openjdk-8-jdk ant

PACKAGE_MANAGER="sudo apt-get -qq -y -o DPkg::Lock::Timeout=-1"

function install_common_packages()
{
    echo "Install common packages"
    $PACKAGE_MANAGER update

    echo "Install clang"
    $PACKAGE_MANAGER install clang
    echo "Install clang-format"
    $PACKAGE_MANAGER install clang-format
    echo "Install build-essential"
    $PACKAGE_MANAGER install build-essential
    echo "Install git"
    $PACKAGE_MANAGER install git
    echo "Install cmake"
    $PACKAGE_MANAGER install cmake
    echo "Install python3"
    $PACKAGE_MANAGER install python3
    echo "Install wget"
    $PACKAGE_MANAGER install wget
    echo "Install unzip"
    $PACKAGE_MANAGER install unzip
    echo "Install binutils-dev"
    $PACKAGE_MANAGER install binutils-dev
}

function install_linux_packages()
{
    echo "Install Linux packages"
    $PACKAGE_MANAGER update

    echo "Install libc++-dev"
    $PACKAGE_MANAGER install libc++-dev
    echo "Install libc++abi-dev"
    $PACKAGE_MANAGER install libc++abi-dev
    echo "Install libx11-dev"
    $PACKAGE_MANAGER install libx11-dev
    echo "Install libxcursor-dev"
    $PACKAGE_MANAGER install libxcursor-dev
    echo "Install libxrandr-dev"
    $PACKAGE_MANAGER install libxrandr-dev
    echo "Install libxss-dev"
    $PACKAGE_MANAGER install libxss-dev
    echo "Install libjack-dev"
    $PACKAGE_MANAGER install libjack-dev
    echo "Install libpulse-dev"
    $PACKAGE_MANAGER install libpulse-dev
    echo "Install libasound-dev"
    $PACKAGE_MANAGER install libasound-dev
    echo "Install freeglut3-dev"
    $PACKAGE_MANAGER install freeglut3-dev
    echo "Install libssl-dev"
    $PACKAGE_MANAGER install libssl-dev
    echo "Install libevent-dev"
    $PACKAGE_MANAGER install libevent-dev
    echo "Install libxi-dev"
    $PACKAGE_MANAGER install libxi-dev
    echo "Install libzstd-dev"
    $PACKAGE_MANAGER install libzstd-dev
}

function install_web_packages()
{
    echo "Install Web packages"
    $PACKAGE_MANAGER update

    echo "Install nodejs"
    $PACKAGE_MANAGER install nodejs
    echo "Install default-jre"
    $PACKAGE_MANAGER install default-jre
}

function install_android_packages()
{
    echo "Install Android packages"
    $PACKAGE_MANAGER update

    echo "Install android-sdk"
    $PACKAGE_MANAGER install android-sdk
    echo "Install openjdk-8-jdk"
    $PACKAGE_MANAGER install openjdk-8-jdk
    echo "Install ant"
    $PACKAGE_MANAGER install ant
}

function setup_emscripten()
{
    echo "Setup Emscripten"
    rm -rf emsdk
    git clone https://github.com/emscripten-core/emsdk.git
    cd emsdk
    ./emsdk list
    ./emsdk install --build=Release --shallow $EMSCRIPTEN_VERSION
    ./emsdk activate --build=Release $EMSCRIPTEN_VERSION
}

function setup_android_ndk()
{
    echo "Setup Android NDK"
    rm -rf android-ndk
    rm -rf "$ANDROID_NDK_VERSION-linux.zip"
    rm -rf "$ANDROID_NDK_VERSION"

    echo "Download Android NDK"
    wget -qq "https://dl.google.com/android/repository/$ANDROID_NDK_VERSION-linux.zip"
    echo "Unzip Android NDK"
    unzip -qq -o "$ANDROID_NDK_VERSION-linux.zip" -d "./"
    mv "$ANDROID_NDK_VERSION" "android-ndk"
    rm -f "$ANDROID_NDK_VERSION-linux.zip"
}

function setup_toolset()
{
    echo "Setup Toolset"

    export CC=/usr/bin/clang
    export CXX=/usr/bin/clang++

    rm -rf build-linux-toolset
    mkdir build-linux-toolset
    cd build-linux-toolset

    cmake -G "Unix Makefiles" -DFO_OUTPUT_PATH="$FO_OUTPUT" -DCMAKE_BUILD_TYPE=Release -DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=0 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=1 -DFO_BUILD_BAKER=1 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0 "$FO_PROJECT_ROOT"
}

function setup_dotnet()
{
    echo "Setup dotnet"

    rm -rf dotnet
    mkdir dotnet
    cd dotnet
    git clone https://github.com/dotnet/runtime.git --depth 1 --branch $FO_DOTNET_RUNTIME
    touch CLONED
}

function verify_workspace_part()
{
    if [[ ! -f "$1-version.txt" || `cat $1-version.txt` != $2 ]]; then
        if [[ ! -z `check_arg check` ]]; then
            echo "Workspace is not ready"
            exit 1
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

if [[ ! -z `check_arg packages all` ]]; then
    verify_workspace_part common-packages 7 install_common_packages
    wait_jobs
    if [[ ! -z `check_arg linux all` ]]; then
        verify_workspace_part linux-packages 6 install_linux_packages
        wait_jobs
    fi
    if [[ ! -z `check_arg web all` ]]; then
        verify_workspace_part web-packages 2 install_web_packages
        wait_jobs
    fi
    if [[ ! -z `check_arg android android-arm64 android-x86 all` ]]; then
        verify_workspace_part android-packages 2 install_android_packages
        wait_jobs
    fi
fi

if [[ ! -z `check_arg toolset all` ]]; then
    verify_workspace_part toolset 5 setup_toolset
fi
if [[ ! -z `check_arg web all` ]]; then
    verify_workspace_part emscripten $EMSCRIPTEN_VERSION setup_emscripten
fi
if [[ ! -z `check_arg android android-arm64 android-x86 all` ]]; then
    verify_workspace_part android-ndk $ANDROID_NDK_VERSION setup_android_ndk
fi
if [[ ! -z `check_arg dotnet all` ]]; then
    verify_workspace_part dotnet $FO_DOTNET_RUNTIME setup_dotnet
fi
wait_jobs

echo "Workspace is ready!"
popd
