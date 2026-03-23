$ErrorActionPreference = "stop"

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

$buildToolsPy = Join-Path $PSScriptRoot "buildtools.py"

Function Get-PythonCommand {
    if (Test-Command py) {
        return @("py", "-3")
    }

    return @("python")
}

Function Invoke-BuildTools {
    Param (
        [string[]] $Arguments
    )

    $pythonCommand = Get-PythonCommand
    if ($pythonCommand.Length -gt 1) {
        & $pythonCommand[0] $pythonCommand[1..($pythonCommand.Length - 1)] $buildToolsPy @Arguments
    }
    else {
        & $pythonCommand[0] $buildToolsPy @Arguments
    }
    if ($LASTEXITCODE -ne 0) {
        throw "BuildTools failed with exit code $LASTEXITCODE"
    }
}

if (!(Test-Path $FO_WORKSPACE)) {
    New-Item -Path "$FO_WORKSPACE" -ItemType Directory | Out-Null
}

$checkOnly = $args -contains "check"

$filteredArgs = @()
foreach ($arg in $args) {
    if ($arg -ne "check") {
        $filteredArgs += $arg
    }
}

while ($True) {
    try {
        $buildToolsArgs = @("prepare-host-workspace", "windows") + $filteredArgs
        if ($checkOnly) {
            $buildToolsArgs += "--check"
        }

        Invoke-BuildTools -Arguments $buildToolsArgs
        Write-Host "Workspace is ready!"
        exit 0
    }
    catch {
    }

    if ($checkOnly) {
        Write-Host "Workspace is not ready!"
        exit 10
    }

    Read-Host -Prompt "Fix things manually and press any key"
}
