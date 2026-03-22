cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall")

# Подключаем файлы, сгенерированные Conan
if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup(TARGETS)
else()
    message(WARNING "Conan build info not found. Make sure to run 'conan install' first.")
endif()

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Поиск Boost через find_package (работает с cmake_find_package генератором)
find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(collision_detection_lib STATIC
	src/model/physics/collision_detector.h
	src/model/physics/collision_detector.cpp
	src/model/physics/geom.h
)
target_link_libraries(collision_detection_lib PUBLIC Threads::Threads)

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
target_link_libraries(game_model_lib PUBLIC Threads::Threads collision_detection_lib)

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
	src/middleware/game_data_store/lost_object_serialization.cpp
	src/middleware/game_data_store/dog_serialization.cpp
	src/middleware/game_data_store/player_serialization.cpp
	src/middleware/game_data_store/game_session_serialization.cpp
	src/app/state_serializer.cpp
	src/web/request_handlers/request_handler.cpp
	src/web/server/http_server.cpp
	src/app/authentication/player_tokens.cpp
	src/app/application.cpp
	src/app/game_session.cpp
	src/app/player.cpp
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
target_link_libraries(game_server PRIVATE Threads::Threads game_model_lib)

# Используем Catch2 через Conan (не через find_package)
# Catch2 уже настроен через conan_basic_setup
add_executable(game_server_tests
	tests/loot-generator-tests.cpp
)
target_link_libraries(game_server_tests PRIVATE game_model_lib)
# Catch2 будет добавлен через conan_basic_setup

add_executable(collision_detection_tests
	tests/collision-detector-tests.cpp
)
target_link_libraries(collision_detection_tests PRIVATE collision_detection_lib)

add_executable(state_serialization_tests
	tests/state-serialization-tests.cpp
)
target_include_directories(state_serialization_tests PRIVATE
	src
	src/middleware
	src/middleware/game_data_store
	src/model/physics
)
target_link_libraries(state_serialization_tests PRIVATE collision_detection_lib)

# Включаем Catch2 тесты через Conan
if(CONAN_CATCH2_ROOT)
    include(${CONAN_CATCH2_ROOT}/Catch.cmake)
endif()

catch_discover_tests(game_server_tests)
catch_discover_tests(collision_detection_tests)
catch_discover_tests(state_serialization_tests)

# Boost.Beast будет использовать std::string_view вместо boost::string_view
add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)
add_compile_definitions(BOOST_THREAD_PROVIDES_FUTURE)