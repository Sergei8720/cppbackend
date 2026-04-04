# Создать образ на основе базового слоя gcc
FROM gcc:11.3 as build

# Установка зависимостей
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan

# Conan
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11 -s build_type=Release

# Копируем исходники
COPY ./src /app/src
COPY ./data /app/data
COPY ./static /app/static
COPY ./tests /app/tests
COPY CMakeLists.txt /app/

# Сборка
RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

# Финальный образ
FROM ubuntu:22.04 as run

RUN groupadd -r www && useradd -r -g www www
USER www

COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data
COPY ./static /app/static

# ВАЖНО: добавляем --tick-period 0 для ручного режима
ENTRYPOINT ["/app/game_server", "--config-file", "/app/data/config.json", "--www-root", "/app/static", "--tick-period", "0"]