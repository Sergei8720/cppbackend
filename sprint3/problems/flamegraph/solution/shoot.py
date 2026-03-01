import argparse
import subprocess
import time
import random
import shlex
import signal
import sys
import os
import requests
from typing import Optional, List

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
PERF_SETUP_TIMEOUT = 2
FLAMEGRAPH_PATH = "./FlameGraph"  # Путь к утилитам FlameGraph


class ProcessManager:
    """Менеджер для безопасного управления процессами"""
    
    def __init__(self):
        self.processes: List[subprocess.Popen] = []
    
    def run(self, command: str, stdout=None, stderr=None, shell: bool = False) -> subprocess.Popen:
        """Безопасный запуск процесса с регистрацией"""
        try:
            if shell:
                process = subprocess.Popen(command, stdout=stdout, stderr=stderr, shell=True)
            else:
                process = subprocess.Popen(shlex.split(command), stdout=stdout, stderr=stderr)
            self.processes.append(process)
            return process
        except Exception as e:
            print(f"Failed to run command '{command}': {e}")
            self.cleanup()
            sys.exit(1)
    
    def stop(self, process: subprocess.Popen, wait: bool = False, timeout: Optional[float] = None) -> bool:
        """Остановка процесса с таймаутом"""
        if process.poll() is not None:
            return True  # Процесс уже завершен
            
        try:
            if wait:
                try:
                    process.wait(timeout=timeout)
                    return True
                except subprocess.TimeoutExpired:
                    print(f"Process {process.pid} did not terminate within {timeout}s, forcing kill")
                    process.kill()
                    process.wait()
                    return False
            else:
                process.terminate()
                return True
        except Exception as e:
            print(f"Error stopping process {process.pid}: {e}")
            return False
    
    def cleanup(self):
        """Завершение всех процессов"""
        for process in self.processes:
            if process.poll() is None:  # Процесс еще работает
                try:
                    process.terminate()
                    try:
                        process.wait(timeout=1)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
                except:
                    pass
        self.processes.clear()


def check_flamegraph_utils() -> bool:
    """Проверка наличия утилит FlameGraph"""
    required_scripts = ['stackcollapse-perf.pl', 'flamegraph.pl']
    for script in required_scripts:
        script_path = os.path.join(FLAMEGRAPH_PATH, script)
        if not os.path.isfile(script_path):
            print(f"Error: {script_path} not found")
            print(f"Please ensure FlameGraph tools are installed in {FLAMEGRAPH_PATH}")
            print("You can clone them from: https://github.com/brendangregg/FlameGraph")
            return False
    return True


def start_server(proc_manager: ProcessManager) -> str:
    """Запуск сервера и проверка его доступности"""
    parser = argparse.ArgumentParser(description='Load test server and generate flamegraph')
    parser.add_argument('server', type=str, help='Server command to run')
    args = parser.parse_args()
    
    print(f"Starting server: {args.server}")
    server_process = proc_manager.run(args.server, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, shell=True)
    
    # Ждем запуска сервера
    time.sleep(1)  # Даем время на инициализацию
    
    # Проверяем, что сервер не упал сразу
    if server_process.poll() is not None:
        stderr = server_process.stderr.read() if server_process.stderr else "No error output"
        print(f"Server failed to start. Exit code: {server_process.returncode}")
        print(f"Stderr: {stderr.decode() if isinstance(stderr, bytes) else stderr}")
        sys.exit(1)
    
    return server_process


def wait_for_server(urls: List[str], timeout: int = SERVER_STARTUP_TIMEOUT) -> bool:
    """Ожидание доступности сервера"""
    print(f"Waiting for server to become available (timeout: {timeout}s)...")
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        for url in urls:
            try:
                full_url = f"http://{url}"
                response = requests.get(full_url, timeout=1)
                if response.status_code == 200:
                    print(f"Server is ready! ({full_url})")
                    return True
            except (requests.ConnectionError, requests.Timeout):
                continue
            except Exception as e:
                print(f"Unexpected error checking {url}: {e}")
        
        time.sleep(0.5)
    
    print(f"Server did not become available within {timeout} seconds")
    return False


def perf_record_of(pid: int) -> str:
    """Генерация команды perf record"""
    return f"perf record -p {pid} -o perf.data -g --freq=100"


