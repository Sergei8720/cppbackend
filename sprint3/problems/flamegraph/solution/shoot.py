import argparse
import subprocess
import time
import random
import shlex
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
    hit = run('curl ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')


def call_perf(pid):
    # Убрали sudo, добавили -g для записи стеков
    return run(f'perf record -o {PERF_DATA} -p {pid} -g')


def call_flamegraph():
    # Убрали shell=True, используем пайпы как в теории
    with open("graph.svg", "w") as graph_file:
        perf_script = subprocess.Popen(
            shlex.split(f"perf script -i {PERF_DATA}"),
            stdout=subprocess.PIPE
        )
        
        stackcollapse = subprocess.Popen(
            shlex.split("./FlameGraph/stackcollapse-perf.pl"),
            stdin=perf_script.stdout,
            stdout=subprocess.PIPE
        )
        
        flamegraph = subprocess.Popen(
            shlex.split("./FlameGraph/flamegraph.pl"),
            stdin=stackcollapse.stdout,
            stdout=graph_file
        )
        
        # Ждем завершения
        perf_script.stdout.close()
        stackcollapse.stdout.close()
        flamegraph.wait()
        stackcollapse.wait()
        perf_script.wait()


def main():
    # Запускаем сервер
    server = run(start_server())
    time.sleep(0.1)  # Даем серверу время на запуск
    
    # Запускаем perf record
    perf = call_perf(server.pid)
    time.sleep(0.1)  # Даем perf время на инициализацию
    
    # Обстреливаем сервер
    make_shots()
    
    # Останавливаем perf через SIGINT (как Ctrl+C)
    perf.send_signal(signal.SIGINT)
    perf.wait()  # Ждем завершения записи
    time.sleep(0.1)
    
    # Останавливаем сервер
    stop(server)
    time.sleep(0.1)
    
    # Строим флеймграф
    call_flamegraph()
    time.sleep(1)
    
    print('Job done')
    print('Flamegraph saved to graph.svg')


if __name__ == "__main__":
    main()