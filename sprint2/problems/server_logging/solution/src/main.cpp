#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>
#include <filesystem>
#include <cstdlib>

#include "json_loader.h"
#include "request_handler.h"
#include "logger.h"

namespace {

template <typename Fn>
void RunWorkers(size_t n, const Fn& fn) {
  n = std::max(static_cast<size_t>(1), n);
  std::vector<std::jthread> workers;
  workers.reserve(n - 1);
  
  while (--n) {
    workers.emplace_back(fn);
  }
  
  fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
  logware::InitLogger();
  
  if (argc != 3) {
    BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage(
      "Usage: game_server <game-config-json>",
      logware::ExitCodeLogData(EXIT_FAILURE));
    return EXIT_FAILURE;
  }
  
  try {
    model::Game game = json_loader::LoadGame(argv[1]);
    std::filesystem::path static_content_root_path{argv[2]};
    
    const unsigned num_threads = std::thread::hardware_concurrency();
    boost::asio::io_context ioc(num_threads);
    
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&ioc](const boost::system::error_code& ec, int signal_number) {
      if (!ec) {
        BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage(
          "server exited",
          logware::ExitCodeLogData(0));
        ioc.stop();
      }
    });
    
    http_handler::RequestHandler handler{game, static_content_root_path};
    
    const auto address = boost::asio::ip::make_address("0.0.0.0");
    constexpr boost::asio::ip::port_type port = 8080;
    
    http_server::ServeHttp(ioc, {address, port}, 
      [&handler](auto&& req, auto&& send) {
        handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
      });
    
    BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage(
      "Server has started...",
      logware::ServerAddressLogData(address.to_string(), port));
    
    RunWorkers(std::max(1u, num_threads), [&ioc] {
      ioc.run();
    });
    
  } catch (const std::exception& ex) {
    BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage(
      "error",
      logware::ExceptionLogData(EXIT_FAILURE, "Server down", ex.what()));
    return EXIT_FAILURE;
  }
  
  return EXIT_SUCCESS;
}
