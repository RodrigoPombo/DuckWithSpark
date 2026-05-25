#include "auth.hpp"
#include "livy_client.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/types/vector.hpp"
#include "duckdb/main/client_context.hpp"
#include <mutex>
#include <unordered_map>

namespace duckdb {

struct CachedSparkSession {
  std::string config_key;
  SessionIdentifiers session;
};

static std::mutex g_session_cache_mutex;
static std::unordered_map<ClientContext *, CachedSparkSession> g_session_cache;

static std::string BuildSessionConfigKey(const std::string &workspace_id,
                                         const std::string &lakehouse_id,
                                         const std::string &environment_id,
                                         const std::string &auth_mode) {
  return workspace_id + "|" + lakehouse_id + "|" + environment_id + "|" +
         auth_mode;
}

static bool SessionLooksInvalid(const std::string &error_message) {
  return error_message.find("Invalid session identifiers") != std::string::npos ||
         error_message.find("Poll statement failed") != std::string::npos ||
         error_message.find("Submit statement failed") != std::string::npos ||
         error_message.find("Session entered terminal failure state") !=
             std::string::npos;
}

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

  std::string environment_id;
  Value environment_id_val;
  if (context.TryGetCurrentSetting("spark_environment_id", environment_id_val) &&
      !environment_id_val.IsNull()) {
    environment_id = environment_id_val.ToString();
    StringUtil::Trim(environment_id);
  }

  // Create Livy client and execute the statement once.
  LivyClient client(workspace_id, lakehouse_id, access_token, environment_id);
  const auto session_config_key =
      BuildSessionConfigKey(workspace_id, lakehouse_id, environment_id, auth_mode);

  SessionIdentifiers session;
  bool used_cached_session = false;
  {
    std::lock_guard<std::mutex> lock(g_session_cache_mutex);
    auto cached_it = g_session_cache.find(&context);
    if (cached_it != g_session_cache.end() &&
        cached_it->second.config_key == session_config_key &&
        !cached_it->second.session.livy_session_id.empty() &&
        !cached_it->second.session.repl_id.empty()) {
      session = cached_it->second.session;
      used_cached_session = true;
    }
  }

  if (!used_cached_session) {
    session = client.CreateSession();
    std::lock_guard<std::mutex> lock(g_session_cache_mutex);
    g_session_cache[&context] = CachedSparkSession{session_config_key, session};
  }

  try {
    return client.ExecuteStatement(session, sql_code);
  } catch (const std::exception &ex) {
    if (!used_cached_session || !SessionLooksInvalid(ex.what())) {
      throw;
    }

    // Cached session can expire or be evicted remotely. Recreate once and retry.
    session = client.CreateSession();
    {
      std::lock_guard<std::mutex> lock(g_session_cache_mutex);
      g_session_cache[&context] = CachedSparkSession{session_config_key, session};
    }
    return client.ExecuteStatement(session, sql_code);
  }
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

  auto statement_result = ExecuteSparkSql(state.GetContext(), sql_code);
  if (statement_result.status != "ok") {
    throw InvalidInputException(statement_result.message);
  }

  result.SetVectorType(VectorType::CONSTANT_VECTOR);
  ConstantVector::SetNull(result, false);
  ConstantVector::GetData<string_t>(result)[0] =
      StringVector::AddString(result, "Spark got ducked, Congrats!");
}

ScalarFunction CreateSparkExecuteFunction() {
  ScalarFunction func("spark_execute", {LogicalType::VARCHAR}, LogicalType::VARCHAR,
                      SparkExecuteScalarFunction);
  func.stability = FunctionStability::VOLATILE;
  return func;
}

} // namespace duckdb
