cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")

# Подключаем Conan
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)  # Используем TARGETS режим!

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Создаем исполняемый файл
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

# Линкуем с Boost через Conan target
target_link_libraries(game_server 
    PRIVATE
    Threads::Threads
    CONAN_PKG::boost  # Вместо ${CONAN_LIBS}
)

# Добавляем директории с заголовками
target_include_directories(game_server 
    PRIVATE
    src
    ${CONAN_INCLUDE_DIRS}
)

add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)