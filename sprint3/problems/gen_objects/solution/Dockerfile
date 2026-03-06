# Создать образ на основе базового слоя gcc (там будет ОС и сам компилятор).
# 11.3 — используемая версия gcc.
FROM gcc:11.3 as build

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

# Копируем conanfile.txt до установки зависимостей
COPY conanfile.txt /app/
RUN mkdir -p /app/build && cd /app/build && \
    conan install .. --build=missing

# Копируем остальные исходники
COPY ./src /app/src
COPY ./tests /app/tests
COPY CMakeLists.txt /app/

# Компилируем проект - ИСПРАВЛЕНО!
RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_PREFIX_PATH=/root/.conan \
          .. && \
    cmake --build .

# Второй контейнер
FROM ubuntu:22.04 as run

# Устанавливаем необходимые библиотеки для запуска
RUN apt update && apt install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# Создадим пользователя www
RUN groupadd -r www && useradd -r -g www www
USER www

# Скопируем приложение со сборочного контейнера
COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data

# Запускаем игровой сервер
ENTRYPOINT ["/app/game_server", "/app/data/config.json"]