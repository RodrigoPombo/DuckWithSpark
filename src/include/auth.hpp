#pragma once

#include <string>

namespace duckdb {

// Get access token from Azure CLI
std::string GetAzureCliToken();

// Get access token using service principal credentials
std::string GetServicePrincipalToken(const std::string &tenant_id,
                                    const std::string &client_id,
                                    const std::string &client_secret);

} // namespace duckdb
