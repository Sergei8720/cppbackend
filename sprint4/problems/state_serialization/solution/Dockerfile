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
    cmake --build . -j$(nproc) && \
    # Создаем директорию для установки и копируем бинарник
    mkdir -p /app/install/bin && \
    cp /app/build/game_server /app/install/bin/ || \
    cp /app/build/bin/game_server /app/install/bin/ 2>/dev/null || \
    find /app/build -name "game_server" -exec cp {} /app/install/bin/ \;

# Финальный образ для запуска сервера
FROM ubuntu:22.04 AS run

# Устанавливаем необходимые библиотеки для запуска
RUN apt update && apt install -y \
    libstdc++6 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Создаем пользователя www
RUN groupadd -r www && useradd -r -g www www

# Копируем собранный сервер (пробуем разные возможные пути)
COPY --from=build /app/install/bin/game_server /app/game_server
# Fallback: если бинарник не найден в install, пробуем скопировать напрямую
COPY --from=build /app/build/game_server /app/game_server 2>/dev/null || true
COPY --from=build /app/build/bin/game_server /app/game_server 2>/dev/null || true

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

# Проверяем, что бинарник скопировался и имеет права на выполнение
RUN test -x /app/game_server || (echo "game_server not found or not executable" && exit 1)

# Переключаемся на непривилегированного пользователя
USER www

# Упрощенный healthcheck (не требует curl)
HEALTHCHECK --interval=30s --timeout=3s --start-period=10s --retries=3 \
  CMD /app/game_server --check-health 2>/dev/null || exit 1

# Используем exec-форму для правильной обработки сигналов
ENTRYPOINT ["/app/game_server"]
CMD ["--config-file", "/app/data/config.json", \
     "--www-root", "/app/static", \
     "--tick-period", "0", \
     "--state-file", "/tmp/volume/state", \
     "--save-state-period", "1000"]