#include "http_server.h"
#include "logger.h"
#include "json_key_storage.h"

namespace http_server {

using namespace std::literals;

void ReportError(beast::error_code ec, std::string_view where) {
    BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error",
        logware::ExceptionLogData(ec.value(), ec.message(), where));
}

void SessionBase::Run() {
    net::dispatch(stream_.get_executor(),
        beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

void SessionBase::Read() {
    request_ = {};
    stream_.expires_after(30s);
    http::async_read(stream_, buffer_, request_,
        beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
}

void SessionBase::OnRead(beast::error_code ec, std::size_t bytes_read) {
    if (ec == http::error::end_of_stream) {
        Close();
        return;
    }
    if (ec) {
        ReportError(ec, "read");
        return;
    }
    HandleRequest(std::move(request_));
}

void SessionBase::OnWrite(bool close, beast::error_code ec, std::size_t bytes_written) {
    if (ec) {
        ReportError(ec, "write");
        return;
    }
    if (close) {
        Close();
        return;
    }
    Read();
}

void SessionBase::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
}

}