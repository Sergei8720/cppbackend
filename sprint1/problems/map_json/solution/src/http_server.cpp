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

template <typename Body, typename Fields>
void SessionBase::Write(http::response<Body, Fields>&& response) {
    auto safe_response = std::make_shared<http::response<Body, Fields>>(std::move(response));
    auto self = GetSharedThis();
    
    http::async_write(stream_, *safe_response,
                      [safe_response, self](beast::error_code ec, std::size_t bytes_written) {
                          self->OnWrite(safe_response->need_eof(), ec, bytes_written);
                      });
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

template <typename RequestHandler>
template <typename Handler>
Session<RequestHandler>::Session(tcp::socket&& socket, Handler&& request_handler)
    : SessionBase(std::move(socket))
    , request_handler_(std::forward<Handler>(request_handler)) {
}

template <typename RequestHandler>
std::shared_ptr<SessionBase> Session<RequestHandler>::GetSharedThis() {
    return this->shared_from_this();
}

template <typename RequestHandler>
void Session<RequestHandler>::HandleRequest(HttpRequest&& request) {
    request_handler_(std::move(request), [self = this->shared_from_this()](auto&& response) {
        self->Write(std::move(response));
    });
}

template <typename RequestHandler>
template <typename Handler>
Listener<RequestHandler>::Listener(net::io_context& ioc, const tcp::endpoint& endpoint, Handler&& request_handler)
    : ioc_(ioc)
    , acceptor_(net::make_strand(ioc))
    , request_handler_(std::forward<Handler>(request_handler)) {
    
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(net::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(net::socket_base::max_listen_connections);
}

template <typename RequestHandler>
void Listener<RequestHandler>::Run() {
    DoAccept();
}

template <typename RequestHandler>
void Listener<RequestHandler>::DoAccept() {
    acceptor_.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
}

template <typename RequestHandler>
void Listener<RequestHandler>::OnAccept(boost::system::error_code ec, tcp::socket socket) {
    if (ec) {
        ReportError(ec, "accept");
        return;
    }
    
    AsyncRunSession(std::move(socket));
    DoAccept();
}

template <typename RequestHandler>
void Listener<RequestHandler>::AsyncRunSession(tcp::socket&& socket) {
    std::make_shared<Session<RequestHandler>>(std::move(socket), request_handler_)->Run();
}

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
    using MyListener = Listener<std::decay_t<RequestHandler>>;
    std::make_shared<MyListener>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
}

// Явные инстанцирования
template class Session<http_handler::RequestHandler>;
template class Listener<http_handler::RequestHandler>;

}  // namespace http_server