cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============= УНИВЕРСАЛЬНЫЙ ПОИСК CONAN =============
set(CONAN_BUILDINFO_PATHS
    "${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake"
    "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake"
    "${CMAKE_SOURCE_DIR}/build/conanbuildinfo_multi.cmake"
    "${CMAKE_SOURCE_DIR}/build/conanbuildinfo.cmake"
    "/app/build/conanbuildinfo_multi.cmake"
    "/app/build/conanbuildinfo.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/conanbuildinfo_multi.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/conanbuildinfo.cmake"
    "${CMAKE_BINARY_DIR}/conan/build/conanbuildinfo_multi.cmake"
    "${CMAKE_BINARY_DIR}/conan/build/conanbuildinfo.cmake"
)

set(CONAN_BUILDINFO_FOUND FALSE)
set(CONAN_IS_MULTI FALSE)

foreach(PATH ${CONAN_BUILDINFO_PATHS})
    if(EXISTS ${PATH})
        message(STATUS "✅ Found conanbuildinfo at: ${PATH}")
        include(${PATH})
        set(CONAN_BUILDINFO_FOUND TRUE)
        if(PATH MATCHES ".*_multi\\.cmake$")
            set(CONAN_IS_MULTI TRUE)
        endif()
        break()
    endif()
endforeach()

if(NOT CONAN_BUILDINFO_FOUND)
    message(FATAL_ERROR "❌ conanbuildinfo file not found!")
endif()

# ВАЖНО: Добавляем путь к сгенерированным Conan файлам в CMAKE_MODULE_PATH и CMAKE_PREFIX_PATH
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})

if(CONAN_IS_MULTI)
    conan_basic_setup(TARGETS)
else()
    conan_basic_setup(TARGETS)
endif()

set(CMAKE_PREFIX_PATH ${CONAN_BOOST_ROOT} ${CMAKE_PREFIX_PATH})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_compile_definitions(
    BOOST_BEAST_USE_STD_STRING_VIEW
    BOOST_THREAD_PROVIDES_FUTURE
    BOOST_JSON_STANDALONE
)

# ============= БИБЛИОТЕКА COLLISION DETECTION =============
add_library(collision_detection_lib STATIC
    src/model/physics/collision_detector.h
    src/model/physics/collision_detector.cpp
    src/model/physics/geom.h
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
    src/model/physics
    src/middleware/utils
)

target_link_libraries(game_model_lib PUBLIC collision_detection_lib CONAN_PKG::boost)

# ============= БИБЛИОТЕКА СЕРИАЛИЗАЦИИ =============
add_library(game_serialization_lib STATIC
    src/middleware/game_data_store/lost_object_serialization.cpp
    src/middleware/game_data_store/dog_serialization.cpp
    src/middleware/game_data_store/player_serialization.cpp
    src/middleware/game_data_store/game_session_serialization.cpp
)

target_include_directories(game_serialization_lib PUBLIC
    src
    src/middleware/game_data_store
    src/model
    src/app
)

target_link_libraries(game_serialization_lib PUBLIC
    game_model_lib
    CONAN_PKG::boost
)

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
    src/app/authentication/player_tokens.cpp
    src/app/application.cpp
    src/app/game_session.cpp
    src/app/player.cpp
    src/web/server/http_server.cpp
    src/web/request_handlers/request_handler.cpp
)

target_include_directories(game_server PRIVATE
    src
    src/app
    src/web
    src/middleware
    src/middleware/json
    src/web/request_handlers
    src/middleware/logging
    src/app/authentication
    src/web/server
    src/middleware/program_options
    src/middleware/time_management
    src/middleware/error_handling
    src/middleware/game_data_store
)

target_link_libraries(game_server PRIVATE 
    Threads::Threads 
    CONAN_PKG::boost 
    game_model_lib
    game_serialization_lib
)

# ============= ТЕСТЫ =============
include(CTest)

# ВАЖНО: Сначала пытаемся найти Catch2 через CONFIG режим с указанием пути
find_package(Catch2 CONFIG QUIET PATHS ${CMAKE_BINARY_DIR})

