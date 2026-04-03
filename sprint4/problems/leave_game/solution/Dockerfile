# Используем многоступенчатую сборку
FROM gcc:11.3 AS build

# Устанавливаем необходимые пакеты
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
      curl \
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

# Находим и копируем бинарник в правильное место
RUN mkdir -p /app/install/bin && \
    (find /app/build -name "game_server" -type f -exec cp {} /app/install/bin/ \; || true) && \
    chmod +x /app/install/bin/game_server

# Финальный образ для запуска сервера
FROM ubuntu:22.04 AS run

# Устанавливаем необходимые библиотеки для запуска
RUN apt update && apt install -y \
    libstdc++6 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Создаем пользователя www
RUN groupadd -r www && useradd -r -g www www

# Копируем собранный сервер
COPY --from=build /app/install/bin/game_server /app/game_server

# Копируем конфигурационные файлы и статику
COPY ./data /app/data
COPY ./static /app/static

# Создаем директорию для state файла с правильными правами
RUN mkdir -p /tmp/volume && \
    chown www:www /tmp/volume && \
    chmod 755 /tmp/volume

# Объявляем том для сохранения состояния между перезапусками
VOLUME ["/tmp/volume"]

# Устанавливаем рабочую директорию
WORKDIR /app

# Проверяем, что бинарник существует
RUN test -f /app/game_server && echo "Binary found" || (echo "Binary not found" && exit 1)

# Переключаемся на непривилегированного пользователя
USER www

# Healthcheck для мониторинга
HEALTHCHECK --interval=30s --timeout=3s --start-period=10s --retries=3 \
  CMD curl -f http://localhost:8080/api/v1/maps || exit 1

# Запускаем сервер с параметрами по умолчанию
ENTRYPOINT ["/app/game_server"]
CMD ["--config-file", "/app/data/config.json", \
     "--www-root", "/app/static", \
     "--tick-period", "50", \
     "--state-file", "/tmp/volume/state", \
     "--save-state-period", "1000"]