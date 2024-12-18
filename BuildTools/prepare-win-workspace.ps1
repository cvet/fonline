$ErrorActionPreference = "stop"

Function Test-Command {
    Param ($Name)
    Try {
        if (Get-Command $Name) {
            return $True
        }
    }
    Catch {
    }
    return $False
}

Function Test-Admin {
    $currentPrincipal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    return $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

Function Test-BuildTools {
    if (!(Get-Module -ListAvailable -Name VSSetup)) {
        Install-Module VSSetup -Scope CurrentUser
    }

    $vspath = Get-VSSetupInstance `
    | Select-VSSetupInstance `
        -Product Microsoft.VisualStudio.Product.Community, `
        Microsoft.VisualStudio.Product.Professional, `
        Microsoft.VisualStudio.Product.Enterprise, `
        Microsoft.VisualStudio.Product.BuildTools `
        -Latest `
    | Select-Object -ExpandProperty InstallationPath

    if ($vspath) {
        return $True
    }
    else {
        return $False
    }
}

Write-Host "Prepare workspace"

$FO_PROJECT_ROOT = $Env:FO_PROJECT_ROOT

if ("$FO_PROJECT_ROOT" -eq "") {
    $FO_PROJECT_ROOT = Resolve-Path "."
}

$FO_ENGINE_ROOT = $Env:FO_ENGINE_ROOT

if ("$FO_ENGINE_ROOT" -eq "") {
    $FO_ENGINE_ROOT = Resolve-Path "$PSScriptRoot/.."
}

$FO_WORKSPACE = $Env:FO_WORKSPACE

if ("$FO_WORKSPACE" -eq "") {
    $FO_WORKSPACE = "$pwd/Workspace"
}

Write-Host "- FO_PROJECT_ROOT=$FO_PROJECT_ROOT"
Write-Host "- FO_ENGINE_ROOT=$FO_ENGINE_ROOT"
Write-Host "- FO_WORKSPACE=$FO_WORKSPACE"

if (!(Test-Path $FO_WORKSPACE)) {
    New-Item -Path "$FO_WORKSPACE" -ItemType Directory | Out-Null
}

while ($True) {
    $ready = $True

    if (!(Test-BuildTools)) {
        Write-Host "Build Tools not found"
        Write-Host "If you planning development in Visual Studio 2022 then install it"
        Write-Host "But if you don't need whole IDE then you may install just Build Tools for Visual Studio 2022"
        Write-Host "All this stuff you can get here: https://visualstudio.microsoft.com/downloads"
        $ready = $False
    }

    if (!(Test-Command cmake)) {
        Write-Host "CMake not found"
        Write-Host "You can get it here: https://cmake.org"
        $ready = $False
    }

    if (!(Test-Command python)) {
        Write-Host "Python not found"
        Write-Host "You can get it here: https://www.python.org"
        $ready = $False
    }

    $toolsetDir = "$FO_WORKSPACE/build-win64-toolset"

    if (!(Test-Path -Path $toolsetDir)) {
        if ($args[0] -Eq "check") {
            Write-Host "Toolset not ready"
            $ready = $False
        }
        else {
            Write-Host "Prepare toolset"
            $OUTPUT_PATH = "$FO_WORKSPACE/output"
            New-Item -Path $toolsetDir -ItemType Directory
            Push-Location -Path $toolsetDir
            cmake -G "Visual Studio 17 2022" -A x64 -DFO_OUTPUT_PATH="$OUTPUT_PATH" -DFO_BUILD_BAKER=1 -DFO_BUILD_ASCOMPILER=1 -DFO_UNIT_TESTS=0 "$FO_PROJECT_ROOT"
            Pop-Location
        }
    }

    # Todo: check VSCode
    # Todo: check WiX Toolset

    if ($ready) {
        Write-Host "Workspace is ready!"
        exit 0
    }
    
    if ($args[0] -Eq "check") {
        Write-Host "Workspace is not ready!"
        exit 10
    }

    Read-Host -Prompt "Fix things manually and press any key"
}
