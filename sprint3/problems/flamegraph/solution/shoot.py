#!/usr/bin/env python3
"""
Скрипт для профилирования сервера и построения флеймграфа.
Запускает сервер, собирает perf данные и генерирует graph.svg
"""

import argparse
import subprocess
import time
import random
import shlex
import signal
import os
import sys
from pathlib import Path

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
FLAMEGRAPH_DIR = "./FlameGraph"


def find_server_binary():
    """Автоматически ищет собранный бинарник сервера"""
    possible_paths = [
        # Пути для локального запуска
        "./sprint1/problems/map_json/solution/build/bin/game_server",
        "../sprint1/problems/map_json/solution/build/bin/game_server",
        "../../sprint1/problems/map_json/solution/build/bin/game_server",
        
        # Путь для GitHub Actions
        "/__w/cppbackend/cppbackend/sprint1/problems/map_json/solution/build/bin/game_server",
        
        # Путь относительно текущей директории
        "sprint1/problems/map_json/solution/build/bin/game_server",
        
        # Если скрипт в корне проекта
        "build/bin/game_server",
        "bin/game_server",
        
        # Путь, переданный через аргументы (будет обработан позже)
    ]
    
    for path in possible_paths:
        if os.path.exists(path):
            return os.path.abspath(path)
    
    # Если ничего не нашли, пробуем найти через find
    try:
        result = subprocess.run(
            'find . -name "game_server" -type f -executable | head -1',
            shell=True, capture_output=True, text=True
        )
        if result.returncode == 0 and result.stdout.strip():
            found_path = result.stdout.strip()
            print(f"Found server via find: {found_path}")
            return os.path.abspath(found_path)
    except Exception as e:
        print(f"Find command failed: {e}")
    
    return None


def find_config_path(server_binary):
    """Ищет файл конфигурации рядом с сервером"""
    possible_configs = [
        # Относительно бинарника
        os.path.join(os.path.dirname(server_binary), "../../data/config.json"),
        os.path.join(os.path.dirname(server_binary), "../data/config.json"),
        os.path.join(os.path.dirname(server_binary), "data/config.json"),
        
        # Абсолютные пути для GitHub Actions
        "/__w/cppbackend/cppbackend/sprint1/problems/map_json/solution/data/config.json",
        
        # В текущей директории
        "data/config.json",
        "./config.json",
        "../data/config.json",
    ]
    
    for config in possible_configs:
        if os.path.exists(config):
            return os.path.abspath(config)
    
    return None


def check_flamegraph_scripts():
    """Проверяет наличие скриптов FlameGraph"""
    stackcollapse = os.path.join(FLAMEGRAPH_DIR, "stackcollapse-perf.pl")
    flamegraph_pl = os.path.join(FLAMEGRAPH_DIR, "flamegraph.pl")
    
    if not os.path.exists(stackcollapse):
        print(f"Error: {stackcollapse} not found!")
        print("\nPlease clone FlameGraph repository:")
        print("  git clone https://github.com/brendangregg/FlameGraph.git")
        print(f"  mv FlameGraph {FLAMEGRAPH_DIR}")
        return False
    
    if not os.path.exists(flamegraph_pl):
        print(f"Error: {flamegraph_pl} not found!")
        return False
    
    # Проверяем, что скрипты исполняемые
    if not os.access(stackcollapse, os.X_OK):
        print(f"Warning: {stackcollapse} is not executable, fixing...")
        os.chmod(stackcollapse, 0o755)
    
    if not os.access(flamegraph_pl, os.X_OK):
        print(f"Warning: {flamegraph_pl} is not executable, fixing...")
        os.chmod(flamegraph_pl, 0o755)
    
    return True


