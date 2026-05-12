# Build script for Spark DuckDB Extension
# Requires: Visual Studio with C++ tools, CMake

$ErrorActionPreference = "Stop"

# Check for CMake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "CMake not found. Please install CMake."
    exit 1
}

# Create build directory
New-Item -ItemType Directory -Force -Path "build\release" | Out-Null

# Configure
Write-Host "Configuring build..." -ForegroundColor Cyan
Push-Location "build\release"
cmake -DCMAKE_BUILD_TYPE=Release ..\..

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed"
    Pop-Location
    exit 1
}

# Build
Write-Host "Building extension..." -ForegroundColor Cyan
cmake --build . --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
    Pop-Location
    exit 1
}

Pop-Location

Write-Host "Build complete!" -ForegroundColor Green
Write-Host "Extension location: build\release\extension\spark_duckdb\spark_duckdb.duckdb_extension" -ForegroundColor Green
