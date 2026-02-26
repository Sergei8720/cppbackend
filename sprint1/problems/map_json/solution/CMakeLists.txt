cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")

# Для cmake_multi подключаем правильный файл
if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
else()
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
endif()

conan_basic_setup()

# Не используем find_package для Boost - он уже в CONAN_LIBS
# Просто подключаем директории
include_directories(${CONAN_INCLUDE_DIRS})

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
    src/boost_json.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/request_handler.cpp
    src/request_handler.h
    src/json_converter.cpp
    src/json_converter.h
)

# Линкуем ВСЕ библиотеки из Conan, включая jsoncpp
target_link_libraries(game_server 
    PRIVATE 
    Threads::Threads 
    ${CONAN_LIBS}  # Вместо jsoncpp
)

add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)

# Для отладки - посмотрим, что линкуется
message(STATUS "CONAN_LIBS: ${CONAN_LIBS}")