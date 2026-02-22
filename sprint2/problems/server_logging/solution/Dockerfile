FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan

# Добавляем шаг для создания профиля Conan
RUN conan profile detect

COPY conanfile.txt /app/

RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing

COPY ./src /app/src
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

FROM ubuntu:22.04 as runtime

RUN apt update && \
    apt install -y \
      libstdc++6 \
    && \
    rm -rf /var/lib/apt/lists/*

COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data
COPY ./static /app/static

EXPOSE 8080

ENTRYPOINT ["/app/game_server", "/app/data/config.json", "/app/static"]