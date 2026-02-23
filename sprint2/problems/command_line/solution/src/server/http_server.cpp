#include "http_server.h"

namespace http_server {

void SessionBase::Run() {
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read, shared_from_this()));
}

void SessionBase::Read() {
    request_ = {};
    stream_.expires_after(30s);
    
    http::async_read(stream_, buffer_, request_,
                     beast::bind_front_handler(&SessionBase::OnRead, shared_from_this()));
}

void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
    if (ec == http::error::end_of_stream) {
        Close();
        return;
    }
    
    if (ec) {
        error_report::ReportError(ec, "read"sv);
        return;
    }
    
    HandleRequest(std::move(request_));
}

void SessionBase::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    stream_.close(ec);
}

}