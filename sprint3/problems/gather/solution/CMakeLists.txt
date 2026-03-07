cmake_minimum_required(VERSION 3.11)

project(collision_detector CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============= УНИВЕРСАЛЬНЫЙ ПОИСК CONAN =============
# Для cmake_multi генератора ищем оба варианта
set(CONAN_BUILDINFO_PATHS
    "${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake"
    "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake"
    "${CMAKE_SOURCE_DIR}/build/conanbuildinfo_multi.cmake"
    "${CMAKE_SOURCE_DIR}/build/conanbuildinfo.cmake"
    "/app/build/conanbuildinfo_multi.cmake"
    "/app/build/conanbuildinfo.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/conanbuildinfo_multi.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/conanbuildinfo.cmake"
    "${CMAKE_BINARY_DIR}/conan/build/conanbuildinfo_multi.cmake"
    "${CMAKE_BINARY_DIR}/conan/build/conanbuildinfo.cmake"
)

set(CONAN_BUILDINFO_FOUND FALSE)
foreach(PATH ${CONAN_BUILDINFO_PATHS})
    if(EXISTS ${PATH})
        message(STATUS "✅ Found conanbuildinfo at: ${PATH}")
        include(${PATH})
        set(CONAN_BUILDINFO_FOUND TRUE)
        break()
    endif()
endforeach()

if(NOT CONAN_BUILDINFO_FOUND)
    message(FATAL_ERROR "❌ conanbuildinfo file not found! Searched in: ${CONAN_BUILDINFO_PATHS}")
endif()

# Применяем настройки Conan - для multi используем TARGETS
if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake")
    conan_basic_setup(TARGETS)
else()
    conan_basic_setup()
endif()

# ============= ВАЖНО: ЯВНО ДОБАВЛЯЕМ ПУТИ К ЗАГОЛОВОЧНЫМ ФАЙЛАМ =============
# Добавляем пути к модулям CMake от Conan
if(DEFINED CONAN_CMAKE_MODULE_PATH)
    list(APPEND CMAKE_MODULE_PATH ${CONAN_CMAKE_MODULE_PATH})
    list(APPEND CMAKE_PREFIX_PATH ${CONAN_CMAKE_MODULE_PATH})
endif()

# Явно добавляем include директории от Conan в глобальные пути
if(CONAN_INCLUDE_DIRS)
    include_directories(${CONAN_INCLUDE_DIRS})
    message(STATUS "📁 Added Conan include dirs: ${CONAN_INCLUDE_DIRS}")
endif()

# Для отладки выводим все переменные Conan
message(STATUS "=== CONAN VARIABLES ===")
message(STATUS "CONAN_INCLUDE_DIRS: ${CONAN_INCLUDE_DIRS}")
message(STATUS "CONAN_LIBS: ${CONAN_LIBS}")
message(STATUS "CONAN_LIB_DIRS: ${CONAN_LIB_DIRS}")
message(STATUS "CONAN_CXX_FLAGS: ${CONAN_CXX_FLAGS}")
message(STATUS "========================")
# ============= КОНЕЦ ДОБАВЛЕНИЯ ПУТЕЙ =============

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ============= БИБЛИОТЕКА COLLISION DETECTION =============
add_library(collision_detection_lib STATIC
    src/collision_detector.cpp
)

target_include_directories(collision_detection_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    $<TARGET_PROPERTY:CONAN_PKG::catch2,INTERFACE_INCLUDE_DIRECTORIES>
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
    $<TARGET_PROPERTY:CONAN_PKG::catch2,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(collision_detection_tests PRIVATE
    collision_detection_lib
    Threads::Threads
    CONAN_PKG::catch2  # Используем таргет Catch2
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

# Для multi генератора добавляем цель для каждого типа сборки
if(CMAKE_CONFIGURATION_TYPES)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        add_custom_target(run_tests_${config}
            COMMAND ${CMAKE_BINARY_DIR}/${config}/bin/collision_detection_tests
            DEPENDS collision_detection_tests
            COMMENT "🚀 Running tests (${config})..."
        )
    endforeach()
endif()
# ============= КОНЕЦ ТЕСТОВ =============