def shoot(proc_manager: ProcessManager, ammo: str) -> bool:
    """Выполнение одного запроса с проверкой результата"""
    try:
        # Используем curl с выводом HTTP кода
        full_url = f"http://{ammo}"
        process = proc_manager.run(
            f'curl -s -o /dev/null -w "%{{http_code}}" "{full_url}"', 
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        
        # Ждем завершения curl
        process.wait(timeout=5)
        
        if process.returncode == 0:
            http_code = process.stdout.read().decode().strip()
            if http_code == '200':
                return True
            else:
                print(f"Warning: {full_url} returned HTTP {http_code}")
        else:
            stderr = process.stderr.read().decode().strip()
            print(f"Warning: curl failed for {full_url}: {stderr}")
            
    except subprocess.TimeoutExpired:
        print(f"Warning: curl timeout for {ammo}")
        proc_manager.stop(process, wait=True)
    except Exception as e:
        print(f"Warning: unexpected error for {ammo}: {e}")
    
    return False


def make_shots(proc_manager: ProcessManager) -> tuple[int, int]:
    """Выполнение серии запросов"""
    print(f"Starting shooting sequence: {SHOOT_COUNT} requests with {COOLDOWN}s cooldown")
    successful = 0
    failed = 0
    
    for i in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        ammo = AMMUNITION[ammo_number]
        
        if shoot(proc_manager, ammo):
            successful += 1
        else:
            failed += 1
        
        # Прогресс каждые 10 запросов
        if (i + 1) % 10 == 0:
            print(f"Progress: {i + 1}/{SHOOT_COUNT} requests completed")
        
        time.sleep(COOLDOWN)
    
    print(f'Shooting complete. Successful: {successful}, Failed: {failed}')
    return successful, failed


def generate_flamegraph(proc_manager: ProcessManager, output_file: str = "graph.svg") -> bool:
    """Генерация flamegraph из собранных данных"""
    print("Generating flamegraph...")
    
    if not os.path.exists("perf.data"):
        print("Error: perf.data not found")
        return False
    
    # Проверяем наличие утилит FlameGraph
    if not check_flamegraph_utils():
        return False
    
    try:
        with open(output_file, "w") as graph_file:
            # Создаем конвейер процессов
            perf_script = proc_manager.run(
                "perf script -i perf.data", 
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            
            stackcollapse = proc_manager.run(
                os.path.join(FLAMEGRAPH_PATH, "stackcollapse-perf.pl"),
                stdin=perf_script.stdout,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            
            flamegraph = proc_manager.run(
                os.path.join(FLAMEGRAPH_PATH, "flamegraph.pl"),
                stdin=stackcollapse.stdout,
                stdout=graph_file,
                stderr=subprocess.PIPE
            )
            
            # Ждем завершения всех процессов
            flamegraph.wait(timeout=30)
            stackcollapse.wait(timeout=10)
            perf_script.wait(timeout=10)
            
            if flamegraph.returncode == 0:
                print(f"Flamegraph generated successfully: {output_file}")
                return True
            else:
                stderr = flamegraph.stderr.read().decode()
                print(f"Error generating flamegraph: {stderr}")
                return False
                
    except subprocess.TimeoutExpired:
        print("Error: Flamegraph generation timed out")
        return False
    except Exception as e:
        print(f"Error during flamegraph generation: {e}")
        return False


def main():
    """Основная функция"""
    proc_manager = ProcessManager()
    server_process = None
    perf_record = None
    exit_code = 0
    
    try:
        # Запуск сервера
        server_process = start_server(proc_manager)
        
        # Ожидание готовности сервера
        if not wait_for_server(AMMUNITION):
            print("Server is not responding. Exiting.")
            sys.exit(1)
        
        # Запуск perf record
        print(f"Starting perf record for PID {server_process.pid}")
        perf_record = proc_manager.run(perf_record_of(server_process.pid))
        time.sleep(PERF_SETUP_TIMEOUT)  # Даем время perf на инициализацию
        
        # Выполнение запросов
        successful, failed = make_shots(proc_manager)
        
        # Остановка perf record
        print("Stopping perf record...")
        proc_manager.stop(perf_record, wait=False)
        time.sleep(1)  # Даем время на запись данных
        
        # Остановка сервера
        print("Stopping server...")
        proc_manager.stop(server_process, wait=True, timeout=5)
        
        # Генерация flamegraph
        if failed < successful:  # Генерируем только если большинство запросов успешны
            if not generate_flamegraph(proc_manager):
                print("Warning: Flamegraph generation failed")
                exit_code = 1
        else:
            print("Skipping flamegraph generation due to high failure rate")
            exit_code = 1
            
        print('Job done')
        
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        exit_code = 130
    except Exception as e:
        print(f"Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        exit_code = 1
    finally:
        # Очистка процессов
        print("Cleaning up...")
        proc_manager.cleanup()
        sys.exit(exit_code)


if __name__ == "__main__":
    main()