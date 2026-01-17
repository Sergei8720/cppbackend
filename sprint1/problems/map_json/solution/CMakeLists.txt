cmake_minimum_required(VERSION 3.14)

project(game_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build tests" OFF)

find_package(Boost 1.78.0 REQUIRED COMPONENTS json)
find_package(Threads REQUIRED)
find_package(jsoncpp REQUIRED)

add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/model.h
    src/model.cpp
    src/tagged.h
    src/boost_json.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/request_handler.cpp
    src/request_handler.h
    src/json_converter.cpp
    src/json_converter.h
)

target_include_directories(game_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(game_server PRIVATE
    Threads::Threads
    Boost::json
    jsoncpp
)