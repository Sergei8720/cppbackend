cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")

# Определяем тип сборки, если не задан
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# Подключаем правильный файл Conan в зависимости от типа сборки
set(CONAN_BUILDINFO_FILE "${CMAKE_BINARY_DIR}/conanbuildinfo_${CMAKE_BUILD_TYPE}.cmake")
if(EXISTS ${CONAN_BUILDINFO_FILE})
    message(STATUS "Using ${CONAN_BUILDINFO_FILE}")
    include(${CONAN_BUILDINFO_FILE})
elseif(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    message(STATUS "Using conanbuildinfo_multi.cmake")
    include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
else()
    message(FATAL_ERROR "Conan buildinfo not found! Run Conan first")
endif()

# Вызываем conan_basic_setup с правильными параметрами
conan_basic_setup(TARGETS)

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
    CONAN_PKG::boost
)

# Добавляем директории с заголовками
target_include_directories(game_server 
    PRIVATE
    src
    ${CONAN_INCLUDE_DIRS}
)

# Используем современный способ указания использовать std::string_view
# Вместо устаревшего BOOST_BEAST_USE_STD_STRING_VIEW
add_compile_definitions(BOOST_NO_CXX17_HDR_STRING_VIEW=0)

# Диагностика
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CONAN_INCLUDE_DIRS: ${CONAN_INCLUDE_DIRS}")