#pragma once

#include "http_client.hpp"
#include <string>

namespace duckdb {

struct SessionIdentifiers {
  std::string hc_session_id;
  std::string livy_session_id;
  std::string repl_id;
  std::string state;
};

struct StatementResult {
  int64_t statement_id;
  std::string state;
  std::string status;
  double execution_time_sec;
  std::string message;
};

class LivyClient {
public:
  LivyClient(const std::string &workspace_id, const std::string &lakehouse_id,
             const std::string &access_token);

  SessionIdentifiers CreateSession();
  void DestroySession(const std::string &hc_session_id);
  StatementResult ExecuteStatement(const SessionIdentifiers &session,
                                   const std::string &code);
  int64_t SubmitStatement(const SessionIdentifiers &session,
                         const std::string &code);
  StatementResult PollStatement(const SessionIdentifiers &session,
                               int64_t statement_id);

private:
  std::string workspace_id_;
  std::string lakehouse_id_;
  std::string access_token_;
  std::string base_url_;
  static constexpr int timeout_seconds_ = 300;
  static constexpr int poll_interval_seconds_ = 5;

  std::string SessionsUrl() const;
  std::string SessionUrl(const std::string &hc_session_id) const;
  std::string StatementUrl(const std::string &livy_session_id,
            const std::string &repl_id) const;
  std::string StatementStatusUrl(const std::string &livy_session_id,
              const std::string &repl_id,
              int64_t statement_id) const;

  SessionIdentifiers PollSessionState(const std::string &hc_session_id,
                                     const std::string &target_state);
};

} // namespace duckdb
