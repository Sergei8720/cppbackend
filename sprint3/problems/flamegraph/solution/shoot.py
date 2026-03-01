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
SERVER_STARTUP_TIMEOUT = 5
PERF_SAMPLING_FREQ = 99


def start_server():
    """Парсинг аргументов командной строки"""
    parser = argparse.ArgumentParser(description='Load test server and generate flamegraph')
    parser.add_argument('server', type=str, help='Server command to run')
    return parser.parse_args().server


def perf_record_of(pid):
    """Генерация команды perf record с явным указанием выходного файла"""
    return f"perf record -p {pid} -o perf.data -g -F {PERF_SAMPLING_FREQ} --call-graph dwarf"


def run(command, stdout=None, stderr=None):
    """Запуск процесса с поддержкой shell команд и созданием новой группы процессов"""
    process = subprocess.Popen(
        command, 
        shell=True, 
        stdout=stdout, 
        stderr=stderr,
        preexec_fn=os.setsid  # Создаем новую группу процессов для удобного завершения
    )
    return process


def stop(process, wait=False, timeout=5):
    """Остановка процесса и всех его дочерних процессов"""
    if process.poll() is not None:
        return True
        
    try:
        # Отправляем SIGINT всей группе процессов
        os.killpg(os.getpgid(process.pid), signal.SIGINT)
        
        if wait:
            try:
                process.wait(timeout=timeout)
                return True
            except subprocess.TimeoutExpired:
                # Если процесс не завершился, убиваем принудительно
                os.killpg(os.getpgid(process.pid), signal.SIGKILL)
                process.wait()
                return False
    except ProcessLookupError:
        # Процесс уже завершился
        return True
    except Exception as e:
        print(f"Error stopping process: {e}")
        return False


def shoot(ammo):
    """Выполнение одного запроса к серверу"""
    full_url = f"http://{ammo}"
    process = run(f'curl -s -o /dev/null -w "%{{http_code}}" "{full_url}"', stdout=subprocess.PIPE)
    
    # Ждем завершения curl
    try:
        process.wait(timeout=2)
        if process.returncode == 0:
            http_code = process.stdout.read().decode().strip()
            if http_code != '200':
                print(f"Warning: {full_url} returned HTTP {http_code}")
    except subprocess.TimeoutExpired:
        stop(process, wait=True)
        print(f"Warning: curl timeout for {full_url}")
    
    time.sleep(COOLDOWN)


def make_shots():
    """Выполнение серии запросов к серверу"""
    print(f"Making {SHOOT_COUNT} requests with {COOLDOWN}s cooldown...")
    
    successful = 0
    failed = 0
    
    for i in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        ammo = AMMUNITION[ammo_number]
        
        # Проверяем результат запроса
        full_url = f"http://{ammo}"
        result = subprocess.run(
            f'curl -s -o /dev/null -w "%{{http_code}}" "{full_url}"', 
            shell=True, 
            capture_output=True, 
            text=True,
            timeout=2
        )
        
        if result.returncode == 0 and result.stdout.strip() == '200':
            successful += 1
        else:
            failed += 1
        
        # Показываем прогресс
        if (i + 1) % 10 == 0:
            print(f"Progress: {i + 1}/{SHOOT_COUNT} requests (OK: {successful}, Failed: {failed})")
        
        time.sleep(COOLDOWN)
    
    print(f'Shooting complete. Successful: {successful}, Failed: {failed}')
    return successful, failed


def check_flamegraph_utils():
    """Проверка наличия утилит FlameGraph"""
    flamegraph_dir = "./FlameGraph"
    
    stackcollapse_pl = os.path.join(flamegraph_dir, "stackcollapse-perf.pl")
    flamegraph_pl = os.path.join(flamegraph_dir, "flamegraph.pl")
    
    if not os.path.exists(stackcollapse_pl):
        print(f"Error: {stackcollapse_pl} not found")
        print("Please install FlameGraph tools from: https://github.com/brendangregg/FlameGraph")
        return False
    
    if not os.path.exists(flamegraph_pl):
        print(f"Error: {flamegraph_pl} not found")
        return False
    
    return True


