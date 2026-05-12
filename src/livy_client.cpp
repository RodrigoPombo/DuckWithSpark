#include "livy_client.hpp"
#include "yyjson.hpp"
#include <chrono>
#include <sstream>
#include <thread>

namespace duckdb {

static constexpr const char *API_VERSION = "2023-12-01";

LivyClient::LivyClient(const std::string &workspace_id,
                       const std::string &lakehouse_id,
                       const std::string &access_token)
    : workspace_id_(workspace_id), lakehouse_id_(lakehouse_id),
      access_token_(access_token),
      base_url_("https://api.fabric.microsoft.com") {}

std::string LivyClient::SessionsUrl() const {
  std::ostringstream oss;
  oss << base_url_ << "/v1/workspaces/" << workspace_id_ << "/lakehouses/"
      << lakehouse_id_ << "/livyapi/versions/" << API_VERSION
      << "/highConcurrencySessions";
  return oss.str();
}

std::string LivyClient::SessionUrl(const std::string &hc_session_id) const {
  return SessionsUrl() + "/" + hc_session_id;
}

std::string LivyClient::StatementUrl(const std::string &livy_session_id,
                                    int64_t repl_id) const {
  std::ostringstream oss;
  oss << SessionsUrl() << "/" << livy_session_id << "/repls/" << repl_id
      << "/statements";
  return oss.str();
}

std::string
LivyClient::StatementStatusUrl(const std::string &livy_session_id,
                              int64_t repl_id, int64_t statement_id) const {
  std::ostringstream oss;
  oss << StatementUrl(livy_session_id, repl_id) << "/" << statement_id;
  return oss.str();
}

SessionIdentifiers LivyClient::CreateSession() {
  HttpClient client;

  // Use deterministic session tag for reuse
  std::string session_tag = "duckdb_" + workspace_id_.substr(0, 8) + "_" +
                           lakehouse_id_.substr(0, 8);

  // Build JSON payload
  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);
  yyjson_mut_obj_add_strcpy(doc, root, "sessionTag", session_tag.c_str());

  const char *json_str = yyjson_mut_write(doc, 0, nullptr);
  std::string payload(json_str);
  free((void *)json_str);
  yyjson_mut_doc_free(doc);

  // POST to create session
  Headers headers;
  headers["Authorization"] = "Bearer " + access_token_;
  headers["Content-Type"] = "application/json";

  auto response = client.Post(SessionsUrl(), headers, payload);

  if (response.status_code != 202) {
    throw std::runtime_error("Create session failed (" +
                           std::to_string(response.status_code) +
                           "): " + response.body);
  }

  // Parse response
  yyjson_doc *resp_doc = yyjson_read(response.body.c_str(), response.body.size(), 0);
  if (!resp_doc) {
    throw std::runtime_error("Failed to parse session response");
  }

  yyjson_val *resp_root = yyjson_doc_get_root(resp_doc);
  yyjson_val *id_val = yyjson_obj_get(resp_root, "id");
  if (!id_val || !yyjson_is_str(id_val)) {
    yyjson_doc_free(resp_doc);
    throw std::runtime_error("Session response missing 'id'");
  }

  std::string hc_session_id = yyjson_get_str(id_val);
  yyjson_doc_free(resp_doc);

  // Poll until Idle
  auto ready_session = PollSessionState(hc_session_id, "Idle");
  return ready_session;
}

void LivyClient::DestroySession(const std::string &hc_session_id) {
  HttpClient client;
  Headers headers;
  headers["Authorization"] = "Bearer " + access_token_;

  auto response = client.Delete(SessionUrl(hc_session_id), headers);
  if (response.status_code < 200 || response.status_code >= 300) {
    throw std::runtime_error("Delete session failed (" +
                           std::to_string(response.status_code) + ")");
  }
}

int64_t LivyClient::SubmitStatement(const SessionIdentifiers &session,
                                   const std::string &code) {
  HttpClient client;

  // Build JSON payload
  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);
  yyjson_mut_obj_add_strcpy(doc, root, "code", code.c_str());
  yyjson_mut_obj_add_strcpy(doc, root, "kind", "sql");

  const char *json_str = yyjson_mut_write(doc, 0, nullptr);
  std::string payload(json_str);
  free((void *)json_str);
  yyjson_mut_doc_free(doc);

  // POST statement
  Headers headers;
  headers["Authorization"] = "Bearer " + access_token_;
  headers["Content-Type"] = "application/json";

  std::string url = StatementUrl(session.livy_session_id, session.repl_id);
  auto response = client.Post(url, headers, payload);

  if (response.status_code != 200) {
    throw std::runtime_error("Submit statement failed (" +
                           std::to_string(response.status_code) +
                           "): " + response.body);
  }

  // Parse response
  yyjson_doc *resp_doc = yyjson_read(response.body.c_str(), response.body.size(), 0);
  if (!resp_doc) {
    throw std::runtime_error("Failed to parse statement response");
  }

  yyjson_val *resp_root = yyjson_doc_get_root(resp_doc);
  yyjson_val *id_val = yyjson_obj_get(resp_root, "id");
  if (!id_val || !yyjson_is_int(id_val)) {
    yyjson_doc_free(resp_doc);
    throw std::runtime_error("Statement response missing 'id'");
  }

  int64_t statement_id = yyjson_get_int(id_val);
  yyjson_doc_free(resp_doc);

  return statement_id;
}

