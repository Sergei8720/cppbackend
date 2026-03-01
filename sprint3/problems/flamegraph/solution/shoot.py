import argparse
import subprocess
import time
import random
import shlex
import os
import signal
import sys
import tempfile

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 100
COOLDOWN = 0.1


def create_dockerfile():
    """Создаем Dockerfile на лету"""
    dockerfile_content = """
FROM gcc:11.3 as build

RUN apt update && \\
    apt install -y \\
      python3-pip \\
      cmake \\
      linux-tools-common \\
      linux-tools-generic \\
      curl \\
    && \\
    pip3 install conan==1.*

# Настраиваем Conan profile с правильным ABI
RUN conan profile new default --detect --force && \\
    conan profile update settings.compiler.libcxx=libstdc++11 default

# Копируем исходники сервера
COPY ../sprint1/map_json/solution /app/solution
COPY ../sprint1/map_json/data /app/data
COPY ../sprint1/map_json/conanfile.txt /app/

# Собираем сервер
WORKDIR /app/build
RUN conan install .. --build=missing && \\
    cmake -DCMAKE_BUILD_TYPE=Release .. && \\
    cmake --build .

# Финальный образ
FROM ubuntu:22.04

RUN apt update && \\
    apt install -y \\
      linux-tools-common \\
      linux-tools-generic \\
      curl \\
      perl \\
      procps \\
    && \\
    rm -rf /var/lib/apt/lists/*

COPY --from=build /app/build/bin/game_server /app/
COPY --from=build /app/data /app/data
COPY FlameGraph /app/FlameGraph

WORKDIR /app

CMD /app/game_server /app/data/config.json
"""
    
    with open('Dockerfile.server', 'w') as f:
        f.write(dockerfile_content)
    return 'Dockerfile.server'


def build_and_run_server():
    """Собирает Docker образ и запускает сервер"""
    print("Building Docker image with server...")
    
    # Создаем Dockerfile
    dockerfile = create_dockerfile()
    
    # Собираем образ
    subprocess.run([
        'docker', 'build',
        '-f', dockerfile,
        '-t', 'game-server-flamegraph',
        '.'  # Запускаем из текущей директории (sprint3)
    ], check=True)
    
    # Запускаем контейнер с сервером в фоне
    container = subprocess.run([
        'docker', 'run',
        '-d',
        '--name', 'game-server-flamegraph',
        '-p', '8080:8080',
        '--privileged',  # Нужно для perf
        'game-server-flamegraph'
    ], capture_output=True, text=True, check=True)
    
    container_id = container.stdout.strip()
    print(f"Container started: {container_id}")
    
    # Ждем запуска сервера
    time.sleep(3)
    
    # Проверяем, что сервер работает
    try:
        subprocess.run(['curl', '-f', 'http://localhost:8080/api/v1/maps'], 
                      capture_output=True, check=True)
        print("Server is healthy")
    except:
        print("Server failed to start")
        docker_logs = subprocess.run(['docker', 'logs', container_id], 
                                    capture_output=True, text=True)
        print(docker_logs.stderr)
        subprocess.run(['docker', 'rm', '-f', container_id])
        sys.exit(1)
    
    return container_id


def run_perf_in_container(container_id):
    """Запускает perf record внутри контейнера"""
    print("Starting perf record...")
    
    # Получаем PID сервера внутри контейнера
    pid_result = subprocess.run([
        'docker', 'exec', container_id, 'pgrep', 'game_server'
    ], capture_output=True, text=True, check=True)
    
    server_pid = pid_result.stdout.strip()
    print(f"Server PID in container: {server_pid}")
    
    # Запускаем perf record в контейнере
    perf = subprocess.Popen([
        'docker', 'exec', container_id,
        'perf', 'record',
        '-F', '99',
        '-g',
        '-p', server_pid,
        '-o', '/tmp/perf.data'
    ])
    
    return perf


def make_shots():
    """Выполняет запросы к серверу"""
    print(f"Making {SHOOT_COUNT} requests...")
    for i in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        url = AMMUNITION[ammo_number]
        
        try:
            subprocess.run(['curl', '-s', url], 
                         capture_output=True, timeout=1)
        except:
            pass
        
        time.sleep(COOLDOWN)
        
        if (i + 1) % 10 == 0:
            print(f"Completed {i + 1}/{SHOOT_COUNT} requests")
    
    print('Shooting complete')


def get_flamegraph_from_container(container_id):
    """Извлекает flamegraph из контейнера"""
    print("Generating flamegraph...")
    
    # Выполняем perf script и генерируем flamegraph внутри контейнера
    subprocess.run([
        'docker', 'exec', container_id,
        'bash', '-c',
        'cd /app && perf script -i /tmp/perf.data | ./FlameGraph/stackcollapse-perf.pl | ./FlameGraph/flamegraph.pl > /tmp/graph.svg'
    ], check=True)
    
    # Копируем результат из контейнера
    subprocess.run([
        'docker', 'cp',
        f'{container_id}:/tmp/graph.svg',
        './graph.svg'
    ], check=True)
    
    print("Flamegraph saved to graph.svg")


def cleanup(container_id):
    """Очищает ресурсы"""
    print("Cleaning up...")
    subprocess.run(['docker', 'stop', container_id], capture_output=True)
    subprocess.run(['docker', 'rm', container_id], capture_output=True)
    if os.path.exists('Dockerfile.server'):
        os.remove('Dockerfile.server')


# --- main logic ---
def main():
    container_id = None
    try:
        # Собираем и запускаем сервер
        container_id = build_and_run_server()
        
        # Запускаем perf
        perf_process = run_perf_in_container(container_id)
        
        # Даем perf время начать запись
        time.sleep(1)
        
        # Выполняем запросы
        make_shots()
        
        # Останавливаем perf
        perf_process.terminate()
        perf_process.wait()
        
        # Генерируем flamegraph
        get_flamegraph_from_container(container_id)
        
        print('Job done successfully!')
        
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
        
    finally:
        if container_id:
            cleanup(container_id)


if __name__ == '__main__':
    main()