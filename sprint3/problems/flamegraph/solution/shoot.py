#!/usr/bin/env python3
import argparse
import subprocess
import time
import random
import signal
import os
import sys
import socket
from pathlib import Path

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

# Используем localhost для тестирования
AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 50  # Уменьшим для GitHub Actions
COOLDOWN = 0.05   # Уменьшим для ускорения
SERVER_STARTUP_TIMEOUT = 10


def start_server():
    """Парсинг аргументов командной строки"""
    parser = argparse.ArgumentParser(description='Load test server and generate flamegraph')
    parser.add_argument('server', type=str, help='Server command to run')
    return parser.parse_args().server


def perf_record_of(pid):
    """Генерация команды perf record для Linux"""
    return f"perf record -p {pid} -o perf.data -g -F 99"


def run(command, stdout=None, stderr=None):
    """Запуск процесса"""
    # В GitHub Actions используем простой запуск без preexec_fn
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
        process.terminate()  # Отправляем SIGTERM
        
        if wait:
            try:
                process.wait(timeout=timeout)
                return True
            except subprocess.TimeoutExpired:
                process.kill()  # Если не завершился, убиваем
                process.wait()
                return False
    except Exception as e:
        print(f"Error stopping process: {e}")
        return False
    
    return True


def shoot(ammo):
    """Выполнение одного запроса к серверу"""
    full_url = f"http://{ammo}"
    try:
        # Используем curl с таймаутом
        result = subprocess.run(
            f'curl -s -o /dev/null -w "%{{http_code}}" "{full_url}"',
            shell=True,
            capture_output=True,
            text=True,
            timeout=2
        )
        
        if result.returncode == 0:
            http_code = result.stdout.strip()
            if http_code != '200':
                print(f"Warning: {full_url} returned HTTP {http_code}")
                return False
            return True
        else:
            return False
    except subprocess.TimeoutExpired:
        print(f"Warning: curl timeout for {full_url}")
        return False
    except Exception as e:
        print(f"Error in shoot: {e}")
        return False


def make_shots():
    """Выполнение серии запросов к серверу"""
    print(f"Making {SHOOT_COUNT} requests...")
    
    successful = 0
    failed = 0
    
    for i in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        ammo = AMMUNITION[ammo_number]
        
        if shoot(ammo):
            successful += 1
        else:
            failed += 1
        
        if (i + 1) % 10 == 0:
            print(f"Progress: {i + 1}/{SHOOT_COUNT} (OK: {successful}, Failed: {failed})")
        
        time.sleep(COOLDOWN)
    
    print(f'Shooting complete. Successful: {successful}, Failed: {failed}')
    return successful, failed


def check_flamegraph_utils():
    """Проверка наличия утилит FlameGraph"""
    # В GitHub Actions они должны быть в scripts/sprint3/FlameGraph/
    flamegraph_dir = Path("../sprint3/FlameGraph")
    
    if not flamegraph_dir.exists():
        # Попробуем другие возможные пути
        alt_paths = [
            Path("./FlameGraph"),
            Path("/usr/local/bin/FlameGraph"),
            Path.home() / "FlameGraph"
        ]
        
        for path in alt_paths:
            if path.exists():
                flamegraph_dir = path
                break
    
    stackcollapse = flamegraph_dir / "stackcollapse-perf.pl"
    flamegraph_pl = flamegraph_dir / "flamegraph.pl"
    
    if not stackcollapse.exists():
        print(f"Error: {stackcollapse} not found")
        print("Please ensure FlameGraph tools are available")
        return False, None, None
    
    if not flamegraph_pl.exists():
        print(f"Error: {flamegraph_pl} not found")
        return False, None, None
    
    print(f"Found FlameGraph tools in {flamegraph_dir}")
    return True, str(stackcollapse), str(flamegraph_pl)


