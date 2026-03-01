# Берем базовый образ из спринта 1
FROM gcc:11.3 as build

# Устанавливаем зависимости как в спринте 1
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
      linux-tools-common \
      linux-tools-generic \
      linux-tools-$(uname -r) \
      curl \
    && \
    pip3 install conan==1.*

# Настраиваем Conan profile с правильным ABI (как в спринте 1)
RUN conan profile new default --detect --force && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

# Копируем исходники сервера из спринта 1
COPY ../sprint1/map_json/solution /app/solution
COPY ../sprint1/map_json/data /app/data
COPY ../sprint1/map_json/conanfile.txt /app/

# Собираем сервер
WORKDIR /app/build
RUN conan install .. --build=missing && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

# Копируем скрипты для флеймграфа
COPY FlameGraph /app/FlameGraph
COPY shoot.py /app/

# Финальный образ
FROM ubuntu:22.04

# Устанавливаем необходимые пакеты для perf и curl
RUN apt update && \
    apt install -y \
      linux-tools-common \
      linux-tools-generic \
      curl \
      perl \
    && \
    rm -rf /var/lib/apt/lists/*

# Копируем собранный сервер и скрипты
COPY --from=build /app/build/bin/game_server /app/
COPY --from=build /app/data /app/data
COPY --from=build /app/FlameGraph /app/FlameGraph
COPY --from=build /app/shoot.py /app/

WORKDIR /app

# Запускаем сервер и скрипт
CMD ./game_server /app/data/config.json & \
    sleep 2 && \
    python3 shoot.py