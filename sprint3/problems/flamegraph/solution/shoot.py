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
    """
    Парсит аргументы командной строки.
    Пример: python3 shoot.py "../sprint1/solution/build/bin/game_server ../sprint1/solution/data/config.json"
    
    nargs=argparse.REMAINDER - собирает все аргументы после названия скрипта в список
    """
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str, nargs=argparse.REMAINDER)
    args = parser.parse_args()
    
    # Объединяем все части команды в одну строку
    if args.server:
        return ' '.join(args.server)
    return None


def run(command, output=None, shell=False):
    """Запускает процесс"""
    if shell:
        process = subprocess.Popen(command, stdout=output, stderr=subprocess.DEVNULL, shell=True)
    else:
        process = subprocess.Popen(shlex.split(command), stdout=output, stderr=subprocess.DEVNULL)
    return process


def stop(process, wait=False):
    """Останавливает процесс"""
    if process.poll() is None and wait:
        process.wait()
    process.terminate()


def shoot(ammo):
    """Делает HTTP запрос"""
    hit = run('curl -s ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    """Обстреливает сервер запросами"""
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')


def find_flamegraph_dir():
    """
    Ищет папку FlameGraph в разных местах.
    Возвращает путь к папке или None, если не найдена.
    """
    # Путь к текущему скрипту
    script_dir = os.path.dirname(os.path.abspath(__file__))
    print(f"Script directory: {script_dir}")
    
    # Поднимаемся на уровень sprint3
    sprint3_dir = os.path.dirname(os.path.dirname(script_dir))  # .../sprint3/problems/flamegraph -> .../sprint3
    print(f"Sprint3 directory: {sprint3_dir}")
    
    # Корень проекта (где лежат sprint1, sprint3, FlameGraph)
    project_root = os.path.dirname(sprint3_dir)  # .../sprint3 -> .../cppbackend
    print(f"Project root: {project_root}")
    
    # Возможные пути к FlameGraph
    possible_paths = [
        # 1. В корне проекта (рядом с sprint1, sprint3)
        os.path.join(project_root, 'FlameGraph'),
        
        # 2. На уровень выше от скрипта (если скрипт в solution, то ../FlameGraph)
        os.path.join(os.path.dirname(script_dir), 'FlameGraph'),
        
        # 3. В той же директории, что и скрипт
        os.path.join(script_dir, 'FlameGraph'),
        
        # 4. Абсолютный путь для CI (GitHub Actions)
        '/home/runner/work/cppbackend/cppbackend/FlameGraph',
        
        # 5. Относительный путь (если запускают из корня)
        'FlameGraph'
    ]
    
    for path in possible_paths:
        if os.path.exists(path):
            print(f"Found FlameGraph at: {path}")
            return path
    
    print("FlameGraph directory not found")
    return None


def generate_flamegraph(flamegraph_dir):
    """
    Генерирует флеймграф из perf.data
    """
    if not os.path.exists('perf.data'):
        print("perf.data not found")
        return False
    
    if os.path.getsize('perf.data') == 0:
        print("perf.data is empty")
        return False
    
    stackcollapse = os.path.join(flamegraph_dir, 'stackcollapse-perf.pl')
    flamegraph_pl = os.path.join(flamegraph_dir, 'flamegraph.pl')
    
    if not os.path.exists(stackcollapse):
        print(f"stackcollapse-perf.pl not found at {stackcollapse}")
        return False
    
    if not os.path.exists(flamegraph_pl):
        print(f"flamegraph.pl not found at {flamegraph_pl}")
        return False
    
    print("Generating flamegraph...")
    
    # Создаем пайплайн: perf script | stackcollapse-perf.pl | flamegraph.pl > graph.svg
    try:
        # perf script - читает perf.data и выводит стектрейсы
        perf_script = subprocess.Popen(
            ['perf', 'script', '-i', 'perf.data'],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL
        )
        
        # stackcollapse-perf.pl - сворачивает стектрейсы
        stackcollapse_proc = subprocess.Popen(
            ['perl', stackcollapse],
            stdin=perf_script.stdout,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL
        )
        
        # flamegraph.pl - создает SVG и записываем в файл
        with open('graph.svg', 'w') as svg_file:
            flamegraph_proc = subprocess.Popen(
                ['perl', flamegraph_pl],
                stdin=stackcollapse_proc.stdout,
                stdout=svg_file,
                stderr=subprocess.DEVNULL
            )
            
            # Закрываем ненужные потоки
            if perf_script.stdout:
                perf_script.stdout.close()
            if stackcollapse_proc.stdout:
                stackcollapse_proc.stdout.close()
            
            # Ждем завершения
            flamegraph_proc.communicate()
        
        print("FlameGraph generated: graph.svg")
        
        # Проверяем, есть ли RequestHandler в SVG
        with open('graph.svg', 'r') as f:
            content = f.read()
            if 'RequestHandler' in content:
                print("✓ RequestHandler found in flamegraph")
            else:
                print("✗ RequestHandler NOT found in flamegraph")
        
        return True
        
    except Exception as e:
        print(f"Error generating flamegraph: {e}")
        return False


def main():
    # Получаем команду для запуска сервера
    server_cmd = start_server()
    if not server_cmd:
        print("Error: Server command not provided")
        print("Usage: python3 shoot.py 'path/to/server path/to/config'")
        sys.exit(1)
    
    print(f"Starting server: {server_cmd}")
    
    # Запускаем сервер
    server = run(server_cmd, shell=True)
    
    # Даем серверу время на запуск
    time.sleep(2)
    
    # Проверяем, что сервер запустился
    if server.poll() is not None:
        print("Error: Server failed to start")
        sys.exit(1)
    
    print(f"Server PID: {server.pid}")
    
    # Запускаем perf record для профилирования сервера
    # perf record -o perf.data -p <PID> -- sleep 10
    perf_record = subprocess.Popen(
        ['perf', 'record', '-o', 'perf.data', '-p', str(server.pid), '--', 'sleep', '10'],
        stderr=subprocess.DEVNULL
    )
    
    # Даем perf время подключиться
    time.sleep(1)
    
    # Обстреливаем сервер
    make_shots()
    
    # Ждем завершения perf record
    perf_record.wait()
    
    # Останавливаем сервер
    stop(server)
    time.sleep(1)
    
    # Ищем FlameGraph и генерируем флеймграф
    flamegraph_dir = find_flamegraph_dir()
    if flamegraph_dir:
        generate_flamegraph(flamegraph_dir)
    else:
        print("Error: Could not find FlameGraph directory")
    
    print('Job done')


if __name__ == "__main__":
    main()