def start_server():
    """Парсит аргументы командной строки"""
    parser = argparse.ArgumentParser(
        description='Profile server and generate flamegraph',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s "./game_server config.json"
  %(prog)s --auto
  %(prog)s --server-path ./build/bin/game_server --config-path ./data/config.json
        """
    )
    
    parser.add_argument('server', type=str, nargs='?', 
                       help='Command to start server (path/to/server path/to/config)')
    
    parser.add_argument('--auto', '-a', action='store_true',
                       help='Automatically find server binary and config')
    
    parser.add_argument('--server-path', '-s', type=str,
                       help='Path to server binary (for use with --config-path)')
    
    parser.add_argument('--config-path', '-c', type=str,
                       help='Path to config file (for use with --server-path)')
    
    parser.add_argument('--shots', '-n', type=int, default=SHOOT_COUNT,
                       help=f'Number of requests to make (default: {SHOOT_COUNT})')
    
    parser.add_argument('--cooldown', '-d', type=float, default=COOLDOWN,
                       help=f'Cooldown between requests in seconds (default: {COOLDOWN})')
    
    parser.add_argument('--flamegraph-dir', '-f', type=str, default=FLAMEGRAPH_DIR,
                       help=f'Path to FlameGraph directory (default: {FLAMEGRAPH_DIR})')
    
    parser.add_argument('--output', '-o', type=str, default='graph.svg',
                       help='Output flamegraph filename (default: graph.svg)')
    
    args = parser.parse_args()
    
    # Обновляем глобальные переменные
    global SHOOT_COUNT, COOLDOWN, FLAMEGRAPH_DIR
    SHOOT_COUNT = args.shots
    COOLDOWN = args.cooldown
    FLAMEGRAPH_DIR = args.flamegraph_dir
    
    # Формируем команду для запуска сервера
    if args.auto:
        # Автоматический поиск
        server_binary = find_server_binary()
        if not server_binary:
            print("Error: Could not find server binary automatically")
            sys.exit(1)
        
        config_path = find_config_path(server_binary)
        if not config_path:
            print(f"Error: Could not find config file for server at {server_binary}")
            sys.exit(1)
        
        server_cmd = f"{server_binary} {config_path}"
        print(f"Auto-detected server: {server_cmd}")
        
    elif args.server_path and args.config_path:
        # Явно указаны пути
        if not os.path.exists(args.server_path):
            print(f"Error: Server binary not found at {args.server_path}")
            sys.exit(1)
        if not os.path.exists(args.config_path):
            print(f"Error: Config file not found at {args.config_path}")
            sys.exit(1)
        
        server_cmd = f"{args.server_path} {args.config_path}"
        
    elif args.server:
        # Команда передана целиком
        server_cmd = args.server
        
    else:
        parser.print_help()
        sys.exit(1)
    
    return server_cmd


def run(command, output=None):
    """Запускает команду и возвращает процесс"""
    try:
        process = subprocess.Popen(
            shlex.split(command), 
            stdout=output, 
            stderr=subprocess.DEVNULL
        )
        return process
    except Exception as e:
        print(f"Error running command '{command}': {e}")
        sys.exit(1)


def stop(process, wait=False):
    """Останавливает процесс"""
    if process is None:
        return
    
    if process.poll() is None:
        if wait:
            process.wait()
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()


def shoot(ammo):
    """Делает один запрос к серверу"""
    # curl -s для тихого режима, -o /dev/null для подавления вывода
    hit = run(f'curl -s -o /dev/null {ammo}', output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    """Делает множество случайных запросов к серверу"""
    print(f"\nMaking {SHOOT_COUNT} requests...")
    start_time = time.time()
    
    for i in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
        
        # Прогресс каждые 10 запросов
        if (i + 1) % 10 == 0:
            elapsed = time.time() - start_time
            print(f"  {i + 1}/{SHOOT_COUNT} requests completed ({elapsed:.1f}s elapsed)")
    
    total_time = time.time() - start_time
    print(f'Shooting complete in {total_time:.1f} seconds')
    print(f'Average request rate: {SHOOT_COUNT/total_time:.1f} req/s')


def check_perf_available():
    """Проверяет доступность perf"""
    try:
        result = subprocess.run(['perf', '--version'], 
                              capture_output=True, text=True)
        if result.returncode != 0:
            print("Error: perf is not available")
            print("Install it with: sudo apt-get install linux-tools-common linux-tools-generic")
            return False
        return True
    except FileNotFoundError:
        print("Error: perf command not found")
        return False


def generate_flamegraph(perf_data, output_file):
    """Генерирует флеймграф из perf.data"""
    print(f"\nGenerating flamegraph to {output_file}...")
    
    try:
        with open(output_file, "w") as graph_file:
            # Первый процесс: perf script читает данные
            perf_script = subprocess.Popen(
                shlex.split(f"perf script -i {perf_data}"),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            
            # Второй процесс: stackcollapse-perf.pl сворачивает стек
            stackcollapse = subprocess.Popen(
                shlex.split(f"{FLAMEGRAPH_DIR}/stackcollapse-perf.pl"),
                stdin=perf_script.stdout,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            
            # Третий процесс: flamegraph.pl генерирует SVG
            flamegraph = subprocess.Popen(
                shlex.split(f"{FLAMEGRAPH_DIR}/flamegraph.pl"),
                stdin=stackcollapse.stdout,
                stdout=graph_file,
                stderr=subprocess.PIPE
            )
            
            # Закрываем пайпы, чтобы избежать дедлоков
            if perf_script.stdout:
                perf_script.stdout.close()
            if stackcollapse.stdout:
                stackcollapse.stdout.close()
            
            # Ждем завершения и проверяем ошибки
            flamegraph.wait()
            stackcollapse.wait()
            perf_script.wait()
            
            # Проверяем на ошибки
            if flamegraph.returncode != 0:
                stderr = flamegraph.stderr.read().decode() if flamegraph.stderr else ""
                print(f"Flamegraph error: {stderr}")
                return False
            
            if stackcollapse.returncode != 0:
                stderr = stackcollapse.stderr.read().decode() if stackcollapse.stderr else ""
                print(f"Stackcollapse error: {stderr}")
                return False
            
            if perf_script.returncode != 0:
                stderr = perf_script.stderr.read().decode() if perf_script.stderr else ""
                print(f"Perf script error: {stderr}")
                return False
            
    except Exception as e:
        print(f"Error generating flamegraph: {e}")
        return False
    
    return True


def main():
    """Основная функция"""
    print("=" * 60)
    print("Server Profiling Script")
    print("=" * 60)
    
    # Получаем команду для запуска сервера
    server_cmd = start_server()
    
    # Проверяем наличие perf
    if not check_perf_available():
        sys.exit(1)
    
    # Проверяем наличие скриптов FlameGraph
    if not check_flamegraph_scripts():
        sys.exit(1)
    
    print(f"\nStarting server: {server_cmd}")
    
    # Запускаем сервер
    server = run(server_cmd, subprocess.DEVNULL)
    time.sleep(1)  # Даем серверу время на инициализацию
    
    # Проверяем, что сервер запустился
    if server.poll() is not None:
        print("Error: Server failed to start")
        sys.exit(1)
    
    print(f"Server started with PID: {server.pid}")
    
    # Удаляем старый файл perf.data если есть
    if os.path.exists(PERF_DATA):
        os.remove(PERF_DATA)
    
    # Запускаем perf record
    print(f"\nStarting perf record (saving to {PERF_DATA})...")
    perf = run(f'perf record -o {PERF_DATA} -p {server.pid} -g')
    time.sleep(0.5)  # Даем perf время на инициализацию
    
    # Обстреливаем сервер запросами
    make_shots()
    
    # Останавливаем perf запись (SIGINT = Ctrl+C)
    print("\nStopping perf record...")
    perf.send_signal(signal.SIGINT)
    perf.wait()
    time.sleep(0.5)
    
    # Останавливаем сервер
    print("Stopping server...")
    stop(server)
    time.sleep(0.5)
    
    # Проверяем, что файл perf.data создан и не пустой
    if not os.path.exists(PERF_DATA):
        print(f"Error: {PERF_DATA} was not created")
        sys.exit(1)
    
    file_size = os.path.getsize(PERF_DATA)
    if file_size == 0:
        print(f"Error: {PERF_DATA} is empty")
        sys.exit(1)
    
    print(f"{PERF_DATA} created, size: {file_size} bytes")
    
    # Строим флеймграф
    output_file = "graph.svg"
    if generate_flamegraph(PERF_DATA, output_file):
        # Проверяем результат
        if os.path.exists(output_file) and os.path.getsize(output_file) > 0:
            svg_size = os.path.getsize(output_file)
            print(f"\n✅ Success! Flamegraph saved to {output_file} ({svg_size} bytes)")
            print(f"\nTo view the flamegraph, open {output_file} in a browser")
        else:
            print(f"\n❌ Error: {output_file} was not created or is empty")
            sys.exit(1)
    else:
        print("\n❌ Failed to generate flamegraph")
        sys.exit(1)
    
    print("\n" + "=" * 60)
    print("Job done")
    print("=" * 60)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\nUnexpected error: {e}")
        sys.exit(1)