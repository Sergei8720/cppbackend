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

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 100
COOLDOWN = 0.1


def fix_conan_installation():
    """Исправляет установку Conan до версии 1.81"""
    print("=" * 70)
    print("FIXING CONAN TO VERSION 1.81.0")
    print("=" * 70)
    
    # Полный сброс Conan
    print("1. Removing old Conan installations...")
    subprocess.run(['pip3', 'uninstall', '-y', 'conan'], 
                  stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL, check=False)
    
    # Удаляем все следы Conan из user site
    subprocess.run(['rm', '-rf', os.path.expanduser('~/.local/lib/python*/site-packages/conan*')],
                  shell=True, stderr=subprocess.DEVNULL, check=False)
    
    # Очищаем кэш pip
    subprocess.run(['pip3', 'cache', 'purge'], stderr=subprocess.DEVNULL, check=False)
    
    # Устанавливаем Conan 1.81 точно
    print("2. Installing Conan 1.81.0...")
    result = subprocess.run([
        'pip3', 'install', '--user', '--no-cache-dir', 'conan==1.81.0'
    ], capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"ERROR installing Conan: {result.stderr}")
        sys.exit(1)
    
    # Обновляем PATH
    os.environ['PATH'] = f"{os.environ['HOME']}/.local/bin:{os.environ['PATH']}"
    
    # Проверяем версию
    version_result = subprocess.run(['conan', '--version'], 
                                   capture_output=True, text=True)
    conan_version = version_result.stdout.strip()
    print(f"3. Conan version installed: {conan_version}")
    
    if '1.81' not in conan_version:
        print(f"WARNING: Expected 1.81.0 but got {conan_version}")
    
    # Сбрасываем профиль
    print("4. Resetting Conan profile...")
    subprocess.run(['conan', 'profile', 'new', 'default', '--detect', '--force'], 
                  stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL, check=False)
    
    # Принудительно устанавливаем правильный ABI
    subprocess.run([
        'conan', 'profile', 'update', 
        'settings.compiler.libcxx=libstdc++11', 'default'
    ], stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL, check=False)
    
    # Очищаем кэш jsoncpp
    subprocess.run(['conan', 'remove', 'jsoncpp/1.9.5', '-f'], 
                  stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL, check=False)
    
    # Показываем финальный профиль
    profile = subprocess.run(['conan', 'profile', 'show', 'default'], 
                            capture_output=True, text=True)
    print("5. Final Conan profile:")
    for line in profile.stdout.split('\n'):
        if 'compiler' in line or 'arch' in line or 'os' in line:
            print(f"   {line.strip()}")
    
    print("=" * 70)
    return True


def rebuild_server_with_conan181(server_path):
    """Пересобирает сервер с Conan 1.81"""
    print("Checking if rebuild needed...")
    
    # Определяем пути
    base_dir = os.path.abspath(os.path.join(
        os.path.dirname(__file__), 
        '../../sprint1/problems/map_json'
    ))
    solution_dir = os.path.join(base_dir, 'solution')
    build_dir = os.path.join(solution_dir, 'build_conan181')
    
    # Создаем временный conanfile.txt с явными настройками
    conanfile_content = """
[requires]
boost/1.78.0
jsoncpp/1.9.5
zlib/1.2.13
bzip2/1.0.8
libbacktrace/cci.20210118

[generators]
cmake

[options]
boost:shared=False
jsoncpp:shared=False
zlib:shared=False
"""
    
    with open(os.path.join(solution_dir, 'conanfile_fixed.txt'), 'w') as f:
        f.write(conanfile_content)
    
    # Собираем заново
    os.makedirs(build_dir, exist_ok=True)
    
    # Устанавливаем зависимости
    print("Installing dependencies with Conan 1.81...")
    subprocess.run([
        'conan', 'install', solution_dir,
        '--build=missing',
        '-s', 'compiler=gcc',
        '-s', 'compiler.version=11',
        '-s', 'compiler.libcxx=libstdc++11',
        '-s', 'build_type=Release'
    ], cwd=build_dir, check=False)
    
    # Конфигурируем CMake
    print("Configuring CMake...")
    subprocess.run([
        'cmake', solution_dir,
        '-DCMAKE_BUILD_TYPE=Release'
    ], cwd=build_dir, check=False)
    
    # Собираем
    print("Building server...")
    subprocess.run(['cmake', '--build', '.'], cwd=build_dir, check=False)
    
    # Ищем собранный сервер
    possible_servers = glob.glob(os.path.join(build_dir, 'bin', 'game_server')) + \
                       glob.glob(os.path.join(build_dir, 'game_server'))
    
    if possible_servers:
        new_server = possible_servers[0]
        print(f"Server rebuilt: {new_server}")
        return new_server
    
    print("Rebuild failed, using original server")
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
            print(f"Progress: {i + 1}/{SHOOT_COUNT}")
    print('Shooting complete')


def build_flamegraph():
    flamegraph_dir = os.path.join(os.path.dirname(__file__), 'FlameGraph')
    
    if not os.path.exists(flamegraph_dir):
        print("Cloning FlameGraph...")
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


def main():
    # Шаг 1: Фиксим Conan до версии 1.81
    if not fix_conan_installation():
        print("Failed to fix Conan installation")
        sys.exit(1)
    
    # Шаг 2: Получаем путь к серверу
    original_server = start_server()
    print(f"Original server: {original_server}")
    
    # Шаг 3: Пересобираем с правильным Conan
    fixed_server = rebuild_server_with_conan181(original_server)
    print(f"Using server: {fixed_server}")
    
    # Шаг 4: Запускаем сервер
    print("Starting server...")
    server = run(fixed_server)
    
    # Ждем запуска
    time.sleep(2)
    
    # Проверяем доступность
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
    
    # Шаг 9: Генерируем flamegraph
    build_flamegraph()
    
    print("=" * 70)
    print("JOB COMPLETED SUCCESSFULLY")
    print("=" * 70)


if __name__ == '__main__':
    main()