# Build Instructions for Spark DuckDB Extension

This extension follows the [DuckDB Extension Template](https://github.com/duckdb/extension-template) pattern.

## Prerequisites

### Required

1. **CMake** (3.5 or higher)
   ```powershell
   winget install Kitware.CMake
   ```

2. **Visual Studio 2022 with C++ Tools**
   - Download: https://visualstudio.microsoft.com/downloads/
   - Select: "Desktop development with C++" workload
   - Or use Build Tools for Visual Studio 2022

3. **Git** (for submodules)

### Recommended for Faster Builds

- **Ninja**: Fast build system
  ```powershell
  winget install Ninja-build.Ninja
  ```

- **ccache**: Compilation cache
  ```powershell
  winget install ccache.ccache
  ```

### Optional: OpenSSL

OpenSSL is required for HTTPS support. The build will attempt to find it automatically.

**Using vcpkg (recommended):**
```powershell
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install openssl:x64-windows
.\vcpkg integrate install
```

## Building

### Quick Build (Windows)

```powershell
cd C:\Users\ropombo\OneDrive - Microsoft\Documents\work_pm\LivyAPIDuckDB\spark_duckdb_cpp
.\build_windows.ps1
```

This will:
1. Check prerequisites
2. Configure the build
3. Compile DuckDB and the extension
4. Output binaries to `build/release/`

### Manual Build (Cross-Platform)

Following the extension template pattern:

```bash
# Initialize submodules (if not already done)
git submodule update --init --recursive

# Build with Make (requires make, cmake, and c++ compiler)
make

# Or build with Ninja (faster, recommended)
GEN=ninja make
```

The build will produce:
- `build/release/duckdb` - DuckDB CLI with extension pre-loaded
- `build/release/extension/spark_duckdb/spark_duckdb.duckdb_extension` - Loadable extension binary

### Build Configuration

The extension build is configured via:
- `Makefile` - Points to extension-ci-tools build system
- `extension_config.cmake` - Specifies extension source directory
- `CMakeLists.txt` - Extension-specific build configuration

## Testing

### Using Pre-Loaded DuckDB CLI

The built `duckdb` binary has the extension already loaded:

```bash
./build/release/duckdb
```

```sql
-- Extension is already loaded, just configure
SET spark_lakehouse_workspace_id = '74e51969-0e25-4d5c-9b97-29ccdf12fcf1';
SET spark_lakehouse_id = '1bf0fd14-f77f-4e50-aff2-e25ed0116357';
SET spark_auth_mode = 'azure_cli';

-- Test it
SELECT * FROM spark_execute('SELECT 1 as test');
```

### Loading Extension Manually

```sql
-- Start any DuckDB instance
duckdb

-- Load the extension
LOAD 'build/release/extension/spark_duckdb/spark_duckdb.duckdb_extension';

-- Configure and use
SET spark_lakehouse_workspace_id = 'your-workspace-id';
SET spark_lakehouse_id = 'your-lakehouse-id';
SET spark_auth_mode = 'azure_cli';

SELECT * FROM spark_execute('
  CREATE TABLE test_table AS
  SELECT 1 as id, ''test'' as name
');
```

## Troubleshooting

### CMake not found

Restart your terminal after installing CMake, or manually add to PATH:
```powershell
$env:PATH += ";C:\Program Files\CMake\bin"
```

### MSVC not found

Use "Developer PowerShell for VS 2022" or "Developer Command Prompt for VS 2022" instead of regular PowerShell.

Or initialize the environment:
```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1"
```

### OpenSSL not found

If using vcpkg:
```powershell
# Make sure vcpkg integration is active
C:\vcpkg\vcpkg integrate install

# Add toolchain file to CMake
cmake -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ...
```

If using manual OpenSSL install:
```powershell
$env:OPENSSL_ROOT_DIR = "C:\Program Files\OpenSSL-Win64"
```

### Submodule not initialized

```bash
git submodule update --init --recursive
```

### Build takes too long

Install Ninja and ccache for faster builds:
```powershell
winget install Ninja-build.Ninja
winget install ccache.ccache
```

Then build with:
```bash
GEN=ninja make
```

## Development

### Project Structure

```
spark_duckdb_cpp/
├── Makefile                       # Build entry point
├── extension_config.cmake         # Extension configuration
├── CMakeLists.txt                 # Extension build rules
├── src/
│   ├── include/                   # Header files
│   └── *.cpp                      # Implementation
├── duckdb/                        # DuckDB submodule
├── extension-ci-tools/            # Build system submodule
└── build/                         # Build output (generated)
```

### Making Changes

1. Edit source files in `src/`
2. Rebuild: `make` or `GEN=ninja make`
3. Test: `./build/release/duckdb`

The extension is automatically linked into the built DuckDB binary, so no manual loading is needed during development.

## Distribution

See the [DuckDB Extension Template docs](https://github.com/duckdb/extension-template) for information on:
- Submitting to community extensions
- Setting up CI/CD
- Versioning and releases
