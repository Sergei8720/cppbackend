# Используем многоступенчатую сборку
FROM gcc:11.3 as build

# Устанавливаем необходимые пакеты
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

# Настраиваем Conan
RUN conan profile new default --detect --force
RUN conan profile update settings.compiler.libcxx=libstdc++11 default

# Копируем исходный код
COPY ./src /app/src
COPY ./tests /app/tests
COPY ./CMakeLists.txt /app/
COPY ./conanfile.txt /app/

# Создаем директорию для сборки
RUN mkdir -p /app/build
WORKDIR /app/build

# Устанавливаем зависимости через Conan
RUN conan install .. --build=missing -s build_type=Release

# Собираем все цели проекта
RUN cmake .. -DCMAKE_BUILD_TYPE=Release \
    && cmake --build . -j$(nproc)

# Финальный образ для запуска сервера и тестов
FROM ubuntu:22.04 as run

# Устанавливаем необходимые библиотеки для запуска
RUN apt update && apt install -y \
    libstdc++6 \
    python3 \
    python3-pip \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Копируем собранный сервер и тесты
COPY --from=build /app/build/bin/game_server /app/game_server
COPY --from=build /app/build/bin/collision_tests /app/collision_tests
# УДАЛЯЕМ game_tests так как его больше нет
# COPY --from=build /app/build/bin/game_tests /app/game_tests
COPY --from=build /app/tests /app/tests

# Копируем конфигурационные файлы
COPY ./data /app/data

WORKDIR /app

# Скрипт для запуска сервера и тестов
RUN echo '#!/bin/bash\n\
# Запускаем сервер в фоне\n\
./game_server --config-file /app/data/config.json --www-root /app/static/ &\n\
SERVER_PID=$!\n\
\n\
# Ждем запуска сервера\n\
sleep 2\n\
\n\
# Запускаем Python тесты\n\
python3 -m pytest /app/tests -v --junitxml=/app/test_results.xml\n\
TEST_RESULT=$?\n\
\n\
# Останавливаем сервер\n\
kill $SERVER_PID\n\
\n\
# Возвращаем результат тестов\n\
exit $TEST_RESULT\n\
' > /app/run_tests.sh && chmod +x /app/run_tests.sh

# Устанавливаем pytest
RUN pip3 install pytest

# Запускаем тесты с сервером
CMD ["/app/run_tests.sh"]