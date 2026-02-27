cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# Conan уже добавил все include директории через conan_basic_setup()
# Поэтому отдельный find_package для Boost не обязателен, но можно оставить

find_package(Boost 1.78.0 REQUIRED)
find_package(Threads REQUIRED)

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

# ИЗМЕНЕНИЕ: линкуем ВСЕ библиотеки из Conan
# ${CONAN_LIBS} содержит boost, jsoncpp и другие зависимости
target_link_libraries(game_server PRIVATE Threads::Threads ${CONAN_LIBS})

add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)