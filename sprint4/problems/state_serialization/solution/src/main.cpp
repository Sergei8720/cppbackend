#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <thread>
#include <boost/asio/signal_set.hpp>
#include <filesystem>
#include <optional>

#include "json_loader.h"
#include "request_handler.h"
#include "logger.h"
#include "application.h"
#include "program_options.h"
#include "state_serializer.h"

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
    logware::InitLogger();
    prog_opt::Args args = prog_opt::ParseCommandLine(argc, argv);
    
    // Дополнительные параметры для state-файла
    std::optional<std::string> state_file;
    std::chrono::milliseconds save_state_period{0};
    
    // Ручной парсинг дополнительных параметров
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--state-file" && i + 1 < argc) {
            state_file = argv[++i];
        } else if (arg == "--save-state-period" && i + 1 < argc) {
            save_state_period = std::chrono::milliseconds(std::stoi(argv[++i]));
        }
    }
    
    try {
        model::Game game = json_loader::LoadGame(args.config_file);
        fs::path sc_root_path{args.www_root};
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        app::Application application(std::move(game), args.tick_period, args.randomize_spawn_points, ioc);
        
        // Инициализация сериализатора состояния
        std::unique_ptr<app::StateSerializer> state_serializer;
        if (state_file.has_value()) {
            // Передаём application в конструктор
            state_serializer = std::make_unique<app::StateSerializer>(
                application, state_file.value(), save_state_period);
            
            // Загружаем сохранённое состояние
            state_serializer->LoadState(ioc);
            
            // Запускаем периодическое сохранение
            if (save_state_period.count() > 0) {
                state_serializer->StartPeriodicSaving(ioc);
            }
        }
        
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&application, &state_serializer, &ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage("server exited"sv,
                                                                        logware::ExitCodeLogData(0));
                // Сохраняем состояние при graceful shutdown
                if (state_serializer) {
                    state_serializer->SaveState();  // без параметров
                }
                ioc.stop();
            }
        });

        http_handler::RequestHandler handler{application, sc_root_path};

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });
        
        BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage("Server has started..."sv,
                                                                logware::ServerAddressLogData(address.to_string(), port));

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                                        logware::ExceptionLogData(EXIT_FAILURE, "Server down"sv, ex.what()));
        return EXIT_FAILURE;
    }
}