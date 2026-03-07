cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============= УНИВЕРСАЛЬНЫЙ ПОИСК CONAN =============
# Ищем все возможные варианты conanbuildinfo (включая multi)
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
set(CONAN_IS_MULTI FALSE)

foreach(PATH ${CONAN_BUILDINFO_PATHS})
    if(EXISTS ${PATH})
        message(STATUS "✅ Found conanbuildinfo at: ${PATH}")
        include(${PATH})
        set(CONAN_BUILDINFO_FOUND TRUE)
        if(PATH MATCHES ".*_multi\\.cmake$")
            set(CONAN_IS_MULTI TRUE)
        endif()
        break()
    endif()
endforeach()

if(NOT CONAN_BUILDINFO_FOUND)
    message(FATAL_ERROR "❌ conanbuildinfo file not found! Searched in: ${CONAN_BUILDINFO_PATHS}")
endif()

# Применяем настройки Conan
if(CONAN_IS_MULTI)
    conan_basic_setup(TARGETS)
else()
    conan_basic_setup(TARGETS)  # TARGETS предпочтительнее для обоих случаев
endif()

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

# Выводим информацию о Conan
message(STATUS "=== CONAN VARIABLES ===")
message(STATUS "CONAN_INCLUDE_DIRS: ${CONAN_INCLUDE_DIRS}")
message(STATUS "CONAN_LIBS: ${CONAN_LIBS}")
message(STATUS "CONAN_LIB_DIRS: ${CONAN_LIB_DIRS}")
message(STATUS "CONAN_CXX_FLAGS: ${CONAN_CXX_FLAGS}")
message(STATUS "========================")
# ============= КОНЕЦ ПОИСКА CONAN =============

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# !!! УДАЛЯЕМ find_package(Boost) - он больше не нужен !!!
# Boost уже включен в ${CONAN_LIBS} и ${CONAN_INCLUDE_DIRS}

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_compile_definitions(
    BOOST_BEAST_USE_STD_STRING_VIEW
    BOOST_JSON_STANDALONE
)

# ============= БИБЛИОТЕКА COLLISION DETECTION =============
add_library(collision_detection_lib STATIC
    src/collision_detector.cpp
)

target_include_directories(collision_detection_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CONAN_INCLUDE_DIRS}  # Добавляем include пути от Conan
)

target_link_libraries(collision_detection_lib PUBLIC
    Threads::Threads
    ${CONAN_LIBS}  # Добавляем библиотеки от Conan (включая Boost)
)
# ============= КОНЕЦ БИБЛИОТЕКИ =============

# ============= ОСНОВНОЙ ИСПОЛНЯЕМЫЙ ФАЙЛ =============
add_executable(game_server
    src/main.cpp
    src/model/map.cpp
    src/model/game.cpp
    src/model/dog.cpp
    src/model/roadmap.cpp
    src/model/support_types.cpp
    src/model/loot_generator.cpp
    src/model/lost_object.cpp
    src/json/json_loader.cpp
    src/json/json_converter.cpp
    src/json/json_model_converter.cpp
    src/request_handlers/request_handler.cpp
    src/utils/request_handlers_utils.cpp
    src/utils/filesystem_utils.cpp
    src/utils/random_generators.cpp
    src/boost_json.cpp
    src/logging/logging_data_storage.cpp
    src/logging/logger.cpp
    src/authentication/player_tokens.cpp
    src/app/application.cpp
    src/app/game_session.cpp
    src/app/player.cpp
    src/server/http_server.cpp
    src/program_options/program_options.cpp
    src/time_management/ticker.cpp
    src/error_handling/error_report.cpp
)

target_include_directories(game_server PRIVATE
    src
    src/json
    src/request_handlers
    src/utils
    src/logging
    src/model
    src/authentication
    src/app
    src/server
    src/program_options
    src/time_management
    src/error_handling
    ${CONAN_INCLUDE_DIRS}
)

target_link_directories(game_server PRIVATE ${CONAN_LIB_DIRS})

target_link_libraries(game_server PRIVATE 
    collision_detection_lib  # Подключаем библиотеку коллизий
    Threads::Threads 
    ${CONAN_LIBS}  # Все библиотеки от Conan
)
# ============= КОНЕЦ ОСНОВНОГО ФАЙЛА =============

# ============= ТЕСТЫ =============
# Тесты для основного приложения
add_executable(game_tests
    tests/loot_generator_tests.cpp
    src/model/loot_generator.cpp
)

target_include_directories(game_tests PRIVATE
    src
    src/model
    ${CONAN_INCLUDE_DIRS}
    $<TARGET_PROPERTY:CONAN_PKG::catch2,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_directories(game_tests PRIVATE ${CONAN_LIB_DIRS})

target_link_libraries(game_tests PRIVATE
    Threads::Threads
    CONAN_PKG::catch2
    ${CONAN_LIBS}  # Добавляем для доступа к Boost, если нужно в тестах
    m
)

target_compile_features(game_tests PRIVATE cxx_std_20)

# Тесты для библиотеки коллизий
add_executable(collision_tests
    tests/collision-detector-tests.cpp
)

target_include_directories(collision_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CONAN_INCLUDE_DIRS}
    $<TARGET_PROPERTY:CONAN_PKG::catch2,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(collision_tests PRIVATE
    collision_detection_lib
    Threads::Threads
    CONAN_PKG::catch2
    ${CONAN_LIBS}  # Добавляем для доступа к Boost, если нужно в тестах
    m
)

target_compile_features(collision_tests PRIVATE cxx_std_20)

# Общая цель для запуска всех тестов
add_custom_target(run_all_tests
    COMMAND game_tests && collision_tests
    DEPENDS game_tests collision_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "🚀 Running all tests..."
)

# Отдельные цели для каждого набора тестов
add_custom_target(run_game_tests
    COMMAND game_tests
    DEPENDS game_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "🚀 Running game tests..."
)

add_custom_target(run_collision_tests
    COMMAND collision_tests
    DEPENDS collision_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "🚀 Running collision detection tests..."
)

# Для multi генератора добавляем цель для каждого типа сборки
if(CMAKE_CONFIGURATION_TYPES)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        add_custom_target(run_tests_${config}
            COMMAND ${CMAKE_BINARY_DIR}/${config}/bin/game_tests && ${CMAKE_BINARY_DIR}/${config}/bin/collision_tests
            DEPENDS game_tests collision_tests
            COMMENT "🚀 Running all tests (${config})..."
        )
    endforeach()
endif()
# ============= КОНЕЦ ТЕСТОВ =============

# Выводим итоговую информацию
message(STATUS "📚 Conan libraries: ${CONAN_LIBS}")
message(STATUS "📁 Conan include dirs: ${CONAN_INCLUDE_DIRS}")
message(STATUS "📂 Conan lib dirs: ${CONAN_LIB_DIRS}")
message(STATUS "📂 Conan module path: ${CONAN_CMAKE_MODULE_PATH}")
message(STATUS "🎮 Build targets:")
message(STATUS "   - game_server (main executable)")
message(STATUS "   - collision_detection_lib (static library)")
message(STATUS "   - game_tests (game tests)")
message(STATUS "   - collision_tests (collision tests)")
message(STATUS "   - run_all_tests (run all tests)")
message(STATUS "   - run_game_tests (run game tests)")
message(STATUS "   - run_collision_tests (run collision tests)")