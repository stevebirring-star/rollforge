<#
    Build the Windows distributables for RollForge:
      * a portable .zip (just the self-contained exe + a README)
      * an Inno Setup installer (.exe)

    Runs in CI (see .github/workflows/release.yml) after the Release app is built,
    but also works locally with Inno Setup installed. The exe is self-contained
    because the app links the MSVC runtime statically (see CMakeLists.txt), so no
    vcredist is required for either the zip or the installer.

    Usage:   packaging\windows\build-packages.ps1 -Version 0.1.0
    Params:
      -AppExe   path to the built RollForge.exe  (default: auto-discover under build\)
      -Version  version string for filenames     (default: 0.0.0)
      -OutDir   output directory                  (default: dist)
#>
param(
    [string]$AppExe  = "",
    [string]$Version = "0.0.0",
    [string]$OutDir  = "dist"
)

$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..\..")   # repo root

# --- locate the built exe ----------------------------------------------------
if (-not $AppExe) {
    $AppExe = Get-ChildItem -Path build -Recurse -Filter RollForge.exe -File -ErrorAction SilentlyContinue |
              Select-Object -First 1 -ExpandProperty FullName
}
if (-not $AppExe -or -not (Test-Path $AppExe)) {
    Write-Error "RollForge.exe not found. Build the Release app first, or pass -AppExe."
}
Write-Host "Using binary: $AppExe"

$OutDirAbs = (New-Item -ItemType Directory -Force -Path $OutDir).FullName

# --- portable zip ------------------------------------------------------------
$stage = Join-Path $env:TEMP "RollForge-$Version-windows-x64"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item $AppExe (Join-Path $stage "RollForge.exe")

$readme = @"
RollForge $Version (portable)

Run RollForge.exe -- no installation needed. The exe is self-contained (the MSVC
runtime is linked statically), so no Visual C++ redistributable is required.

Settings and recovery files are stored per-user under %APPDATA%\RollForge.
Windows audio uses WASAPI (default) and DirectSound (fallback); ASIO is not built in.
"@
Set-Content -Path (Join-Path $stage "README.txt") -Value $readme -Encoding UTF8

$zip = Join-Path $OutDirAbs "RollForge-$Version-windows-x64.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
Write-Host "zip:       $zip"

# --- Inno Setup installer ----------------------------------------------------
$iscc = (Get-Command iscc.exe -ErrorAction SilentlyContinue).Source
if (-not $iscc) {
    foreach ($cand in @(
        "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
        "C:\Program Files\Inno Setup 6\ISCC.exe")) {
        if (Test-Path $cand) { $iscc = $cand; break }
    }
}
if (-not $iscc) { Write-Error "ISCC (Inno Setup) not found -- install Inno Setup 6." }
Write-Host "Using ISCC: $iscc"

& $iscc "/DSourceExe=$AppExe" "/DMyAppVersion=$Version" "/DOutDir=$OutDirAbs" "packaging\windows\rollforge.iss"
if ($LASTEXITCODE -ne 0) { Write-Error "ISCC failed with exit code $LASTEXITCODE." }
Write-Host "installer: $OutDirAbs\RollForge-$Version-setup.exe"

Write-Host "Done. Artifacts in $OutDirAbs :"
Get-ChildItem $OutDirAbs | Format-Table Name, Length
