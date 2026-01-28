#include "http_server.h"

#include <iostream>

namespace http_server {

using namespace std::literals;

void ReportError(beast::error_code ec, std::string_view where) {
    std::cerr << where << ": " << ec.message() << std::endl;
}

Session::Session(tcp::socket&& socket, RequestHandler request_handler)
    : stream_(std::move(socket))
    , request_handler_(std::move(request_handler)) {
}

void Session::Run() {
    net::dispatch(stream_.get_executor(),
        [self = shared_from_this()]() {
            self->Read();
        });
}

void Session::Read() {
    request_ = {};
    stream_.expires_after(30s);
    
    http::async_read(stream_, buffer_, request_,
        [self = shared_from_this()]
        (beast::error_code ec, std::size_t bytes_read) {
            self->OnRead(ec, bytes_read);
        });
}

void Session::OnRead(beast::error_code ec, std::size_t bytes_read) {
    if (ec == http::error::end_of_stream) {
        Close();
        return;
    }
    
    if (ec) {
        ReportError(ec, "read");
        return;
    }
    
    request_handler_(std::move(request_),
        [self = shared_from_this()](http::response<http::string_body>&& response) {
            auto safe_response = std::make_shared<http::response<http::string_body>>(std::move(response));
            
            http::async_write(self->stream_, *safe_response,
                [self, safe_response]
                (beast::error_code ec, std::size_t bytes_written) {
                    self->OnWrite(safe_response->need_eof(), ec, bytes_written);
                });
        });
}

void Session::OnWrite(bool close, beast::error_code ec, std::size_t bytes_written) {
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

void Session::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
}

Listener::Listener(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler request_handler)
    : ioc_(ioc)
    , acceptor_(net::make_strand(ioc))
    , request_handler_(std::move(request_handler)) {
    
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(net::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(net::socket_base::max_listen_connections);
}

void Listener::Run() {
    DoAccept();
}

void Listener::DoAccept() {
    acceptor_.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
}

void Listener::OnAccept(sys::error_code ec, tcp::socket socket) {
    if (ec) {
        ReportError(ec, "accept");
        return;
    }

    std::make_shared<Session>(std::move(socket), request_handler_)->Run();
    DoAccept();
}

void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
    std::make_shared<Listener>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
}

}  // namespace http_server