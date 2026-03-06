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

# Применяем настройки Conan
conan_basic_setup(TARGETS)

# Добавляем пути к модулям CMake от Conan
list(APPEND CMAKE_MODULE_PATH ${CONAN_CMAKE_MODULE_PATH})
list(APPEND CMAKE_PREFIX_PATH ${CONAN_CMAKE_MODULE_PATH})
# ============= КОНЕЦ ПОИСКА CONAN =============

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Boost 1.78.0 REQUIRED COMPONENTS system filesystem)
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

target_link_directories(game_server PRIVATE ${CONAN_LIB_DIRS})

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

target_link_directories(catch_tests PRIVATE ${CONAN_LIB_DIRS})

# Явно указываем, что Catch2 уже установлен через Conan
# и используем CONAN_PKG::catch2 таргет вместо find_package
target_link_libraries(catch_tests PRIVATE
    Threads::Threads
    CONAN_PKG::catch2  # Это таргет, созданный Conan
    m
)

# Альтернативно, если CONAN_PKG::catch2 не работает, используем:
# target_link_libraries(catch_tests PRIVATE
#     Threads::Threads
#     ${CONAN_LIBS_CATCH2}
#     ${CONAN_LIBS_CATCH2_MAIN}
#     m
# )

target_compile_features(catch_tests PRIVATE cxx_std_20)

# Для отладки
add_custom_command(TARGET catch_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "🔧 Test executable built with CONAN_PKG::catch2"
)

# Добавляем цель для запуска тестов
add_custom_target(run_tests
    COMMAND catch_tests
    DEPENDS catch_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "🚀 Running Catch2 tests..."
)
# ============= КОНЕЦ ТЕСТОВ =============

# Выводим информацию
message(STATUS "📚 Conan libraries: ${CONAN_LIBS}")
message(STATUS "📁 Conan include dirs: ${CONAN_INCLUDE_DIRS}")
message(STATUS "📂 Conan lib dirs: ${CONAN_LIB_DIRS}")
message(STATUS "📂 Conan module path: ${CONAN_CMAKE_MODULE_PATH}")