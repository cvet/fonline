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

    echo "Install clang"
    sudo apt-get -qq -y install clang
    echo "Install clang-format-12"
    sudo apt-get -qq -y install clang-format-12
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
    echo "Install binutils-dev"
    sudo apt-get -qq -y install binutils-dev
}

function install_linux_packages()
{
    echo "Install Linux packages"
    sudo apt-get -qq -y update

    echo "Install libc++-dev"
    sudo apt-get -qq -y install libc++-dev
    echo "Install libc++abi-dev"
    sudo apt-get -qq -y install libc++abi-dev
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

verify_workspace_part common-packages 7 install_common_packages
wait_jobs
if [ ! -z `check_arg linux all` ]; then
    verify_workspace_part linux-packages 6 install_linux_packages
    wait_jobs
fi
if [ ! -z `check_arg web all` ]; then
    verify_workspace_part web-packages 2 install_web_packages
    wait_jobs
fi
if [ ! -z `check_arg android android-arm64 android-x86 all` ]; then
    verify_workspace_part android-packages 2 install_android_packages
    wait_jobs
fi

if [ ! -z `check_arg web all` ]; then
    verify_workspace_part emscripten $EMSCRIPTEN_VERSION setup_emscripten
fi
if [ ! -z `check_arg android android-arm64 android-x86 all` ]; then
    verify_workspace_part android-ndk $ANDROID_NDK_VERSION setup_android_ndk
fi
wait_jobs

echo "Workspace is ready!"
