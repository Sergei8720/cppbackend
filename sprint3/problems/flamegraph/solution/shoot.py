#!/usr/bin/env python3
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


def start_server():
    """Парсит аргумент командной строки с командой запуска сервера"""
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str, help='Command to start the server (e.g., "path/to/server path/to/config")')
    return parser.parse_args().server


def perf_record_of(pid):
    """Возвращает команду для запуска perf record с привязкой к процессу"""
    # -F 99: частота сэмплирования 99 Гц (меньше нагрузка на процессор)
    # -g: запись стека вызовов
    return f"perf record -p {pid} -o perf.data -g -F 99"


def run(command, output=None, cwd=None):
    """Запускает процесс и возвращает объект процесса"""
    try:
        # Разбиваем команду на аргументы с учетом кавычек
        args = shlex.split(command)
        
        # Создаем процесс
        process = subprocess.Popen(
            args, 
            stdout=output, 
            stderr=subprocess.PIPE if output == subprocess.DEVNULL else None,
            cwd=cwd,
            text=True
        )
        return process
    except Exception as e:
        print(f"Error running command '{command}': {e}", file=sys.stderr)
        sys.exit(1)


def stop(process, wait=False):
    """Останавливает процесс корректно"""
    if process and process.poll() is None:
        if wait:
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass
        process.terminate()
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def shoot(ammo):
    """Делает один запрос к серверу через curl"""
    try:
        # -s: тихий режим, -o /dev/null: не сохранять ответ, -w: вывести только код ответа
        hit = run(f'curl -s -o /dev/null -w "%{{http_code}}" {ammo}', 
                 output=subprocess.PIPE)
        stdout, _ = hit.communicate(timeout=2)
        time.sleep(COOLDOWN)
        stop(hit, wait=True)
        return stdout.strip()
    except Exception as e:
        print(f"Error shooting {ammo}: {e}", file=sys.stderr)
        return None


def make_shots():
    """Делает случайные запросы к серверу"""
    print(f'Starting shooting {SHOOT_COUNT} requests...')
    successful = 0
    
    for i in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        result = shoot(AMMUNITION[ammo_number])
        if result == "200":
            successful += 1
        
        if (i + 1) % 10 == 0:
            print(f'  Completed {i + 1}/{SHOOT_COUNT} shots '
                  f'(successful: {successful}/{i+1})')
    
    print(f'Shooting complete. Successful requests: {successful}/{SHOOT_COUNT}')
    return successful > 0


def find_flamegraph_scripts():
    """Находит скрипты FlameGraph в различных местах"""
    possible_paths = [
        './FlameGraph',
        '../FlameGraph',
        '/usr/local/FlameGraph',
        os.path.join(os.path.dirname(os.path.abspath(__file__)), 'FlameGraph'),
        os.path.join(os.path.dirname(os.path.abspath(__file__)), '../FlameGraph'),
    ]
    
    for path in possible_paths:
        stackcollapse = os.path.join(path, 'stackcollapse-perf.pl')
        flamegraph = os.path.join(path, 'flamegraph.pl')
        
        if os.path.exists(stackcollapse) and os.path.exists(flamegraph):
            print(f'Found FlameGraph scripts in: {path}')
            return stackcollapse, flamegraph
    
    # Если не нашли, пробуем использовать системные (если установлены)
    return 'stackcollapse-perf.pl', 'flamegraph.pl'


