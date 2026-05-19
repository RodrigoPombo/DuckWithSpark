#define DUCKDB_EXTENSION_MAIN

#include "spark_duckdb_extension.hpp"
#include "duckdb/main/extension/extension_loader.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>
#include <duckdb/parser/parsed_data/create_table_function_info.hpp>

namespace duckdb {

// Forward declarations
ScalarFunction CreateSparkExecuteFunction();

static void LoadInternal(ExtensionLoader &loader) {
  auto &config = DBConfig::GetConfig(loader.GetDatabaseInstance());

  // Register session variables
  config.AddExtensionOption("spark_lakehouse_workspace_id",
                           "Workspace ID for Spark execution",
                           LogicalType::VARCHAR);

  config.AddExtensionOption("spark_lakehouse_id", "Lakehouse ID for Spark execution",
                           LogicalType::VARCHAR);

  config.AddExtensionOption("spark_auth_mode",
                           "Auth mode: azure_cli, service_principal, access_token",
                           LogicalType::VARCHAR);

  config.AddExtensionOption("spark_tenant_id", "Azure tenant ID (for service_principal)",
                           LogicalType::VARCHAR);

  config.AddExtensionOption("spark_client_id", "Azure client ID (for service_principal)",
                           LogicalType::VARCHAR);

  config.AddExtensionOption("spark_client_secret",
                           "Azure client secret (for service_principal)",
                           LogicalType::VARCHAR);

  config.AddExtensionOption("spark_access_token", "Access token (for access_token mode)",
                           LogicalType::VARCHAR);

  // Register scalar function for Spark execution status.
  loader.RegisterFunction(CreateSparkExecuteFunction());
}

void SparkDuckDBExtension::Load(ExtensionLoader &loader) {
  LoadInternal(loader);
}

std::string SparkDuckDBExtension::Name() { return "spark_duckdb"; }

std::string SparkDuckDBExtension::Version() const {
#ifdef EXT_VERSION_SPARK_DUCKDB
  return EXT_VERSION_SPARK_DUCKDB;
#else
  return "0.1.0";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(spark_duckdb, loader) {
  duckdb::LoadInternal(loader);
}

}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
