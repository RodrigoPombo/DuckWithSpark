#include "auth.hpp"
#include "http_client.hpp"
#include "duckdb/common/exception.hpp"
#include "yyjson.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace duckdb {
using namespace duckdb_yyjson; // NOLINT

static constexpr const char *POWER_BI_SCOPE = "https://analysis.windows.net/powerbi/api/.default";

std::string GetAzureCliToken() {
  // Capture stderr so authentication failures include actionable Azure CLI details.
  std::string command = "az account get-access-token --scope " +
                        std::string(POWER_BI_SCOPE) +
                        " --query accessToken -o tsv 2>&1";

  auto *pipe = POPEN(command.c_str(), "r");
  if (!pipe) {
    throw IOException("Failed to invoke Azure CLI");
  }

  std::string output;
  char buffer[512];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }

  auto exit_code = PCLOSE(pipe);

  // Trim whitespace
  output.erase(output.find_last_not_of(" \n\r\t") + 1);
  output.erase(0, output.find_first_not_of(" \n\r\t"));

  if (exit_code != 0 || output.empty()) {
    if (!output.empty()) {
      throw IOException("Azure CLI token acquisition failed: %s", output.c_str());
    }
    throw IOException("Azure CLI token acquisition failed with empty output. Run 'az login'.");
  }

  return output;
}

static std::string UrlEncode(const std::string &value) {
  static const char *hex = "0123456789ABCDEF";
  std::string encoded;
  encoded.reserve(value.size() * 3);

  for (auto ch : value) {
    auto uch = static_cast<unsigned char>(ch);
    if ((uch >= 'A' && uch <= 'Z') || (uch >= 'a' && uch <= 'z') ||
        (uch >= '0' && uch <= '9') || uch == '-' || uch == '_' ||
        uch == '.' || uch == '~') {
      encoded.push_back(static_cast<char>(uch));
    } else {
      encoded.push_back('%');
      encoded.push_back(hex[(uch >> 4) & 0xF]);
      encoded.push_back(hex[uch & 0xF]);
    }
  }
  return encoded;
}

std::string GetServicePrincipalToken(const std::string &tenant_id,
                                    const std::string &client_id,
                                    const std::string &client_secret) {
  HttpClient client(30000); // 30 second timeout

  auto url = "https://login.microsoftonline.com/" + tenant_id + "/oauth2/v2.0/token";

  auto body = "grant_type=client_credentials&client_id=" + UrlEncode(client_id) +
              "&client_secret=" + UrlEncode(client_secret) +
              "&scope=" + UrlEncode(POWER_BI_SCOPE);

  HttpHeaders headers;
  headers.push_back({"Content-Type", "application/x-www-form-urlencoded"});

  auto response = client.Post(url, headers, body, "application/x-www-form-urlencoded");

  if (response.status != 200) {
    throw IOException("Service principal token acquisition failed (HTTP %d): %s",
                     response.status, response.body.c_str());
  }

  // Parse JSON response
  auto *doc = yyjson_read(response.body.c_str(), response.body.size(), 0);
  if (!doc) {
    throw IOException("Failed to parse token response JSON");
  }

  auto *root = yyjson_doc_get_root(doc);
  if (!root || !yyjson_is_obj(root)) {
    yyjson_doc_free(doc);
    throw IOException("Token response was not a JSON object");
  }

  auto *access_token_val = yyjson_obj_get(root, "access_token");
  if (!access_token_val || !yyjson_is_str(access_token_val)) {
    yyjson_doc_free(doc);
    throw IOException("Token response missing 'access_token' field");
  }

  std::string access_token = yyjson_get_str(access_token_val);
  yyjson_doc_free(doc);

  return access_token;
}

} // namespace duckdb
