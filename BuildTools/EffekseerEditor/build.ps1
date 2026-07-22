[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $SourceRoot,

    [Parameter(Mandatory = $true)]
    [string] $BuildRoot,

    [Parameter(Mandatory = $true)]
    [string] $OutputPath,

    [Parameter(Mandatory = $true)]
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration,

    [Parameter()]
    [string] $Generator
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string] $FilePath,

        [Parameter(Mandatory = $true)]
        [string[]] $Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath $($Arguments -join ' ')"
    }
}

function Copy-DirectoryContents {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Source,

        [Parameter(Mandatory = $true)]
        [string] $Destination
    )

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Copy-Item -Path (Join-Path $Source '*') -Destination $Destination -Recurse -Force
}

$sourceRootPath = (Resolve-Path -LiteralPath $SourceRoot).Path
$sharedThirdPartyPath = (Resolve-Path -LiteralPath (Split-Path -Parent $sourceRootPath)).Path
$buildRootPath = [IO.Path]::GetFullPath($BuildRoot)
$outputPathFull = [IO.Path]::GetFullPath($OutputPath)
$outputLeaf = Split-Path -Leaf $outputPathFull

if ($outputLeaf -notmatch '^EffekseerEditor-Windows-win64(?:-.+)?$') {
    throw "Refusing to replace unexpected Effekseer Editor output directory: $outputPathFull"
}

$nativeBuildPath = Join-Path $buildRootPath 'native'
$managedBuildPath = Join-Path $buildRootPath 'managed'
$managedPublishPath = Join-Path $managedBuildPath 'publish'
$nativeLayoutStampPath = Join-Path $nativeBuildPath '.fonline-layout'
$nativeLayoutStamp = "effekseer-editor-shared-third-party-v1`n$sourceRootPath`n$sharedThirdPartyPath"

if (Test-Path -LiteralPath $nativeBuildPath) {
    $nativeLayoutMatches =
        (Test-Path -LiteralPath $nativeLayoutStampPath) -and
        ([IO.File]::ReadAllText($nativeLayoutStampPath) -eq $nativeLayoutStamp)

    if (-not $nativeLayoutMatches) {
        $expectedNativeBuildPath = [IO.Path]::GetFullPath((Join-Path $buildRootPath 'native'))
        if ($nativeBuildPath -ne $expectedNativeBuildPath -or (Split-Path -Leaf $nativeBuildPath) -ne 'native') {
            throw "Refusing to replace unexpected Effekseer Editor native build directory: $nativeBuildPath"
        }

        Remove-Item -LiteralPath $nativeBuildPath -Recurse -Force
    }
}

