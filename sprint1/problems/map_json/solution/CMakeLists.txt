cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")

# Подключаем правильный файл для cmake_multi
if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    message(STATUS "Using conanbuildinfo_multi.cmake")
    include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
else()
    message(STATUS "Using conanbuildinfo.cmake")
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
endif()

# Режим TARGETS создаст цели для каждой библиотеки
conan_basic_setup(TARGETS)

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

# Линкуем через цели Conan и явно указываем директории с заголовками
target_link_libraries(game_server 
    PRIVATE 
    Threads::Threads 
    CONAN_PKG::boost
    CONAN_PKG::jsoncpp
)

# Получаем пути к заголовочным файлам из целей Conan
get_target_property(BOOST_INCLUDE_DIRS CONAN_PKG::boost INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(JSONCPP_INCLUDE_DIRS CONAN_PKG::jsoncpp INTERFACE_INCLUDE_DIRECTORIES)

# Добавляем директории с заголовками
target_include_directories(game_server PRIVATE 
    ${BOOST_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)

# Диагностика
message(STATUS "BOOST_INCLUDE_DIRS: ${BOOST_INCLUDE_DIRS}")
message(STATUS "JSONCPP_INCLUDE_DIRS: ${JSONCPP_INCLUDE_DIRS}")