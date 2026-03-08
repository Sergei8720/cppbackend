cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============= УНИВЕРСАЛЬНЫЙ ПОИСК CONAN =============
# Ищем все возможные варианты conanbuildinfo (включая multi)
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
    message(FATAL_ERROR "❌ conanbuildinfo file not found! Searched in: ${CONAN_BUILDINFO_PATHS}")
endif()

# Применяем настройки Conan с TARGETS режимом
if(CONAN_IS_MULTI)
    conan_basic_setup(TARGETS)
else()
    conan_basic_setup(TARGETS)
endif()

# Добавляем пути от Conan в CMAKE_PREFIX_PATH для поиска пакетов
set(CMAKE_PREFIX_PATH ${CONAN_BOOST_ROOT} ${CMAKE_PREFIX_PATH})
# Явно указываем где искать Catch2
set(Catch2_DIR ${CONAN_CATCH2_ROOT}/lib/cmake/Catch2)

# ============= КОНЕЦ ПОИСКА CONAN =============

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_compile_definitions(
    BOOST_BEAST_USE_STD_STRING_VIEW
    BOOST_JSON_STANDALONE
)

# ============= БИБЛИОТЕКА COLLISION DETECTION =============
add_library(collision_detection_lib STATIC
    src/model/physics/collision_detector.h
    src/model/physics/collision_detector.cpp
    src/model/physics/geom.h
)
target_link_libraries(collision_detection_lib PUBLIC CONAN_PKG::boost Threads::Threads)

# ============= КОНЕЦ БИБЛИОТЕКИ =============

# ============= ОСНОВНОЙ ИСПОЛНЯЕМЫЙ ФАЙЛ =============
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

add_executable(game_server
    src/main.cpp
    src/middleware/json/json_loader.cpp
    src/middleware/json/json_converter.cpp
    src/middleware/json/json_model_converter.cpp
    src/middleware/json/boost_json.cpp
    src/web/request_handlers/request_handler.cpp
    src/middleware/logging/logging_data_storage.cpp
    src/middleware/logging/logger.cpp
    src/app/authentication/player_tokens.cpp
    src/app/application.cpp
    src/app/game_session.cpp
    src/app/player.cpp
    src/web/server/http_server.cpp
    src/middleware/program_options/program_options.cpp
    src/middleware/time_management/ticker.cpp
    src/middleware/error_handling/error_report.cpp
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
)

target_link_libraries(game_server PRIVATE Threads::Threads CONAN_PKG::boost game_model_lib)

# ============= ТЕСТЫ =============
# Включаем CTest
include(CTest)

# Используем CONAN_PKG::catch2 для линковки
# Тесты для основного приложения
add_executable(game_tests
    tests/loot_generator_tests.cpp
    tests/collision-detector-tests.cpp
)

target_include_directories(game_tests PRIVATE
    src
    src/model
    src/model/physics
    ${CONAN_INCLUDE_DIRS_CATCH2}
)

target_link_libraries(game_tests PRIVATE
    game_model_lib  # Добавляем линковку с библиотекой модели игры
    collision_detection_lib
    Threads::Threads
    CONAN_PKG::catch2
    m
)

target_compile_features(game_tests PRIVATE cxx_std_20)

# Тесты для библиотеки коллизий (отдельный исполняемый файл)
add_executable(collision_tests
    tests/collision-detector-tests.cpp
)

target_include_directories(collision_tests PRIVATE
    src
    src/model/physics
    ${CONAN_INCLUDE_DIRS_CATCH2}
)

target_link_libraries(collision_tests PRIVATE
    game_model_lib  # Добавляем линковку с библиотекой модели игры
    collision_detection_lib
    Threads::Threads
    CONAN_PKG::catch2
    m
)

target_compile_features(collision_tests PRIVATE cxx_std_20)

# Регистрируем тесты в CTest
if(BUILD_TESTING)
    add_test(NAME game_tests COMMAND game_tests)
    add_test(NAME collision_tests COMMAND collision_tests)
endif()

# Boost.Beast будет использовать std::string_view вместо boost::string_view
add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)
add_compile_definitions(BOOST_THREAD_PROVIDES_FUTURE)

# Общая цель для запуска всех тестов
add_custom_target(run_all_tests
    COMMAND game_tests && collision_tests
    DEPENDS game_tests collision_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "🚀 Running all tests..."
)

# Отдельные цели для каждого набора тестов
add_custom_target(run_game_tests
    COMMAND game_tests
    DEPENDS game_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "🚀 Running game tests..."
)

add_custom_target(run_collision_tests
    COMMAND collision_tests
    DEPENDS collision_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "🚀 Running collision detection tests..."
)

# Для multi генератора добавляем цель для каждого типа сборки
if(CMAKE_CONFIGURATION_TYPES)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        add_custom_target(run_tests_${config}
            COMMAND ${CMAKE_BINARY_DIR}/${config}/bin/game_tests && ${CMAKE_BINARY_DIR}/${config}/bin/collision_tests
            DEPENDS game_tests collision_tests
            COMMENT "🚀 Running all tests (${config})..."
        )
    endforeach()
endif()
# ============= КОНЕЦ ТЕСТОВ =============

message(STATUS "🎮 Build targets:")
message(STATUS "   - game_server (main executable)")
message(STATUS "   - collision_detection_lib (static library)")
message(STATUS "   - game_model_lib (game model library)")
message(STATUS "   - game_tests (unified game tests)")
message(STATUS "   - collision_tests (collision tests)")
message(STATUS "   - run_all_tests (run all tests)")
message(STATUS "   - run_game_tests (run game tests)")
message(STATUS "   - run_collision_tests (run collision tests)")