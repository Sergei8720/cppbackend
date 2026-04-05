FROM gcc:11.3 AS build

RUN apt update && \
    apt install -y python3-pip cmake && \
    pip3 install "conan<2"

RUN conan profile new default --detect --force && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

COPY conanfile.txt /app/
RUN mkdir -p /app/build && cd /app/build && \
    conan install .. --build=missing -s build_type=Release

COPY ./src /app/src
COPY ./tests /app/tests
COPY ./CMakeLists.txt /app/
COPY ./data /app/data
COPY ./static /app/static

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . -j$(nproc)

FROM ubuntu:22.04 AS run

RUN apt update && apt install -y libstdc++6 ca-certificates && rm -rf /var/lib/apt/lists/*

COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data
COPY ./static /app/static

WORKDIR /app

ENTRYPOINT ["/app/game_server"]
# CMD ["--config-file", "/app/data/config.json", "--www-root", "/app/static", "--tick-period", "0"]

# Альтернативный вариант БЕЗ tick-period (если тесты падают)
CMD ["--config-file", "/app/data/config.json", "--www-root", "/app/static"]