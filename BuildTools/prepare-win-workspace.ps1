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
        | Select-VSSetupInstance -Version "[16.0,17.0)" `
            -Product Microsoft.VisualStudio.Product.Community, `
                Microsoft.VisualStudio.Product.Professional, `
                Microsoft.VisualStudio.Product.Enterprise, `
                Microsoft.VisualStudio.Product.BuildTools `
            -Latest `
        | Select-Object -ExpandProperty InstallationPath
    if ($vspath) {
        return $True
    } else {
        return $False
    }
}

Write-Host "Prepare workspace"

$FO_ROOT = $Env:FO_ROOT
if ($FO_ROOT -eq "") {
    $FO_ROOT = Resolve-Path "$PSScriptRoot/../"
}

$FO_WORKSPACE = $Env:FO_WORKSPACE
if ($FO_WORKSPACE -eq "") {
    $FO_WORKSPACE = "$pwd\Workspace"
}

$FO_CMAKE_CONTRIBUTION = $Env:FO_CMAKE_CONTRIBUTION

Write-Host "- FO_ROOT=$FO_ROOT"
Write-Host "- FO_WORKSPACE=$FO_WORKSPACE"
Write-Host "- FO_CMAKE_CONTRIBUTION=$FO_CMAKE_CONTRIBUTION"

if (!(Test-Path $FO_WORKSPACE)) {
    New-Item -Path "$FO_WORKSPACE" -ItemType Directory | Out-Null
}

Set-Location -Path $FO_WORKSPACE

while ($True) {
    $ready = $True


    if (!(Test-BuildTools)) {
        Write-Host "Build Tools not found"
        Write-Host "If you planning development in Visual Studio 2019 then install it"
        Write-Host "But if you don't need whole IDE then you may install just Build Tools for Visual Studio 2019"
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
        } else {
            Write-Host "Prepare toolset"
            OUTPUT_PATH=$FO_WORKSPACE/output
            mkdir $toolsetDir
            cd $toolsetDir
            cmake -G "Visual Studio 16 2019" -A x64 -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH" -DFONLINE_BUILD_BAKER=1 -DFONLINE_BUILD_ASCOMPILER=1 -DFONLINE_UNIT_TESTS=0 -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION" "$FO_ROOT"
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
