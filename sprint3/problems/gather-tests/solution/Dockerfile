# Создать образ на основе базового слоя gcc
FROM gcc:11.3 AS build

# Выполнить установку зависимостей внутри контейнера.
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

# СОЗДАЕМ И НАСТРАИВАЕМ ПРОФИЛЬ CONAN
RUN conan profile new default --detect --force
RUN conan profile update settings.compiler.libcxx=libstdc++11 default
RUN conan profile show default

# Копируем все исходники
COPY ./src /app/src
COPY ./tests /app/tests
COPY ./CMakeLists.txt /app/
COPY ./conanfile.txt /app/

# Проверяем структуру (для отладки)
RUN echo "=== SRC directory ===" && ls -la /app/src/ && \
    echo "=== TESTS directory ===" && ls -la /app/tests/

# Устанавливаем зависимости Conan
RUN mkdir -p /app/build && cd /app/build && \
    conan install .. --build=missing

# Компилируем тесты
RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_PREFIX_PATH=/root/.conan \
          .. && \
    cmake --build . --target collision_detection_tests

# Второй контейнер для запуска тестов
FROM ubuntu:22.04 AS run

# Устанавливаем необходимые библиотеки для запуска
RUN apt update && apt install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# Создадим пользователя www
RUN groupadd -r www && useradd -r -g www www

# Копируем тесты со сборочного контейнера
COPY --from=build /app/build/bin/collision_detection_tests /app/

USER www

# Запускаем тесты
ENTRYPOINT ["/app/collision_detection_tests"]