#include "sdk.h"
//
#include "json_loader.h"
#include "request_handler.h"
#include "logger.h"
#include "application.h"
#include "program_options.h"
#include "saving_settings.h"
#include "state_serializer.h"

#include <boost/asio/io_context.hpp>
#include <thread>
#include <boost/asio/signal_set.hpp>
#include <filesystem>
#include <chrono>
#include <memory>
#include <atomic>
#include <fstream>

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace fs = std::filesystem;

namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(size_t n, const Fn& fn) {
    n = std::max(static_cast<size_t>(1), n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    // 0. Инициализация логгера.
    logware::InitLogger();
    
    BOOST_LOG_TRIVIAL(info) << "========================================";
    BOOST_LOG_TRIVIAL(info) << "Game server starting...";
    BOOST_LOG_TRIVIAL(info) << "========================================";
    
    prog_opt::Args args = prog_opt::ParseCommandLine(argc, argv);
    
    BOOST_LOG_TRIVIAL(info) << "Command line arguments:";
    BOOST_LOG_TRIVIAL(info) << "  --config-file: " << args.config_file;
    BOOST_LOG_TRIVIAL(info) << "  --www-root: " << args.www_root;
    BOOST_LOG_TRIVIAL(info) << "  --tick-period: " << args.tick_period;
    BOOST_LOG_TRIVIAL(info) << "  --randomize-spawn-points: " << args.randomize_spawn_points;
    BOOST_LOG_TRIVIAL(info) << "  --state-file: " << (args.state_file.empty() ? "none" : args.state_file);
    BOOST_LOG_TRIVIAL(info) << "  --save-state-period: " << args.save_state_period;
    
    // Проверяем директорию для state файла
    if (!args.state_file.empty()) {
        std::filesystem::path state_path(args.state_file);
        auto parent = state_path.parent_path();
        
        if (!parent.empty()) {
            BOOST_LOG_TRIVIAL(info) << "State file directory: " << parent.string();
            
            // Проверяем, существует ли директория
            if (std::filesystem::exists(parent)) {
                BOOST_LOG_TRIVIAL(info) << "Directory exists, checking permissions...";
                
                // Проверяем права на запись
                std::ofstream test_file(parent / "test_write.tmp");
                if (test_file.is_open()) {
                    test_file << "test";
                    test_file.close();
                    std::filesystem::remove(parent / "test_write.tmp");
                    BOOST_LOG_TRIVIAL(info) << "Directory is writable";
                } else {
                    BOOST_LOG_TRIVIAL(error) << "Directory is NOT writable!";
                }
            } else {
                BOOST_LOG_TRIVIAL(warning) << "Directory does not exist, will attempt to create";
                std::error_code ec;
                std::filesystem::create_directories(parent, ec);
                if (ec) {
                    BOOST_LOG_TRIVIAL(error) << "Failed to create directory: " << ec.message();
                } else {
                    BOOST_LOG_TRIVIAL(info) << "Directory created successfully";
                    
                    // Проверяем права после создания
                    std::ofstream test_file(parent / "test_write.tmp");
                    if (test_file.is_open()) {
                        test_file << "test";
                        test_file.close();
                        std::filesystem::remove(parent / "test_write.tmp");
                        BOOST_LOG_TRIVIAL(info) << "New directory is writable";
                    } else {
                        BOOST_LOG_TRIVIAL(error) << "New directory is NOT writable!";
                    }
                }
            }
        }
    }
    
    std::atomic<bool> final_save_done{false};
    
    try {
        // 1. Загружаем карту из файла и построить модель игры
        BOOST_LOG_TRIVIAL(info) << "Loading game config from " << args.config_file;
        model::Game game = json_loader::LoadGame(args.config_file);
        BOOST_LOG_TRIVIAL(info) << "Game config loaded successfully";

        // 2. Устанавливаем путь до статического контента.
        fs::path sc_root_path{args.www_root};

        // 3. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        BOOST_LOG_TRIVIAL(info) << "Using " << num_threads << " threads";
        net::io_context ioc(num_threads);

        // 4. Создание application
        auto application = std::make_shared<app::Application>(std::move(game),
                                                              args.tick_period,
                                                              args.randomize_spawn_points,
                                                              ioc);
        BOOST_LOG_TRIVIAL(info) << "Application created";

        // 5. Инициализация настроек сохранения
        std::unique_ptr<app::StateSerializer> state_serializer;
        if (!args.state_file.empty()) {
            auto save_period = args.save_state_period > 0 
                ? std::chrono::milliseconds(args.save_state_period)
                : std::chrono::milliseconds(0);
            
            BOOST_LOG_TRIVIAL(info) << "State serialization enabled: state_file=" << args.state_file
                                   << ", save_period=" << save_period.count() << "ms";
            
            state_serializer = std::make_unique<app::StateSerializer>(
                *application, 
                args.state_file, 
                save_period
            );
            
            // Восстанавливаем состояние
            bool load_success = state_serializer->LoadState(ioc);
            if (!load_success) {
                // Если файл существует, но произошла ошибка - завершаем работу
                if (std::filesystem::exists(args.state_file)) {
                    BOOST_LOG_TRIVIAL(error) << "Failed to load game state from " << args.state_file;
                    return EXIT_FAILURE;
                }
                // Если файла нет - начинаем с чистого листа
                BOOST_LOG_TRIVIAL(info) << "Starting with clean state (no state file or empty)";
            }
            
            // Запускаем периодическое сохранение
            if (save_period.count() > 0) {
                state_serializer->StartPeriodicSaving(ioc);
                BOOST_LOG_TRIVIAL(info) << "Periodic saving started with period " << save_period.count() << "ms";
            } else {
                BOOST_LOG_TRIVIAL(info) << "Periodic saving disabled (period=0)";
            }
        } else {
            BOOST_LOG_TRIVIAL(info) << "State serialization disabled (no state-file specified)";
        }

        // 6. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc, &state_serializer, &final_save_done](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec && !final_save_done.exchange(true)) {
                BOOST_LOG_TRIVIAL(info) << "Received signal " << signal_number << ", saving state...";
                if (state_serializer) {
                    try {
                        state_serializer->FinalSave();
                        BOOST_LOG_TRIVIAL(info) << "State saved successfully on signal";
                    } catch (const std::exception& e) {
                        BOOST_LOG_TRIVIAL(error) << "Failed to save state on signal: " << e.what();
                    }
                }
                BOOST_LOG_TRIVIAL(info) << "Stopping io_context...";
                ioc.stop();
            }
        });
        BOOST_LOG_TRIVIAL(info) << "Signal handlers registered";

        // 7. Создаём обработчик HTTP-запросов и связываем его с моделью игры, задаем путь до статического контента.
        http_handler::RequestHandler handler{application, sc_root_path};

        // 8. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        BOOST_LOG_TRIVIAL(info) << "Starting HTTP server on " << address << ":" << port;
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });
        
        // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
        BOOST_LOG_TRIVIAL(info) << "Server has started...";
        BOOST_LOG_TRIVIAL(info) << "========================================";

        // 9. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
        
        // 10. Сохраняем состояние при нормальном завершении (если не было сохранено по сигналу)
        if (state_serializer && !final_save_done.exchange(true)) {
            BOOST_LOG_TRIVIAL(info) << "Normal shutdown, saving final state...";
            state_serializer->FinalSave();
        }
        
        BOOST_LOG_TRIVIAL(info) << "========================================";
        BOOST_LOG_TRIVIAL(info) << "Server stopped";
        BOOST_LOG_TRIVIAL(info) << "========================================";
        
    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "Server error: " << ex.what();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}