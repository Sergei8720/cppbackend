#include "http_server.h"
#include <iostream>

namespace http_server {

void ReportError(beast::error_code ec, std::string_view what) {
    std::cerr << what << ": " << ec.message() << std::endl;
}

Session::Session(tcp::socket&& socket, const RequestHandler& handle_request)
    : stream_(std::move(socket))
    , handle_request_(handle_request) {
}

void Session::Run() {
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&Session::Read,
                  shared_from_this()));
}

void Session::Read() {
    request_ = {};
    stream_.expires_after(std::chrono::seconds(30));
    http::async_read(stream_, buffer_, request_,
                     beast::bind_front_handler(&Session::OnRead,
                     shared_from_this()));
}

void Session::OnRead(beast::error_code ec, std::size_t bytes_read) {
    if (ec == http::error::end_of_stream) {
        return Close();
    }
    if (ec) {
        return ReportError(ec, "read");
    }
    HandleRequest();
}

void Session::OnWrite(bool close, beast::error_code ec, std::size_t bytes_written) {
    if (ec) {
        return ReportError(ec, "write");
    }
    if (close) {
        return Close();
    }
    Read();
}

void Session::HandleRequest() {
    response_ = handle_request_(std::move(request_));
    http::async_write(stream_, response_,
                      beast::bind_front_handler(&Session::OnWrite,
                      shared_from_this(), response_.need_eof()));
}

void Session::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    
    // Логирование ошибки, если она произошла (но не критично)
    if (ec && ec != beast::errc::not_connected) {
        std::cerr << "Session close error: " << ec.message() << std::endl;
    }
}

void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, 
               const RequestHandler& handle_request) {
    auto listener = std::make_shared<Listener>(ioc, endpoint, handle_request);
    listener->Run();
}

Listener::Listener(net::io_context& ioc, const tcp::endpoint& endpoint, 
                   const RequestHandler& handle_request)
    : ioc_(ioc)
    , acceptor_(net::make_strand(ioc))
    , handle_request_(handle_request) {
    beast::error_code ec;
    
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
        ReportError(ec, "open");
        return;
    }
    
    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
        ReportError(ec, "set_option");
        return;
    }
    
    acceptor_.bind(endpoint, ec);
    if (ec) {
        ReportError(ec, "bind");
        return;
    }
    
    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
        ReportError(ec, "listen");
        return;
    }
}

void Listener::Run() {
    DoAccept();
}

void Listener::DoAccept() {
    acceptor_.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(&Listener::OnAccept, shared_from_this()));
}

void Listener::OnAccept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
        ReportError(ec, "accept");
    } else {
        std::make_shared<Session>(std::move(socket), handle_request_)->Run();
    }
    DoAccept();
}

}  // namespace http_server