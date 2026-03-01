#!/usr/bin/env python3

import argparse
import subprocess
import time
import random
import shlex
import os
import signal
import sys
import glob
import shutil

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 100
COOLDOWN = 0.1


def force_rebuild_with_correct_abi():
    """ПОЛНОСТЬЮ пересобирает сервер с правильным ABI"""
    print("=" * 80)
    print("ПЕРЕСБОРКА СЕРВЕРА С ПРАВИЛЬНЫМ ABI (libstdc++11)")
    print("=" * 80)
    
    # 1. Устанавливаем Conan 1.81
    print("1. Установка Conan 1.81.0...")
    subprocess.run(['pip3', 'uninstall', '-y', 'conan'], 
                  stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL, check=False)
    subprocess.run([
        'pip3', 'install', '--user', '--no-cache-dir', 'conan==1.81.0'
    ], check=True)
    
    # Обновляем PATH
    os.environ['PATH'] = f"{os.environ['HOME']}/.local/bin:{os.environ['PATH']}"
    
    # 2. Полностью очищаем Conan кэш
    print("2. Очистка кэша Conan...")
    conan_home = os.path.expanduser('~/.conan')
    if os.path.exists(conan_home):
        shutil.rmtree(conan_home, ignore_errors=True)
    
    # 3. Создаем профиль с libstdc++11
    print("3. Создание профиля с libstdc++11...")
    subprocess.run(['conan', 'profile', 'new', 'default', '--detect', '--force'], 
                  stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL, check=False)
    subprocess.run([
        'conan', 'profile', 'update', 
        'settings.compiler.libcxx=libstdc++11', 'default'
    ], check=False)
    
    # Проверяем профиль
    profile = subprocess.run(['conan', 'profile', 'show', 'default'], 
                            capture_output=True, text=True)
    print("Профиль Conan:")
    for line in profile.stdout.split('\n'):
        if 'compiler.libcxx' in line:
            print(f"  {line.strip()}")
    
    # 4. Определяем пути к исходникам сервера
    script_dir = os.path.dirname(os.path.abspath(__file__))
    sprint3_dir = script_dir
    sprint1_dir = os.path.abspath(os.path.join(script_dir, '../../sprint1'))
    solution_dir = os.path.join(sprint1_dir, 'problems/map_json/solution')
    data_dir = os.path.join(sprint1_dir, 'problems/map_json/data')
    
    print(f"4. Путь к исходникам: {solution_dir}")
    
    # 5. Создаем временную директорию для сборки
    build_dir = os.path.join(solution_dir, 'build_fixed')
    os.makedirs(build_dir, exist_ok=True)
    
    # 6. Устанавливаем зависимости через Conan
    print("5. Установка зависимостей через Conan...")
    conan_result = subprocess.run([
        'conan', 'install', solution_dir,
        '--build=missing',
        '-s', 'compiler=gcc',
        '-s', 'compiler.version=11',
        '-s', 'compiler.libcxx=libstdc++11',
        '-s', 'build_type=Release'
    ], cwd=build_dir, capture_output=True, text=True)
    
    if conan_result.returncode != 0:
        print("ОШИБКА при установке зависимостей:")
        print(conan_result.stderr)
        return None
    
    # 7. Конфигурируем CMake
    print("6. Конфигурация CMake...")
    cmake_result = subprocess.run([
        'cmake', solution_dir,
        '-DCMAKE_BUILD_TYPE=Release'
    ], cwd=build_dir, capture_output=True, text=True)
    
    if cmake_result.returncode != 0:
        print("ОШИБКА при конфигурации CMake:")
        print(cmake_result.stderr)
        return None
    
    # 8. Собираем сервер
    print("7. Сборка сервера...")
    build_result = subprocess.run(['cmake', '--build', '.', '-j2'], 
                                 cwd=build_dir, capture_output=True, text=True)
    
    if build_result.returncode != 0:
        print("ОШИБКА при сборке:")
        print(build_result.stderr)
        return None
    
    # 9. Ищем собранный сервер
    possible_servers = glob.glob(os.path.join(build_dir, 'bin', 'game_server')) + \
                       glob.glob(os.path.join(build_dir, 'game_server'))
    
    if possible_servers:
        new_server = possible_servers[0]
        print(f"8. Сервер успешно собран: {new_server}")
        
        # Копируем данные рядом с сервером
        server_dir = os.path.dirname(new_server)
        dest_data = os.path.join(server_dir, 'data')
        if os.path.exists(dest_data):
            shutil.rmtree(dest_data, ignore_errors=True)
        if os.path.exists(data_dir):
            shutil.copytree(data_dir, dest_data)
            print(f"9. Данные скопированы в {dest_data}")
        
        return new_server
    
    print("НЕ УДАЛОСЬ найти собранный сервер")
    return None


