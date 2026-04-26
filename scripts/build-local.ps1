# =============================================
# SynthOrbis UNI — 本地构建脚本
# 支持：Windows (PowerShell) / Linux/macOS (Bash)
# 用法：
#   Windows:  .\scripts\build-local.ps1
#   Linux/macOS:  bash scripts/build-local.sh
# =============================================

param(
    [ValidateSet("debug", "release")]
    [string]$BuildType = "release",

    [ValidateSet("windows", "linux", "macos", "all")]
    [string]$Platform = "windows",

    [switch]$Clean,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"

function Write-Step($msg) {
    Write-Host "[BUILD] $msg" -ForegroundColor Cyan
}

function Invoke-CMakeBuild($platform, $buildType) {
    $buildPath = Join-Path $BuildDir $platform
    $srcPath = Join-Path $ProjectRoot "engine/librime"

    if ($Clean -and (Test-Path $buildPath)) {
        Write-Step "Clean build directory: $buildPath"
        Remove-Item -Recurse -Force $buildPath
    }

    if (-not (Test-Path $buildPath)) {
        New-Item -ItemType Directory -Force -Path $buildPath | Out-Null
    }

    Push-Location $buildPath

    try {
        Write-Step "Configure librime for $platform ($buildType)..."
        $cmakeArgs = @(
            "..",
            "-DCMAKE_BUILD_TYPE=$buildType",
            "-DBUILD_TESTING=ON"
        )

        if ($platform -eq "windows") {
            $cmakeArgs += @(
                "-G", "Visual Studio 17 2022",
                "-A", "x64"
            )
        } elseif ($platform -eq "linux") {
            $cmakeArgs += @("-G", "Ninja")
        } elseif ($platform -eq "macos") {
            $cmakeArgs += @("-G", "Xcode")
        }

        if ($Verbose) { $cmakeArgs += "--debug-output" }

        cmake @cmakeArgs

        Write-Step "Build librime..."
        if ($platform -eq "windows") {
            cmake --build . --config $buildType --parallel
        } else {
            cmake --build . --config $buildType --parallel
        }

        Write-Step "Run tests..."
        ctest -C $buildType --output-on-failure

        Write-Host "[BUILD] SUCCESS: $platform output at $buildPath" -ForegroundColor Green

    } catch {
        Write-Host "[BUILD] FAILED on $platform`: $_" -ForegroundColor Red
        Pop-Location
        throw
    }

    Pop-Location
}

# ── 主逻辑 ──────────────────────────────────────
Write-Host ""
Write-Host "╔═══════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║   SynthOrbis UNI — librime Build Script   ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""

if ($Platform -eq "all") {
    @("windows", "linux", "macos") | ForEach-Object { Invoke-CMakeBuild $_ $BuildType }
} else {
    Invoke-CMakeBuild $Platform $BuildType
}

Write-Host ""
Write-Host "Build complete. Artifacts: build/$Platform/" -ForegroundColor Green
