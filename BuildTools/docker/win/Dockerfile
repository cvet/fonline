# Usage:
# docker build -m 2GB -t win-builder Engine/BuildTools/docker/win
# docker run --rm -v "${{github.workspace}}:C:/work" -m 4GB --cpus 2 win-builder ./Engine/BuildTools/build.cmd ...

FROM mcr.microsoft.com/dotnet/framework/sdk:4.8

SHELL ["powershell", "-Command"]

ADD https://aka.ms/vs/17/release/channel C:/VisualStudio.chman
ADD https://aka.ms/vs/17/release/vs_buildtools.exe C:/vs_buildtools.exe

RUN Start-Process 'C:/vs_buildtools.exe' \
    -ArgumentList '--quiet', '--wait', '--norestart', '--nocache', \
        '--installPath', 'C:/BuildTools', \
        '--add', 'Microsoft.VisualStudio.Workload.VCTools', \
        '--remove', 'Microsoft.VisualStudio.Component.Windows10SDK.10240', \
        '--remove', 'Microsoft.VisualStudio.Component.Windows10SDK.10586', \
        '--remove', 'Microsoft.VisualStudio.Component.Windows10SDK.14393', \
        '--remove', 'Microsoft.VisualStudio.Component.Windows81SDK', \
        '--includeRecommended' \
    -Wait

RUN [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; \
    Invoke-WebRequest -Uri 'https://get.scoop.sh' -OutFile 'C:/install-scoop.ps1'; \
    &'C:/install-scoop.ps1' -RunAsAdmin
RUN scoop install cmake
RUN scoop install git
RUN scoop install python

ENV CMAKE_GENERATOR="Visual Studio 17 2022"
RUN git config --global --add safe.directory '*'
WORKDIR C:/work

ENTRYPOINT ["powershell", "-Command"]
