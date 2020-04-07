# SetCustomVersion version
# DisableNativeScripting
# DisableAngelScriptScripting
# DisableMonoScripting
# AddContent dir(s)
# AddResources packName dir(s)
# AddRawResources dir(s)
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
# CreatePackage package devName niceName author version config devBuild
# AddClientToPackage package platform packType [customConfig]
# AddServerToPackage package platform packType [customConfig]

# Content
AddContent( "Critters" "Items" "Maps" )
AddContent( "Dialogs" )

# Resources
AddResources( "TheGame" "Resources" )

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
AddClientToPackage( "Test" "Win32" "Raw" )
AddClientToPackage( "Test" "Web" "Wasm" "LocalWebTest" )
AddServerToPackage( "Test" "Win64" "Raw" )

# Production builds
CreatePackage( "Production" "TheGame" "The Game" "MeCoolLtd" "1.0.0" "Default" NO )
AddClientToPackage( "Production" "Win32" "Raw" )
AddClientToPackage( "Production" "Win32" "Wix" )
AddClientToPackage( "Production" "Win32" "Zip" )
AddClientToPackage( "Production" "Android" "Arm+Arm64+x86" )
AddClientToPackage( "Production" "Web" "Wasm+Js" )
AddClientToPackage( "Production" "macOS" "Bundle" )
AddClientToPackage( "Production" "iOS" "Bundle" )
AddClientToPackage( "Production" "Linux" "Zip" )
AddServerToPackage( "Production" "Win64" "Raw" )
AddServerToPackage( "Production" "Win64" "Zip" )
AddServerToPackage( "Production" "Linux" "Raw" )
AddServerToPackage( "Production" "Linux" "Tar" )
