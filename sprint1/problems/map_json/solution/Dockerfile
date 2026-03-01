FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

# Убираем принудительную настройку профиля
# Пусть Conan сам создает профиль по умолчанию
RUN conan profile new default --detect --force || true
# Не обновляем compiler.libcxx

COPY conanfile.txt /app/
RUN mkdir -p /app/build && cd /app/build && \
    conan install .. --build=missing

COPY ./src /app/src
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .