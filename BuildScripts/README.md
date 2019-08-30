# FOnline Engine : Build scripts

Build scripts (sh/bat) may be called both from current directory or repository root (e.g. BuildScripts/linux.sh).\
Following environment variables optionally may be set before starting build scripts:

### FO_BUILD_DEST (default: *Build*)

Path to build directory, where all required configuration (_.sln_/_Makefile_/etc.) and compiled binaries will be stored.\
Default behaviour is build in repository in 'Build' directory which is already placed to gitignore and not follow to futher commit.

`FO_BUILD_DEST=c:/fonline-build`

### FO_ROOT *(default: . or ../ automatic detection)*

Path to root directory of FOnline repository.

`FO_ROOT=c:/fonline-repo`

### FO_INSTALL_PACKAGES *(default: 1)*

Automatically install packages needed for build.
 
`FO_INSTALL_PACKAGES=0`

### FO_FTP_DEST *(default: not specified)*

Address of FTP server where binary files will be uploaded after build.

`FO_FTP_DEST=127.0.0.1`

### FO_FTP_USER *(default: not specified)*

FTP user name and password, separated by colon. This variable is required if **FO_FTP_DEST** is set.

`FO_FTP_USER=user:password`

### FO_COPY_DEST *(default: not specified)*

Path to directory where binary files will be copied after build.
 
`FO_COPY_DEST=c:/fonline-bin`
