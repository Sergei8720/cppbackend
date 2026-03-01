cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
# Убираем флаг -g для Windows (он не поддерживается MSVC)
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(Boost 1.78.0 REQUIRED)
find_package(Threads REQUIRED)

# Добавляем определение для Windows
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7 target
    # Явно указываем использование динамической библиотеки (/MD)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
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

target_link_libraries(game_server PRIVATE
    ${CONAN_LIBS}
    Threads::Threads
    jsoncpp
)


add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)