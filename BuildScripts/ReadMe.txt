You must specify following environment variables before start build scripts: 

FO_BUILD_DEST=FOnlineBuild # Path to build output
FO_SOURCE=FOnlineSource # Path to fonline sources
FO_FTP_DEST=127.0.0.1 (optional) # Upload binaries to this ftp after build
FO_FTP_USER=user:password (optional)
FO_COPY_DEST=D:\_COPY (optional) # Copy binaries to this dir after build

Helping tips for setup agents for GoCD continuous delivery server.
You can find it here: https://www.gocd.org/

Setup on Windows:
Install gocd
Install cmake
Create user go/password (admin), assign to Go services. Need more permissions.

Setup on Linux:
Install gocd

Setup on Mac:
Install gocd
Install cmake
Install xcode

After this you just run needed build script, for example look at 'gocd_example.png'.
