cmake_minimum_required(VERSION 3.11)

project(collision_detector CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============= УНИВЕРСАЛЬНЫЙ ПОИСК CONAN =============
set(CONAN_BUILDINFO_PATHS
    "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake"
    "${CMAKE_SOURCE_DIR}/build/conanbuildinfo.cmake"
    "/app/build/conanbuildinfo.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/conanbuildinfo.cmake"
    "${CMAKE_BINARY_DIR}/conan/build/conanbuildinfo.cmake"
)

set(CONAN_BUILDINFO_FOUND FALSE)
foreach(PATH ${CONAN_BUILDINFO_PATHS})
    if(EXISTS ${PATH})
        message(STATUS "✅ Found conanbuildinfo.cmake at: ${PATH}")
        include(${PATH})
        set(CONAN_BUILDINFO_FOUND TRUE)
        break()
    endif()
endforeach()

if(NOT CONAN_BUILDINFO_FOUND)
    message(FATAL_ERROR "❌ conanbuildinfo.cmake not found! Searched in: ${CONAN_BUILDINFO_PATHS}")
endif()

# Применяем настройки Conan - ЭТО ВАЖНО!
conan_basic_setup(TARGETS)

# Добавляем пути к модулям CMake от Conan
list(APPEND CMAKE_MODULE_PATH ${CONAN_CMAKE_MODULE_PATH})
list(APPEND CMAKE_PREFIX_PATH ${CONAN_CMAKE_MODULE_PATH})
# ============= КОНЕЦ ПОИСКА CONAN =============

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ============= БИБЛИОТЕКА COLLISION DETECTION =============
add_library(collision_detection_lib STATIC
    src/collision_detector.cpp
)

target_include_directories(collision_detection_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CONAN_INCLUDE_DIRS}
)

target_link_libraries(collision_detection_lib PUBLIC
    Threads::Threads
)
# ============= КОНЕЦ БИБЛИОТЕКИ =============

# ============= ТЕСТЫ =============
add_executable(collision_detection_tests
    tests/collision-detector-tests.cpp
)

target_include_directories(collision_detection_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CONAN_INCLUDE_DIRS}
)

# ИСПРАВЛЕНО: используем CONAN_PKG::catch2 target
target_link_libraries(collision_detection_tests PRIVATE
    collision_detection_lib
    Threads::Threads
    CONAN_PKG::catch2  # Conan автоматически добавит и Catch2Main, и Catch2
    m
)

target_compile_features(collision_detection_tests PRIVATE cxx_std_20)

# Добавляем цель для запуска тестов
add_custom_target(run_tests
    COMMAND collision_detection_tests
    DEPENDS collision_detection_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "🚀 Running collision detection tests..."
)
# ============= КОНЕЦ ТЕСТОВ =============

# Выводим информацию для отладки
message(STATUS "📚 Conan libraries: ${CONAN_LIBS}")
message(STATUS "📁 Conan include dirs: ${CONAN_INCLUDE_DIRS}")
message(STATUS "📂 Conan lib dirs: ${CONAN_LIB_DIRS}")