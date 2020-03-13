# AddContent pathPattern
# AddResources packName pathPattern
# AddRawResources pathPattern
# AddScriptApi headerPath
# AddNativeIncludeDir dir
# AddNativeSource pathPattern
# AddAngelScriptSource pathPattern
# AddMonoAssembly assembly commonRefs serverRefs clientRefs mapperRefs
# AddMonoSource assembly pathPattern
# CreateConfig config inheritenConfig
# TweakConfig config option value
# CreatePackage package niceName devName author version
# AddClientToPackage package platform config packType
# AddServerToPackage package platform config packType

# Content
AddContent( "Critters/*.focr" )
AddContent( "Items/*.foitem" )
AddContent( "Maps/*.fomap" )
AddContent( "Maps/*.foloc" )

# Resources
AddResources( "TheGame" "Resources/**" )

# Scripts
AddScriptApi( "Scripts/MyScriptApi.h" )
AddNativeIncludeDir( "Scripts" )
AddNativeSource( "Scripts/*.cpp" )
AddAngelScriptSource( "Scripts/*.fos" )
AddMonoAssembly( "TheGame" "System" "" "" "" )
AddMonoSource( "TheGame" "Scripts/*.cs" )

# Default config
CreateConfig( "Default" "" )
TweakConfig( "Default" "RemoteHost" "1.2.3.4" )
TweakConfig( "Default" "RemotePort" "4013" )

# Test config
CreateConfig( "LocalTest" "Default" )
TweakConfig( "LocalTest" "RemoteHost" "localhost" )
TweakConfig( "LocalTest" "RemotePort" "4014" )
CreateConfig( "LocalWebTest" "LocalTest" )
TweakConfig( "LocalWebTest" "MyOpt" "42" )

# Test builds
CreatePackage( "Test" "The Game" "TheGame" "MeCoolLtd" "0.0.1" )
AddClientToPackage( "Test" "Win32" "LocalTest" "raw" )
AddClientToPackage( "Test" "Web" "LocalWebTest" "wasm" )
AddServerToPackage( "Test" "Win64" "LocalTest" "raw" )

# Production builds
CreatePackage( "Production" "The Game" "TheGame" "MeCoolLtd" "1.0.0" )
AddClientToPackage( "Production" "Win32" "Default" "raw" )
AddClientToPackage( "Production" "Win32" "Default" "wix" )
AddClientToPackage( "Production" "Win32" "Default" "zip" )
AddClientToPackage( "Production" "Android" "Default" "arm-arm64-x86" )
AddClientToPackage( "Production" "Web" "Default" "wasm-js" )
AddClientToPackage( "Production" "Mac" "Default" "bundle" )
AddClientToPackage( "Production" "iOS" "Default" "bundle" )
AddClientToPackage( "Production" "Linux" "Default" "zip" )
AddServerToPackage( "Production" "Win64" "Default" "raw" )
AddServerToPackage( "Production" "Win64" "Default" "zip" )
AddServerToPackage( "Production" "Linux" "Default" "raw" )
AddServerToPackage( "Production" "Linux" "Default" "tar" )
