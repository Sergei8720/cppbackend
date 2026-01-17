#include "http_server.h"

#include <iostream>

namespace http_server {

using namespace std::literals;

void ReportError(beast::error_code ec, std::string_view where) {
    std::cerr << where << ": " << ec.message() << std::endl;
}

SessionBase::SessionBase(tcp::socket&& socket) 
    : stream_(std::move(socket)) {
}

void SessionBase::Run() {
    net::dispatch(stream_.get_executor(),
        [self = shared_from_this()]() {
            self->Read();
        });
}

void SessionBase::Read() {
    request_ = {};
    stream_.expires_after(30s);
    
    http::async_read(stream_, buffer_, request_,
        [self = shared_from_this()]
        (beast::error_code ec, std::size_t bytes_read) {
            self->OnRead(ec, bytes_read);
        });
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

}  // namespace http_server