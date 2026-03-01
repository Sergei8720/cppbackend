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


def start_server():
    parser = argparse.ArgumentParser()
    # Используем nargs=argparse.REMAINDER чтобы собрать все аргументы с пробелами
    parser.add_argument('server', type=str, nargs=argparse.REMAINDER)
    args = parser.parse_args()
    return ' '.join(args.server) if args.server else None


def perf_record_of(pid):
    return f"perf record -p {pid} -o perf.data -g -- sleep 10"
    # Добавили -- sleep 10 чтобы perf сам завершился через 10 секунд


def run(command, output=None, shell=False):
    if shell:
        process = subprocess.Popen(command, stdout=output, stderr=subprocess.DEVNULL, shell=True)
    else:
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


def find_flamegraph_dir():
    """Ищет папку FlameGraph в разных местах"""
    # Текущая директория
    current_dir = os.path.dirname(os.path.abspath(__file__))
    
    # Возможные пути
    possible_paths = [
        './FlameGraph',  # как в оригинале
        '../FlameGraph',
        os.path.join(current_dir, 'FlameGraph'),
        os.path.join(os.path.dirname(current_dir), 'FlameGraph'),
        '/home/runner/work/cppbackend/cppbackend/FlameGraph',  # для CI
        'FlameGraph'
    ]
    
    for path in possible_paths:
        if os.path.exists(path):
            return path
    return './FlameGraph'  # возвращаем оригинал если ничего не нашли


def main():
    server_cmd = start_server()
    if not server_cmd:
        print("Error: Server command not provided")
        return
    
    print(f"Starting server: {server_cmd}")
    
    # Запускаем сервер (shell=True для команд с пробелами)
    server = run(server_cmd, subprocess.DEVNULL, shell=True)
    time.sleep(0.5)  # увеличил задержку
    
    print(f"Server PID: {server.pid}")
    
    # Запускаем perf record
    perf_record = run(perf_record_of(server.pid))
    time.sleep(0.5)  # увеличил задержку
    
    # Обстреливаем сервер
    make_shots()
    
    # Останавливаем perf record
    perf_record.send_signal(signal.SIGINT)
    perf_record.wait()  # ждем завершения perf
    
    # Останавливаем сервер
    time.sleep(0.1)
    stop(server)
    time.sleep(1)
    
    # Генерируем флеймграф
    flamegraph_dir = find_flamegraph_dir()
    print(f"Using FlameGraph from: {flamegraph_dir}")
    
    stackcollapse = os.path.join(flamegraph_dir, 'stackcollapse-perf.pl')
    flamegraph_pl = os.path.join(flamegraph_dir, 'flamegraph.pl')
    
    if os.path.exists('perf.data') and os.path.getsize('perf.data') > 0:
        with open("graph.svg", "w") as graph_file:
            perf_script = subprocess.Popen(
                shlex.split("perf script -i perf.data"), 
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL
            )
            
            flamegraph_stackcollapse = subprocess.Popen(
                ['perl', stackcollapse],
                stdin=perf_script.stdout, 
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL
            )
            
            flamegraph_output = subprocess.Popen(
                ['perl', flamegraph_pl],
                stdin=flamegraph_stackcollapse.stdout, 
                stdout=graph_file,
                stderr=subprocess.DEVNULL
            )
            
            # Ждем завершения пайплайна
            flamegraph_output.communicate()
            
            # Закрываем процессы
            perf_script.stdout.close()
            flamegraph_stackcollapse.stdout.close()
            
            print("FlameGraph generated: graph.svg")
            
            # Проверяем наличие RequestHandler
            with open('graph.svg', 'r') as f:
                content = f.read()
                if 'RequestHandler' in content:
                    print("✓ RequestHandler found")
                else:
                    print("✗ RequestHandler not found")
    else:
        print("Error: perf.data is empty or does not exist")
    
    print('Job done')


if __name__ == "__main__":
    main()