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


def setup_conan():
    """Настраивает Conan перед сборкой сервера"""
    print("Setting up Conan with correct ABI...")
    
    # Устанавливаем переменные окружения
    os.environ['CONAN_USER_HOME'] = '/tmp/conan-home'
    
    # Создаем профиль с правильным ABI
    subprocess.run([
        'conan', 'profile', 'new', 'default', '--detect', '--force'
    ], check=False)
    
    subprocess.run([
        'conan', 'profile', 'update', 
        'settings.compiler.libcxx=libstdc++11', 'default'
    ], check=False)
    
    # Удаляем jsoncpp из кэша, чтобы переустановить с правильным ABI
    subprocess.run([
        'conan', 'remove', 'jsoncpp/1.9.5', '-f'
    ], check=False)
    
    print("Conan setup complete")


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
    hit = run('curl ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')


def build_flamegraph():
    flamegraph_dir = os.path.join(os.path.dirname(__file__), 'FlameGraph')
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


# --- main logic ---

# Настраиваем Conan перед всем остальным
setup_conan()

server_path = start_server()
print(f"Starting server from: {server_path}")

# Запускаем сервер
server = run(server_path)

# даём серверу немного времени подняться
time.sleep(1)

# запускаем perf record с привязкой к PID сервера
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

make_shots()

# корректно останавливаем perf (SIGINT как при Ctrl+C)
perf.send_signal(signal.SIGINT)
perf.wait()

stop(server, wait=True)
time.sleep(1)

build_flamegraph()

print('Job done')