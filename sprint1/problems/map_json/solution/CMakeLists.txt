cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")

# Проверяем, что conanbuildinfo.cmake существует
if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup(TARGETS)
else()
    message(WARNING "Conan: conanbuildinfo.cmake not found. Skipping Conan setup.")
endif()

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/sdk.h
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

# Явно линкуем с Conan-целями
target_link_libraries(game_server
    CONAN_PKG::boost
    CONAN_PKG::jsoncpp
)

add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)
