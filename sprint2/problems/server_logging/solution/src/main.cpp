#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <filesystem>
#include <iostream>
#include <thread>

#include "json_loader.h"
#include "logger.h"
#include "request_handler.h"
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

}

int main(int argc, const char* argv[]) {
  logware::InitLogger();

  if (argc != 3) {
    BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage(
        "Usage: game_server <game-config-json>"sv,
        logware::ExitCodeLogData(EXIT_FAILURE));
    return EXIT_FAILURE;
  }

  try {
    model::Game game = json_loader::LoadGame(argv[1]);
    fs::path static_content_root_path{argv[2]};

    const unsigned num_threads = std::thread::hardware_concurrency();
    net::io_context io_context(num_threads);

    net::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&io_context](const sys::error_code& ec,
                                     [[maybe_unused]] int signal_number) {
      if (!ec) {
        BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage(
            "server exited"sv, logware::ExitCodeLogData(0));
        io_context.stop();
      }
    });

    http_handler::RequestHandler handler{game, static_content_root_path};

    const auto address = net::ip::make_address("0.0.0.0");
    constexpr net::ip::port_type port = 8080;
    http_server::ServeHttp(
        io_context, {address, port},
        [&handler](auto&& req, auto&& send) {
          handler(std::forward<decltype(req)>(req),
                  std::forward<decltype(send)>(send));
        });

    BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage(
        "Server has started..."sv,
        logware::ServerAddressLogData(address.to_string(), port));

    RunWorkers(std::max(1u, num_threads),
               [&io_context] { io_context.run(); });
  } catch (const std::exception& ex) {
    BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage(
        "error"sv, logware::ExceptionLogData(EXIT_FAILURE, "Server down"sv,
                                             ex.what()));
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}