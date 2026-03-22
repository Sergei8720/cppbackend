# Используем многоступенчатую сборку
FROM gcc:11.3 AS build

# Устанавливаем необходимые пакеты
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

# Настраиваем Conan
RUN conan profile new default --detect --force && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

# Копируем исходный код
COPY ./src /app/src
COPY ./tests /app/tests
COPY ./CMakeLists.txt /app/
COPY ./conanfile.txt /app/
COPY ./data /app/data
COPY ./static /app/static

# Создаем директорию для сборки
RUN mkdir -p /app/build
WORKDIR /app/build

# Устанавливаем зависимости через Conan
RUN conan install .. --build=missing -s build_type=Release

# Собираем только основной сервер (без тестов)
RUN cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF && \
    cmake --build . -j$(nproc)

# Финальный образ для запуска сервера
FROM ubuntu:22.04 AS run

# Устанавливаем необходимые библиотеки для запуска
RUN apt update && apt install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# Создаем пользователя www
RUN groupadd -r www && useradd -r -g www www

# Копируем собранный сервер
COPY --from=build /app/build/bin/game_server /app/game_server

# Копируем конфигурационные файлы и статику
COPY ./data /app/data
COPY ./static /app/static

# Создаем директорию для state файла
RUN mkdir -p /tmp/volume && chown www:www /tmp/volume

USER www

# Запускаем сервер
ENTRYPOINT ["/app/game_server", \
            "--config-file", "/app/data/config.json", \
            "--www-root", "/app/static", \
            "--tick-period", "0", \
            "--state-file", "/tmp/volume/state", \
            "--save-state-period", "1000"]