Function Ask-Path-For-Project {
    [System.Reflection.Assembly]::LoadWithPartialName("System.Windows.Forms") | Out-Null

    $folder = New-Object System.Windows.Forms.FolderBrowserDialog
    $folder.Description = "Select a folder where project folder with files will be generated"

    if ($folder.ShowDialog() -eq "OK") {
        return $folder.SelectedPath
    }
}

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

        Write-Host "Done!"
        Invoke-Item "$projectPath\$projectName"
    }
}