StatementResult LivyClient::PollStatement(const SessionIdentifiers &session,
                                         int64_t statement_id) {
  HttpClient client;
  Headers headers;
  headers["Authorization"] = "Bearer " + access_token_;

  std::string url =
      StatementStatusUrl(session.livy_session_id, session.repl_id, statement_id);

  auto start = std::chrono::steady_clock::now();

  while (true) {
    auto response = client.Get(url, headers);

    if (response.status_code != 200) {
      throw std::runtime_error("Poll statement failed (" +
                             std::to_string(response.status_code) + ")");
    }

    // Parse response
    yyjson_doc *doc = yyjson_read(response.body.c_str(), response.body.size(), 0);
    if (!doc) {
      throw std::runtime_error("Failed to parse statement status");
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *state_val = yyjson_obj_get(root, "state");
    std::string state = yyjson_is_str(state_val) ? yyjson_get_str(state_val) : "";

    if (state == "available") {
      auto end = std::chrono::steady_clock::now();
      double elapsed =
          std::chrono::duration<double>(end - start).count();

      StatementResult result;
      result.statement_id = statement_id;
      result.state = state;
      result.execution_time_sec = elapsed;

      yyjson_val *output_val = yyjson_obj_get(root, "output");
      if (output_val) {
        yyjson_val *status_val = yyjson_obj_get(output_val, "status");
        result.status =
            yyjson_is_str(status_val) ? yyjson_get_str(status_val) : "unknown";

        if (result.status == "error") {
          yyjson_val *evalue = yyjson_obj_get(output_val, "evalue");
          result.message =
              yyjson_is_str(evalue) ? yyjson_get_str(evalue) : "Unknown error";
        } else {
          result.message = "Statement completed";
        }
      } else {
        result.status = "unknown";
        result.message = "No output available";
      }

      yyjson_doc_free(doc);
      return result;
    }

    yyjson_doc_free(doc);

    // Check timeout
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() >
        timeout_seconds_) {
      throw std::runtime_error("Timeout waiting for statement " +
                             std::to_string(statement_id));
    }

    std::this_thread::sleep_for(std::chrono::seconds(poll_interval_seconds_));
  }
}

StatementResult LivyClient::ExecuteStatement(const SessionIdentifiers &session,
                                            const std::string &code) {
  int64_t statement_id = SubmitStatement(session, code);
  return PollStatement(session, statement_id);
}

SessionIdentifiers
LivyClient::PollSessionState(const std::string &hc_session_id,
                            const std::string &target_state) {
  HttpClient client;
  Headers headers;
  headers["Authorization"] = "Bearer " + access_token_;

  std::string url = SessionUrl(hc_session_id);
  auto start = std::chrono::steady_clock::now();

  while (true) {
    auto response = client.Get(url, headers);

    if (response.status_code != 200) {
      throw std::runtime_error("Poll session failed (" +
                             std::to_string(response.status_code) + ")");
    }

    // Parse response
    yyjson_doc *doc = yyjson_read(response.body.c_str(), response.body.size(), 0);
    if (!doc) {
      throw std::runtime_error("Failed to parse session status");
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *state_val = yyjson_obj_get(root, "state");
    std::string state = yyjson_is_str(state_val) ? yyjson_get_str(state_val) : "";

    if (state == target_state) {
      SessionIdentifiers result;
      result.hc_session_id = hc_session_id;
      result.state = state;

      yyjson_val *session_id_val = yyjson_obj_get(root, "sessionId");
      if (yyjson_is_str(session_id_val)) {
        result.livy_session_id = yyjson_get_str(session_id_val);
      }

      yyjson_val *repl_id_val = yyjson_obj_get(root, "replId");
      if (yyjson_is_int(repl_id_val)) {
        result.repl_id = yyjson_get_int(repl_id_val);
      }

      yyjson_doc_free(doc);
      return result;
    }

    yyjson_doc_free(doc);

    // Check timeout
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() >
        timeout_seconds_) {
      throw std::runtime_error("Timeout waiting for session state '" +
                             target_state + "'. Last state: '" + state + "'");
    }

    std::this_thread::sleep_for(std::chrono::seconds(poll_interval_seconds_));
  }
}

} // namespace duckdb
