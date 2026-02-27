import argparse
import subprocess
import time
import random
import shlex
import signal
import os

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 100
COOLDOWN = 0.1

PERF_DATA = "perf.data"


def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str, help='Command to start server (path/to/server path/to/config)')
    args = parser.parse_args()
    return args.server


def run(command, output=None):
    """Run command and return process"""
    process = subprocess.Popen(
        shlex.split(command), 
        stdout=output, 
        stderr=subprocess.DEVNULL
    )
    return process


def stop(process, wait=False):
    """Stop process gracefully"""
    if process.poll() is None and wait:
        process.wait()
    process.terminate()


def shoot(ammo):
    """Make single request to server"""
    # Добавляем -s для тихого режима и -o /dev/null для подавления вывода
    hit = run(f'curl -s -o /dev/null {ammo}', output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    """Make multiple random requests to server"""
    print(f"Making {SHOOT_COUNT} requests...")
    for i in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
        if (i + 1) % 10 == 0:
            print(f"  {i + 1}/{SHOOT_COUNT} requests completed")
    print('Shooting complete')


def main():
    # Получаем команду для запуска сервера
    server_cmd = start_server()
    print(f"Starting server: {server_cmd}")
    
    # Запускаем сервер
    server = run(server_cmd, subprocess.DEVNULL)
    time.sleep(1)  # Даем серверу время на инициализацию
    
    # Проверяем, что сервер запустился
    if server.poll() is not None:
        print("Error: Server failed to start")
        sys.exit(1)
    
    print(f"Server started with PID: {server.pid}")
    
    # Запускаем perf record
    print(f"Starting perf record...")
    perf = run(f'perf record -o {PERF_DATA} -p {server.pid} -g')
    time.sleep(0.5)  # Даем perf время на инициализацию
    
    # Обстреливаем сервер запросами
    make_shots()
    
    # Останавливаем perf запись (SIGINT = Ctrl+C)
    print("Stopping perf record...")
    perf.send_signal(signal.SIGINT)
    perf.wait()
    time.sleep(0.5)
    
    # Останавливаем сервер
    print("Stopping server...")
    stop(server)
    time.sleep(0.5)
    
    # Проверяем, что файл perf.data создан
    if not os.path.exists(PERF_DATA):
        print(f"Error: {PERF_DATA} not created")
        sys.exit(1)
    
    print(f"Generating flamegraph...")
    
    # Строим флеймграф через двойной пайп
    with open("graph.svg", "w") as graph_file:
        # Первый процесс: perf script читает данные
        perf_script = subprocess.Popen(
            shlex.split(f"perf script -i {PERF_DATA}"),
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL
        )
        
        # Второй процесс: stackcollapse-perf.pl сворачивает стек
        stackcollapse = subprocess.Popen(
            shlex.split("./FlameGraph/stackcollapse-perf.pl"),
            stdin=perf_script.stdout,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL
        )
        
        # Третий процесс: flamegraph.pl генерирует SVG
        flamegraph = subprocess.Popen(
            shlex.split("./FlameGraph/flamegraph.pl"),
            stdin=stackcollapse.stdout,
            stdout=graph_file,
            stderr=subprocess.DEVNULL
        )
        
        # Закрываем пайпы, чтобы избежать дедлоков
        perf_script.stdout.close()
        stackcollapse.stdout.close()
        
        # Ждем завершения всех процессов
        flamegraph.wait()
        stackcollapse.wait()
        perf_script.wait()
    
    # Проверяем результат
    if os.path.exists("graph.svg") and os.path.getsize("graph.svg") > 0:
        print("Success! Flamegraph saved to graph.svg")
    else:
        print("Error: graph.svg was not created or is empty")
        sys.exit(1)
    
    print('Job done')


if __name__ == "__main__":
    import sys
    main()