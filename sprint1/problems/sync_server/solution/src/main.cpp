#ifdef _WIN32
#include <sdkddkver.h>
#endif

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>#ifdef _WIN32
#include <sdkddkver.h>
#endif

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

using tcp = net::ip::tcp;
using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

class HttpServer {
 public:
  explicit HttpServer(unsigned short port)
      : port_(port),
        acceptor_(io_context_,
                  tcp::endpoint(net::ip::make_address("0.0.0.0"), port)) {}

  void Run() {
    std::cout << "Server has started..." << std::endl;

    while (true) {
      tcp::socket socket(io_context_);
      acceptor_.accept(socket);
      std::cout << "Connection received" << std::endl;

      std::thread thread(&HttpServer::ProcessConnection, this,
                         std::move(socket));
      thread.detach();
    }
  }

 private:
  static constexpr std::string_view kContentTypeTextHtml = "text/html";

  unsigned short port_;
  net::io_context io_context_;
  tcp::acceptor acceptor_;

  static std::optional<StringRequest> ReadRequest(
      tcp::socket& socket, beast::flat_buffer& buffer) {
    beast::error_code error_code;
    StringRequest request;

    http::read(socket, buffer, request, error_code);

    if (error_code == http::error::end_of_stream) {
      return std::nullopt;
    }

    if (error_code) {
      throw std::runtime_error("Failed to read request: " +
                               error_code.message());
    }

    return request;
  }

  static void LogRequest(const StringRequest& request) {
    std::cout << request.method_string() << ' ' << request.target()
              << std::endl;

    for (const auto& header : request) {
      std::cout << "  " << header.name_string() << ": " << header.value()
                << std::endl;
    }
  }

  static StringResponse CreateResponse(http::status status,
                                       unsigned http_version,
                                       bool keep_alive) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, kContentTypeTextHtml);
    response.keep_alive(keep_alive);
    return response;
  }

  static StringResponse CreateGetResponse(http::status status,
                                          std::string_view body,
                                          unsigned http_version,
                                          bool keep_alive) {
    StringResponse response = CreateResponse(status, http_version, keep_alive);
    response.body() = body;
    response.content_length(body.size());
    return response;
  }

  static StringResponse CreateHeadResponse(http::status status,
                                           std::size_t body_size,
                                           unsigned http_version,
                                           bool keep_alive) {
    StringResponse response = CreateResponse(status, http_version, keep_alive);
    response.content_length(body_size);
    return response;
  }

  static StringResponse CreateMethodNotAllowedResponse(
      unsigned http_version, bool keep_alive) {
    StringResponse response =
        CreateResponse(http::status::method_not_allowed, http_version,
                       keep_alive);
    response.set(http::field::allow, "GET, HEAD");
    response.body() = "Invalid method";
    response.content_length(response.body().size());
    return response;
  }

  static std::string GenerateGreeting(std::string_view target) {
    std::stringstream stream;
    if (target.length() > 1) {
      stream << "Hello, " << target.substr(1);
    } else {
      stream << "Hello, World";
    }
    return stream.str();
  }

  static StringResponse HandleRequest(StringRequest&& request) {
    const std::string greeting = GenerateGreeting(request.target());

    switch (request.method()) {
      case http::verb::get: {
        return CreateGetResponse(http::status::ok, greeting, request.version(),
                                 request.keep_alive());
      }
      case http::verb::head: {
        return CreateHeadResponse(http::status::ok, greeting.size(),
                                  request.version(), request.keep_alive());
      }
      default: {
        return CreateMethodNotAllowedResponse(request.version(),
                                              request.keep_alive());
      }
    }
  }

  void ProcessConnection(tcp::socket socket) {
    try {
      beast::flat_buffer buffer;

      while (auto request = ReadRequest(socket, buffer)) {
        LogRequest(*request);
        StringResponse response = HandleRequest(std::move(*request));
        http::write(socket, response);

        if (response.need_eof()) {
          break;
        }
      }
    } catch (const std::exception& exception) {
      std::cerr << exception.what() << std::endl;
    }

    beast::error_code error_code;
    socket.shutdown(tcp::socket::shutdown_send, error_code);
  }
};

