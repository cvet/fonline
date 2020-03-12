# AddContent pathPattern
# AddResources packName pathPattern
# AddRawResources pathPattern
# AddScriptApi headerPath
# AddNativeIncludeDir dir
# AddNativeSource pathPattern
# AddAngelScriptSource pathPattern
# AddMonoAssembly assembly commonRefs serverRefs clientRefs mapperRefs
# AddMonoSource assembly pathPattern
# CreateConfig cfgName inheritenCfgName
# TweakConfig cfgName option value
# AddClientPackage platform cfgName packageType
# AddServerPackage platform cfgName packageType

# Content
AddContent( "Critters/*.focr" )
AddContent( "Items/*.foitem" )
AddContent( "Maps/*.fomap" )
AddContent( "Maps/*.foloc" )

# Resources
AddResources( "TheGame" "Resources" )

# Scripts
AddScriptApi( "Scripts/MyScriptApi.h" )
AddNativeIncludeDir( "Scripts" )
AddNativeSource( "Scripts/*.cpp" )
AddAngelScriptSource( "Scripts/*.fos" )
AddMonoAssembly( "TheGame" "System" "" "" "" )
AddMonoSource( "TheGame" "Scripts/*.cs" )

# Default config
CreateConfig( "default" "" )
TweakConfig( "default" "RemoteHost" "1.2.3.4" )
TweakConfig( "default" "RemotePort" "4013" )

# Test config
CreateConfig( "test" "default" )
TweakConfig( "test" "RemoteHost" "localhost" )

# Test builds
AddClientPackage( "win32" "test" "raw" )
AddClientPackage( "web" "test" "wasm" )
AddServerPackage( "win64" "test" "raw" )

# Production builds
AddClientPackage( "win32" "default" "raw" )
AddClientPackage( "win32" "default" "wix" )
AddClientPackage( "win32" "default" "zip" )
AddClientPackage( "android" "default" "arm-arm64-x86" )
AddClientPackage( "web" "default" "wasm-js" )
AddClientPackage( "mac" "default" "bundle" )
AddClientPackage( "ios" "default" "bundle" )
AddClientPackage( "linux" "default" "zip" )
AddServerPackage( "win64" "default" "raw" )
AddServerPackage( "win64" "default" "zip" )
AddServerPackage( "linux" "default" "raw" )
AddServerPackage( "linux" "default" "tar" )
