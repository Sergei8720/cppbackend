cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")

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

# Используем режим NO_OUTPUT_DIRS чтобы получить переменные с путями
conan_basic_setup(NO_OUTPUT_DIRS)

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

# Преобразуем тип сборки в верхний регистр для переменных Conan
string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)

# Добавляем директории с заголовками из Conan
target_include_directories(game_server PRIVATE 
    ${CONAN_INCLUDE_DIRS_${BUILD_TYPE_UPPER}}
)

# Линкуем библиотеки
target_link_libraries(game_server 
    PRIVATE 
    Threads::Threads 
    ${CONAN_LIBS_${BUILD_TYPE_UPPER}}
)

add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)

# Диагностика
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "BUILD_TYPE_UPPER: ${BUILD_TYPE_UPPER}")
message(STATUS "CONAN_INCLUDE_DIRS_${BUILD_TYPE_UPPER}: ${CONAN_INCLUDE_DIRS_${BUILD_TYPE_UPPER}}")
message(STATUS "CONAN_LIBS_${BUILD_TYPE_UPPER}: ${CONAN_LIBS_${BUILD_TYPE_UPPER}}")