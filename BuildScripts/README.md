# FOnline Engine : build scripts

Following environment variables must be set before starting build scripts:

### Required: FO_BUILD_DEST
Path to build directory, where all required configuration (_.sln_/_Makefile_/etc.) and compiled binaries will be stored.
It is adviced to keep build directory outside of repository, and avoid in-source builds.

`FO_BUILD_DEST=FOnlineBuild`

Additional documentation describing build process can be found here: [CMake documentation](https://cmake.org/documentation).

### Required: FO_ROOT
Path to root directory of FOnline repository.

`FO_ROOT=FOnlineSource`

### Optional: FO_FTP_DEST
Address of FTP server where binary files will be uploaded after build.

`FO_FTP_DEST=127.0.0.1`

### Optional: FO_FTP_USER
FTP user name and password, separated by colon. This variable is required if **FO_FTP_DEST** is set.

`FO_FTP_USER=user:password`

### Optional: FO_COPY_DEST
Path to directory where binary files will be copied after build.
 
`FO_COPY_DEST=C:\FONLINE-COPY`

### Optional: FO_INSTALL_PACKAGES
Automatically install packages needed for build.
 
`FO_INSTALL_PACKAGES=1`

### Optional: GoCD
Additional documentation describing integratation with GoCD continuous delivery system can be found here: [GoCD documentation](https://docs.gocd.org/).

#### Windows setup
- Install GoCD
- Install CMake
- Create user go/password (admin), assign to Go services. Needs additional permissions.

#### Linux setup
- Install GoCD

#### Mac setup
- Install GoCD
- Install CMake
- Install Xcode

If everything is set correctly, all what's required is to run a proper build script; see [example](gocd_example.png).