def generate_flamegraph():
    """Генерация flamegraph с проверкой наличия RequestHandler"""
    found, stackcollapse, flamegraph_pl = check_flamegraph_utils()
    if not found:
        return False
    
    # Проверяем наличие perf.data
    if not Path("perf.data").exists():
        print("Error: perf.data not found")
        return False
    
    if Path("perf.data").stat().st_size == 0:
        print("Error: perf.data is empty")
        return False
    
    print("Generating flamegraph...")
    
    # Создаем временный файл для данных perf script
    with open("perf.out", "w") as perf_file:
        perf_script = subprocess.run(
            ["perf", "script", "-i", "perf.data"],
            stdout=perf_file,
            stderr=subprocess.PIPE,
            text=True
        )
        
        if perf_script.returncode != 0:
            print(f"perf script error: {perf_script.stderr}")
            return False
    
    # Проверяем наличие RequestHandler в данных
    with open("perf.out", "r") as f:
        content = f.read()
        if "RequestHandler" in content:
            print("✓ Found RequestHandler in perf data")
        else:
            print("⚠ Warning: RequestHandler not found in perf data")
    
    # Генерируем flamegraph
    with open("perf.out", "r") as perf_file, open("graph.svg", "w") as svg_file:
        # stackcollapse
        stackcollapse_proc = subprocess.Popen(
            [stackcollapse],
            stdin=perf_file,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        # flamegraph
        flamegraph_proc = subprocess.Popen(
            [flamegraph_pl],
            stdin=stackcollapse_proc.stdout,
            stdout=svg_file,
            stderr=subprocess.PIPE,
            text=True
        )
        
        stackcollapse_proc.stdout.close()
        flamegraph_proc.wait()
        stackcollapse_proc.wait()
        
        if flamegraph_proc.returncode != 0:
            _, stderr = flamegraph_proc.communicate()
            print(f"Flamegraph error: {stderr}")
            return False
    
    # Проверяем результат
    if Path("graph.svg").exists() and Path("graph.svg").stat().st_size > 0:
        with open("graph.svg", "r") as f:
            svg_content = f.read()
            
            # Ищем методы RequestHandler
            patterns = [
                "http_handler::RequestHandler",
                "RequestHandler::operator()",
                "RequestHandler::IsMapRequest",
                "RequestHandler::MakeMapList"
            ]
            
            found = [p for p in patterns if p in svg_content]
            
            if found:
                print("✓ SUCCESS: Found RequestHandler methods in flamegraph")
                for f in found:
                    print(f"  - {f}")
                return True
            else:
                print("⚠ WARNING: RequestHandler methods not found in SVG")
                return False
    else:
        print("Error: graph.svg is empty or not created")
        return False


def wait_for_server():
    """Ожидание готовности сервера"""
    print("Waiting for server to start...")
    
    for attempt in range(SERVER_STARTUP_TIMEOUT * 2):
        try:
            # Проверяем TCP соединение
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(0.5)
            result = sock.connect_ex(('localhost', 8080))
            sock.close()
            
            if result == 0:
                # Проверяем HTTP ответ
                try:
                    result = subprocess.run(
                        'curl -s -o /dev/null -w "%{http_code}" "http://localhost:8080/api/v1/maps"',
                        shell=True,
                        capture_output=True,
                        text=True,
                        timeout=1
                    )
                    if result.returncode == 0:
                        print(f"Server is ready (attempt {attempt + 1})")
                        return True
                except:
                    pass
            
            if attempt % 2 == 0:
                print(f"  Waiting... ({attempt//2 + 1}/{SERVER_STARTUP_TIMEOUT})")
            
            time.sleep(0.5)
        except Exception as e:
            print(f"  Check error: {e}")
            time.sleep(0.5)
    
    return False


def main():
    """Основная функция"""
    # Получаем команду для запуска сервера
    server_cmd = start_server()
    
    # Запускаем сервер
    print(f"Starting server: {server_cmd}")
    server = run(server_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    
    # Проверяем, что сервер запустился
    time.sleep(1)
    if server.poll() is not None:
        stderr = server.stderr.read().decode() if server.stderr else ""
        print(f"Error: Server failed to start (exit code: {server.returncode})")
        if stderr:
            print(f"stderr: {stderr[:200]}")
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
    successful, failed = make_shots()
    
    # Останавливаем perf record
    print("Stopping perf record...")
    stop(perf_record, wait=True)
    time.sleep(1)
    
    # Генерируем flamegraph
    flamegraph_success = generate_flamegraph()
    
    # Останавливаем сервер
    print("Stopping server...")
    stop(server, wait=True)
    
    # Очищаем временные файлы
    for f in ["perf.out", "perf.data"]:
        if Path(f).exists():
            Path(f).unlink()
    
    # Возвращаем код возврата для CI/CD
    if flamegraph_success and failed == 0:
        print("\n✓ All tests passed")
        sys.exit(0)
    else:
        print("\n✗ Tests failed")
        sys.exit(1)


if __name__ == "__main__":
    main()