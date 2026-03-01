#!/usr/bin/env python3

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


def force_correct_abi():
    """Принудительно настраивает Conan на новый ABI"""
    print("=" * 60)
    print("FORCING CORRECT ABI (libstdc++11) FOR CONAN")
    print("=" * 60)
    
    # Полностью сбрасываем Conan
    os.environ['CONAN_USER_HOME'] = '/tmp/conan-home-correct'
    
    # Удаляем старый кэш
    subprocess.run(['rm', '-rf', '/tmp/conan-home-correct'], check=False)
    
    # Создаем новый профиль с правильным ABI
    subprocess.run(['conan', 'profile', 'new', 'default', '--detect', '--force'], 
                  check=False)
    
    # Устанавливаем новый ABI
    subprocess.run([
        'conan', 'profile', 'update', 
        'settings.compiler.libcxx=libstdc++11', 'default'
    ], check=False)
    
    # Удаляем все кэшированные пакеты jsoncpp
    subprocess.run(['conan', 'remove', 'jsoncpp/1.9.5', '-f'], check=False)
    
    # Проверяем настройки
    result = subprocess.run(['conan', 'profile', 'show', 'default'], 
                          capture_output=True, text=True)
    print("Current Conan profile:")
    print(result.stdout)
    
    # Создаем файл с настройками для conan install
    with open('/tmp/conan_settings.txt', 'w') as f:
        f.write("""
compiler=gcc
compiler.version=11
compiler.libcxx=libstdc++11
build_type=Release
""")
    
    print("Conab ABI forced to libstdc++11")
    print("=" * 60)


def rebuild_server_with_correct_abi(server_path):
    """Пересобирает сервер с правильным ABI"""
    print("Checking if server needs rebuild...")
    
    # Проверяем, есть ли уже собранный сервер
    if os.path.exists(server_path):
        # Проверяем, с каким ABI собран сервер
        try:
            result = subprocess.run(['strings', server_path, '|', 'grep', 'jsoncpp'],
                                  shell=True, capture_output=True, text=True)
            if '6557f18c' in result.stdout:
                print("Server already has correct ABI")
                return server_path
        except:
            pass
    
    print("Rebuilding server with correct ABI...")
    
    # Пути к исходникам
    solution_dir = os.path.abspath(os.path.join(
        os.path.dirname(__file__), 
        '../../sprint1/problems/map_json/solution'
    ))
    
    # Пересобираем с правильным ABI
    build_dir = os.path.join(solution_dir, 'build_fixed')
    os.makedirs(build_dir, exist_ok=True)
    
    # Устанавливаем зависимости с правильным ABI
    subprocess.run([
        'conan', 'install', solution_dir,
        '--build=missing',
        '-s', 'compiler=gcc',
        '-s', 'compiler.version=11',
        '-s', 'compiler.libcxx=libstdc++11',
        '-s', 'build_type=Release'
    ], cwd=build_dir, check=False)
    
    # Конфигурируем CMake
    subprocess.run([
        'cmake', solution_dir,
        '-DCMAKE_BUILD_TYPE=Release'
    ], cwd=build_dir, check=False)
    
    # Собираем
    subprocess.run(['cmake', '--build', '.'], cwd=build_dir, check=False)
    
    new_server = os.path.join(build_dir, 'bin', 'game_server')
    if os.path.exists(new_server):
        print(f"Server rebuilt successfully: {new_server}")
        return new_server
    
    return server_path


def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str)
    return parser.parse_args().server


def run(command, output=None):
    process = subprocess.Popen(
        shlex.split(command),
        stdout=output,
        stderr=subprocess.DEVNULL
    )
    return process


def stop(process, wait=False):
    if process.poll() is None:
        process.terminate()
        if wait:
            process.wait()


def shoot(ammo):
    hit = run('curl -s ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for i in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
        if (i + 1) % 10 == 0:
            print(f"Shots fired: {i + 1}/{SHOOT_COUNT}")
    print('Shooting complete')


def build_flamegraph():
    flamegraph_dir = os.path.join(os.path.dirname(__file__), 'FlameGraph')
    
    # Проверяем наличие FlameGraph
    if not os.path.exists(flamegraph_dir):
        print("Cloning FlameGraph repository...")
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
        print("Flamegraph generated: graph.svg")
    else:
        print("Failed to generate flamegraph")


# --- main logic ---

def main():
    # Шаг 1: Принудительно настраиваем Conan на правильный ABI
    force_correct_abi()
    
    # Шаг 2: Получаем путь к серверу
    server_path = start_server()
    print(f"Original server path: {server_path}")
    
    # Шаг 3: Пересобираем сервер если нужно
    fixed_server = rebuild_server_with_correct_abi(server_path)
    print(f"Using server: {fixed_server}")
    
    # Шаг 4: Запускаем сервер
    server = run(fixed_server)
    
    # Даём серверу время подняться
    print("Waiting for server to start...")
    time.sleep(2)
    
    # Проверяем что сервер отвечает
    try:
        subprocess.run(['curl', '-f', 'http://localhost:8080/api/v1/maps'],
                      capture_output=True, timeout=2)
        print("Server is responding")
    except:
        print("Server failed to respond")
        stop(server, wait=True)
        sys.exit(1)
    
    # Шаг 5: Запускаем perf
    print("Starting perf record...")
    perf = subprocess.Popen(
        [
            'perf', 'record',
            '-F', '99',
            '-g',
            '-p', str(server.pid),
            '-o', 'perf.data'
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )
    
    # Шаг 6: Делаем запросы
    make_shots()
    
    # Шаг 7: Останавливаем perf
    perf.send_signal(signal.SIGINT)
    perf.wait()
    
    # Шаг 8: Останавливаем сервер
    stop(server, wait=True)
    time.sleep(1)
    
    # Шаг 9: Генерируем flamegraph
    build_flamegraph()
    
    print('Job done successfully!')


if __name__ == '__main__':
    main()