if(NOT Catch2_FOUND)
    # Если не нашли через CONFIG, пробуем через модульный режим
    find_package(Catch2 REQUIRED MODULE)
endif()

# Тест генератора лута
add_executable(loot_generator_tests
    tests/loot-generator-tests.cpp
)

target_include_directories(loot_generator_tests PRIVATE
    src
    src/model
    src/model/physics
)

target_link_libraries(loot_generator_tests PRIVATE
    game_model_lib
    Catch2::Catch2
)

# Тест детектора коллизий
add_executable(collision_detection_tests
    tests/collision-detector-tests.cpp
)

target_include_directories(collision_detection_tests PRIVATE
    src
    src/model/physics
)

target_link_libraries(collision_detection_tests PRIVATE
    collision_detection_lib
    Catch2::Catch2
)

# Тест сериализации состояния
add_executable(state_serialization_tests
    tests/state-serialization-tests.cpp
)

target_include_directories(state_serialization_tests PRIVATE
    src
    src/middleware/game_data_store
    src/model
    src/model/physics
    src/app
)

target_link_libraries(state_serialization_tests PRIVATE
    game_model_lib
    game_serialization_lib
    Catch2::Catch2
)

# Регистрируем тесты с Catch2
include(Catch)
catch_discover_tests(loot_generator_tests)
catch_discover_tests(collision_detection_tests)
catch_discover_tests(state_serialization_tests)

# ============= ТЕСТЫ ДЛЯ ЗАПУСКА ЧЕРЕЗ PYTEST =============
add_custom_target(run_tests
    COMMAND python3 -m pytest ${CMAKE_SOURCE_DIR}/tests -v
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "🚀 Running Python tests..."
)

add_custom_target(run_cpp_tests
    COMMAND ctest --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "🚀 Running all C++ tests..."
)

add_custom_target(run_game_server_tests
    COMMAND ${CMAKE_BINARY_DIR}/bin/loot_generator_tests
    COMMAND ${CMAKE_BINARY_DIR}/bin/collision_detection_tests
    COMMAND ${CMAKE_BINARY_DIR}/bin/state_serialization_tests
    DEPENDS loot_generator_tests collision_detection_tests state_serialization_tests
    COMMENT "🚀 Running all C++ tests..."
)

# ============= ДОПОЛНИТЕЛЬНЫЕ ЦЕЛИ =============
add_custom_target(clean_all
    COMMAND rm -rf ${CMAKE_BINARY_DIR}/bin/*
    COMMAND rm -rf ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND rm -rf ${CMAKE_BINARY_DIR}/CMakeFiles
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "🧹 Cleaning build artifacts..."
)

add_custom_target(run_server
    COMMAND ${CMAKE_BINARY_DIR}/bin/game_server --config-file ${CMAKE_SOURCE_DIR}/data/config.json --www-root ${CMAKE_SOURCE_DIR}/static
    DEPENDS game_server
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "🚀 Running game server..."
)

# ============= ИНФОРМАЦИЯ О ЦЕЛЯХ СБОРКИ =============
message(STATUS "🎮 Build targets:")
message(STATUS "   - game_server (main executable)")
message(STATUS "   - collision_detection_lib (static library)")
message(STATUS "   - game_model_lib (game model library)")
message(STATUS "   - game_serialization_lib (serialization library)")
message(STATUS "   - loot_generator_tests (loot generator tests)")
message(STATUS "   - collision_detection_tests (collision tests)")
message(STATUS "   - state_serialization_tests (state serialization tests)")
message(STATUS "")
message(STATUS "🎯 Test targets:")
message(STATUS "   - run_cpp_tests (run all C++ tests via CTest)")
message(STATUS "   - run_game_server_tests (run all C++ tests directly)")
message(STATUS "   - run_tests (run Python tests)")
message(STATUS "")
message(STATUS "🔧 Utility targets:")
message(STATUS "   - clean_all (clean build artifacts)")
message(STATUS "   - run_server (run game server with default config)")