def generate_flamegraph():
    """Генерация flamegraph из perf.data с проверкой наличия RequestHandler"""
    if not check_flamegraph_utils():
        return False
    
    # Проверяем наличие perf.data
    if not os.path.exists("perf.data"):
        print("Error: perf.data not found")
        return False
    
    if os.path.getsize("perf.data") == 0:
        print("Error: perf.data is empty")
        return False
    
    print("Generating flamegraph...")
    print("Looking for http_handler::RequestHandler methods in call stack...")
    
    # Создаем конвейер для генерации flamegraph
    # 1. perf script - читает perf.data и выводит в текстовом формате
    perf_script = subprocess.Popen(
        ["perf", "script", "-i", "perf.data"],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True
    )
    
    # 2. stackcollapse-perf.pl - объединяет стектрейсы
    stackcollapse = subprocess.Popen(
        ["./FlameGraph/stackcollapse-perf.pl"],
        stdin=perf_script.stdout,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True
    )
    
    # Закрываем stdout первого процесса, так как он уже передан во второй
    perf_script.stdout.close()
    
    # Открываем файл для записи SVG
    with open("graph.svg", "w") as graph_file:
        # 3. flamegraph.pl - генерирует SVG
        flamegraph = subprocess.Popen(
            ["./FlameGraph/flamegraph.pl"],
            stdin=stackcollapse.stdout,
            stdout=graph_file,
            stderr=subprocess.PIPE,
            text=True
        )
        
        # Закрываем stdout второго процесса
        stackcollapse.stdout.close()
        
        # Ждем завершения и проверяем ошибки
        flamegraph.wait(timeout=30)
        _, stderr = flamegraph.communicate()
        
        if flamegraph.returncode != 0:
            print(f"Flamegraph generation warning: {stderr}")
    
    # Ждем завершения остальных процессов
    stackcollapse.wait(timeout=10)
    perf_script.wait(timeout=10)
    
    # Проверяем результат
    if os.path.exists("graph.svg") and os.path.getsize("graph.svg") > 0:
        # Проверяем наличие методов RequestHandler в SVG
        with open("graph.svg", "r") as f:
            svg_content = f.read()
            
            # Ищем различные варианты именования RequestHandler методов
            request_handler_patterns = [
                "http_handler::RequestHandler",
                "RequestHandler::operator()",
                "RequestHandler::IsMapRequest",
                "RequestHandler::SplitUrl",
                "RequestHandler::MakeMapList",
                "RequestHandler::MakeMapById"
            ]
            
            found_patterns = []
            for pattern in request_handler_patterns:
                if pattern in svg_content:
                    found_patterns.append(pattern)
            
            if found_patterns:
                print("✓ SUCCESS: Found RequestHandler methods in flamegraph:")
                for pattern in found_patterns:
                    print(f"  - {pattern}")
                return True
            else:
                print("✗ WARNING: RequestHandler methods NOT found in flamegraph")
                print("  This might indicate that:")
                print("  - Perf didn't sample during request handling")
                print("  - Sampling frequency is too low")
                print("  - The server wasn't processing requests when perf was recording")
                
                # Показываем первые несколько строк из стека для диагностики
                print("\nFirst few stack frames from perf data:")
                perf_check = subprocess.run(
                    ["perf", "script", "-i", "perf.data", "--head", "10"],
                    capture_output=True,
                    text=True
                )
                print(perf_check.stdout[:500] + "...")
                
                return False
    else:
        print("Error: Failed to generate flamegraph")
        return False


def wait_for_server():
    """Проверка, что сервер отвечает на запросы"""
    import socket
    
    print("Waiting for server to start...")
    max_attempts = 10
    
    for attempt in range(max_attempts):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(1)
            result = sock.connect_ex(('localhost', 8080))
            sock.close()
            
            if result == 0:
                # Дополнительно проверяем, что сервер отвечает на API
                try:
                    test_result = subprocess.run(
                        'curl -s -o /dev/null -w "%{http_code}" "http://localhost:8080/api/v1/maps"',
                        shell=True,
                        capture_output=True,
                        text=True,
                        timeout=1
                    )
                    if test_result.returncode == 0:
                        print(f"Server is ready (attempt {attempt + 1}/{max_attempts})")
                        return True
                except:
                    pass
            
            print(f"  Waiting... ({attempt + 1}/{max_attempts})")
            time.sleep(1)
        except Exception as e:
            print(f"  Check error: {e}")
    
    return False


def main():
    """Основная функция"""
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
    
    # Ждем, пока сервер начнет отвечать
    if not wait_for_server():
        print("Error: Server is not responding")
        stop(server, wait=True)
        sys.exit(1)
    
    # Даем серверу время на полную инициализацию
    time.sleep(1)
    
    # Запускаем perf record
    print(f"Starting perf record for PID {server.pid}")
    perf_record = run(perf_record_of(server.pid), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    # Даем perf время на инициализацию
    time.sleep(1)
    
    # Выполняем обстрел сервера запросами
    successful, failed = make_shots()
    
    # Завершаем perf record
    print("Stopping perf record...")
    stop(perf_record, wait=True)
    
    # Даем время на завершение записи
    time.sleep(2)
    
    # Генерируем flamegraph
    success = generate_flamegraph()
    
    # Завершаем сервер
    print("Stopping server...")
    stop(server, wait=True)
    
    # Финальный вердикт
    if success and failed == 0:
        print("\n✓✓✓ JOB COMPLETED SUCCESSFULLY ✓✓✓")
        print("  - All requests successful")
        print("  - RequestHandler methods found in flamegraph")
        print("  - Output file: graph.svg")
        sys.exit(0)
    elif success and failed > 0:
        print("\n⚠ JOB COMPLETED WITH WARNINGS ⚠")
        print(f"  - Failed requests: {failed}/{SHOOT_COUNT}")
        print("  - RequestHandler methods found in flamegraph")
        print("  - Output file: graph.svg")
        sys.exit(1)
    else:
        print("\n✗✗✗ JOB FAILED ✗✗✗")
        print("  - Flamegraph does not contain required RequestHandler methods")
        sys.exit(1)


if __name__ == "__main__":
    main()