$ErrorActionPreference = 'stop'

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
            -Require Microsoft.VisualStudio.Workload.MSBuildTools `
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

Function Ask-Path-For-Project {
    [System.Reflection.Assembly]::LoadWithPartialName("System.Windows.Forms") | Out-Null

    $folder = New-Object System.Windows.Forms.FolderBrowserDialog
    $folder.Description = "Select a folder where project folder with files will be generated"

    if ($folder.ShowDialog() -eq "OK") {
        return $folder.SelectedPath
    }
}

Write-Host "Hello, Developer! Let's see what's going on..."

while ($True) {
    $chocoNeeded = $False
    $canGenProject = $True

    if (!(Test-Command wsl)) {
        Write-Host "WSL not found"
        Write-Host "Follow this link to get inforamtion about how to install WSL2"
        Write-Host "https://docs.microsoft.com/en-us/windows/wsl/install-win10"
        $canGenProject = $False
    }

    # Todo: check WSL vetsion
    # Todo: check WSL active distro

    if (!(Test-BuildTools)) {
        Write-Host "Build Tools not found"
        Write-Host "If you planning development in Visual Studio 2019 then install it"
        Write-Host "But if you don't need whole IDE then you may install just Build Tools for Visual Studio 2019"
        Write-Host "All this stuff you can get here: https://visualstudio.microsoft.com/downloads"
        Write-Host "(chocovs) Or install Visual Studio 2019 Community Edition automatically within Chocolatey"
        Write-Host "(chocobt) Or install Visual Studio Build Tools 2019 automatically within Chocolatey"
        $chocoNeeded = $True
        $canGenProject = $False
    }

    if (!(Test-Command cmake)) {
        Write-Host "CMake not found"
        Write-Host "You can get it here: https://cmake.org"
        Write-Host "(chococmake) Or install CMake automatically within Chocolatey"
        $chocoNeeded = $True
        $canGenProject = $False
    }

    # Todo: check VSCode
    # Todo: check WiX Toolset

    if ($chocoNeeded -And !(Test-Command choco)) {
        Write-Host "Chocolatey can install all necessary stuff automatically"
        Write-Host "Additional information you can find here: https://chocolatey.org"
        Write-Host "How to install Chocolatey you can find here: https://chocolatey.org/install"
        Write-Host "(choco) Or you can install Chocolatey automatically here and now"
    }

    if ($canGenProject) {
        Write-Host "(gen) Seems to all is fine, we can generate new project now"
    }

    while ($True) {
        $answer = Read-Host -Prompt "Type command or leave empty to exit setup"
        if ($answer -Eq "choco") {
            if (!(Test-Admin)) {
                Write-Host "To install Chocolatey here you must run this setup under administrative privileges"
            } else {
                if ((Get-ExecutionPolicy) -Eq "Restricted") {
                    Set-ExecutionPolicy AllSigned
                }
                Set-ExecutionPolicy Bypass -Scope Process -Force
                [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
                iex ((New-Object System.Net.WebClient).DownloadString("https://chocolatey.org/install.ps1"))
                break
            }
        } elseif (($answer.Length -Gt 5) -And ($answer.Substring(0, 5) -Eq "choco")) {
            if (!(Test-Admin)) {
                Write-Host "To install Chocolatey packages you must run this setup under administrative privileges"
            } elseif (!(Test-Command choco)) {
                Write-Host "(choco) Chocolatey not found, please install it first"
            } else {
                if ($answer -Eq "chocovs") {
                    choco install -y visualstudio2019community
                } elseif ($answer -Eq "chocobt") {
                    choco install -y visualstudio2019buildtools
                } elseif ($answer -Eq "chococmake") {
                    choco install -y cmake
                }
                break
            }
        } elseif ($answer -Eq "gen") {
            $projectName = Read-Host -Prompt "Type project name"
            if ($projectName) {
                $projectPath = Ask-Path-For-Project
                if ($projectPath) {
                    if (!(Test-Path "$projectPath\$projectName")) {
                        New-Item -Path $projectPath -Name $projectName -ItemType "directory" | Out-Null
                    }
                    $rootPath = Split-Path $MyInvocation.MyCommand.Path
                    $rootPathEx = $rootPath.Replace("\", "\\")
                    $projectPathEx = $projectPath.Replace("\", "\\")
                    $rootPathWsl = wsl wslpath -a "$rootPathEx"
                    $projectPathWsl = wsl wslpath -a "$projectPathEx"
                    $rootPathWsl = $rootPathWsl.Trim().TrimEnd("/")
                    $projectPathWsl = $projectPathWsl.Trim().TrimEnd("/")
                    cmd /C "wsl cd '$projectPathWsl/$projectName'; '$rootPathWsl/BuildTools/generate-project.sh' '$projectName'"
                }
            }
        } elseif ($answer -Eq "") {
            Write-Host "Good luck!"
            return
        } else {
            Write-Host "Invalid command"
        }
    }
}
