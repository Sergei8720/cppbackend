cmake_minimum_required(VERSION 3.15)
project(sync_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Отключаем C компилятор, так как нам нужен только C++
enable_language(CXX)

# Поиск зависимостей через Conan
find_package(Boost 1.81.0 REQUIRED)

# Создаем исполняемый файл
add_executable(sync_server main.cpp)

# Подключаем зависимости
target_link_libraries(sync_server 
    PUBLIC 
        Boost::boost
        Boost::system
)

# Настройка свойств для цели
set_target_properties(sync_server PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
