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


def fix_conan_version():
    """Принудительно ставит Conan 1.81"""
    print("=" * 70)
    print("УСТАНОВКА CONAN 1.81.0")
    print("=" * 70)
    
    # Удаляем старый Conan
    subprocess.run(['pip3', 'uninstall', '-y', 'conan'], 
                  stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL, check=False)
    
    # Чистим кэш
    subprocess.run(['rm', '-rf', os.path.expanduser('~/.conan')], 
                  stderr=subprocess.DEVNULL, check=False)
    
    # Ставим Conan 1.81
    subprocess.run([
        'pip3', 'install', '--user', '--no-cache-dir', 'conan==1.81.0'
    ], check=True)
    
    # Обновляем PATH
    os.environ['PATH'] = f"{os.environ['HOME']}/.local/bin:{os.environ['PATH']}"
    
    # Проверяем версию
    result = subprocess.run(['conan', '--version'], capture_output=True, text=True)
    print(f"Conan version: {result.stdout.strip()}")
    
    # Создаем профиль с libstdc++11
    subprocess.run(['conan', 'profile', 'new', 'default', '--detect', '--force'], check=False)
    subprocess.run([
        'conan', 'profile', 'update', 
        'settings.compiler.libcxx=libstdc++11', 'default'
    ], check=False)
    
    # Очищаем jsoncpp
    subprocess.run(['conan', 'remove', 'jsoncpp/1.9.5', '-f'], 
                  stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL, check=False)
    
    print("=" * 70)


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
            print(f"Прогресс: {i + 1}/{SHOOT_COUNT}")
    print('Стрельба завершена')


def build_flamegraph():
    flamegraph_dir = os.path.join(os.path.dirname(__file__), 'FlameGraph')
    
    if not os.path.exists(flamegraph_dir):
        print("Клонируем FlameGraph...")
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
        print("Flamegraph создан: graph.svg")
    else:
        print("Ошибка создания flamegraph")


def main():
    # Шаг 1: Фиксим Conan
    fix_conan_version()
    
    # Шаг 2: Запускаем сервер (оригинальный, он уже должен собраться правильно)
    server_path = start_server()
    print(f"Запускаем сервер: {server_path}")
    
    # Шаг 3: Запускаем сервер
    server = run(server_path)
    
    # Ждем запуска
    time.sleep(2)
    
    # Проверяем доступность
    try:
        subprocess.run(['curl', '-f', 'http://localhost:8080/api/v1/maps'],
                      capture_output=True, timeout=2)
        print("Сервер отвечает")
    except:
        print("Сервер не отвечает")
        stop(server, wait=True)
        sys.exit(1)
    
    # Шаг 4: Запускаем perf
    print("Запускаем perf record...")
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
    
    # Шаг 5: Делаем запросы
    make_shots()
    
    # Шаг 6: Останавливаем perf
    perf.send_signal(signal.SIGINT)
    perf.wait()
    
    # Шаг 7: Останавливаем сервер
    stop(server, wait=True)
    
    # Шаг 8: Генерируем flamegraph
    build_flamegraph()
    
    print("=" * 70)
    print("ЗАДАНИЕ ВЫПОЛНЕНО УСПЕШНО!")
    print("=" * 70)


if __name__ == '__main__':
    main()