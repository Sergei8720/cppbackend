cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall")

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_compile_definitions(
    BOOST_BEAST_USE_STD_STRING_VIEW
    BOOST_JSON_STANDALONE
    BOOST_THREAD_PROVIDES_FUTURE
)

# ============= БИБЛИОТЕКА COLLISION DETECTION =============
add_library(collision_detection_lib STATIC
    src/model/collision_detector.h
    src/model/collision_detector.cpp
    src/model/geom.h
)
target_link_libraries(collision_detection_lib PUBLIC CONAN_PKG::boost Threads::Threads)

# ============= БИБЛИОТЕКА МОДЕЛИ ИГРЫ =============
add_library(game_model_lib STATIC
    src/model/map.cpp
    src/model/game.cpp
    src/model/dog.cpp
    src/model/roadmap.cpp
    src/model/support_types.cpp
    src/model/loot_generator.cpp
    src/model/lost_object.cpp
    src/model/item_dog_provider.cpp
    src/middleware/utils/request_handlers_utils.cpp
    src/middleware/utils/filesystem_utils.cpp
    src/middleware/utils/random_generators.cpp
)

target_include_directories(game_model_lib PUBLIC 
    src/model
    src/middleware/utils
)

target_link_libraries(game_model_lib PUBLIC collision_detection_lib CONAN_PKG::boost)

# ============= ОСНОВНОЙ ИСПОЛНЯЕМЫЙ ФАЙЛ =============
add_executable(game_server
    src/main.cpp
    src/middleware/json/json_loader.cpp
    src/middleware/json/json_converter.cpp
    src/middleware/json/json_model_converter.cpp
    src/middleware/json/boost_json.cpp
    src/middleware/logging/logging_data_storage.cpp
    src/middleware/logging/logger.cpp
    src/middleware/program_options/program_options.cpp
    src/middleware/time_management/ticker.cpp
    src/middleware/error_handling/error_report.cpp
    src/middleware/api/api_v1_request_handlers_storage.h
    src/middleware/api/api_v1_request_handlers_executor.h
    src/middleware/api/request_handler_node.h
    src/middleware/api/static_file_request_handlers_storage.h
    src/middleware/api/static_file_request_handlers_executor.h
    src/web/request_handler.cpp
    src/web/server/http_server.cpp
    src/app/authentication/token_generator.cpp
    src/app/authentication/player_tokens.cpp
    src/app/application.cpp
    src/app/game_session.cpp
    src/app/player.cpp
    src/middleware/game_data_store/dog_serialization.cpp
    src/middleware/game_data_store/game_session_serialization.cpp
    src/middleware/game_data_store/lost_object_serialization.cpp
    src/middleware/game_data_store/player_serialization.cpp
    src/middleware/game_data_store/state_serializer.cpp
    src/database/connection_pool.cpp
    src/database/database.cpp
    src/retirement/retirement_tracker.cpp
)

target_include_directories(game_server PRIVATE
    src
    src/app
    src/app/authentication
    src/web
    src/web/server
    src/web/request_handlers
    src/middleware
    src/middleware/api
    src/middleware/json
    src/middleware/logging
    src/middleware/program_options
    src/middleware/time_management
    src/middleware/error_handling
    src/middleware/utils
    src/middleware/game_data_store
    src/model
    src/database
    src/retirement
)

target_link_libraries(game_server PRIVATE
    Threads::Threads
    CONAN_PKG::boost
    CONAN_PKG::libpqxx
    game_model_lib
)

message(STATUS "Build target: game_server")