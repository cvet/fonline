if (!(Get-Module -ListAvailable -Name VSSetup)) {
    Install-Module VSSetup -Scope CurrentUser
}

$path = Get-VSSetupInstance `
    | Select-VSSetupInstance -Version "[16.0,17.0)" `
        -Require Microsoft.VisualStudio.Workload.MSBuildTools `
        -Product Microsoft.VisualStudio.Product.Community, `
            Microsoft.VisualStudio.Product.Professional, `
            Microsoft.VisualStudio.Product.Enterprise, `
            Microsoft.VisualStudio.Product.BuildTools `
        -Latest `
    | Select-Object -ExpandProperty InstallationPath
if ($path -And (Test-Path ($path + "\MSBuild\Current\Bin\MSBuild.exe"))) {
    return $path + "\MSBuild\Current\Bin\MSBuild.exe"
}
