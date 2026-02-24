#pragma once
#include "sdk.h"
#include "logger.h"
#include "error_report.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string_view>

namespace http_server {

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;

using tcp = net::ip::tcp;
using namespace std::literals;

class SessionBase : public std::enable_shared_from_this<SessionBase> {
public:
    SessionBase(const SessionBase&) = delete;
    SessionBase& operator=(const SessionBase&) = delete;
    
    void Run();
    
    std::string GetRemoteIp() {
        try {
            return stream_.socket().remote_endpoint().address().to_string();
        } catch (const sys::system_error&) {
            return "";
        }
    }

    template <typename Body, typename Fields>
    void WriteResponse(http::response<Body, Fields>&& response) {
        Write(std::move(response));
    }

protected:
    explicit SessionBase(tcp::socket&& socket)
        : stream_(std::move(socket)) {
    }
    
    virtual ~SessionBase() = default;

    template <typename Body, typename Fields>
    void Write(http::response<Body, Fields>&& response) {
        auto safe_response = std::make_shared<http::response<Body, Fields>>(std::move(response));
        auto self = shared_from_this();
        auto write_start = boost::posix_time::microsec_clock::local_time();
        
        http::async_write(stream_, *safe_response,
            [safe_response, self, write_start](beast::error_code ec, std::size_t bytes_written) {
                if (ec) {
                    error_report::ReportError(ec, "write"sv);
                    return;
                }
                
                auto write_end = boost::posix_time::microsec_clock::local_time();
                auto duration = (write_end - write_start).total_milliseconds();
                
                BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage("response sent"sv,
                    logware::ResponseLogData<Body, Fields>(
                        self->GetRemoteIp(),
                        duration,
                        *safe_response
                    ));
                
                if (safe_response->need_eof()) {
                    self->Close();
                } else {
                    self->Read();
                }
            });
    }

    void SetReceivedRequestTime(boost::posix_time::ptime time) {
        received_request_moment_ = time;
    }

private:
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
    boost::posix_time::ptime received_request_moment_;

    void Read();
    void OnRead(beast::error_code ec, std::size_t bytes_read);
    void Close();
    
    virtual void HandleRequest(http::request<http::string_body>&& request) = 0;
};

template <typename RequestHandler>
class Session : public SessionBase {
public:
    template <typename Handler>
    Session(tcp::socket&& socket, Handler&& request_handler)
        : SessionBase(std::move(socket))
        , request_handler_(std::forward<Handler>(request_handler)) {
    }

private:
    RequestHandler request_handler_;

    void HandleRequest(http::request<http::string_body>&& request) override {
        SetReceivedRequestTime(boost::posix_time::microsec_clock::local_time());
        
        BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage("request received"sv,
            logware::RequestLogData(GetRemoteIp(), request));
        
        auto self = this->shared_from_this();
        request_handler_(std::move(request),
            [self](auto&& response) {
                self->WriteResponse(std::move(response));
            });
    }
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
public:
    template <typename Handler>
    Listener(net::io_context& ioc, const tcp::endpoint& endpoint, Handler&& request_handler)
        : ioc_(ioc)
        , acceptor_(net::make_strand(ioc))
        , request_handler_(std::forward<Handler>(request_handler)) {
        
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(net::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(net::socket_base::max_listen_connections);
    }

    void Run() {
        DoAccept();
    }
    
private:
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler request_handler_;

    void DoAccept() {
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
    }

    void OnAccept(sys::error_code ec, tcp::socket socket) {
        if (ec) {
            error_report::ReportError(ec, "accept"sv);
            DoAccept();
            return;
        }

        std::make_shared<Session<RequestHandler>>(std::move(socket), request_handler_)->Run();
        DoAccept();
    }
};

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
    using MyListener = Listener<std::decay_t<RequestHandler>>;
    std::make_shared<MyListener>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
}

}