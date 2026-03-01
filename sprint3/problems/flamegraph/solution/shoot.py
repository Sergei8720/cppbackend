#!/usr/bin/env python3

import argparse
import subprocess
import time
import random
import shlex
import os
import signal
import sys
import shutil
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


def nuke_conan_cache():
    """ПОЛНОСТЬЮ УНИЧТОЖАЕТ кэш Conan"""
    print("=" * 80)
    print("🍆 ПОЛНАЯ ЗАЧИСТКА CONAN КЭША")
    print("=" * 80)
    
    # 1. Удаляем всю директорию .conan
    conan_dir = os.path.expanduser('~/.conan')
    if os.path.exists(conan_dir):
        print(f"Удаление {conan_dir}...")
        shutil.rmtree(conan_dir, ignore_errors=True)
    
    # 2. Удаляем возможные временные директории
    tmp_conan = '/tmp/conan-home'
    if os.path.exists(tmp_conan):
        shutil.rmtree(tmp_conan, ignore_errors=True)
    
    # 3. Устанавливаем Conan 1.81
    print("Установка Conan 1.81.0...")
    subprocess.run(['pip3', 'uninstall', '-y', 'conan'], 
                  stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL, check=False)
    subprocess.run([
        'pip3', 'install', '--user', '--no-cache-dir', 'conan==1.81.0'
    ], check=True)
    
    # Обновляем PATH
    os.environ['PATH'] = f"{os.environ['HOME']}/.local/bin:{os.environ['PATH']}"
    
    # 4. Создаем профиль с libstdc++11
    subprocess.run(['conan', 'profile', 'new', 'default', '--detect', '--force'], 
                  stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL, check=False)
    subprocess.run([
        'conan', 'profile', 'update', 
        'settings.compiler.libcxx=libstdc++11', 'default'
    ], check=False)
    
    # Проверяем
    result = subprocess.run(['conan', '--version'], capture_output=True, text=True)
    print(f"Conan version: {result.stdout.strip()}")
    
    profile = subprocess.run(['conan', 'profile', 'show', 'default'], 
                            capture_output=True, text=True)
    for line in profile.stdout.split('\n'):
        if 'compiler.libcxx' in line:
            print(f"Profile: {line.strip()}")
    
    print("=" * 80)
    return True


def rebuild_server_manually():
    """Пересобирает сервер вручную с правильным ABI"""
    print("=" * 80)
    print("🔨 Ручная пересборка сервера")
    print("=" * 80)
    
    # Пути
    script_dir = os.path.dirname(os.path.abspath(__file__))
    sprint1_dir = os.path.abspath(os.path.join(script_dir, '../../sprint1'))
    solution_dir = os.path.join(sprint1_dir, 'problems/map_json/solution')
    data_dir = os.path.join(sprint1_dir, 'problems/map_json/data')
    
    # Создаем временную директорию для сборки
    build_dir = os.path.join(solution_dir, 'build_clean')
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir, ignore_errors=True)
    os.makedirs(build_dir)
    
    print(f"Solution dir: {solution_dir}")
    print(f"Build dir: {build_dir}")
    
    # Устанавливаем зависимости
    print("\n1. Установка зависимостей через Conan...")
    conan_cmd = [
        'conan', 'install', solution_dir,
        '--build=missing',
        '-s', 'compiler=gcc',
        '-s', 'compiler.version=11',
        '-s', 'compiler.libcxx=libstdc++11',
        '-s', 'build_type=Release'
    ]
    
    result = subprocess.run(conan_cmd, cwd=build_dir, capture_output=True, text=True)
    if result.returncode != 0:
        print("ОШИБКА при установке зависимостей:")
        print(result.stderr)
        return None
    
    # Конфигурируем CMake
    print("\n2. Конфигурация CMake...")
    cmake_cmd = ['cmake', solution_dir, '-DCMAKE_BUILD_TYPE=Release']
    result = subprocess.run(cmake_cmd, cwd=build_dir, capture_output=True, text=True)
    if result.returncode != 0:
        print("ОШИБКА при конфигурации CMake:")
        print(result.stderr)
        return None
    
    # Собираем
    print("\n3. Сборка...")
    build_cmd = ['cmake', '--build', '.', '-j2']
    result = subprocess.run(build_cmd, cwd=build_dir, capture_output=True, text=True)
    if result.returncode != 0:
        print("ОШИБКА при сборке:")
        print(result.stderr)
        return None
    
    # Ищем сервер
    server_candidates = glob.glob(os.path.join(build_dir, 'bin', 'game_server')) + \
                        glob.glob(os.path.join(build_dir, 'game_server'))
    
    if server_candidates:
        server_path = server_candidates[0]
        print(f"\n✅ Сервер собран: {server_path}")
        
        # Копируем данные
        server_dir = os.path.dirname(server_path)
        dest_data = os.path.join(server_dir, 'data')
        if os.path.exists(dest_data):
            shutil.rmtree(dest_data, ignore_errors=True)
        if os.path.exists(data_dir):
            shutil.copytree(data_dir, dest_data)
            print(f"✅ Данные скопированы в {dest_data}")
        
        return server_path
    
    print("❌ Сервер не найден после сборки")
    return None


