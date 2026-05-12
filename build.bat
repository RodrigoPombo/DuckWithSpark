@echo off
echo Building Spark DuckDB Extension...
echo.

REM Set up MSVC environment
echo Setting up Visual Studio environment...
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if %errorlevel% neq 0 (
    echo ERROR: Failed to set up Visual Studio environment
    exit /b 1
)

REM Verify rc.exe is available
where rc.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: rc.exe not found. Please ensure Windows SDK is installed.
    echo You may need to install Windows SDK from Visual Studio Installer.
    exit /b 1
)

REM Get the short path (8.3 format) to avoid issues with spaces in "OneDrive - Microsoft"
for %%I in ("%~dp0.") do set "SHORT_PATH=%%~sI"
echo Working directory: %SHORT_PATH%

REM Add msys64 make to PATH
set PATH=C:\msys64\usr\bin;%PATH%

REM Change to short path and build
cd /d "%SHORT_PATH%"
make

echo.
echo Build complete!
echo Extension location: build\release\extension\spark_duckdb\spark_duckdb.duckdb_extension
