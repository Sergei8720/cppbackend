#!/usr/bin/env python3
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
    parser.add_argument('server', type=str, help='Command to start the server')
    return parser.parse_args().server


def perf_record_of(pid):
    # Увеличиваем частоту сэмплирования и глубину стека
    return f"perf record -p {pid} -o perf.data -g -F 99"


def run(command, output=None, cwd=None):
    """Run a command and return the process"""
    try:
        process = subprocess.Popen(
            shlex.split(command), 
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
    """Stop a process gracefully"""
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
    """Make a single request to the server"""
    try:
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
    """Make random requests to the server"""
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


def create_flamegraph():
    """Create flamegraph from perf.data"""
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
    
    # Проверяем наличие скриптов FlameGraph
    flamegraph_dir = './FlameGraph'
    if not os.path.exists(flamegraph_dir):
        # Пробуем найти в родительском каталоге
        flamegraph_dir = '../FlameGraph'
    
    stackcollapse_script = os.path.join(flamegraph_dir, 'stackcollapse-perf.pl')
    flamegraph_script = os.path.join(flamegraph_dir, 'flamegraph.pl')
    
    if not os.path.exists(stackcollapse_script):
        print(f'Warning: {stackcollapse_script} not found', file=sys.stderr)
        # Пробуем использовать системные скрипты
        stackcollapse_script = 'stackcollapse-perf.pl'
        flamegraph_script = 'flamegraph.pl'
    
    # Создаем флеймграф
    with open("graph.svg", "w") as graph_file:
        # Сначала получаем список символов для проверки
        print("Checking symbols in perf data...")
        nm_process = subprocess.run(
            shlex.split("perf script -i perf.data | head -50"),
            capture_output=True,
            text=True
        )
        if 'RequestHandler' in nm_process.stdout:
            print("Found RequestHandler symbols in perf data")
        else:
            print("Warning: No RequestHandler symbols found in perf data")
            print("First few lines of perf script:")
            print(nm_process.stdout[:500])
        
        # perf script -> stackcollapse -> flamegraph
        perf_script = subprocess.Popen(
            shlex.split("perf script -i perf.data"), 
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        flamegraph_stackcollapse = subprocess.Popen(
            shlex.split(stackcollapse_script), 
            stdin=perf_script.stdout, 
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        perf_script.stdout.close()
        
        flamegraph_output = subprocess.Popen(
            shlex.split(flamegraph_script), 
            stdin=flamegraph_stackcollapse.stdout, 
            stdout=graph_file,
            stderr=subprocess.PIPE,
            text=True
        )
        flamegraph_stackcollapse.stdout.close()
        
        # Ждем завершения и проверяем ошибки
        flamegraph_output.wait(timeout=30)
        
        # Проверяем наличие RequestHandler в итоговом файле
        graph_file.flush()
    
    # Проверяем созданный файл
    if os.path.exists('graph.svg'):
        size = os.path.getsize('graph.svg')
        print(f'graph.svg created, size: {size} bytes')
        
        if size > 0:
            with open('graph.svg', 'r') as f:
                content = f.read()
                handler_count = content.count('http_handler::RequestHandler')
                print(f'Found {handler_count} occurrences of RequestHandler in flamegraph')
                
                if handler_count == 0:
                    print('Warning: No RequestHandler calls in flamegraph')
                    # Сохраняем отладочную информацию
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
    
    print('Flamegraph created: graph.svg')
    return True


def check_debug_symbols(server_pid):
    """Проверяем наличие отладочных символов"""
    print("Checking debug symbols...")
    try:
        result = subprocess.run(
            shlex.split(f"cat /proc/{server_pid}/maps"),
            capture_output=True,
            text=True
        )
        if result.returncode == 0:
            print("Process memory maps:")
            print(result.stdout[:500])
        
        # Проверяем символы в исполняемом файле
        result = subprocess.run(
            shlex.split("nm -C build/bin/game_server | grep RequestHandler || true"),
            capture_output=True,
            text=True,
            cwd=os.path.dirname(os.path.abspath(__file__))
        )
        if result.stdout:
            print("Found symbols in executable:")
            print(result.stdout)
        else:
            print("No symbols found - binary might be stripped")
            
    except Exception as e:
        print(f"Error checking debug symbols: {e}")


def main():
    # Сохраняем текущую директорию
    original_dir = os.getcwd()
    
    try:
        # Запускаем сервер
        print('Starting server...')
        server_command = start_server()
        
        # Переходим в директорию, где будут создаваться файлы
        output_dir = os.environ.get('DIRECTORY', original_dir)
        os.makedirs(output_dir, exist_ok=True)
        os.chdir(output_dir)
        print(f'Working directory: {output_dir}')
        
        server = run(server_command, output=subprocess.DEVNULL)
        time.sleep(0.5)  # Даем серверу время на запуск
        
        if server.poll() is not None:
            print('Error: Server failed to start', file=sys.stderr)
            # Выводим ошибку сервера
            if server.stderr:
                print(server.stderr.read(), file=sys.stderr)
            sys.exit(1)
        
        print(f'Server started with PID: {server.pid}')
        
        # Проверяем отладочные символы
        check_debug_symbols(server.pid)
        
        # Запускаем perf record
        print('Starting perf record...')
        perf_record = run(perf_record_of(server.pid))
        time.sleep(0.1)
        
        # Делаем запросы
        shots_successful = make_shots()
        
        if not shots_successful:
            print('Warning: No successful requests made to server')
        
        # Останавливаем perf record
        print('Stopping perf record...')
        perf_record.send_signal(signal.SIGINT)
        try:
            perf_record.wait(timeout=5)
        except subprocess.TimeoutExpired:
            perf_record.kill()
        
        # Останавливаем сервер
        print('Stopping server...')
        stop(server, wait=True)
        
        # Создаем флеймграф
        time.sleep(1)  # Даем время на запись данных
        flamegraph_created = create_flamegraph()
        
        if not flamegraph_created:
            print('Warning: Flamegraph creation failed', file=sys.stderr)
            # Для отладки сохраняем perf.data
            if os.path.exists('perf.data'):
                print('perf.data preserved for debugging')
        
        # Проверяем результаты для тестов
        if os.path.exists('perf.data'):
            print(f'perf.data exists: {os.path.getsize("perf.data") > 0}')
        if os.path.exists('graph.svg'):
            size = os.path.getsize('graph.svg')
            print(f'graph.svg exists: {size > 0}')
            if size > 0:
                with open('graph.svg', 'r') as f:
                    content = f.read()
                    print(f'RequestHandler in graph: {content.count("http_handler::RequestHandler") > 0}')
        
        print('Job done')
        
    finally:
        # Возвращаемся в исходную директорию
        os.chdir(original_dir)


if __name__ == '__main__':
    main()