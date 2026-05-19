#include "auth.hpp"
#include "livy_client.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/types/vector.hpp"
#include "duckdb/main/client_context.hpp"

namespace duckdb {

static StatementResult ExecuteSparkSql(ClientContext &context,
                                       const std::string &sql_code) {
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

  // Create Livy client and execute the statement once.
  LivyClient client(workspace_id, lakehouse_id, access_token);
  SessionIdentifiers session = client.CreateSession();
  return client.ExecuteStatement(session, sql_code);
}

static void SparkExecuteScalarFunction(DataChunk &args, ExpressionState &state,
                                       Vector &result) {
  auto size = args.size();
  if (size == 0) {
    result.SetVectorType(VectorType::CONSTANT_VECTOR);
    ConstantVector::SetNull(result, true);
    return;
  }

  args.Flatten();
  if (FlatVector::IsNull(args.data[0], 0)) {
    result.SetVectorType(VectorType::CONSTANT_VECTOR);
    ConstantVector::SetNull(result, true);
    return;
  }

  auto sql_data = FlatVector::GetData<string_t>(args.data[0]);
  std::string sql_code = sql_data[0].GetString();

  std::string status_message;
  try {
    auto statement_result = ExecuteSparkSql(state.GetContext(), sql_code);
    if (statement_result.status == "ok") {
      status_message = "OK!";
    } else {
      status_message = "ERROR: " + statement_result.message;
    }
  } catch (std::exception &e) {
    status_message = "ERROR: " + std::string(e.what());
  }

  result.SetVectorType(VectorType::CONSTANT_VECTOR);
  ConstantVector::SetNull(result, false);
  ConstantVector::GetData<string_t>(result)[0] = StringVector::AddString(result, status_message);
}

ScalarFunction CreateSparkExecuteFunction() {
  ScalarFunction func("spark_execute", {LogicalType::VARCHAR}, LogicalType::VARCHAR,
                      SparkExecuteScalarFunction);
  func.stability = FunctionStability::VOLATILE;
  func.SetNullHandling(FunctionNullHandling::SPECIAL_HANDLING);
  return func;
}

} // namespace duckdb
