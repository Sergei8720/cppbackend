#include "logging_data_storage.h"

namespace logware {

void tag_invoke(json::value_from_tag, json::value& jv, const RequestLogData& request) {
    jv = {
        {IP, request.ip},
        {URL, request.url},
        {METHOD, request.method}
    };
}

void tag_invoke(json::value_from_tag, json::value& jv, const ServerAddressLogData& server_address) {
    jv = {
        {PORT, server_address.port},
        {ADDRESS, server_address.address}
    };
}

void tag_invoke(json::value_from_tag, json::value& jv, const ExceptionLogData& exception) {
    jv = {
        {CODE, exception.code},
        {TEXT, std::string(exception.text)},
        {WHERE, std::string(exception.where)}
    };
}

void tag_invoke(json::value_from_tag, json::value& jv, const ExitCodeLogData& exit_code) {
    jv = {
        {CODE, exit_code.code}
    };
}

}