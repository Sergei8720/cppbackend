cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")

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

# Определяем переменные для текущего типа сборки
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CONAN_CURRENT_INCLUDE_DIRS ${CONAN_INCLUDE_DIRS_DEBUG})
    set(CONAN_CURRENT_LIBS ${CONAN_LIBS_DEBUG})
    message(STATUS "Using Debug configuration")
else()
    set(CONAN_CURRENT_INCLUDE_DIRS ${CONAN_INCLUDE_DIRS_RELEASE})
    set(CONAN_CURRENT_LIBS ${CONAN_LIBS_RELEASE})
    message(STATUS "Using Release configuration")
endif()

# Добавляем директории с заголовками из Conan
target_include_directories(game_server PRIVATE ${CONAN_CURRENT_INCLUDE_DIRS})

# Линкуем библиотеки
target_link_libraries(game_server 
    PRIVATE 
    Threads::Threads 
    ${CONAN_CURRENT_LIBS}
)

# Добавляем определение для Boost.Beast
target_compile_definitions(game_server PRIVATE BOOST_BEAST_USE_STD_STRING_VIEW)

# Диагностика для проверки правильности настроек
message(STATUS "CONAN_CURRENT_INCLUDE_DIRS: ${CONAN_CURRENT_INCLUDE_DIRS}")
message(STATUS "CONAN_CURRENT_LIBS: ${CONAN_CURRENT_LIBS}")
message(STATUS "Boost include dirs found: ")
foreach(dir ${CONAN_CURRENT_INCLUDE_DIRS})
    if(dir MATCHES ".*boost.*")
        message(STATUS "  ${dir}")
    endif()
endforeach()