cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")

# Устанавливаем директорию для выходных файлов
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Устанавливаем тип сборки, если он не задан
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Подключаем правильный файл для cmake_multi
if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    message(STATUS "Using conanbuildinfo_multi.cmake")
    include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
else()
    message(STATUS "Using conanbuildinfo.cmake")
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
endif()

conan_basic_setup()

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

# Линкуем библиотеки для текущего типа сборки
target_link_libraries(game_server 
    PRIVATE 
    Threads::Threads 
    ${CONAN_LIBS_${CMAKE_BUILD_TYPE}}
)

add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")