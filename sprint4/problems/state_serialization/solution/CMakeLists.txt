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

# ============= ОСНОВНОЙ ИСПОЛНЯЕМЫЙ ФАЙЛ =============
add_executable(game_server
    src/main.cpp
    src/middleware/json/json_loader.cpp
    src/middleware/json/json_converter.cpp
    src/middleware/json/json_model_converter.cpp
    src/middleware/json/boost_json.cpp
    src/middleware/logging/logging_data_storage.cpp
    src/middleware/logging/logger.cpp
    src/app/authentication/player_tokens.cpp
    src/app/application.cpp
    src/app/game_session.cpp
    src/app/player.cpp
    src/web/server/http_server.cpp
    src/web/request_handlers/request_handler.cpp
    src/middleware/program_options/program_options.cpp
    src/middleware/time_management/ticker.cpp
    src/middleware/error_handling/error_report.cpp
    src/middleware/game_data_store/lost_object_serialization.cpp
    src/middleware/game_data_store/dog_serialization.cpp
    src/middleware/game_data_store/player_serialization.cpp
    src/middleware/game_data_store/game_session_serialization.cpp
    src/app/state_serializer.cpp
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

target_link_libraries(game_server PRIVATE Threads::Threads CONAN_PKG::boost game_model_lib)

# ============= ТЕСТЫ =============
include(CTest)

# Получаем пути к Catch2 от Conan
if(DEFINED CONAN_INCLUDE_DIRS_CATCH2)
    message(STATUS "Catch2 include dirs: ${CONAN_INCLUDE_DIRS_CATCH2}")
    set(CATCH2_INCLUDE_DIR ${CONAN_INCLUDE_DIRS_CATCH2})
else()
    # Пробуем найти Catch2 в стандартных местах
    find_path(CATCH2_INCLUDE_DIR catch2/catch_test_macros.hpp
        PATHS
        /usr/local/include
        /usr/include
        ${CONAN_CATCH2_ROOT}/include
        ${CMAKE_SOURCE_DIR}/build/install/include
    )
    message(STATUS "Catch2 include dir found: ${CATCH2_INCLUDE_DIR}")
endif()

# Тесты
add_executable(loot_generator_tests
    tests/loot-generator-tests.cpp
)

target_include_directories(loot_generator_tests PRIVATE
    src
    src/model
    ${CATCH2_INCLUDE_DIR}
)

target_link_libraries(loot_generator_tests PRIVATE
    game_model_lib
    Threads::Threads
    CONAN_PKG::catch2
    m
)

target_compile_features(loot_generator_tests PRIVATE cxx_std_20)

add_executable(collision_detector_tests
    tests/collision-detector-tests.cpp
)

target_include_directories(collision_detector_tests PRIVATE
    src
    src/model/physics
    ${CATCH2_INCLUDE_DIR}
)

target_link_libraries(collision_detector_tests PRIVATE
    collision_detection_lib
    Threads::Threads
    CONAN_PKG::catch2
    m
)

target_compile_features(collision_detector_tests PRIVATE cxx_std_20)

add_executable(state_serialization_tests
    tests/state-serialization-tests.cpp
)

target_include_directories(state_serialization_tests PRIVATE
    src
    src/model
    src/model/physics
    src/middleware
    src/middleware/game_data_store
    ${CATCH2_INCLUDE_DIR}
)

target_link_libraries(state_serialization_tests PRIVATE
    game_model_lib
    collision_detection_lib
    Threads::Threads
    CONAN_PKG::catch2
    m
)

target_compile_features(state_serialization_tests PRIVATE cxx_std_20)

if(BUILD_TESTING)
    add_test(NAME loot_generator_tests COMMAND loot_generator_tests)
    add_test(NAME collision_detector_tests COMMAND collision_detector_tests)
    add_test(NAME state_serialization_tests COMMAND state_serialization_tests)
endif()

# Добавляем цели для запуска тестов
add_custom_target(run_loot_tests
    COMMAND loot_generator_tests
    DEPENDS loot_generator_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "🚀 Running loot generator tests..."
)

add_custom_target(run_collision_tests
    COMMAND collision_detector_tests
    DEPENDS collision_detector_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "🚀 Running collision detection tests..."
)

add_custom_target(run_state_tests
    COMMAND state_serialization_tests
    DEPENDS state_serialization_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "🚀 Running state serialization tests..."
)

add_custom_target(run_all_tests
    COMMAND loot_generator_tests && collision_detector_tests && state_serialization_tests
    DEPENDS loot_generator_tests collision_detector_tests state_serialization_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "🚀 Running all tests..."
)

message(STATUS "🎮 Build targets:")
message(STATUS "   - game_server (main executable)")
message(STATUS "   - collision_detection_lib (static library)")
message(STATUS "   - game_model_lib (game model library)")
message(STATUS "   - loot_generator_tests (loot generator tests)")
message(STATUS "   - collision_detector_tests (collision tests)")
message(STATUS "   - state_serialization_tests (state serialization tests)")
message(STATUS "   - run_loot_tests (run loot generator tests)")
message(STATUS "   - run_collision_tests (run collision detection tests)")
message(STATUS "   - run_state_tests (run state serialization tests)")
message(STATUS "   - run_all_tests (run all tests)")