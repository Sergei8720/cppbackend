#!/usr/bin/env python3
import argparse
import subprocess
import time
import random
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
COOLDOWN = 0.05


def start_server():
    """Парсинг аргументов командной строки"""
    parser = argparse.ArgumentParser(description='Load test server and generate flamegraph')
    parser.add_argument('server', type=str, help='Server command to run')
    return parser.parse_args().server


def perf_record_of(pid):
    """Генерация команды perf record"""
    return f"perf record -p {pid} -o perf.data -g -F 99"


def run(command, stdout=None, stderr=None):
    """Запуск процесса"""
    process = subprocess.Popen(
        command,
        shell=True,
        stdout=stdout,
        stderr=stderr
    )
    return process


def stop(process, wait=False, timeout=5):
    """Остановка процесса"""
    if process.poll() is not None:
        return True

    try:
        process.terminate()
        
        if wait:
            try:
                process.wait(timeout=timeout)
                return True
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                return False
    except Exception as e:
        print(f"Error stopping process: {e}")
        return False
    
    return True


def make_shots():
    """Выполнение серии запросов к серверу"""
    print(f"Making {SHOOT_COUNT} requests...")
    
    successful = 0
    failed = 0
    
    for i in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        ammo = AMMUNITION[ammo_number]
        
        try:
            # Отправляем запрос и проверяем результат
            result = subprocess.run(
                f'curl -s -o /dev/null -w "%{{http_code}}" "http://{ammo}"',
                shell=True,
                capture_output=True,
                text=True,
                timeout=1
            )
            
            if result.returncode == 0 and result.stdout.strip() == '200':
                successful += 1
            else:
                failed += 1
        except:
            failed += 1
        
        if (i + 1) % 10 == 0:
            print(f"Progress: {i + 1}/{SHOOT_COUNT} (OK: {successful}, Failed: {failed})")
        
        time.sleep(COOLDOWN)
    
    print(f'Shooting complete. Successful: {successful}, Failed: {failed}')
    return successful, failed


def find_flamegraph_tools():
    """Поиск утилит FlameGraph"""
    # Пути для поиска (относительно текущей директории)
    search_paths = [
        Path("FlameGraph"),                    # ./FlameGraph
        Path("../FlameGraph"),                  # ../FlameGraph
        Path("../../../FlameGraph"),            # на уровень выше
        Path("/home/runner/work/FlameGraph"),   # возможный путь в CI
    ]
    
    for path in search_paths:
        if path.exists():
            stackcollapse = path / "stackcollapse-perf.pl"
            flamegraph = path / "flamegraph.pl"
            
            if stackcollapse.exists() and flamegraph.exists():
                print(f"Found FlameGraph tools in {path}")
                return stackcollapse, flamegraph
    
    # Если не нашли, пробуем найти через git clone (как в build.sh)
    if not Path("FlameGraph").exists():
        print("Cloning FlameGraph repository...")
        subprocess.run(
            "git clone https://github.com/brendangregg/FlameGraph",
            shell=True,
            check=True
        )
        stackcollapse = Path("FlameGraph") / "stackcollapse-perf.pl"
        flamegraph = Path("FlameGraph") / "flamegraph.pl"
        if stackcollapse.exists() and flamegraph.exists():
            return stackcollapse, flamegraph
    
    return None, None


