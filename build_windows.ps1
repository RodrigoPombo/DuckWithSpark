# Build script for Spark DuckDB Extension on Windows
$ErrorActionPreference = "Stop"

Write-Host "Spark DuckDB Extension - Windows Build" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check CMake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: CMake not found!" -ForegroundColor Red
    Write-Host "Install with: winget install Kitware.CMake" -ForegroundColor Yellow
    exit 1
}
Write-Host "CMake found" -ForegroundColor Green

# Set build directory
$BUILD_DIR = "build\release"
$EXT_CONFIG = Join-Path $PSScriptRoot "extension_config.cmake"

Write-Host ""
Write-Host "Building extension..." -ForegroundColor Cyan

# Create build directory
New-Item -ItemType Directory -Force -Path $BUILD_DIR | Out-Null

# Change to build directory
Push-Location $BUILD_DIR

try {
    # Configure
    Write-Host "Configuring..." -ForegroundColor Yellow
    cmake -DCMAKE_BUILD_TYPE=Release -DDUCKDB_EXTENSION_CONFIGS="$EXT_CONFIG" ..\..\duckdb

    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }

    # Build
    Write-Host "Building..." -ForegroundColor Yellow
    cmake --build . --config Release --parallel

    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }

    Write-Host ""
    Write-Host "Build successful!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Extension binaries:" -ForegroundColor Cyan
    Write-Host "  - Loadable: build\release\extension\spark_duckdb\spark_duckdb.duckdb_extension" -ForegroundColor White
    Write-Host "  - DuckDB CLI: build\release\duckdb.exe" -ForegroundColor White
    Write-Host ""
    Write-Host "To test, run: .\build\release\duckdb.exe" -ForegroundColor Cyan

} catch {
    Write-Host ""
    Write-Host "Build failed" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
} finally {
    Pop-Location
}
