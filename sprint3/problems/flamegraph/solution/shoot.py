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


def main():
    # Запускаем сервер
    server = run(start_server(), subprocess.DEVNULL)
    time.sleep(0.1)  # Даем серверу время на запуск
    
    # Запускаем perf record для профилирования сервера
    perf_record = run(f'perf record -o {PERF_DATA} -p {server.pid} -g')
    time.sleep(0.1)  # Даем perf время на инициализацию
    
    # Обстреливаем сервер запросами
    make_shots()
    
    # Корректно завершаем perf record (Ctrl+C = SIGINT)
    perf_record.send_signal(signal.SIGINT)
    time.sleep(0.1)  # Даем время на завершение записи
    
    # Останавливаем сервер
    stop(server)
    
    # Небольшая пауза, чтобы убедиться, что perf.data полностью записан
    time.sleep(1)
    
    # Строим флеймграф через двойной пайп
    # perf script | ./FlameGraph/stackcollapse-perf.pl | ./FlameGraph/flamegraph.pl > graph.svg
    with open("graph.svg", "w") as graph_file:
        # Первый процесс: perf script читает наши данные
        perf_script = subprocess.Popen(
            shlex.split(f"perf script -i {PERF_DATA}"), 
            stdout=subprocess.PIPE
        )
        
        # Второй процесс: stackcollapse-perf.pl сворачивает стек
        stackcollapse = subprocess.Popen(
            shlex.split("./FlameGraph/stackcollapse-perf.pl"),
            stdin=perf_script.stdout,
            stdout=subprocess.PIPE
        )
        
        # Третий процесс: flamegraph.pl генерирует SVG
        flamegraph = subprocess.Popen(
            shlex.split("./FlameGraph/flamegraph.pl"),
            stdin=stackcollapse.stdout,
            stdout=graph_file
        )
        
        # Ждем завершения всех процессов в правильном порядке
        # (закрываем пайпы, чтобы избежать дедлоков)
        perf_script.stdout.close()
        stackcollapse.stdout.close()
        
        # Ждем завершения
        stop(perf_script, True)
        stop(stackcollapse, True)
        stop(flamegraph, True)
    
    print('Job done')
    print('Flamegraph saved to graph.svg')


if __name__ == "__main__":
    main()