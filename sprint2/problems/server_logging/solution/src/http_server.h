#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <chrono>
#include <memory>
#include <string>

#include "logger.h"
#include "sdk.h"

namespace http_server {

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;

using tcp = net::ip::tcp;
using namespace std::literals;

void ReportError(beast::error_code ec, std::string_view what);

class SessionBase : public std::enable_shared_from_this<SessionBase> {
 public:
  SessionBase(const SessionBase&) = delete;
  SessionBase& operator=(const SessionBase&) = delete;

  void Run();

  template <typename Body, typename Fields>
  void Write(http::response<Body, Fields>&& response) {
    auto safe_response =
        std::make_shared<http::response<Body, Fields>>(std::move(response));
    auto start_time = std::chrono::steady_clock::now();

    http::async_write(
        stream_, *safe_response,
        [self = shared_from_this(), safe_response, start_time](
            beast::error_code ec, std::size_t bytes_written) {
          if (!ec) {
            auto end_time = std::chrono::steady_clock::now();
            auto response_time = 
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time).count();
            
            logware::ResponseLogData response_data;
            response_data.response_time = response_time;
            response_data.code = safe_response->result_int();
            
            auto content_type_it = safe_response->find(http::field::content_type);
            if (content_type_it != safe_response->end()) {
              response_data.content_type = std::string(content_type_it->value());
            } else {
              response_data.content_type = "";
            }
            
            BOOST_LOG_TRIVIAL(info) 
                << logware::CreateLogMessage("response sent", response_data);
          }
          self->OnWrite(true, ec, bytes_written);
        });
  }

 protected:
  explicit SessionBase(tcp::socket&& socket) : stream_(std::move(socket)) {}

  using HttpRequest = http::request<http::string_body>;

  virtual ~SessionBase() = default;

  void SetReceivedRequestTime(
      const boost::posix_time::ptime& received_request_moment) {
    received_request_moment_ = received_request_moment;
  }

  long GetDurationFromTimeReceivedRequest_ms(
      const boost::posix_time::ptime& to_moment) const {
    boost::posix_time::time_duration duration =
        to_moment - received_request_moment_;
    return duration.total_milliseconds();
  }

  const std::string& GetRemoteIp() {
    static std::string remote_ip;
    try {
      auto temp = stream_.socket().remote_endpoint().address().to_string();
      remote_ip = temp;
    } catch (...) {
    }
    return remote_ip;
  }

 private:
  beast::tcp_stream stream_;
  beast::flat_buffer buffer_;
  HttpRequest request_;
  boost::posix_time::ptime received_request_moment_;

  void Read();
  void OnRead(beast::error_code ec, std::size_t bytes_read);
  void OnWrite(bool close, beast::error_code ec, std::size_t bytes_written);
  void Close();
  virtual void HandleRequest(HttpRequest&& request) = 0;
};

template <typename RequestHandler>
class Session : public SessionBase {
 public:
  template <typename Handler>
  Session(tcp::socket&& socket, Handler&& request_handler)
      : SessionBase(std::move(socket)),
        request_handler_(std::forward<Handler>(request_handler)) {}

 private:
  RequestHandler request_handler_;

  void HandleRequest(HttpRequest&& request) override {
    SetReceivedRequestTime(boost::posix_time::microsec_clock::local_time());
    
    // Логирование запроса с IP-адресом
    logware::RequestLogData request_data;
    request_data.ip = GetRemoteIp();
    request_data.uri = std::string(request.target());
    request_data.method = std::string(request.method_string());
    BOOST_LOG_TRIVIAL(info) 
        << logware::CreateLogMessage("request received", request_data);
    
    request_handler_(
        std::move(request),
        [self = this->shared_from_this()](auto&& response) {
          self->Write(std::move(response));
        });
  }
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
 public:
  template <typename Handler>
  Listener(net::io_context& ioc, const tcp::endpoint& endpoint,
           Handler&& request_handler)
      : ioc_(ioc),
        acceptor_(net::make_strand(ioc)),
        request_handler_(std::forward<Handler>(request_handler)) {
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(net::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(net::socket_base::max_listen_connections);
  }

  void Run() { DoAccept(); }

 private:
  net::io_context& ioc_;
  tcp::acceptor acceptor_;
  RequestHandler request_handler_;

  void DoAccept() {
    acceptor_.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(&Listener::OnAccept,
                                  this->shared_from_this()));
  }

  void OnAccept(sys::error_code ec, tcp::socket socket) {
    if (ec) {
      logware::ErrorLogData error_data;
      error_data.code = ec.value();
      error_data.text = ec.message();
      error_data.where = "accept";
      BOOST_LOG_TRIVIAL(error) 
          << logware::CreateLogMessage("error", error_data);
      return;
    }

    AsyncRunSession(std::move(socket));
    DoAccept();
  }

  void AsyncRunSession(tcp::socket&& socket) {
    std::make_shared<Session<RequestHandler>>(std::move(socket),
                                              request_handler_)
        ->Run();
  }
};

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint,
               RequestHandler&& handler) {
  using MyListener = Listener<std::decay_t<RequestHandler>>;
  std::make_shared<MyListener>(ioc, endpoint,
                               std::forward<RequestHandler>(handler))
      ->Run();
}

}  // namespace http_server