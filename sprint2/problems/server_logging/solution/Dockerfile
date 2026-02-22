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

# Создать профиль default вручную перед обновлением
RUN conan profile new default --detect --force

# Теперь можно обновить профиль
RUN conan profile update settings.compiler.libcxx=libstdc++11 default

COPY conanfile.txt /app/
RUN mkdir -p /app/build && cd /app/build && \
    conan install .. --build=missing

# Копируем все исходники, включая заголовочные файлы
COPY ./src /app/src
COPY ./include /app/include 2>/dev/null || true
COPY ./CMakeLists.txt /app/
# Копируем все .h и .cpp файлы из корня, если они там есть
COPY *.h *.cpp /app/ 2>/dev/null || true

# Альтернативный вариант - копировать всё содержимое текущей директории
# COPY . /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

# Второй контейнер в том же докерфайле
FROM ubuntu:22.04 as run

# Создадим пользователя www
RUN groupadd -r www && useradd -r -g www www
USER www

# Скопируем приложение со сборочного контейнера в директорию /app.
COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data

# Запускаем игровой сервер
ENTRYPOINT ["/app/game_server", "/app/data/config.json"]