<#
.SYNOPSIS
    Development environment setup for volatile-membench (Windows).

.DESCRIPTION
    Checks for and installs all required (and optional) dependencies to build
    volatile-membench on Windows.  Uses winget as the primary package manager;
    falls back to Chocolatey when winget is unavailable.

    Required : CMake 3.20+, C11 compiler (MSVC / GCC via MSYS2 / Clang)
    Optional : CUDA Toolkit (NVIDIA GPU benchmarks)
               Git (source control)

.PARAMETER SkipOptional
    Skip installation of optional dependencies (CUDA, Git).

.PARAMETER NoBuild
    Only install dependencies -- do not configure or build the project.

.EXAMPLE
    .\scripts\setup.ps1
    .\scripts\setup.ps1 -SkipOptional
    .\scripts\setup.ps1 -NoBuild
#>

[CmdletBinding()]
param(
    [switch]$SkipOptional,
    [switch]$NoBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# -- Helpers -------------------------------------------------------------------

function Write-Banner  { param([string]$msg) Write-Host "`n=== $msg ===" -ForegroundColor Cyan }
function Write-Ok      { param([string]$msg) Write-Host "  [OK]   $msg" -ForegroundColor Green }
function Write-Warn    { param([string]$msg) Write-Host "  [WARN] $msg" -ForegroundColor Yellow }
function Write-Err     { param([string]$msg) Write-Host "  [ERR]  $msg" -ForegroundColor Red }
function Write-Info    { param([string]$msg) Write-Host "  [..]   $msg" -ForegroundColor Gray }

function Test-Command {
    param([string]$Name)
    $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Get-CMakeVersion {
    if (Test-Command 'cmake') {
        $raw = & cmake --version 2>&1 | Select-Object -First 1
        if ($raw -match '(\d+\.\d+(\.\d+)?)') { return [version]$Matches[1] }
    }
    return $null
}

# Detect a usable package manager (winget preferred, choco fallback)
function Get-PackageManager {
    if (Test-Command 'winget') { return 'winget' }
    if (Test-Command 'choco')  { return 'choco'  }
    return $null
}

function Install-WithPkgMgr {
    param(
        [string]$WingetId,
        [string]$ChocoId,
        [string]$Label
    )
    $pm = Get-PackageManager
    if ($pm -eq 'winget') {
        Write-Info "Installing $Label via winget ..."
        winget install --id $WingetId --accept-source-agreements --accept-package-agreements --silent
    }
    elseif ($pm -eq 'choco') {
        Write-Info "Installing $Label via Chocolatey ..."
        choco install $ChocoId -y
    }
    else {
        Write-Err "No package manager found (winget / choco). Please install $Label manually."
        return $false
    }
    return $true
}

function Initialize-VsDevEnv {
    # Use vswhere to locate VS and import the vcvarsall environment into this PS session.
    $vswherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswherePath)) { return $false }

    $vsPath = & $vswherePath -latest -property installationPath 2>$null
    if (-not $vsPath) { return $false }

    # Determine host architecture for vcvarsall argument
    $arch = if ([Environment]::Is64BitOperatingSystem) { 'amd64' } else { 'x86' }

    $vcvarsall = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
    if (-not (Test-Path $vcvarsall)) { return $false }

    Write-Info "Activating VS Developer Environment from: $vsPath"

    # Run vcvarsall in a cmd child process and capture the resulting env vars
    $envBefore = @{}
    $output = & cmd.exe /c "`"$vcvarsall`" $arch >nul 2>&1 && set" 2>&1
    foreach ($line in $output) {
        if ($line -match '^([^=]+)=(.*)$') {
            $name  = $Matches[1]
            $value = $Matches[2]
            # Import into current PowerShell session
            [System.Environment]::SetEnvironmentVariable($name, $value, 'Process')
        }
    }

    # Update PS $env:Path explicitly so Test-Command picks it up
    $env:Path = [System.Environment]::GetEnvironmentVariable('Path', 'Process')

    return (Test-Command 'cl')
}

# -- Check admin (recommend but don't require) --------------------------------

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Warn "Running without Administrator privileges -- some installs may require elevation."
}

# -- 1. Git --------------------------------------------------------------------

Write-Banner "Git"
if (Test-Command 'git') {
    $gitVer = (& git --version 2>&1) -replace 'git version ', ''
    Write-Ok "Git $gitVer found"
}
elseif (-not $SkipOptional) {
    Write-Warn "Git not found -- attempting install ..."
    Install-WithPkgMgr -WingetId 'Git.Git' -ChocoId 'git' -Label 'Git'
    # Refresh PATH
    $env:Path = [System.Environment]::GetEnvironmentVariable('Path','Machine') + ';' +
                [System.Environment]::GetEnvironmentVariable('Path','User')
    if (Test-Command 'git') { Write-Ok "Git installed successfully" }
    else { Write-Warn "Git installation may require a new terminal session to take effect" }
}
else {
    Write-Info "Git not found (skipped -- optional)"
}

# -- 2. C/C++ Compiler (MSVC) -------------------------------------------------

Write-Banner "C Compiler"

$hasCompiler = $false

# Check for MSVC (cl.exe) already on PATH
if (Test-Command 'cl') {
    $oldPref = $ErrorActionPreference; $ErrorActionPreference = 'SilentlyContinue'
    $clOut = (& cl 2>&1 | Select-Object -First 1) -join ''
    $ErrorActionPreference = $oldPref
    Write-Ok "MSVC cl.exe found -- $clOut"
    $hasCompiler = $true
}

# If cl not on PATH, try to activate the VS Developer Environment
if ((-not $hasCompiler)) {
    Write-Info "cl.exe not on PATH -- looking for Visual Studio installation ..."
    if (Initialize-VsDevEnv) {
        $oldPref = $ErrorActionPreference; $ErrorActionPreference = 'SilentlyContinue'
        $clOut = (& cl 2>&1 | Select-Object -First 1) -join ''
        $ErrorActionPreference = $oldPref
        Write-Ok "MSVC cl.exe activated via VS Developer Environment -- $clOut"
        $hasCompiler = $true
    }
}

# Check for GCC
if ((-not $hasCompiler) -and (Test-Command 'gcc')) {
    $gccVer = & gcc --version 2>&1 | Select-Object -First 1
    Write-Ok "GCC found -- $gccVer"
    $hasCompiler = $true
}

# Check for Clang
if ((-not $hasCompiler) -and (Test-Command 'clang')) {
    $clangVer = & clang --version 2>&1 | Select-Object -First 1
    Write-Ok "Clang found -- $clangVer"
    $hasCompiler = $true
}

if (-not $hasCompiler) {
    Write-Err "No C compiler found."
    Write-Info "Recommended: Install Visual Studio 2019+ with 'Desktop development with C++' workload."
    Write-Info "  -> winget install Microsoft.VisualStudio.2022.BuildTools"
    Write-Info "  -> Then select 'C++ build tools' in the VS Installer."
    Write-Info "Alternative: Install MSYS2 + MinGW-w64 (winget install MSYS2.MSYS2)"
    $proceed = Read-Host "Continue anyway? (y/N)"
    if ($proceed -notmatch '^[Yy]') { exit 1 }
}

# -- 3. CMake ------------------------------------------------------------------

Write-Banner "CMake"

$cmakeMinVersion = [version]'3.20'
$cmakeVer = Get-CMakeVersion

if ($cmakeVer -and ($cmakeVer -ge $cmakeMinVersion)) {
    Write-Ok "CMake $cmakeVer found (>= $cmakeMinVersion required)"
}
elseif ($cmakeVer) {
    Write-Warn "CMake $cmakeVer found but >= $cmakeMinVersion is required -- upgrading ..."
    Install-WithPkgMgr -WingetId 'Kitware.CMake' -ChocoId 'cmake' -Label 'CMake'
    $env:Path = [System.Environment]::GetEnvironmentVariable('Path','Machine') + ';' +
                [System.Environment]::GetEnvironmentVariable('Path','User')
}
else {
    Write-Warn "CMake not found -- installing ..."
    Install-WithPkgMgr -WingetId 'Kitware.CMake' -ChocoId 'cmake' -Label 'CMake'
    $env:Path = [System.Environment]::GetEnvironmentVariable('Path','Machine') + ';' +
                [System.Environment]::GetEnvironmentVariable('Path','User')
}

# Verify
$cmakeVer = Get-CMakeVersion
if ($cmakeVer -and ($cmakeVer -ge $cmakeMinVersion)) {
    Write-Ok "CMake $cmakeVer ready"
}
else {
    Write-Err "CMake >= $cmakeMinVersion still not available. Please install manually from https://cmake.org/download/"
}

# -- 4. CUDA Toolkit (optional) ------------------------------------------------

Write-Banner "CUDA Toolkit (optional)"

if ($SkipOptional) {
    Write-Info "Skipped (--SkipOptional)"
}
else {
    $nvccFound = Test-Command 'nvcc'
    if ($nvccFound) {
        $nvccVer = & nvcc --version 2>&1 | Select-String 'release' | Select-Object -First 1
        Write-Ok "CUDA nvcc found -- $nvccVer"
    }
    else {
        Write-Warn "CUDA Toolkit not found."
        Write-Info "GPU benchmarks will be disabled at build time (CPU-only is fine)."
        Write-Info "To enable GPU benchmarks, install the CUDA Toolkit:"
        Write-Info "  -> https://developer.nvidia.com/cuda-downloads"
        Write-Info "  -> Or: winget install Nvidia.CUDA"
    }
}

# -- 5. Ninja build system (optional, recommended) ----------------------------

Write-Banner "Ninja (optional, faster builds)"

if (Test-Command 'ninja') {
    $ninjaVer = & ninja --version 2>&1
    Write-Ok "Ninja $ninjaVer found"
}
elseif (-not $SkipOptional) {
    Write-Info "Ninja not found -- installing (recommended for faster builds) ..."
    Install-WithPkgMgr -WingetId 'Ninja-build.Ninja' -ChocoId 'ninja' -Label 'Ninja'
    $env:Path = [System.Environment]::GetEnvironmentVariable('Path','Machine') + ';' +
                [System.Environment]::GetEnvironmentVariable('Path','User')
    if (Test-Command 'ninja') { Write-Ok "Ninja installed" }
}
else {
    Write-Info "Ninja not found (skipped -- optional)"
}

# -- 6. Summary ----------------------------------------------------------------

Write-Banner "Summary"
Write-Host ""

$gitStatus       = if (Test-Command 'git')   { 'OK' } else { 'MISSING (optional)' }
$cmakeStatus     = if (Get-CMakeVersion)     { "$(Get-CMakeVersion)" } else { 'MISSING' }
$compilerStatus  = if ($hasCompiler)         { 'OK' } else { 'MISSING' }
$cudaStatus      = if (Test-Command 'nvcc')  { 'OK' } else { 'Not found (optional)' }
$ninjaStatus     = if (Test-Command 'ninja') { 'OK' } else { 'Not found (optional)' }

$summary = @(
    [PSCustomObject]@{ Tool = 'Git';        Status = $gitStatus },
    [PSCustomObject]@{ Tool = 'CMake';      Status = $cmakeStatus },
    [PSCustomObject]@{ Tool = 'C Compiler'; Status = $compilerStatus },
    [PSCustomObject]@{ Tool = 'CUDA nvcc';  Status = $cudaStatus },
    [PSCustomObject]@{ Tool = 'Ninja';      Status = $ninjaStatus }
)
$summary | Format-Table -AutoSize

# -- 7. Configure and Build ----------------------------------------------------

if (-not $NoBuild) {
    Write-Banner "Configuring and Building"

    $srcRoot = Split-Path -Parent $PSScriptRoot   # repo root
    Push-Location $srcRoot

    $preset = 'default'
    if (-not (Test-Command 'nvcc')) {
        $preset = 'no-gpu'
        Write-Info "Using 'no-gpu' preset (CUDA not available)"
    }

    try {
        Write-Info "cmake --preset $preset --fresh"
        & cmake --preset $preset --fresh
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

        $buildDir = if ($preset -eq 'no-gpu') { 'build-cpu' } else { 'build' }
        Write-Info "cmake --build $buildDir"
        & cmake --build $buildDir
        if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }

        Write-Ok "Build succeeded!"

        Write-Info "Running tests ..."
        Push-Location $buildDir
        & ctest --output-on-failure
        Pop-Location

        if ($LASTEXITCODE -eq 0) {
            Write-Ok "All tests passed!"
        }
        else {
            Write-Warn "Some tests failed -- check output above."
        }
    }
    catch {
        Write-Err $_.Exception.Message
    }
    finally {
        Pop-Location
    }
}

Write-Host ""
Write-Ok "Setup complete. Happy hacking!"
Write-Host ""
