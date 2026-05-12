#include "auth.hpp"
#include "livy_client.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include <duckdb/common/exception.hpp>
#include <duckdb/common/string_util.hpp>

namespace duckdb {

struct SparkExecuteBindData : public TableFunctionData {
  std::string sql_code;
};

static unique_ptr<FunctionData> SparkExecuteBind(ClientContext &context,
                                                 TableFunctionBindInput &input,
                                                 vector<LogicalType> &return_types,
                                                 vector<string> &names) {
  auto result = make_uniq<SparkExecuteBindData>();

  // Get SQL code from first parameter
  result->sql_code = input.inputs[0].GetValue<string>();

  // Define output schema
  names.emplace_back("status");
  return_types.emplace_back(LogicalType::VARCHAR);

  names.emplace_back("execution_time_sec");
  return_types.emplace_back(LogicalType::DOUBLE);

  names.emplace_back("message");
  return_types.emplace_back(LogicalType::VARCHAR);

  return std::move(result);
}

static void SparkExecuteFunction(ClientContext &context, TableFunctionInput &data_p,
                                 DataChunk &output) {
  auto &bind_data = data_p.bind_data->Cast<SparkExecuteBindData>();

  try {
    // Read session variables
    Value workspace_id_val;
    if (!context.TryGetCurrentSetting("spark_lakehouse_workspace_id", workspace_id_val) ||
        workspace_id_val.IsNull()) {
      throw InvalidInputException(
          "spark_lakehouse_workspace_id not set. Use: SET "
          "spark_lakehouse_workspace_id = 'your-workspace-id';");
    }
    std::string workspace_id = workspace_id_val.ToString();

    Value lakehouse_id_val;
    if (!context.TryGetCurrentSetting("spark_lakehouse_id", lakehouse_id_val) ||
        lakehouse_id_val.IsNull()) {
      throw InvalidInputException(
          "spark_lakehouse_id not set. Use: SET spark_lakehouse_id = "
          "'your-lakehouse-id';");
    }
    std::string lakehouse_id = lakehouse_id_val.ToString();

    Value auth_mode_val;
    if (!context.TryGetCurrentSetting("spark_auth_mode", auth_mode_val) ||
        auth_mode_val.IsNull()) {
      throw InvalidInputException(
          "spark_auth_mode not set. Use: SET spark_auth_mode = 'azure_cli';");
    }
    std::string auth_mode = StringUtil::Lower(auth_mode_val.ToString());

    // Get access token based on auth mode
    std::string access_token;
    if (auth_mode == "azure_cli") {
      access_token = GetAzureCliToken();
    } else if (auth_mode == "service_principal") {
      Value tenant_id_val, client_id_val, client_secret_val;
      if (!context.TryGetCurrentSetting("spark_tenant_id", tenant_id_val) ||
          !context.TryGetCurrentSetting("spark_client_id", client_id_val) ||
          !context.TryGetCurrentSetting("spark_client_secret", client_secret_val)) {
        throw InvalidInputException("Service principal auth requires: spark_tenant_id, "
                                   "spark_client_id, spark_client_secret");
      }
      access_token = GetServicePrincipalToken(tenant_id_val.ToString(),
                                             client_id_val.ToString(),
                                             client_secret_val.ToString());
    } else if (auth_mode == "access_token") {
      Value token_val;
      if (!context.TryGetCurrentSetting("spark_access_token", token_val)) {
        throw InvalidInputException(
            "access_token mode requires: spark_access_token");
      }
      access_token = token_val.ToString();
    } else {
      throw InvalidInputException("Invalid spark_auth_mode: " + auth_mode);
    }

    // Create Livy client
    LivyClient client(workspace_id, lakehouse_id, access_token);

    // Create or reuse session
    // TODO: Implement global session cache
    SessionIdentifiers session = client.CreateSession();

    // Execute statement
    auto result = client.ExecuteStatement(session, bind_data.sql_code);

    // Return result
    output.SetValue(0, 0, Value(result.status));
    output.SetValue(1, 0, Value::DOUBLE(result.execution_time_sec));
    output.SetValue(2, 0, Value(result.message));
    output.SetCardinality(1);

  } catch (std::exception &e) {
    output.SetValue(0, 0, Value("error"));
    output.SetValue(1, 0, Value::DOUBLE(0.0));
    output.SetValue(2, 0, Value(std::string(e.what())));
    output.SetCardinality(1);
  }
}

TableFunction CreateSparkExecuteFunction() {
  TableFunction func("spark_execute", {LogicalType::VARCHAR}, SparkExecuteFunction,
                    SparkExecuteBind);
  func.projection_pushdown = false;
  return func;
}

} // namespace duckdb
