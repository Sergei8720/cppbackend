import argparse
import subprocess
import time
import random
import shlex
import os
import signal
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
    parser.add_argument('server', type=str, nargs=argparse.REMAINDER)
    return parser.parse_args().server


def run(command, output=None, shell=False):
    if shell:
        process = subprocess.Popen(command, stdout=output, stderr=subprocess.DEVNULL, shell=True)
    else:
        process = subprocess.Popen(shlex.split(command), stdout=output, stderr=subprocess.DEVNULL)
    return process


def stop(process, wait=False):
    if process.poll() is None and wait:
        process.wait()
    process.terminate()


def shoot(ammo):
    hit = run('curl -s ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')


def main():
    # Получаем команду запуска сервера
    server_cmd_parts = start_server()
    if not server_cmd_parts:
        print("Error: Server command not provided")
        sys.exit(1)
    
    server_cmd = ' '.join(server_cmd_parts)
    print(f"Starting server: {server_cmd}")
    
    # Запускаем сервер
    server = run(server_cmd, shell=True)
    
    # Даем серверу время на запуск
    time.sleep(2)
    
    # Проверяем, что сервер запущен
    if server.poll() is not None:
        print("Error: Server failed to start")
        sys.exit(1)
    
    # Запускаем perf record для профилирования сервера
    perf_record = subprocess.Popen(
        ['perf', 'record', '-o', 'perf.data', '-p', str(server.pid), '-- sleep 10'],
        stderr=subprocess.DEVNULL
    )
    
    # Даем perf record время на подключение к процессу
    time.sleep(1)
    
    # Выполняем обстрел сервера запросами
    make_shots()
    
    # Ждем завершения perf record
    perf_record.wait()
    
    # Останавливаем сервер
    stop(server)
    time.sleep(1)
    
    # Проверяем наличие FlameGraph скриптов
    flamegraph_dir = os.path.join(os.path.dirname(__file__), 'FlameGraph')
    if not os.path.exists(flamegraph_dir):
        # Пробуем другие пути
        possible_paths = [
            '../FlameGraph',
            './FlameGraph',
            '/home/runner/work/cppbackend/cppbackend/FlameGraph'
        ]
        for path in possible_paths:
            if os.path.exists(path):
                flamegraph_dir = path
                break
    
    # Генерируем флеймграф
    if os.path.exists('perf.data') and os.path.getsize('perf.data') > 0:
        print(f"Generating flamegraph using {flamegraph_dir}")
        
        stackcollapse = os.path.join(flamegraph_dir, 'stackcollapse-perf.pl')
        flamegraph_pl = os.path.join(flamegraph_dir, 'flamegraph.pl')
        
        if os.path.exists(stackcollapse) and os.path.exists(flamegraph_pl):
            # Создаем пайплайн для генерации флеймграфа
            with open('graph.svg', 'w') as svg_file:
                perf_script = subprocess.Popen(
                    ['perf', 'script', '-i', 'perf.data'],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.DEVNULL
                )
                
                stackcollapse_proc = subprocess.Popen(
                    ['perl', stackcollapse],
                    stdin=perf_script.stdout,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.DEVNULL
                )
                
                flamegraph_proc = subprocess.Popen(
                    ['perl', flamegraph_pl],
                    stdin=stackcollapse_proc.stdout,
                    stdout=svg_file,
                    stderr=subprocess.DEVNULL
                )
                
                # Закрываем потоки
                perf_script.stdout.close()
                stackcollapse_proc.stdout.close()
                flamegraph_proc.communicate()
            
            print('FlameGraph generated: graph.svg')
            
            # Проверяем, содержит ли флеймграф RequestHandler
            with open('graph.svg', 'r') as f:
                content = f.read()
                if 'RequestHandler' in content:
                    print("✓ RequestHandler found in flamegraph")
                else:
                    print("✗ RequestHandler NOT found in flamegraph")
        else:
            print(f"Error: FlameGraph scripts not found in {flamegraph_dir}")
            print(f"stackcollapse-perf.pl exists: {os.path.exists(stackcollapse)}")
            print(f"flamegraph.pl exists: {os.path.exists(flamegraph_pl)}")
    else:
        print('Error: perf.data is empty or does not exist')
    
    print('Job done')


if __name__ == "__main__":
    main()