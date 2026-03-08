# Используем многоступенчатую сборку
FROM gcc:11.3 as build

# Устанавливаем необходимые пакеты
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

# Настраиваем Conan
RUN conan profile new default --detect --force
RUN conan profile update settings.compiler.libcxx=libstdc++11 default

# Копируем исходный код
COPY ./src /app/src
COPY ./tests /app/tests
COPY ./CMakeLists.txt /app/
COPY ./conanfile.txt /app/

# Создаем директорию для сборки
RUN mkdir -p /app/build
WORKDIR /app/build

# Устанавливаем зависимости через Conan
RUN conan install .. --build=missing -s build_type=Release

# Собираем все цели проекта (game_server и все тесты)
RUN cmake .. -DCMAKE_BUILD_TYPE=Release \
    && cmake --build . -j$(nproc)

# Финальный образ для запуска тестов
FROM ubuntu:22.04 as run

# Устанавливаем необходимые библиотеки для запуска
RUN apt update && apt install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# Копируем собранные тесты из build stage
COPY --from=build /app/build/bin/collision_tests /app/collision_tests
COPY --from=build /app/build/bin/game_tests /app/game_tests
COPY --from=build /app/tests /app/tests

WORKDIR /app

# Запускаем тесты (можно запустить любой или оба)
CMD ["./collision_tests"]