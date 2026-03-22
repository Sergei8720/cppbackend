# Создать образ на основе базового слоя gcc (там будет ОС и сам компилятор).
# 11.3 — используемая версия gcc.
FROM gcc:11.3 AS build

# Выполнить установку зависимостей внутри контейнера.
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan

# Настраиваем профиль Conan (обязательно для conan install)
RUN conan profile detect --force

# копируем conanfile.txt в контейнер и запускаем conan install
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11 -s build_type=Release

# только после этого копируем остальные исходники
COPY ./src /app/src
COPY ./data /app/data
COPY ./static /app/static
COPY ./CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .


# Второй контейнер в том же докерфайле
FROM ubuntu:22.04 AS run

# Устанавливаем необходимые библиотеки для запуска
RUN apt update && apt install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# Создадим пользователя www
RUN groupadd -r www && useradd -r -g www www

# Скопируем приложение со сборочного контейнера
COPY --from=build /app/build/bin/game_server /app/game_server

# Копируем данные и статику
COPY ./data /app/data
COPY ./static /app/static

# Создаем директорию для state файла
RUN mkdir -p /tmp/volume && chown www:www /tmp/volume

USER www

# Запускаем игровой сервер с правильными аргументами
# --tick-period 0 означает ручное управление временем (для тестов)
ENTRYPOINT ["/app/game_server", \
            "--config-file", "/app/data/config.json", \
            "--www-root", "/app/static", \
            "--tick-period", "0", \
            "--state-file", "/tmp/volume/state"]