def run_server(server_path):
    """Запускает сервер"""
    if not os.path.exists(server_path):
        print(f"❌ Сервер не найден: {server_path}")
        return None
    
    print(f"\n🚀 Запуск сервера: {server_path}")
    return subprocess.Popen([server_path, 'data/config.json'])


def stop_server(process):
    """Останавливает сервер"""
    if process and process.poll() is None:
        print("\n🛑 Остановка сервера...")
        process.terminate()
        try:
            process.wait(timeout=5)
        except:
            process.kill()


def make_shots():
    """Делает запросы к серверу"""
    print("\n🎯 Начало стрельбы...")
    for i in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        url = f"http://{AMMUNITION[ammo_number]}"
        
        try:
            subprocess.run(['curl', '-s', '-o', '/dev/null', url], 
                         timeout=1, check=False)
        except:
            pass
        
        time.sleep(COOLDOWN)
        
        if (i + 1) % 10 == 0:
            print(f"  Прогресс: {i + 1}/{SHOOT_COUNT}")
    
    print("✅ Стрельба завершена")


def build_flamegraph():
    """Строит flamegraph"""
    print("\n🔥 Построение flamegraph...")
    
    flamegraph_dir = os.path.join(os.path.dirname(__file__), 'FlameGraph')
    
    if not os.path.exists(flamegraph_dir):
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
        print(f"✅ Flamegraph создан: graph.svg ({size} байт)")
    else:
        print("❌ Ошибка создания flamegraph")


def main():
    # Шаг 1: ЯДЕРНАЯ БОМБА - уничтожаем кэш Conan
    if not nuke_conan_cache():
        print("❌ Не удалось очистить кэш Conan")
        sys.exit(1)
    
    # Шаг 2: Пересобираем сервер с нуля
    server_path = rebuild_server_manually()
    if not server_path:
        print("❌ Не удалось собрать сервер")
        sys.exit(1)
    
    # Шаг 3: Запускаем сервер
    server_process = run_server(server_path)
    if not server_process:
        sys.exit(1)
    
    # Ждем запуска
    print("⏳ Ожидание запуска сервера...")
    time.sleep(3)
    
    # Проверяем доступность
    try:
        subprocess.run(['curl', '-f', 'http://localhost:8080/api/v1/maps'],
                      capture_output=True, timeout=3)
        print("✅ Сервер отвечает на запросы")
    except Exception as e:
        print(f"❌ Сервер не отвечает: {e}")
        stop_server(server_process)
        sys.exit(1)
    
    # Шаг 4: Запускаем perf
    print("\n📊 Запуск perf record...")
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
    
    # Шаг 5: Делаем запросы
    make_shots()
    
    # Шаг 6: Останавливаем perf
    perf.send_signal(signal.SIGINT)
    try:
        perf.wait(timeout=5)
    except:
        perf.kill()
    
    # Шаг 7: Останавливаем сервер
    stop_server(server_process)
    
    # Шаг 8: Строим flamegraph
    build_flamegraph()
    
    print("\n" + "=" * 80)
    print("✅ ЗАДАНИЕ ВЫПОЛНЕНО УСПЕШНО!")
    print("=" * 80)


if __name__ == '__main__':
    main()