import argparse
import subprocess
import time
import random
import shlex
import os
import signal

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
    parser.add_argument('server', type=str)
    return parser.parse_args().server

def run(command, output=None):
    process = subprocess.Popen(shlex.split(command), stdout=output, stderr=subprocess.DEVNULL)
    return process

def stop(process, wait=False):
    if process.poll() is None and wait:
        process.wait()
    process.terminate()

def shoot(ammo):
    hit = run('curl ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)

def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')

def get_server_pid(server_process):
    """Получаем PID запущенного сервера"""
    return server_process.pid

def record_perf(server_pid):
    """Запускаем perf record для сбора трассировки"""
    perf_command = [
        'perf', 'record',
        '-o', 'perf.data',
        '-p', str(server_pid),
        '-g'  # включение записи стека вызовов
    ]
    return subprocess.Popen(perf_command)

def generate_flamegraph():
    """Генерируем флеймграф из собранных данных"""
    # Получаем данные из perf.data
    perf_script = subprocess.run(
        ['perf', 'script', '-i', 'perf.data'],
        capture_output=True,
        text=True
    )

    # Пропускаем через stackcollapse-perf.pl
    stackcollapse = subprocess.run(
        ['./FlameGraph/stackcollapse-perf.pl'],
        input=perf_script.stdout,
        capture_output=True,
        text=True
    )

    # Генерируем SVG через flamegraph.pl
    with open('graph.svg', 'w') as f:
        subprocess.run(
            ['./FlameGraph/flamegraph.pl'],
            input=stackcollapse.stdout,
            stdout=f,
            text=True
        )

# Основной поток выполнения
if __name__ == '__main__':
    # Запускаем сервер
    server_cmd = start_server()
    server_process = run(server_cmd)

    # Ждём немного, чтобы сервер успел запуститься
    time.sleep(2)

    try:
        # Получаем PID сервера
        server_pid = get_server_pid(server_process)

        # Запускаем сбор данных perf
        perf_process = record_perf(server_pid)

        # Выполняем обстрелы запросами
        make_shots()

        # Останавливаем сбор данных perf (отправляем SIGINT)
        perf_process.send_signal(signal.SIGINT)
        perf_process.wait(timeout=10)  # ждём завершения записи perf.data

        # Генерируем флеймграф
        generate_flamegraph()
        print('Flamegraph generated: graph.svg')

    except Exception as e:
        print(f'Error during profiling: {e}')
    finally:
        # Всегда останавливаем сервер
        stop(server_process, wait=True)
        time.sleep(1)
        print('Job done')
