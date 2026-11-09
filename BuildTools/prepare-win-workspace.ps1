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

# Returns the first interpreter that meets MIN_PYTHON_VERSION in buildtools.py, preferring the py launcher
Function Get-PythonCommand {
    $ErrorActionPreference = "Continue"
    $probe = "import sys; sys.exit(int(sys.version_info < (3, 12)))"
    $candidates = @(
        [pscustomobject]@{ Executable = "py"; Arguments = @("-3") },
        [pscustomobject]@{ Executable = "python"; Arguments = @() }
    )

    foreach ($candidate in $candidates) {
        if (Get-Command $candidate.Executable -ErrorAction SilentlyContinue) {
            $candidateArgs = $candidate.Arguments
            & $candidate.Executable @candidateArgs -c $probe 2>&1 | Out-Null
            if ($LASTEXITCODE -eq 0) {
                return $candidate
            }
        }
    }

    throw "Python 3.12 or newer not found (tried py -3 and python)"
}

$python = Get-PythonCommand

if (!(Test-Path $FO_WORKSPACE)) {
    New-Item -Path "$FO_WORKSPACE" -ItemType Directory | Out-Null
}

# Feature names pass through unchanged: buildtools.py owns the vocabulary and rejects names Windows does not support
$checkOnly = $args -contains "check"
$features = @($args | Where-Object { $_ -ne "check" })

$buildToolsArgs = @("prepare-host-workspace", "windows") + $features
if ($checkOnly) {
    $buildToolsArgs += "--check"
}

$pythonArgs = $python.Arguments
& $python.Executable @pythonArgs $buildToolsPy @buildToolsArgs
$exitCode = $LASTEXITCODE

if ($exitCode -eq 0) {
    Write-Host "Workspace is ready!"
    exit 0
}

# Exit code 2 is an argument error rather than a missing workspace part
if ($checkOnly -and ($exitCode -ne 2)) {
    Write-Host "Workspace is not ready!"
    exit 10
}

throw "BuildTools failed with exit code $exitCode"
