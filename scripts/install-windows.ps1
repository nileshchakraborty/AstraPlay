# AstraPlay Windows 1-Line Automated Installer
# Usage: irm https://raw.githubusercontent.com/nileshchakraborty/AstraPlay/master/scripts/install-windows.ps1 | iex
$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "               AstraPlay Windows Installer                " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host ""

$arch = if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") { "arm64" } else { "x64" }
Write-Host "Detected Architecture: $arch" -ForegroundColor Green

$releaseUrl = "https://github.com/nileshchakraborty/AstraPlay/releases/latest/download"
$installerName = if ($arch -eq "arm64") { "astraplay-windows-arm64-installer.exe" } else { "astraplay-windows-installer.exe" }
$downloadUrl = "$releaseUrl/$installerName"

$tempDir = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), [System.Guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
$installerPath = Join-Path $tempDir $installerName

try {
    Write-Host "Downloading latest AstraPlay ($installerName)..." -ForegroundColor Yellow
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 -bor [Net.SecurityProtocolType]::Tls13
    Invoke-WebRequest -Uri $downloadUrl -OutFile $installerPath -UseBasicParsing

    Write-Host "Running installer..." -ForegroundColor Green
    Start-Process -FilePath $installerPath -Wait

    Write-Host ""
    Write-Host "==========================================================" -ForegroundColor Cyan
    Write-Host "         AstraPlay installation finished successfully!    " -ForegroundColor Cyan
    Write-Host "==========================================================" -ForegroundColor Cyan
}
catch {
    Write-Host "Error during installation: $_" -ForegroundColor Red
}
finally {
    if (Test-Path $tempDir) {
        Remove-Item -Path $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}
