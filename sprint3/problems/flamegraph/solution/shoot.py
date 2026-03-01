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


# Запускаем сервер
server_command = start_server()
server = run(server_command)

# Даем серверу время на запуск
time.sleep(2)

# Запускаем perf record для профилирования сервера
# Находим PID сервера и запускаем perf record
perf_record = subprocess.Popen(
    ['perf', 'record', '-o', 'perf.data', '-p', str(server.pid)],
    stderr=subprocess.DEVNULL
)

# Даем perf record время на подключение к процессу
time.sleep(1)

# Выполняем обстрел сервера запросами
make_shots()

# Останавливаем perf record (отправляем SIGINT, как Ctrl+C)
perf_record.send_signal(signal.SIGINT)
perf_record.wait()

# Останавливаем сервер
stop(server)
time.sleep(1)

# Генерируем флеймграф
# Проверяем, что файл perf.data существует и не пустой
if os.path.exists('perf.data') and os.path.getsize('perf.data') > 0:
    # Создаем пайплайн для генерации флеймграфа
    # perf script | ./FlameGraph/stackcollapse-perf.pl | ./FlameGraph/flamegraph.pl > graph.svg
    
    perf_script = subprocess.Popen(
        ['perf', 'script', '-i', 'perf.data'],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL
    )
    
    stackcollapse = subprocess.Popen(
        ['./FlameGraph/stackcollapse-perf.pl'],
        stdin=perf_script.stdout,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL
    )
    
    flamegraph = subprocess.Popen(
        ['./FlameGraph/flamegraph.pl'],
        stdin=stackcollapse.stdout,
        stdout=open('graph.svg', 'w'),
        stderr=subprocess.DEVNULL
    )
    
    # Закрываем потоки и ждем завершения
    perf_script.stdout.close()
    stackcollapse.stdout.close()
    flamegraph.communicate()
    
    print('FlameGraph generated: graph.svg')
else:
    print('Error: perf.data is empty or does not exist')

print('Job done')