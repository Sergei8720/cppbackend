#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <filesystem>
#include <iostream>
#include <thread>

#include "json_loader.h"
#include "request_handler.h"
#include "logging_request_handler.h"
#include "logger.h"

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
  if (argc != 3) {
    std::cerr << "Usage: game_server <game-config-json> <static-content-path>"
              << std::endl;
    return EXIT_FAILURE;
  }

  // Инициализируем логгер
  logging::Init();
  int exit_code = EXIT_SUCCESS;
  
  try {
    model::Game game = json_loader::LoadGame(argv[1]);
    fs::path static_root_path{argv[2]};
    
    const unsigned num_threads = std::thread::hardware_concurrency();
    net::io_context ioc(num_threads);
    
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&ioc](const sys::error_code& ec, int signal_number) {
      if (!ec) {
        ioc.stop();
      }
    });
    
    http_handler::RequestHandler handler{game, static_root_path};
    
    // Оборачиваем обработчик в логгирующий декоратор
    LoggingRequestHandler<http_handler::RequestHandler> logging_handler{handler};

    const auto address = net::ip::make_address("0.0.0.0");
    constexpr net::ip::port_type port = 8080;
    const auto endpoint = net::ip::tcp::endpoint{address, port};
    
    // Логируем запуск сервера
    boost::json::value start_data{
        {"port", port},
        {"address", address.to_string()}
    };
    BOOST_LOG_TRIVIAL(info) << logging::additional_data = start_data
                            << "server started";

    http_server::ServeHttp(ioc, endpoint,
        [&logging_handler](auto&& req, auto&& send) {
          logging_handler(std::forward<decltype(req)>(req),
                  std::forward<decltype(send)>(send));
        });
    
    RunWorkers(std::max(1u, num_threads), [&ioc] {
      ioc.run();
    });

  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;
    exit_code = EXIT_FAILURE;
    
    // Логируем ошибку, из-за которой сервер не смог запуститься
    boost::json::value error_data{
        {"code", exit_code},
        {"exception", ex.what()}
    };
    BOOST_LOG_TRIVIAL(error) << logging::additional_data = error_data
                             << "server exited";
    return exit_code;
  }
  
  // Логируем успешное завершение работы сервера
  boost::json::value exit_data{{"code", exit_code}};
  BOOST_LOG_TRIVIAL(info) << logging::additional_data = exit_data
                          << "server exited";
  
  return exit_code;
}