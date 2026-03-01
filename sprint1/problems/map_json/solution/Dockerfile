# Создать образ на основе базового слоя gcc (там будет ОС и сам компилятор).
# 11.4 — используемая версия gcc в спринте 3.
FROM gcc:11.4 as build

# Выполнить установку зависимостей внутри контейнера.
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

# Создать профиль default с настройками как в спринте 3
RUN conan profile new default --detect --force

# ВАЖНО: Устанавливаем старый ABI как в спринте 3
RUN conan profile update settings.compiler.libcxx=libstdc++ default

# Принудительно очищаем кэш
RUN conan remove jsoncpp/1.9.5 -f || true
RUN conan remove boost/1.78.0 -f || true
RUN conan remove zlib/1.2.13 -f || true

COPY conanfile.txt /app/
RUN mkdir -p /app/build && cd /app/build && \
    # Явно указываем все настройки как в спринте 3
    conan install .. \
        --build=missing \
        -s compiler=gcc \
        -s compiler.version=11 \
        -s compiler.libcxx=libstdc++ \
        -s build_type=Release

# Проверяем хеш jsoncpp
RUN cd /app/build && \
    echo "JSONCPP HASH:" && \
    conan info . | grep jsoncpp

# только после этого копируем остальные исходники
COPY ./src /app/src
COPY CMakeLists.txt /app/

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