def create_flamegraph():
    """Создает флеймграф из perf.data"""
    print('Creating flamegraph...')
    
    # Проверяем наличие файла perf.data
    if not os.path.exists('perf.data'):
        print('Error: perf.data not found', file=sys.stderr)
        return False
    
    file_size = os.path.getsize('perf.data')
    print(f'perf.data size: {file_size} bytes')
    
    if file_size == 0:
        print('Error: perf.data is empty', file=sys.stderr)
        return False
    
    # Находим скрипты FlameGraph
    stackcollapse_script, flamegraph_script = find_flamegraph_scripts()
    
    # Создаем флеймграф
    with open("graph.svg", "w") as graph_file:
        # perf script - извлекаем данные профилирования
        perf_script = subprocess.Popen(
            shlex.split("perf script -i perf.data"), 
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        # stackcollapse-perf.pl - сворачиваем стеки
        flamegraph_stackcollapse = subprocess.Popen(
            shlex.split(stackcollapse_script), 
            stdin=perf_script.stdout, 
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        perf_script.stdout.close()
        
        # flamegraph.pl - генерируем SVG
        flamegraph_output = subprocess.Popen(
            shlex.split(flamegraph_script), 
            stdin=flamegraph_stackcollapse.stdout, 
            stdout=graph_file,
            stderr=subprocess.PIPE,
            text=True
        )
        flamegraph_stackcollapse.stdout.close()
        
        # Ждем завершения
        flamegraph_output.wait(timeout=30)
    
    # Проверяем созданный файл
    if os.path.exists('graph.svg'):
        size = os.path.getsize('graph.svg')
        print(f'graph.svg created, size: {size} bytes')
        
        if size > 0:
            with open('graph.svg', 'r') as f:
                content = f.read()
                # Ищем упоминания RequestHandler (именно так проверяют тесты)
                handler_count = content.count('http_handler::RequestHandler')
                print(f'Found {handler_count} occurrences of RequestHandler in flamegraph')
                
                if handler_count == 0:
                    print('Warning: No RequestHandler calls in flamegraph')
                    # Для отладки сохраняем первые несколько строк perf script
                    with open('perf_script_sample.txt', 'w') as debug:
                        subprocess.run(
                            shlex.split("perf script -i perf.data | head -200"),
                            stdout=debug
                        )
        else:
            print('Error: graph.svg is empty', file=sys.stderr)
            return False
    else:
        print('Error: graph.svg not created', file=sys.stderr)
        return False
    
    return True


def check_server_availability(server_pid):
    """Проверяет, отвечает ли сервер на запросы"""
    try:
        # Делаем пробный запрос
        result = subprocess.run(
            shlex.split("curl -s -o /dev/null -w '%{http_code}' localhost:8080/api/v1/maps"),
            capture_output=True,
            text=True,
            timeout=2
        )
        if result.returncode == 0 and result.stdout.strip() == "200":
            print('Server is responding correctly')
            return True
        else:
            print(f'Server returned: {result.stdout.strip()}')
            return False
    except Exception as e:
        print(f'Error checking server: {e}')
        return False


def main():
    # Сохраняем текущую директорию
    original_dir = os.getcwd()
    
    try:
        # Получаем команду запуска сервера
        server_command = start_server()
        print(f'Server command: {server_command}')
        
        # Определяем директорию для выходных файлов (из переменной окружения или текущей)
        output_dir = os.environ.get('DIRECTORY', original_dir)
        os.makedirs(output_dir, exist_ok=True)
        os.chdir(output_dir)
        print(f'Working directory: {output_dir}')
        
        # Запускаем сервер
        print('Starting server...')
        server = run(server_command, output=subprocess.DEVNULL)
        time.sleep(1)  # Даем серверу время на запуск
        
        # Проверяем, что сервер запустился
        if server.poll() is not None:
            print('Error: Server failed to start', file=sys.stderr)
            # Выводим ошибку сервера для отладки
            if server.stderr:
                stderr_output = server.stderr.read()
                if stderr_output:
                    print(f'Server stderr: {stderr_output}', file=sys.stderr)
            sys.exit(1)
        
        print(f'Server started with PID: {server.pid}')
        
        # Проверяем, что сервер отвечает
        if not check_server_availability(server.pid):
            print('Warning: Server is not responding correctly')
        
        # Запускаем perf record
        print('Starting perf record...')
        perf_record = run(perf_record_of(server.pid))
        time.sleep(0.1)
        
        # Делаем запросы к серверу
        shots_successful = make_shots()
        
        # Останавливаем perf record (посылаем SIGINT для корректного завершения)
        print('Stopping perf record...')
        perf_record.send_signal(signal.SIGINT)
        try:
            perf_record.wait(timeout=5)
        except subprocess.TimeoutExpired:
            perf_record.kill()
        
        # Останавливаем сервер
        print('Stopping server...')
        stop(server, wait=True)
        
        # Даем время на запись данных
        time.sleep(1)
        
        # Создаем флеймграф
        flamegraph_created = create_flamegraph()
        
        if not flamegraph_created:
            print('Warning: Flamegraph creation failed', file=sys.stderr)
            sys.exit(1)
        
        # Финальная проверка для тестов
        perf_data_exists = os.path.exists('perf.data') and os.path.getsize('perf.data') > 0
        graph_exists = os.path.exists('graph.svg') and os.path.getsize('graph.svg') > 0
        
        print(f'Final check:')
        print(f'  perf.data exists and non-empty: {perf_data_exists}')
        print(f'  graph.svg exists and non-empty: {graph_exists}')
        
        if graph_exists:
            with open('graph.svg', 'r') as f:
                content = f.read()
                has_handler = 'http_handler::RequestHandler' in content
                print(f'  graph.svg contains RequestHandler: {has_handler}')
        
        print('Job done')
        
    finally:
        # Возвращаемся в исходную директорию
        os.chdir(original_dir)


if __name__ == '__main__':
    main()