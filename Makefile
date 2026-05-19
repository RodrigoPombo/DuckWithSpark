PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Configuration of extension
EXT_NAME=spark_duckdb
EXT_CONFIG=${PROJ_DIR}extension_config.cmake

# Include the Makefile from extension-ci-tools
include extension-ci-tools/makefiles/duckdb_extension.Makefile

# Some repos do not have a top-level test directory.
# Keep format targets compatible with both layouts.
FORMAT_DIRECTORIES := src $(if $(wildcard test),test,)

format-check:
	$(MAKE) -C duckdb format-check T="--workdir $$PWD --directories $(FORMAT_DIRECTORIES)"

format-fix:
	$(MAKE) -C duckdb format-fix T="--workdir $$PWD --directories $(FORMAT_DIRECTORIES)"

format-main:
	$(MAKE) -C duckdb format-main T="--workdir $$PWD --directories $(FORMAT_DIRECTORIES)"
