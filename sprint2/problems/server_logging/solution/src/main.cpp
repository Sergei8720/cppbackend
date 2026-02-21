#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <filesystem>
#include <iostream>
#include <thread>

#include "json_loader.h"
#include "logger.h"
#include "request_handler.h"
#include "http_server.h"
#include "sdk.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace fs = std::filesystem;

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
  std::cout << "Starting server..." << std::endl;
  
  logware::InitLogger();

  if (argc != 3) {
    std::cerr << "Usage: game_server <game-config-json> <static-content-path>" << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "Config path: " << argv[1] << std::endl;
  std::cout << "Static path: " << argv[2] << std::endl;

  try {
    model::Game game = json_loader::LoadGame(argv[1]);
    fs::path static_content_root_path{argv[2]};
    
    std::cout << "Game loaded, maps count: " << game.GetMaps().size() << std::endl;
    std::cout << "Static path exists: " << fs::exists(static_content_root_path) << std::endl;

    const unsigned num_threads = std::thread::hardware_concurrency();
    net::io_context io_context(num_threads);

    net::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&io_context](const sys::error_code& ec,
                                     [[maybe_unused]] int signal_number) {
      if (!ec) {
        logware::ServerExitLogData exit_data;
        exit_data.code = 0;
        BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage("server exited", exit_data);
        io_context.stop();
      }
    });

    http_handler::RequestHandler handler{game, static_content_root_path};

    const auto address = net::ip::make_address("0.0.0.0");
    constexpr net::ip::port_type port = 8080;
    
    logware::ServerStartLogData start_data;
    start_data.port = port;
    start_data.address = address.to_string();
    BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage("server started", start_data);

    std::cout << "Starting HTTP server on port " << port << "..." << std::endl;
    
    http_server::ServeHttp(
        io_context, {address, port},
        [&handler](auto&& req, auto&& send) {
          handler(std::forward<decltype(req)>(req),
                  std::forward<decltype(send)>(send));
        });

    std::cout << "HTTP server started, running workers..." << std::endl;

    RunWorkers(std::max(1u, num_threads),
               [&io_context] { io_context.run(); });
               
    std::cout << "Server stopped." << std::endl;
  } catch (const std::exception& ex) {
    std::cerr << "Exception: " << ex.what() << std::endl;
    logware::ServerExitLogData exit_data;
    exit_data.code = EXIT_FAILURE;
    exit_data.exception = ex.what();
    BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("server exited", exit_data);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}