def generate_flamegraph():
    """Генерация flamegraph с проверкой наличия RequestHandler"""
    stackcollapse, flamegraph_pl = find_flamegraph_tools()
    
    if not stackcollapse or not flamegraph_pl:
        print("Error: FlameGraph tools not found")
        return False
    
    # Проверяем perf.data
    if not Path("perf.data").exists():
        print("Error: perf.data not found")
        return False
    
    if Path("perf.data").stat().st_size == 0:
        print("Error: perf.data is empty")
        return False
    
    print("Generating flamegraph...")
    print("Looking for http_handler::RequestHandler methods...")
    
    try:
        # Создаем временный файл с данными perf script
        with open("perf.script", "w") as script_file:
            perf_script = subprocess.run(
                ["perf", "script", "-i", "perf.data"],
                stdout=script_file,
                stderr=subprocess.PIPE,
                text=True
            )
            
            if perf_script.returncode != 0:
                print(f"perf script error: {perf_script.stderr}")
                return False
        
        # Проверяем наличие RequestHandler в сырых данных
        with open("perf.script", "r") as f:
            content = f.read()
            if "http_handler::RequestHandler" in content:
                print("  ✓ Found RequestHandler in perf data")
            else:
                print("  ⚠ Warning: RequestHandler not found in perf data")
        
        # Генерируем flamegraph
        with open("perf.script", "r") as script_file, open("graph.svg", "w") as svg_file:
            stackcollapse_proc = subprocess.Popen(
                [str(stackcollapse)],
                stdin=script_file,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            
            flamegraph_proc = subprocess.Popen(
                [str(flamegraph_pl)],
                stdin=stackcollapse_proc.stdout,
                stdout=svg_file,
                stderr=subprocess.PIPE,
                text=True
            )
            
            stackcollapse_proc.stdout.close()
            
            # Ждем завершения и проверяем ошибки
            flamegraph_stdout, flamegraph_stderr = flamegraph_proc.communicate()
            stackcollapse_stdout, stackcollapse_stderr = stackcollapse_proc.communicate()
            
            if flamegraph_proc.returncode != 0:
                print(f"flamegraph.pl error: {flamegraph_stderr}")
                return False
        
        # Финальная проверка
        if Path("graph.svg").exists() and Path("graph.svg").stat().st_size > 0:
            with open("graph.svg", "r") as f:
                svg_content = f.read()
                
                # Проверяем наличие методов RequestHandler (как требует тест)
                if "http_handler::RequestHandler" in svg_content:
                    print("✓ SUCCESS: http_handler::RequestHandler found in flamegraph")
                    
                    # Дополнительно ищем конкретные методы
                    methods = [
                        "operator()",
                        "IsMapRequest",
                        "SplitUrl",
                        "MakeMapList",
                        "MakeMapById"
                    ]
                    
                    found_methods = [m for m in methods if m in svg_content]
                    if found_methods:
                        print(f"  Found methods: {', '.join(found_methods)}")
                    
                    return True
                else:
                    print("✗ FAILURE: http_handler::RequestHandler NOT found in flamegraph")
                    print("  This is required by the test")
                    
                    # Показываем первые несколько строк для отладки
                    print("\nFirst 20 lines of graph.svg:")
                    print("\n".join(svg_content.split("\n")[:20]))
                    return False
        else:
            print("Error: graph.svg is empty or not created")
            return False
            
    except Exception as e:
        print(f"Error generating flamegraph: {e}")
        return False
    finally:
        # Очищаем временный файл
        if Path("perf.script").exists():
            Path("perf.script").unlink()


def wait_for_server():
    """Ожидание готовности сервера"""
    print("Waiting for server to start...")
    
    for attempt in range(10):
        try:
            # Проверяем, что сервер отвечает
            result = subprocess.run(
                'curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/api/v1/maps',
                shell=True,
                capture_output=True,
                text=True,
                timeout=1
            )
            
            if result.returncode == 0:
                if result.stdout.strip() == '200':
                    print(f"Server is ready (attempt {attempt + 1}/10)")
                    return True
                else:
                    print(f"  Server returned HTTP {result.stdout.strip()}")
            else:
                print(f"  Curl error: {result.stderr}")
        except subprocess.TimeoutExpired:
            print("  Connection timeout")
        except Exception as e:
            print(f"  Error: {e}")
        
        time.sleep(1)
    
    return False


def main():
    """Основная функция"""
    # Проверяем, что запущены с sudo (нужно для perf)
    if os.geteuid() != 0:
        print("Warning: perf record requires root privileges")
        print("Run with: sudo python3 shoot.py '...'")
    
    # Получаем команду для запуска сервера
    server_cmd = start_server()
    
    # Запускаем сервер
    print(f"Starting server: {server_cmd}")
    server = run(server_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    # Проверяем, что сервер запустился
    time.sleep(1)
    if server.poll() is not None:
        print("Error: Server failed to start")
        sys.exit(1)
    
    # Ждем готовности сервера
    if not wait_for_server():
        print("Error: Server is not responding")
        stop(server, wait=True)
        sys.exit(1)
    
    # Запускаем perf record
    print(f"Starting perf record for PID {server.pid}")
    perf_record = run(perf_record_of(server.pid), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    # Даем perf время на инициализацию
    time.sleep(1)
    
    # Выполняем запросы
    make_shots()
    
    # Останавливаем perf record
    print("Stopping perf record...")
    stop(perf_record, wait=True)
    time.sleep(1)
    
    # Генерируем flamegraph
    success = generate_flamegraph()
    
    # Останавливаем сервер
    print("Stopping server...")
    stop(server, wait=True)
    
    # Выходим с соответствующим кодом
    if success:
        print("\n✓ Test passed: graph.svg contains http_handler::RequestHandler")
        sys.exit(0)
    else:
        print("\n✗ Test failed: http_handler::RequestHandler not found in graph.svg")
        sys.exit(1)


if __name__ == "__main__":
    main()