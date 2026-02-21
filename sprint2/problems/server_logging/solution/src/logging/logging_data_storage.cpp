#include "logging_data_storage.h"

namespace logware {

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                const RequestLogData& request) {
  jv = {{kIp, json::value_from(request.ip)},
        {kUrl, json::value_from(request.url)},
        {kMethod, json::value_from(request.method)}};
}

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                const ServerAddressLogData& server_address) {
  jv = {{kPort, json::value_from(server_address.port)},
        {kAddress, json::value_from(server_address.address)}};
}

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                const ExceptionLogData& exception) {
  jv = {{kCode, json::value_from(exception.code)},
        {kText, json::value_from(exception.text)},
        {kWhere, json::value_from(exception.where)}};
}

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                const ExitCodeLogData& exit_code) {
  jv = {{kCode, json::value_from(exit_code.code)}};
}

}  // namespace logware