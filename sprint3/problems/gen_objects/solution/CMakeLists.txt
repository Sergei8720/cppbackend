cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============= УНИВЕРСАЛЬНЫЙ ПОИСК CONAN =============
set(CONAN_BUILDINFO_PATHS
    "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake"
    "${CMAKE_SOURCE_DIR}/build/conanbuildinfo.cmake"
    "/app/build/conanbuildinfo.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/conanbuildinfo.cmake"
    "${CMAKE_BINARY_DIR}/conan/build/conanbuildinfo.cmake"
)

set(CONAN_BUILDINFO_FOUND FALSE)
foreach(PATH ${CONAN_BUILDINFO_PATHS})
    if(EXISTS ${PATH})
        message(STATUS "✅ Found conanbuildinfo.cmake at: ${PATH}")
        include(${PATH})
        set(CONAN_BUILDINFO_FOUND TRUE)
        break()
    endif()
endforeach()

if(NOT CONAN_BUILDINFO_FOUND)
    message(FATAL_ERROR "❌ conanbuildinfo.cmake not found! Searched in: ${CONAN_BUILDINFO_PATHS}")
endif()

# Применяем настройки Conan с поддержкой таргетов
conan_basic_setup(TARGETS)
# ============= КОНЕЦ ПОИСКА CONAN =============

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_compile_definitions(
    BOOST_BEAST_USE_STD_STRING_VIEW
    BOOST_JSON_STANDALONE
)

# ============= ОСНОВНОЙ ИСПОЛНЯЕМЫЙ ФАЙЛ =============
add_executable(game_server
    src/main.cpp
    src/model/map.cpp
    src/model/game.cpp
    src/model/dog.cpp
    src/model/roadmap.cpp
    src/model/support_types.cpp
    src/model/loot_generator.cpp
    src/model/lost_object.cpp
    src/json/json_loader.cpp
    src/json/json_converter.cpp
    src/json/json_model_converter.cpp
    src/request_handlers/request_handler.cpp
    src/utils/request_handlers_utils.cpp
    src/utils/filesystem_utils.cpp
    src/utils/random_generators.cpp
    src/boost_json.cpp
    src/logging/logging_data_storage.cpp
    src/logging/logger.cpp
    src/authentication/player_tokens.cpp
    src/app/application.cpp
    src/app/game_session.cpp
    src/app/player.cpp
    src/server/http_server.cpp
    src/program_options/program_options.cpp
    src/time_management/ticker.cpp
    src/error_handling/error_report.cpp
)

target_include_directories(game_server PRIVATE
    src
    src/json
    src/request_handlers
    src/utils
    src/logging
    src/model
    src/authentication
    src/app
    src/server
    src/program_options
    src/time_management
    src/error_handling
    ${CONAN_INCLUDE_DIRS}
)

target_link_libraries(game_server PRIVATE 
    Threads::Threads 
    ${CONAN_LIBS}
)
# ============= КОНЕЦ ОСНОВНОГО ФАЙЛА =============

# ============= ТЕСТЫ =============
add_executable(catch_tests
    tests/loot_generator_tests.cpp
    src/model/loot_generator.cpp
)

target_include_directories(catch_tests PRIVATE
    src
    src/model
    ${CONAN_INCLUDE_DIRS}
)

target_link_libraries(catch_tests PRIVATE
    Threads::Threads
    ${CONAN_LIBS}
    m
)

# Явно указываем стандарт C++ для тестов
target_compile_features(catch_tests PRIVATE cxx_std_20)

# ИСПРАВЛЕНО: правильный генератор выражения для отладки
add_custom_command(TARGET catch_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "🔧 catch_tests linked libraries:"
    COMMAND ${CMAKE_COMMAND} -E echo "$<TARGET_PROPERTY:catch_tests,LINK_LIBRARIES>"
    COMMAND ${CMAKE_COMMAND} -E echo "🔧 catch_tests include directories:"
    COMMAND ${CMAKE_COMMAND} -E echo "$<TARGET_PROPERTY:catch_tests,INCLUDE_DIRECTORIES>"
    COMMAND ${CMAKE_COMMAND} -E echo "🚀 Test executable built successfully!"
)

# Добавляем цель для запуска тестов
add_custom_target(run_tests
    COMMAND catch_tests
    DEPENDS catch_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "🚀 Running Catch2 tests..."
)
# ============= КОНЕЦ ТЕСТОВ =============

# Выводим информацию о найденных библиотеках
message(STATUS "📚 Conan libraries: ${CONAN_LIBS}")
message(STATUS "📁 Conan include dirs: ${CONAN_INCLUDE_DIRS}")