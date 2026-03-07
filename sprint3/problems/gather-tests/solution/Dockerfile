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

RUN mkdir -p /app/build && cd /app/build && \
    conan install .. --build=missing

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_PREFIX_PATH=/root/.conan \
          .. && \
    cmake --build . --target collision_detection_tests

FROM ubuntu:22.04 AS run

# Устанавливаем минимально необходимые пакеты
RUN apt update && apt install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd -r www && useradd -r -g www www

RUN mkdir -p /app/build
COPY --from=build /app/build/bin/collision_detection_tests /app/build/

USER www

# Проверяем права на выполнение
RUN chmod +x /app/build/collision_detection_tests && \
    ls -la /app/build/

ENTRYPOINT ["/app/build/collision_detection_tests"]