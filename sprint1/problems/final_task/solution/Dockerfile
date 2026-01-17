# Создать образ на основе базового слоя gcc (там будет ОС и сам компилятор).
# 11.3 — используемая версия gcc.
FROM gcc:11.3 as build

# Выполнить установку зависимостей внутри контейнера.
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan

# Копируем conanfile.txt в контейнер и запускаем conan install
COPY conanfile.txt /app/
RUN mkdir -p /app/build && cd /app/build && \
    conan install .. --build=missing

# Копируем исходный код
COPY ./src /app/src
COPY ./data /app/data
COPY CMakeLists.txt /app/

# Собираем проект
RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

# Второй контейнер в том же докерфайле
FROM ubuntu:22.04 as run

# Устанавливаем необходимые библиотеки для запуска
RUN apt update && \
    apt install -y \
      libstdc++6 \
      libgcc-s1 \
    && \
    rm -rf /var/lib/apt/lists/*

# Создадим пользователя www
RUN groupadd -r www && useradd -r -g www www
USER www

# Скопируем приложение со сборочного контейнера
COPY --from=build /app/build/game_server /app/
COPY ./data /app/data

# Запускаем игровой сервер
ENTRYPOINT ["/app/game_server", "/app/data/config.json"]