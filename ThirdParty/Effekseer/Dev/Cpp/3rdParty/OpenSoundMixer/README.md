OpenSoundMixer2
==============

This is a sound mixer for multiplatforms.

This project includes the following projects.

* libogg-1.3.2
* libvorbis-1.3.5

## How to build

- Windows

```
cd scripts
GenerateProjects_x64.bat
```

Open a project in build directory

- Linux

```
cd scripts
GenerateProjectsWithSanitize.sh
cd ../build
make
```

## Requirements

### Common

* Python3.x
* CMake 3.16 or later

### Windows

* WASAPI

### Ubuntu

- apt-get install libpulse-dev

### MacOSX

* OpenAL

## Notes

`BUILD_TEST=ON` builds the `OpenSoundMixerTest` executable.

If you also need the sample audio files used by that executable at runtime, build the `TestData` target explicitly or configure with `-D DOWNLOAD_TEST_DATA=ON`.
