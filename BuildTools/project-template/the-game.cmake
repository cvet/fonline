# AddContent pathPattern(s)
# AddResources packName rootDir pathPattern(s)
# AddRawResources rootDir pathPattern(s)
# AddScriptApi headerPath(s)
# AddNativeIncludeDir dir(s)
# AddNativeSource pathPattern(s)
# AddAngelScriptSource pathPattern(s)
# AddMonoAssembly assembly
# AddMonoCommonReference assembly ref(s)
# AddMonoServerReference assembly ref(s)
# AddMonoClientReference assembly ref(s)
# AddMonoMapperReference assembly ref(s)
# AddMonoCommonSource assembly pathPattern(s)
# AddMonoServerSource assembly pathPattern(s)
# AddMonoClientSource assembly pathPattern(s)
# AddMonoMapperSource assembly pathPattern(s)
# CreateConfig config inheritenConfig
# TweakConfig config option value
# CreatePackage package devName niceName author version config devMode
# AddClientToPackage package platform packType [customConfig]
# AddServerToPackage package platform packType [customConfig]

# Content
AddContent( "Critters/*.focr" )
AddContent( "Items/*.foitem" )
AddContent( "Maps/*.fomap" "Maps/*.foloc" )

# Resources
AddResources( "TheGame" "Resources" "*.*" )

# Scripts
AddScriptApi( "Scripts/MyScriptApi.h" )
AddNativeIncludeDir( "Scripts" )
AddNativeSource( "Scripts/Test.cpp" )
AddAngelScriptSource( "Scripts/Test.fos" )
AddMonoAssembly( "TheGame" )
AddMonoCommonReference( "TheGame" "System" "System.Core" )
AddMonoCommonSource( "TheGame" "Scripts/Test.cs" )

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
CreatePackage( "Test" "TheGame" "The Game (wip)" "MeCoolLtd" "0.0.1" "LocalTest" YES )
AddClientToPackage( "Test" "Win32" "raw" )
AddClientToPackage( "Test" "Web" "wasm" "LocalWebTest" )
AddServerToPackage( "Test" "Win64" "raw" )

# Production builds
CreatePackage( "Production" "TheGame" "The Game" "MeCoolLtd" "1.0.0" "Default" NO )
AddClientToPackage( "Production" "Win32" "raw" )
AddClientToPackage( "Production" "Win32" "wix" )
AddClientToPackage( "Production" "Win32" "zip" )
AddClientToPackage( "Production" "Android" "arm-arm64-x86" )
AddClientToPackage( "Production" "Web" "wasm-js" )
AddClientToPackage( "Production" "macOS" "bundle" )
AddClientToPackage( "Production" "iOS" "bundle" )
AddClientToPackage( "Production" "Linux" "zip" )
AddServerToPackage( "Production" "Win64" "raw" )
AddServerToPackage( "Production" "Win64" "zip" )
AddServerToPackage( "Production" "Linux" "raw" )
AddServerToPackage( "Production" "Linux" "tar" )
