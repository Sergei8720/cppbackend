# Создать образ на основе базового слоя gcc (там будет ОС и сам компилятор).
FROM gcc:11.3 as build

# Выполнить установку зависимостей внутри контейнера.
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

# Создать профиль default
RUN conan profile new default --detect --force
RUN conan profile update settings.compiler.libcxx=libstdc++11 default

# Копируем conanfile.txt сначала для установки зависимостей
COPY conanfile.txt /app/
RUN mkdir -p /app/build && cd /app/build && \
    conan install .. --build=missing

# Копируем все исходные файлы из директории src
# (включая все .h и .cpp файлы)
COPY ./src /app/src
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

# Второй контейнер
FROM ubuntu:22.04 as run

RUN groupadd -r www && useradd -r -g www www
USER www

COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data

ENTRYPOINT ["/app/game_server", "/app/data/config.json"]