if (Test-Path -LiteralPath $outputPathFull) {
    Remove-Item -LiteralPath $outputPathFull -Recurse -Force
}
if (Test-Path -LiteralPath $managedPublishPath) {
    Remove-Item -LiteralPath $managedPublishPath -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $nativeBuildPath, $managedBuildPath, $managedPublishPath, $outputPathFull | Out-Null
[IO.File]::WriteAllText($nativeLayoutStampPath, $nativeLayoutStamp)

$cmakeOutputPath = $outputPathFull.Replace('\', '/')
$cmakeSharedThirdPartyPath = $sharedThirdPartyPath.Replace('\', '/')
$configureArguments = @(
    '-S', $sourceRootPath,
    '-B', $nativeBuildPath,
    '-DCMAKE_POLICY_VERSION_MINIMUM=3.15',
    '-DCMAKE_COMPILE_WARNING_AS_ERROR=ON',
    '-DBUILD_VIEWER=ON',
    '-DBUILD_EDITOR=OFF',
    '-DBUILD_TEST=OFF',
    '-DBUILD_EXAMPLES=OFF',
    '-DBUILD_TOOLS=OFF',
    '-DBUILD_UNITYPLUGIN=OFF',
    '-DBUILD_DX9=OFF',
    '-DBUILD_DX11=ON',
    '-DBUILD_DX12=OFF',
    '-DBUILD_GL=ON',
    '-DBUILD_VULKAN=OFF',
    '-DBUILD_WEBGPU=OFF',
    '-DEFFEKSEER_ENABLE_GIF_RECORDING=OFF',
    '-DUSE_OPENAL=OFF',
    '-DUSE_DSOUND=OFF',
    '-DUSE_XAUDIO2=OFF',
    '-DUSE_MSVC_RUNTIME_LIBRARY_DLL=OFF',
    "-DFONLINE_SHARED_THIRD_PARTY_DIR=$cmakeSharedThirdPartyPath",
    "-DEFFEKSEER_RELEASE_DIR=$cmakeOutputPath"
)

if ($Generator) {
    $configureArguments += @('-G', $Generator)
}

if ($Generator -like 'Visual Studio*') {
    $configureArguments += @('-A', 'x64')
}

Invoke-Checked -FilePath 'cmake' -Arguments $configureArguments
Invoke-Checked -FilePath 'cmake' -Arguments @(
    '--build', $nativeBuildPath,
    '--config', $Configuration,
    '--target',
    'Viewer',
    'EffekseerMaterialEditor',
    'EffekseerMaterialCompilerGL',
    'EffekseerMaterialCompilerDX11',
    '--parallel', [Environment]::ProcessorCount.ToString()
)

$editorProject = Join-Path $sourceRootPath 'Dev\Editor\Effekseer\Effekseer.csproj'
Invoke-Checked -FilePath 'dotnet' -Arguments @(
    'publish', $editorProject,
    '--configuration', $Configuration,
    '--runtime', 'win-x64',
    '--self-contained', 'true',
    '--output', $managedPublishPath,
    '--nologo',
    "-p:FOnlineBuildRoot=$managedBuildPath"
)

Copy-Item -Path (Join-Path $managedPublishPath '*') -Destination $outputPathFull -Recurse -Force
$languageSourcePath = Join-Path $sourceRootPath 'Dev\release\resources\languages\en'
$languageOutputPath = Join-Path $outputPathFull 'resources\languages\en'
Copy-DirectoryContents -Source $languageSourcePath -Destination $languageOutputPath
Copy-DirectoryContents -Source (Join-Path $sourceRootPath 'Dev\release\resources\meshes') -Destination (Join-Path $outputPathFull 'resources\meshes')
Copy-DirectoryContents -Source (Join-Path $sourceRootPath 'ResourceData\tool\resources\fonts') -Destination (Join-Path $outputPathFull 'resources\fonts')
Copy-DirectoryContents -Source (Join-Path $sourceRootPath 'ResourceData\tool\resources\icons') -Destination (Join-Path $outputPathFull 'resources\icons')
Copy-Item -LiteralPath (Join-Path $sourceRootPath 'LICENSE_TOOL') -Destination (Join-Path $outputPathFull 'LICENSE_TOOL') -Force

$requiredFiles = @(
    'Effekseer.exe',
    'Effekseer.dll',
    'EffekseerCore.dll',
    'EffekseerCoreGUI.dll',
    'Viewer.dll',
    'EffekseerMaterialEditor.exe',
    'tools\EffekseerMaterialCompilerGL.dll',
    'tools\EffekseerMaterialCompilerDX11.dll',
    'resources\languages\en\Effekseer.csv',
    'resources\fonts\FOnlineEffekseerLatin-Regular.otf',
    'resources\icons\AppIcon.png',
    'resources\meshes\sphere.obj',
    'LICENSE_TOOL'
)

foreach ($requiredFile in $requiredFiles) {
    $requiredPath = Join-Path $outputPathFull $requiredFile
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Effekseer Editor payload is incomplete: $requiredPath"
    }
}

Write-Host "Effekseer Editor staged to $outputPathFull"