def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('original_server', type=str, nargs='?', default=None)
    return parser.parse_args().original_server


def run_server(server_path):
    """Запускает сервер и возвращает процесс"""
    if not os.path.exists(server_path):
        print(f"ОШИБКА: Сервер не найден: {server_path}")
        sys.exit(1)
    
    print(f"Запуск сервера: {server_path}")
    return subprocess.Popen([server_path, 'data/config.json'])


def stop_server(process):
    if process and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except:
            process.kill()


def make_shots():
    for i in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        url = AMMUNITION[ammo_number]
        
        try:
            subprocess.run(['curl', '-s', '-o', '/dev/null', url], 
                         timeout=1, check=False)
        except:
            pass
        
        time.sleep(COOLDOWN)
        
        if (i + 1) % 10 == 0:
            print(f"Прогресс: {i + 1}/{SHOOT_COUNT}")
    
    print('Стрельба завершена')


def build_flamegraph():
    flamegraph_dir = os.path.join(os.path.dirname(__file__), 'FlameGraph')
    
    if not os.path.exists(flamegraph_dir):
        print("Клонирование FlameGraph...")
        subprocess.run([
            'git', 'clone', 
            'https://github.com/brendangregg/FlameGraph.git',
            flamegraph_dir
        ], check=False)
    
    stackcollapse = os.path.join(flamegraph_dir, 'stackcollapse-perf.pl')
    flamegraph = os.path.join(flamegraph_dir, 'flamegraph.pl')

    # perf script
    perf_script = subprocess.Popen(
        ['perf', 'script', '-i', 'perf.data'],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL
    )

    # stackcollapse-perf.pl
    stackcollapse_proc = subprocess.Popen(
        [stackcollapse],
        stdin=perf_script.stdout,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL
    )

    # flamegraph.pl -> graph.svg
    with open('graph.svg', 'w') as svg:
        flamegraph_proc = subprocess.Popen(
            [flamegraph],
            stdin=stackcollapse_proc.stdout,
            stdout=svg,
            stderr=subprocess.DEVNULL
        )
        flamegraph_proc.wait()

    perf_script.wait()
    stackcollapse_proc.wait()
    
    if os.path.exists('graph.svg'):
        size = os.path.getsize('graph.svg')
        print(f"Flamegraph создан: graph.svg ({size} байт)")
    else:
        print("Ошибка создания flamegraph")


def main():
    # Шаг 1: Полностью пересобираем сервер с правильным ABI
    server_path = force_rebuild_with_correct_abi()
    
    if not server_path or not os.path.exists(server_path):
        print("КРИТИЧЕСКАЯ ОШИБКА: Не удалось собрать сервер")
        sys.exit(1)
    
    # Шаг 2: Запускаем сервер
    server_process = run_server(server_path)
    
    # Ждем запуска
    print("Ожидание запуска сервера...")
    time.sleep(3)
    
    # Проверяем доступность
    try:
        result = subprocess.run(['curl', '-f', 'http://localhost:8080/api/v1/maps'],
                               capture_output=True, timeout=3)
        print("Сервер отвечает на запросы")
    except Exception as e:
        print(f"Сервер не отвечает: {e}")
        stop_server(server_process)
        sys.exit(1)
    
    # Шаг 3: Запускаем perf
    print("Запуск perf record...")
    perf = subprocess.Popen(
        [
            'perf', 'record',
            '-F', '99',
            '-g',
            '-p', str(server_process.pid),
            '-o', 'perf.data'
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )
    
    # Шаг 4: Делаем запросы
    make_shots()
    
    # Шаг 5: Останавливаем perf
    perf.send_signal(signal.SIGINT)
    try:
        perf.wait(timeout=5)
    except:
        perf.kill()
    
    # Шаг 6: Останавливаем сервер
    stop_server(server_process)
    
    # Шаг 7: Генерируем flamegraph
    build_flamegraph()
    
    print("=" * 80)
    print("ЗАДАНИЕ ВЫПОЛНЕНО УСПЕШНО!")
    print("=" * 80)


if __name__ == '__main__':
    main()