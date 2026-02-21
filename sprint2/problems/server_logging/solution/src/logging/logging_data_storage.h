#pragma once

#include <boost/beast/http.hpp>
#include <boost/date_time.hpp>
#include <boost/json.hpp>
#include <string>
#include <string_view>
#include <unordered_map>

namespace logware {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

using HttpRequest = http::request<http::string_body>;
using namespace std::literals;

inline const std::string kIp = "ip";
inline const std::string kUrl = "URI";
inline const std::string kMethod = "method";
inline const std::string kResponseTime = "response_time";
inline const std::string kCode = "code";
inline const std::string kContentType = "content_type";
inline const std::string kPort = "port";
inline const std::string kAddress = "address";
inline const std::string kText = "text";
inline const std::string kWhere = "where";
inline const std::string kTimestamp = "timestamp";
inline const std::string kData = "data";
inline const std::string kMessage = "message";

struct RequestLogData {
  RequestLogData(std::string ip_addr, const HttpRequest& req)
      : ip(ip_addr), url(req.target()), method(req.method_string()) {}

  std::string ip;
  std::string url;
  std::string method;
};

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                const RequestLogData& request);

template <typename Body, typename Fields>
struct ResponseLogData {
  ResponseLogData(std::string ip_addr, long res_time,
                  const http::response<Body, Fields>& res)
      : ip(ip_addr),
        response_time(res_time),
        code(res.result_int()),
        content_type(res[http::field::content_type]) {}

  std::string ip;
  long response_time;
  int code;
  std::string content_type;
};

template <typename Body, typename Fields>
void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                const ResponseLogData<Body, Fields>& response) {
  jv = {{kIp, json::value_from(response.ip)},
        {kResponseTime, json::value_from(response.response_time)},
        {kCode, json::value_from(response.code)},
        {kContentType, json::value_from(response.content_type)}};
}

struct ServerAddressLogData {
  ServerAddressLogData(std::string addr, uint32_t prt)
      : address(addr), port(prt) {}

  std::string address;
  uint32_t port;
};

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                const ServerAddressLogData& server_address);

struct ExceptionLogData {
  ExceptionLogData(int code, std::string_view text, std::string_view where)
      : code(code), text(text), where(where) {}

  int code;
  std::string_view text;
  std::string_view where;
};

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                const ExceptionLogData& exception);

struct ExitCodeLogData {
  int code;
};

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                ExitCodeLogData const& exit_code);

template <class T>
struct LogMessage {
  LogMessage(std::string_view msg, T&& custom_data)
      : message(msg), data(std::forward<T>(custom_data)) {
    timestamp = boost::posix_time::to_iso_extended_string(
        boost::posix_time::microsec_clock::local_time());
  }

  std::string_view message;
  T data;
  std::string timestamp;
};

template <class T>
void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                const LogMessage<T>& msg) {
  jv = {{kTimestamp, json::value_from(msg.timestamp)},
        {kData, json::value_from(msg.data)},
        {kMessage, json::value_from(msg.message)}};
}

}