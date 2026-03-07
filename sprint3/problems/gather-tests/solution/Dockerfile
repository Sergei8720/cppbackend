FROM gcc:11.3 AS build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

RUN conan profile new default --detect --force
RUN conan profile update settings.compiler.libcxx=libstdc++11 default
RUN conan profile show default

# Копируем все исходники
COPY ./src /app/src
COPY ./tests /app/tests
COPY ./CMakeLists.txt /app/
COPY ./conanfile.txt /app/

# Проверяем структуру
RUN echo "=== SRC directory ===" && ls -la /app/src/ && \
    echo "=== TESTS directory ===" && ls -la /app/tests/

# Устанавливаем зависимости Conan
RUN mkdir -p /app/build && cd /app/build && \
    conan install .. --build=missing

# Компилируем тесты
RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_PREFIX_PATH=/root/.conan \
          .. && \
    cmake --build . --target collision_detection_tests

# Проверяем, где создался исполняемый файл (для отладки)
RUN echo "=== Build output ===" && ls -la /app/build/bin/ || echo "bin dir not found" && \
    echo "=== Build root ===" && ls -la /app/build/

FROM ubuntu:22.04 AS run

RUN apt update && apt install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd -r www && useradd -r -g www www

# Копируем тесты со сборочного контейнера - ИСПРАВЛЕНО!
COPY --from=build /app/build/bin/collision_detection_tests /app/collision_detection_tests

USER www

# Проверяем, что файл скопировался
RUN test -f /app/collision_detection_tests || echo "File not copied!"

# Запускаем тесты - ИСПРАВЛЕНО!
ENTRYPOINT ["/app/collision_detection_tests"]