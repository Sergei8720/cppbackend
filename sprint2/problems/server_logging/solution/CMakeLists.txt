cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# Подключаем Boost.Log и его зависимости
find_package(Boost 1.78.0 REQUIRED COMPONENTS log log_setup thread filesystem date_time regex chrono atomic)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  link_libraries(${Boost_LIBRARIES})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(game_server
  src/main.cpp
  src/http_server.cpp
  src/http_server.h
  src/sdk.h
  src/model.h
  src/model.cpp
  src/tagged.h
  src/request_handler.cpp
  src/request_handler.h
  src/logger.h
  src/logging_request_handler.h
  src/json/json_loader.h
  src/json/json_loader.cpp
  src/json/json_converter.cpp
  src/json/json_converter.h
  src/response_storage/api_v1_response_storage.cpp
  src/response_storage/api_v1_response_storage.h
  src/response_storage/static_file_response_storage.cpp
  src/response_storage/static_file_response_storage.h
  src/utils/filesystem_utils.cpp
  src/utils/filesystem_utils.h
  src/utils/url_utils.cpp
  src/utils/url_utils.h
)

target_include_directories(game_server PRIVATE
  src
  src/json
  src/response_storage
  src/utils
)

target_link_libraries(game_server PRIVATE
    Threads::Threads
    jsoncpp
    Boost::log
    Boost::log_setup
    Boost::thread
    Boost::filesystem
    Boost::date_time
    Boost::regex
    Boost::chrono
    Boost::atomic
    )
add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)