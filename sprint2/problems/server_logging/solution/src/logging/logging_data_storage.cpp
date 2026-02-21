#include "logging_data_storage.h"

namespace logware {

void tag_invoke(json::value_from_tag, json::value& jv, const RequestLogData& data) {
  jv = {
      {kIp, data.ip},
      {kUri, data.uri},
      {kMethod, data.method}
  };
}

void tag_invoke(json::value_from_tag, json::value& jv, const ResponseLogData& data) {
  json::object obj;
  obj[kResponseTime] = data.response_time;
  obj[kCode] = data.code;
  if (data.content_type.empty()) {
    obj[kContentType] = nullptr;
  } else {
    obj[kContentType] = data.content_type;
  }
  jv = std::move(obj);
}

void tag_invoke(json::value_from_tag, json::value& jv, const ServerStartLogData& data) {
  jv = {
      {kPort, data.port},
      {kAddress, data.address}
  };
}

void tag_invoke(json::value_from_tag, json::value& jv, const ServerExitLogData& data) {
  json::object obj;
  obj[kCode] = data.code;
  if (data.exception.has_value()) {
    obj[kException] = data.exception.value();
  }
  jv = std::move(obj);
}

void tag_invoke(json::value_from_tag, json::value& jv, const ErrorLogData& data) {
  jv = {
      {kCode, data.code},
      {kText, data.text},
      {kWhere, data.where}
  };
}

}  // namespace logware