# Многоступенчатая сборка
FROM gcc:11.3 AS build

# Устанавливаем зависимости для сборки
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install "conan<2"

# Настраиваем Conan profile
RUN conan profile new default --detect --force && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

# Копируем conanfile.txt и устанавливаем зависимости
COPY conanfile.txt /app/
RUN mkdir -p /app/build && cd /app/build && \
    conan install .. --build=missing -s build_type=Release

# Копируем исходный код
COPY ./src /app/src
COPY ./tests /app/tests
COPY ./CMakeLists.txt /app/
COPY ./data /app/data
COPY ./static /app/static

# Сборка проекта
RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . -j$(nproc)

# Финальный образ
FROM ubuntu:22.04 AS run

# Устанавливаем runtime зависимости
RUN apt update && apt install -y \
    libstdc++6 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Создаем непривилегированного пользователя (опционально, закомментировано для тестов)
# RUN groupadd -r www && useradd -r -g www www
# USER www

# Копируем бинарник и данные
COPY --from=build /app/build/bin/game_server /app/game_server
COPY ./data /app/data
COPY ./static /app/static

# Рабочая директория
WORKDIR /app

# Проверяем наличие бинарника
RUN test -f /app/game_server && echo "✓ Binary found" || (echo "✗ Binary not found" && exit 1)

# Точка входа
ENTRYPOINT ["/app/game_server"]

# Параметры по умолчанию
# --tick-period 0 - ручное управление временем (нужно для тестов)
# Если тесты не проходят, раскомментируйте строку ниже и закомментируйте CMD
CMD ["--config-file", "/app/data/config.json", "--www-root", "/app/static", "--tick-period", "0"]

# Альтернативный вариант БЕЗ tick-period (если тесты падают)
# CMD ["--config-file", "/app/data/config.json", "--www-root", "/app/static"]