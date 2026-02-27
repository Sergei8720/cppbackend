import argparse
import subprocess
import time
import random
import shlex
import signal
import os

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 100
COOLDOWN = 0.1

PERF_DATA = "perf.data"


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
    hit = run('curl -s ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')


def main():
    # Запускаем сервер (путь должен быть передан как аргумент)
    server_cmd = start_server()
    print(f"Starting server: {server_cmd}")
    server = run(server_cmd, subprocess.DEVNULL)
    time.sleep(1)  # Даем серверу больше времени на запуск
    
    # Запускаем perf record
    print(f"Starting perf record for PID {server.pid}")
    perf = run(f'perf record -o {PERF_DATA} -p {server.pid} -g')
    time.sleep(0.5)
    
    # Обстреливаем сервер
    make_shots()
    
    # Останавливаем perf
    perf.send_signal(signal.SIGINT)
    perf.wait()
    time.sleep(0.5)
    
    # Останавливаем сервер
    stop(server)
    time.sleep(0.5)
    
    # Строим флеймграф
    print("Generating flamegraph...")
    with open("graph.svg", "w") as graph_file:
        perf_script = subprocess.Popen(
            shlex.split(f"perf script -i {PERF_DATA}"),
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL
        )
        
        stackcollapse = subprocess.Popen(
            shlex.split("./FlameGraph/stackcollapse-perf.pl"),
            stdin=perf_script.stdout,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL
        )
        
        flamegraph = subprocess.Popen(
            shlex.split("./FlameGraph/flamegraph.pl"),
            stdin=stackcollapse.stdout,
            stdout=graph_file,
            stderr=subprocess.DEVNULL
        )
        
        flamegraph.wait()
        stackcollapse.wait()
        perf_script.wait()
    
    print('Job done')
    print('Flamegraph saved to graph.svg')


if __name__ == "__main__":
    main()