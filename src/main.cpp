#include "detail/sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <algorithm>
#include <thread>

#include "server/ticker.h"
#include "configuration/json_loader.h"
#include "configuration/server_configuration.h"
#include "infrastructure/serializing_listener.h"
#include "request_processing/request_handler.h"
#include "detail/logger.h"
#include "metadata/loot_data.h"
#include "postgres/postgres.h"

using namespace std::literals;
namespace net = boost::asio;
 
namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
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
    config::Args args = config::ParseCommandLine(argc, argv);
    if (args.show_help) {
        return EXIT_SUCCESS;
    }

    logger::InitLogging();

    try {
        // read the configuration file and configure the game
        metadata::LootMetaPerMap loot_meta;
        json_loader::GameSettings game_settings = json_loader::LoadGame(args.config_file, loot_meta);
        model::Game* game = game_settings.game.get();
        game->SetRandomizeSpawnPoints(args.randomize_spawn_points);

        const char* db_url = std::getenv("GAME_DB_URL");
        if (!db_url) {
            throw std::runtime_error("GAME_DB_URL is not specified");
        }

        const unsigned num_threads = std::thread::hardware_concurrency();
        const std::size_t pool_capacity = std::max(1u, num_threads);
        postgres::Database db{db_url, pool_capacity};

        application::Application application(*game, db.GetUnitOfWorkFactory(), game_settings.dog_retirement_time_sec);

        // Инициализируем io_context
        net::io_context ioc(num_threads);

        // strand для выполнения запросов к API
        auto api_strand = net::make_strand(ioc);

        std::unique_ptr<infrastructure::ServerState> server_state;
        std::unique_ptr<infrastructure::SerializingListener> serializing_listener;

        if (!args.state_file.empty()) {
            server_state = std::make_unique<infrastructure::ServerState>();

            // Если файл есть — пробуем восстановить, иначе стартуем с нуля
            if (std::filesystem::exists(args.state_file)) {
                const auto state = server_state->Load(args.state_file);
                application.RestoreState(state);
            }

            std::optional<std::chrono::milliseconds> save_interval;
            if (args.save_state_period_ms.has_value()) {
                save_interval = std::chrono::milliseconds{*args.save_state_period_ms};
            }

            serializing_listener = std::make_unique<infrastructure::SerializingListener>(
                args.state_file, *server_state, application, save_interval
            );

            application.SetOnTickCallback([&](std::chrono::milliseconds delta) {
                serializing_listener->OnTick(delta);
            });
        }

        // Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        auto* ioc_ptr = &ioc;
        auto* listener_ptr = serializing_listener.get();
        signals.async_wait([ioc_ptr, listener_ptr, api_strand](const boost::system::error_code& ec, int) mutable {
            if (ec) {
                return;
            }

            if (listener_ptr) {
                net::dispatch(api_strand, [ioc_ptr, listener_ptr]() {
                    listener_ptr->SaveNow();
                    ioc_ptr->stop();
                });
            } else {
                ioc_ptr->stop();
            }
        });
        
        const bool auto_tick_enabled = args.tick_period_ms.has_value();
        if (auto_tick_enabled) {
            auto ticker = std::make_shared<server::Ticker>(
                        api_strand, 
                        std::chrono::milliseconds(*args.tick_period_ms),
                        [&application](std::chrono::milliseconds delta) { 
                            application.Tick(delta); 
                        }
            );
            ticker->Start();
        }

        // Создаём обработчик HTTP-запросов и связываем его с моделью игры
        auto handler = std::make_shared<http_handler::RequestHandler>(application, 
                                                                    loot_meta, 
                                                                    args.www_root, 
                                                                    api_strand, 
                                                                    auto_tick_enabled);


        // Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            (*handler)(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы        
        logger::LogServerStart(port, address.to_string());

        // Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    } 
    catch (const std::exception& ex) {
        logger::LogServerStop(EXIT_FAILURE, ex.what());
        return EXIT_FAILURE;
    }

    logger::LogServerStop(EXIT_SUCCESS);
}
