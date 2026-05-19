#pragma once

#include "duckdb.hpp"

namespace duckdb {

class SparkDuckDBExtension : public Extension {
public:
  void Load(ExtensionLoader &loader) override;
  std::string Name() override;
  std::string Version() const override;
};

// DuckDB generated loader expects this exact CamelCase for static loading.
using SparkDuckdbExtension = SparkDuckDBExtension;

} // namespace duckdb
