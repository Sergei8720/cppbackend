import argparse
import subprocess
import time
import random
import shlex
import signal
import os
import sys

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 100
COOLDOWN = 0.1


def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str)
    return parser.parse_args().server


def perf_record_of(pid):
    return f"perf record -p {pid} -o perf.data -g"


def run(command, stdout=None, stderr=None):
    """Запуск процесса с поддержкой shell команд"""
    # Используем shell=True для команд с пробелами и аргументами
    process = subprocess.Popen(command, shell=True, stdout=stdout, stderr=stderr, preexec_fn=os.setsid)
    return process


def stop(process, wait=False):
    """Остановка процесса и всех его дочерних процессов"""
    if process.poll() is None:
        if wait:
            try:
                # Отправляем SIGINT всей группе процессов
                os.killpg(os.getpgid(process.pid), signal.SIGINT)
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                # Если процесс не завершился, убиваем принудительно
                os.killpg(os.getpgid(process.pid), signal.SIGKILL)
                process.wait()
        else:
            os.killpg(os.getpgid(process.pid), signal.SIGINT)


def shoot(ammo):
    """Выполнение одного запроса к серверу"""
    full_url = f"http://{ammo}"
    # Используем shell=True для curl
    process = run(f'curl -s {full_url} > /dev/null', stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(process, wait=True)


def make_shots():
    """Выполнение серии запросов к серверу"""
    print(f"Making {SHOOT_COUNT} requests...")
    for i in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
        
        # Показываем прогресс каждые 10 запросов
        if (i + 1) % 10 == 0:
            print(f"Completed {i + 1}/{SHOOT_COUNT} requests")
    
    print('Shooting complete')


def main():
    # Получаем команду для запуска сервера
    server_cmd = start_server()
    
    # Создаем каталог для FlameGraph, если он не существует
    flamegraph_dir = "./FlameGraph"
    if not os.path.exists(flamegraph_dir):
        print(f"Error: {flamegraph_dir} directory not found")
        print("Please ensure FlameGraph tools are installed in the current directory")
        sys.exit(1)
    
    # Запускаем сервер
    print(f"Starting server: {server_cmd}")
    server = run(server_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    # Даем серверу время на запуск
    time.sleep(1)
    
    # Проверяем, что сервер запустился
    if server.poll() is not None:
        print("Error: Server failed to start")
        sys.exit(1)
    
    # Запускаем perf record для сбора данных о производительности сервера
    print(f"Starting perf record for PID {server.pid}")
    perf_record = run(perf_record_of(server.pid), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    # Даем perf время на инициализацию
    time.sleep(0.5)
    
    # Выполняем обстрел сервера запросами
    make_shots()
    
    # Завершаем perf record (отправляем SIGINT для корректного завершения записи)
    print("Stopping perf record...")
    stop(perf_record, wait=True)
    
    # Даем время на завершение записи
    time.sleep(1)
    
    # Проверяем, что файл perf.data создан
    if not os.path.exists("perf.data"):
        print("Error: perf.data not created")
        stop(server, wait=True)
        sys.exit(1)
    
    print("Generating flamegraph...")
    
    # Создаем конвейер для генерации flamegraph
    # 1. perf script - читает perf.data и выводит в текстовом формате
    perf_script = run(
        "perf script -i perf.data", 
        stdout=subprocess.PIPE, 
        stderr=subprocess.DEVNULL
    )
    
    # 2. stackcollapse-perf.pl - объединяет стектрейсы
    stackcollapse = run(
        f"{flamegraph_dir}/stackcollapse-perf.pl", 
        stdin=perf_script.stdout, 
        stdout=subprocess.PIPE, 
        stderr=subprocess.DEVNULL
    )
    
    # Перенаправляем stdout первого процесса в stdin второго
    perf_script.stdout.close()
    
    # 3. flamegraph.pl - генерирует SVG
    with open("graph.svg", "w") as graph_file:
        flamegraph = run(
            f"{flamegraph_dir}/flamegraph.pl", 
            stdin=stackcollapse.stdout, 
            stdout=graph_file, 
            stderr=subprocess.DEVNULL
        )
        
        # Закрываем ненужные дескрипторы
        stackcollapse.stdout.close()
        
        # Ждем завершения всех процессов
        flamegraph.wait()
        stackcollapse.wait()
        perf_script.wait()
    
    # Проверяем, что flamegraph создан
    if os.path.exists("graph.svg") and os.path.getsize("graph.svg") > 0:
        print("Flamegraph generated successfully: graph.svg")
        
        # Проверяем, содержит ли flamegraph вызовы RequestHandler
        with open("graph.svg", "r") as f:
            content = f.read()
            if "RequestHandler" in content:
                print("✓ Flamegraph contains RequestHandler calls")
            else:
                print("⚠ Warning: RequestHandler calls not found in flamegraph")
    else:
        print("Error: Failed to generate flamegraph")
    
    # Завершаем сервер
    print("Stopping server...")
    stop(server, wait=True)
    
    print('Job done')


if __name__ == "__main__":
    main()