#include "http_server.h"

#include <iostream>

#include "logger.h"

namespace http_server {

using namespace std::literals;

void ReportError(beast::error_code ec, std::string_view where) {
  BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage(
      "error"sv, logware::ExceptionLogData(0, ec.message(), ec.what()));
}

void SessionBase::Run() {
  net::dispatch(stream_.get_executor(),
                beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

void SessionBase::Read() {
  request_ = {};
  stream_.expires_after(30s);
  http::async_read(stream_, buffer_, request_,
                   beast::bind_front_handler(&SessionBase::OnRead,
                                             GetSharedThis()));
}

void SessionBase::OnRead(beast::error_code ec,
                         [[maybe_unused]] std::size_t bytes_read) {
  if (ec == http::error::end_of_stream) {
    Close();
    return;
  }
  if (ec) {
    ReportError(ec, "read"sv);
    return;
  }
  HandleRequest(std::move(request_));
}

void SessionBase::OnWrite(bool close, beast::error_code ec,
                          [[maybe_unused]] std::size_t bytes_written) {
  if (ec) {
    ReportError(ec, "write"sv);
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

const std::string& SessionBase::GetRemoteIp() {
  static std::string remote_ip;
  try {
    auto temp = stream_.socket().remote_endpoint().address().to_string();
    remote_ip = temp;
  } catch (...) {
  }
  return remote_ip;
}

}