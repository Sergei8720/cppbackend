cmake_minimum_required(VERSION 3.15)
project(sync_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.81.0 REQUIRED COMPONENTS system)

add_executable(sync_server main.cpp)
target_link_libraries(sync_server Boost::system)