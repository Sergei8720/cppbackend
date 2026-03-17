# Используем многоступенчатую сборку
FROM gcc:11.3 as build

# Устанавливаем необходимые пакеты
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.63.0

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

# Устанавливаем необходимые библиотеки для запуска и Python для тестов
RUN apt update && apt install -y \
    libstdc++6 \
    python3 \
    python3-pip \
    python3-venv \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Устанавливаем Python-зависимости для тестов
RUN pip3 install pytest==7.1.2 pytest-xprocess==0.20.0 pytest-quickcheck==0.9.0 docker==6.1.3

# Копируем собранный сервер и тесты
COPY --from=build /app/build/bin/game_server /app/game_server
COPY --from=build /app/build/bin/collision_tests /app/collision_tests
COPY --from=build /app/build/bin/game_tests /app/game_tests

# Копируем конфигурационные файлы
COPY ./data /app/data

# Копируем тестовые файлы Python - ИСПРАВЛЕНО для GitHub Actions
# Поднимаемся на 4 уровня вверх, чтобы попасть в корень репозитория
COPY ../../../../tests/test_s04_state_serialization.py /app/tests/

WORKDIR /app

# Создаем директорию для тестов, если её нет
RUN mkdir -p /app/tests

# Скрипт для запуска сервера и тестов
RUN echo '#!/bin/bash\n\
set -e\n\
echo "Starting game server..."\n\
./game_server --config-file /app/data/config.json --www-root /app/static/ --state-file /tmp/volume/state &\n\
SERVER_PID=$!\n\
\n\
# Функция для остановки сервера\n\
cleanup() {\n\
    echo "Stopping server..."\n\
    kill $SERVER_PID 2>/dev/null || true\n\
    wait $SERVER_PID 2>/dev/null || true\n\
}\n\
trap cleanup EXIT\n\
\n\
# Ждем запуска сервера\n\
sleep 2\n\
if ! kill -0 $SERVER_PID 2>/dev/null; then\n\
    echo "ERROR: Server failed to start"\n\
    exit 1\n\
fi\n\
echo "Server started with PID $SERVER_PID"\n\
\n\
# Запускаем тесты\n\
echo "Running tests..."\n\
python3 -m pytest /app/tests/test_s04_state_serialization.py -v --junitxml=/app/test_results.xml\n\
TEST_RESULT=$?\n\
\n\
echo "Tests completed with exit code $TEST_RESULT"\n\
exit $TEST_RESULT\n\
' > /app/run_tests.sh && chmod +x /app/run_tests.sh

CMD ["/app/run_tests.sh"]