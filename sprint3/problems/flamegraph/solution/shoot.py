# Минимальные изменения для работоспособности
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
    parser.add_argument('server', type=str, nargs=argparse.REMAINDER)
    args = parser.parse_args()
    return ' '.join(args.server) if args.server else None


def perf_record_of(pid):
    return f"perf record -p {pid} -o perf.data -g"


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


# Основной код с минимальными изменениями
server_cmd = start_server()
if not server_cmd:
    print("Error: Server command not provided")
    exit(1)

server = run(server_cmd, subprocess.DEVNULL, shell=True)
time.sleep(0.5)

perf_record = run(perf_record_of(server.pid))
time.sleep(0.5)

make_shots()

perf_record.send_signal(signal.SIGINT)
perf_record.wait()

time.sleep(0.1)
stop(server)
time.sleep(1)

# Ищем FlameGraph
flamegraph_dir = './FlameGraph'
if not os.path.exists(flamegraph_dir):
    # Пробуем другие пути
    for path in ['../FlameGraph', '/home/runner/work/cppbackend/cppbackend/FlameGraph']:
        if os.path.exists(path):
            flamegraph_dir = path
            break

stackcollapse = os.path.join(flamegraph_dir, 'stackcollapse-perf.pl')
flamegraph_pl = os.path.join(flamegraph_dir, 'flamegraph.pl')

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
    
    flamegraph_output.communicate()

print('Job done')