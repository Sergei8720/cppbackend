FROM gcc:11.3 AS build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

RUN conan profile new default --detect --force
RUN conan profile update settings.compiler.libcxx=libstdc++11 default

COPY ./src /app/src
COPY ./tests /app/tests
COPY ./CMakeLists.txt /app/
COPY ./conanfile.txt /app/

# Создаем директорию для сборки
RUN mkdir -p /app/build

# Устанавливаем зависимости через Conan
WORKDIR /app/build
RUN conan install .. --build=missing -s build_type=Release

# Собираем проект
RUN cmake .. -DCMAKE_BUILD_TYPE=Release \
    && cmake --build . --target collision_detection_tests -j$(nproc)

FROM ubuntu:22.04 AS run

# Устанавливаем минимально необходимые пакеты
RUN apt update && apt install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd -r www && useradd -r -g www www

# Создаем директорию /app
RUN mkdir -p /app

# Копируем исполняемый файл в /app/ (простой путь)
COPY --from=build /app/build/bin/collision_detection_tests /app/collision_detection_tests

# Устанавливаем права
RUN chmod +x /app/collision_detection_tests && \
    ls -la /app/

# Теперь переключаемся на непривилегированного пользователя
USER www

WORKDIR /app
ENTRYPOINT ["/app/collision_detection_tests"]