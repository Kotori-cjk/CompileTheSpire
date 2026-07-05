<#
.SYNOPSIS
    CompileTheSpire one-click release packager
.DESCRIPTION
    Packages a Release build into a portable ZIP ready for GitHub Releases.
.PARAMETER QtPath
    Path to Qt mingw_64 directory (default: D:\QT_Homework\6.11.0\mingw_64)
.PARAMETER ExePath
    Path to Release-built CompileTheSpire.exe (default: auto-detect)
.PARAMETER Version
    Version string for the ZIP filename (default: 0.1)
.PARAMETER KeepDir
    Keep staging directory after packaging for inspection
.EXAMPLE
    .\scripts\release.ps1
    .\scripts\release.ps1 -Version 0.2 -KeepDir
#>

param(
    [string]$QtPath = "D:\QT_Homework\6.11.0\mingw_64",
    [string]$ExePath = "",
    [string]$Version = "0.1",
    [switch]$KeepDir
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = Split-Path -Parent $ScriptDir

# Auto-detect the actual source directory (where CMakeLists.txt lives).
# The repo structure may be:  repo-root/ProjectName/CMakeLists.txt
$SourceDir = $ProjectDir
if (-not (Test-Path (Join-Path $SourceDir "CMakeLists.txt"))) {
    # Check one level deeper for a subdirectory with CMakeLists.txt
    $SubDirs = Get-ChildItem -Path $ProjectDir -Directory
    foreach ($sd in $SubDirs) {
        if (Test-Path (Join-Path $sd.FullName "CMakeLists.txt")) {
            $SourceDir = $sd.FullName
            break
        }
    }
}
if (-not (Test-Path (Join-Path $SourceDir "CMakeLists.txt"))) {
    Write-Err "Cannot find CMakeLists.txt. Use -ExePath to specify the exe directly."
    exit 1
}

function Write-Step { param([string]$M) Write-Host "  >> $M" -ForegroundColor Gray }
function Write-OK   { param([string]$M) Write-Host " [OK] $M" -ForegroundColor Green }
function Write-Warn { param([string]$M) Write-Host "[WARN] $M" -ForegroundColor Yellow }
function Write-Err  { param([string]$M) Write-Host "[ERROR] $M" -ForegroundColor Red }
function Write-Hdr  { param([string]$M) Write-Host $M -ForegroundColor Cyan }

function Copy-DirectoryContents {
    param([string]$Source, [string]$Dest)
    if (-not (Test-Path $Source)) { return $false }
    New-Item -ItemType Directory -Force -Path $Dest | Out-Null
    $items = Get-ChildItem -Path $Source
    foreach ($item in $items) {
        $target = Join-Path $Dest $item.Name
        if ($item.PSIsContainer) {
            Copy-DirectoryContents -Source $item.FullName -Dest $target
        } else {
            Copy-Item -Path $item.FullName -Destination $target -Force
        }
    }
    return $true
}

Write-Host ""
Write-Hdr "========================================"
Write-Hdr "  CompileTheSpire Release Packager"
Write-Hdr "========================================"
Write-Host ""

# 1. Locate windeployqt
$Windeployqt = Join-Path $QtPath "bin\windeployqt.exe"
if (-not (Test-Path $Windeployqt)) {
    Write-Err "windeployqt.exe not found at: $Windeployqt"
    Write-Warn "Use -QtPath to specify the correct Qt mingw_64 directory."
    exit 1
}
Write-OK "windeployqt: $Windeployqt"

# 2. Locate Release exe
if ([string]::IsNullOrEmpty($ExePath)) {
    $BuildDir = Join-Path $SourceDir "build"
    if (Test-Path $BuildDir) {
        $AllExes = Get-ChildItem -Path $BuildDir -Recurse -Filter "CompileTheSpire.exe" -ErrorAction SilentlyContinue
        if ($AllExes.Count -gt 0) {
            $ReleaseExe = $AllExes |
                Where-Object { $_.DirectoryName -match "Release" -and $_.DirectoryName -notmatch "Debug" } |
                Sort-Object LastWriteTime -Descending |
                Select-Object -First 1
            if ($null -eq $ReleaseExe) {
                $ReleaseExe = $AllExes | Sort-Object LastWriteTime -Descending | Select-Object -First 1
            }
            $ExePath = $ReleaseExe.FullName
        }
    }
}

if ([string]::IsNullOrEmpty($ExePath) -or -not (Test-Path $ExePath)) {
    Write-Err "Cannot find Release-build CompileTheSpire.exe"
    Write-Warn "Build Release mode in Qt Creator first, or use -ExePath."
    exit 1
}
Write-OK "Exe: $ExePath"

if ($ExePath -match "Debug") {
    Write-Warn "Path contains 'Debug' -- this may not be a Release build!"
}

# 3. Create staging directory
$ReleaseDir = Join-Path $ProjectDir "release\CompileTheSpire_v$Version"
if (Test-Path $ReleaseDir) {
    Write-Step "Removing old staging dir..."
    Remove-Item -Recurse -Force $ReleaseDir
}
New-Item -ItemType Directory -Force -Path $ReleaseDir | Out-Null
Write-OK "Staging: $ReleaseDir"

# 4. Copy exe
$ExeName = Split-Path $ExePath -Leaf
Write-Step "Copying $ExeName ..."
Copy-Item -Path $ExePath -Destination $ReleaseDir
Write-OK "$ExeName copied"

# 5. windeployqt (stderr warnings from Qt tools are harmless,
#    suppress them so they don't terminate the script)
$TargetExe = Join-Path $ReleaseDir $ExeName
Write-Step "Running windeployqt (this may take several seconds)..."
$args = @($TargetExe, "--no-translations", "--no-compiler-runtime")
$prevEAP = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$null = & $Windeployqt @args 2>&1
$ErrorActionPreference = $prevEAP
if (-not (Test-Path (Join-Path $ReleaseDir "platforms\qwindows.dll"))) {
    Write-Err "windeployqt failed: platforms/qwindows.dll missing"
    exit 1
}
Write-OK "windeployqt done"

# 6. Copy runtime assets
Write-Step "Copying runtime assets..."

# 6a. levels/
$src = Join-Path $SourceDir "levels"
$dst = Join-Path $ReleaseDir "levels"
if (Copy-DirectoryContents -Source $src -Dest $dst) {
    $n = (Get-ChildItem -Path $dst -Filter "*.json" -Recurse).Count
    Write-OK "levels/ ($n json files)"
} else {
    Write-Warn "levels/ not found"
}

# 6b. assets/audio/
$src = Join-Path $SourceDir "assets\audio"
$dst = Join-Path $ReleaseDir "assets\audio"
if (Copy-DirectoryContents -Source $src -Dest $dst) {
    $n = (Get-ChildItem -Path $dst -File).Count
    Write-OK "assets/audio/ ($n files)"
} else {
    Write-Warn "assets/audio/ not found"
}

# 6c. beginner_tips_intro.png (loaded from filesystem at runtime)
$src = Join-Path $SourceDir "assets\beginner_tips_intro.png"
$dst = Join-Path $ReleaseDir "assets"
if (Test-Path $src) {
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Copy-Item -Path $src -Destination $dst -Force
    Write-OK "assets/beginner_tips_intro.png"
} else {
    Write-Warn "beginner_tips_intro.png not found (non-fatal)"
}

# 6d. savedata/ (empty dir for runtime saves)
$dst = Join-Path $ReleaseDir "savedata"
New-Item -ItemType Directory -Force -Path $dst | Out-Null
Write-OK "savedata/ (empty)"

# 6e. Remove .gitkeep files
Get-ChildItem -Recurse -Path $ReleaseDir -Filter ".gitkeep" -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue

Write-OK "Asset copy complete"

# 7. Summary
Write-Host ""
Write-Hdr "  Staging directory summary:"
$total = 0
Get-ChildItem -Recurse -Path $ReleaseDir -File | ForEach-Object { $total += $_.Length }
$totalMB = [math]::Round($total / 1MB, 1)

Get-ChildItem -Path $ReleaseDir |
    ForEach-Object {
        $t = if ($_.PSIsContainer) { "DIR" } else { "FILE" }
        $s = if (-not $_.PSIsContainer) { "  ($([math]::Round($_.Length/1KB,1)) KB)" } else { "" }
        Write-Host "      [$t] $($_.Name)$s"
    }
Write-Host ""
Write-Hdr "  Total size: ~${totalMB} MB"
Write-Host ""

# 8. ZIP
$ZipPath = Join-Path $ProjectDir "release\CompileTheSpire_v${Version}.zip"
if (Test-Path $ZipPath) { Remove-Item -Force $ZipPath }
Write-Step "Creating ZIP archive..."

Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($ReleaseDir, $ZipPath)

if (Test-Path $ZipPath) {
    $zipMB = [math]::Round((Get-Item $ZipPath).Length / 1MB, 1)
    Write-OK "ZIP created: $ZipPath (${zipMB} MB)"
} else {
    Write-Err "ZIP creation failed"
    exit 1
}

# 9. Cleanup
if (-not $KeepDir) {
    Write-Step "Removing staging dir..."
    Remove-Item -Recurse -Force $ReleaseDir
    Write-OK "Cleaned up"
} else {
    Write-Step "Staging dir kept: $ReleaseDir"
}

# Done
Write-Host ""
Write-Hdr "========================================"
Write-Hdr "  Release packaging complete!"
Write-Hdr ""
Write-Hdr "  ZIP: $ZipPath"
Write-Hdr "========================================"
Write-Host ""
Write-Host "  Next steps:"
Write-Host "    1. Extract ZIP to a folder, double-click CompileTheSpire.exe to test"
Write-Host "    2. Upload to GitHub Releases: https://github.com/Kotori-cjk/CompileTheSpire/releases"
Write-Host "    3. Test on a PC without Qt installed to confirm it works"
Write-Host ""