int main() {
  constexpr unsigned short kPort = 8080;
  HttpServer server(kPort);
  server.Run();
  return 0;
}

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

using tcp = net::ip::tcp;
using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

class HttpServer {
 public:
  explicit HttpServer(unsigned short port)
      : port_(port),
        acceptor_(io_context_,
                  tcp::endpoint(net::ip::make_address("0.0.0.0"), port)) {}

  void Run() {
    std::cout << "Server has started..." << std::endl;

    while (true) {
      tcp::socket socket(io_context_);
      acceptor_.accept(socket);
      std::cout << "Connection received" << std::endl;

      std::thread thread(&HttpServer::ProcessConnection, this,
                         std::move(socket));
      thread.detach();
    }
  }

 private:
  static constexpr std::string_view kContentTypeTextHtml = "text/html";
  static constexpr std::string_view kAllowMethods = "GET, HEAD";

  unsigned short port_;
  net::io_context io_context_;
  tcp::acceptor acceptor_;

  static std::optional<StringRequest> ReadRequest(
      tcp::socket& socket, beast::flat_buffer& buffer) {
    beast::error_code error_code;
    StringRequest request;

    http::read(socket, buffer, request, error_code);

    if (error_code == http::error::end_of_stream) {
      return std::nullopt;
    }

    if (error_code) {
      throw std::runtime_error("Failed to read request: " +
                               error_code.message());
    }

    return request;
  }

  static void LogRequest(const StringRequest& request) {
    std::cout << request.method_string() << ' ' << request.target()
              << std::endl;

    for (const auto& header : request) {
      std::cout << "  " << header.name_string() << ": " << header.value()
                << std::endl;
    }
  }

  static StringResponse CreateResponse(http::status status,
                                       unsigned http_version,
                                       bool keep_alive) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, kContentTypeTextHtml);
    response.keep_alive(keep_alive);
    return response;
  }

  static StringResponse CreateGetResponse(http::status status,
                                          std::string_view body,
                                          unsigned http_version,
                                          bool keep_alive) {
    StringResponse response = CreateResponse(status, http_version, keep_alive);
    response.body() = body;
    response.content_length(body.size());
    return response;
  }

  static StringResponse CreateHeadResponse(http::status status,
                                           std::size_t body_size,
                                           unsigned http_version,
                                           bool keep_alive) {
    StringResponse response = CreateResponse(status, http_version, keep_alive);
    response.content_length(body_size);
    return response;
  }

  static StringResponse CreateMethodNotAllowedResponse(
      unsigned http_version, bool keep_alive) {
    StringResponse response =
        CreateResponse(http::status::method_not_allowed, http_version,
                       keep_alive);
    response.set(http::field::allow, kAllowMethods);
    response.body() = "Invalid method";
    response.content_length(response.body().size());
    return response;
  }

  static std::string GenerateGreeting(std::string_view target) {
    std::string greeting = "Hello, ";
    
    if (target.length() > 1) {
      greeting += target.substr(1);
    } else {
      greeting += "World";
    }
    
    return greeting;
  }

  static StringResponse HandleRequest(StringRequest&& request) {
    const std::string greeting = GenerateGreeting(request.target());

    switch (request.method()) {
      case http::verb::get: {
        return CreateGetResponse(http::status::ok, greeting, request.version(),
                                 request.keep_alive());
      }
      case http::verb::head: {
        return CreateHeadResponse(http::status::ok, greeting.size(),
                                  request.version(), request.keep_alive());
      }
      default: {
        return CreateMethodNotAllowedResponse(request.version(),
                                              request.keep_alive());
      }
    }
  }

  void ProcessConnection(tcp::socket socket) {
    try {
      beast::flat_buffer buffer;

      while (auto request = ReadRequest(socket, buffer)) {
        LogRequest(*request);
        StringResponse response = HandleRequest(std::move(*request));
        http::write(socket, response);

        if (response.need_eof()) {
          break;
        }
      }
    } catch (const std::exception& exception) {
      std::cerr << exception.what() << std::endl;
    }

    beast::error_code error_code;
    socket.shutdown(tcp::socket::shutdown_send, error_code);
  }
};

int main() {
  constexpr unsigned short kPort = 8080;
  HttpServer server(kPort);
  server.Run();
  return 0;
}
