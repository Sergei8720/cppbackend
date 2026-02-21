#include "logging_data_storage.h"

namespace logware {

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const RequestLogData& data) {
  jv = {
      {"ip", data.ip},
      {"URI", data.uri},
      {"method", data.method}
  };
}

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const ResponseLogData& data) {
  boost::json::object obj;
  obj["response_time"] = data.response_time;
  obj["code"] = data.code;
  if (data.content_type.empty()) {
    obj["content_type"] = nullptr;
  } else {
    obj["content_type"] = data.content_type;
  }
  jv = std::move(obj);
}

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const ServerStartLogData& data) {
  jv = {
      {"port", data.port},
      {"address", data.address}
  };
}

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const ServerExitLogData& data) {
  boost::json::object obj;
  obj["code"] = data.code;
  if (data.exception.has_value()) {
    obj["exception"] = data.exception.value();
  }
  jv = std::move(obj);
}

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const ErrorLogData& data) {
  jv = {
      {"code", data.code},
      {"text", data.text},
      {"where", data.where}
  };
}

}  // namespace logware