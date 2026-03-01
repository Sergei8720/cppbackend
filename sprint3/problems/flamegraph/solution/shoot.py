import argparse
import subprocess
import time
import random
import shlex
import signal
import os
import shutil

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


def rebuild_server_with_correct_abi():
    """Пересобирает сервер с правильным ABI и компиляцией jsoncpp"""
    print("=" * 70)
    print("ПЕРЕСБОРКА СЕРВЕРА С ПРАВИЛЬНЫМ ABI")
    print("=" * 70)
    
    # Очищаем кэш Conan для jsoncpp
    subprocess.run(['conan', 'remove', 'jsoncpp/1.9.6', '-f'], 
                  stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)
    
    # Пути
    script_dir = os.path.dirname(os.path.abspath(__file__))
    solution_dir = os.path.abspath(os.path.join(
        script_dir, '../../sprint1/problems/map_json/solution'))
    
    # Создаём директорию для сборки
    build_dir = os.path.join(solution_dir, 'build_fixed')
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
    os.makedirs(build_dir)
    
    # Устанавливаем зависимости с ПРИНУДИТЕЛЬНОЙ компиляцией jsoncpp
    print("\n📦 Установка зависимостей...")
    subprocess.run([
        'conan', 'install', solution_dir,
        '--build=missing',
        '--build=jsoncpp',  # КЛЮЧЕВОЙ МОМЕНТ!
        '-s', 'compiler=gcc',
        '-s', 'compiler.version=11',
        '-s', 'compiler.libcxx=libstdc++11',
        '-s', 'build_type=Release'
    ], cwd=build_dir, check=True)
    
    # Конфигурация CMake
    print("\n⚙️  Конфигурация CMake...")
    subprocess.run([
        'cmake', solution_dir,
        '-DCMAKE_BUILD_TYPE=Release'
    ], cwd=build_dir, check=True)
    
    # Сборка
    print("\n🔨 Сборка...")
    subprocess.run(['cmake', '--build', '.', '-j2'], 
                  cwd=build_dir, check=True)
    
    server_path = os.path.join(build_dir, 'bin', 'game_server')
    if os.path.exists(server_path):
        print(f"\n✅ Сервер собран: {server_path}")
        return server_path
    
    return None


def run_command(command, output=None):
    process = subprocess.Popen(shlex.split(command), stdout=output, stderr=subprocess.DEVNULL)
    return process


def stop_process(process, wait=False):
    if process.poll() is None and wait:
        process.wait()
    process.terminate()


def shoot(ammo):
    hit = run_command('curl -s ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop_process(hit, wait=True)


def make_shots():
    for i in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
        if (i + 1) % 10 == 0:
            print(f"  Прогресс: {i + 1}/{SHOOT_COUNT}")
    print('✅ Стрельба завершена')


# --- main logic ---

# Игнорируем аргумент командной строки - будем использовать свой сервер
parser = argparse.ArgumentParser()
parser.add_argument('server', type=str, nargs='?', default='')
args = parser.parse_args()

# Пересобираем сервер с правильным ABI
server_path = rebuild_server_with_correct_abi()
if not server_path:
    print("❌ Не удалось собрать сервер")
    exit(1)

# Запускаем сервер
server = run_command(server_path, subprocess.DEVNULL)
time.sleep(1)

# Проверяем, что сервер работает
try:
    subprocess.run(['curl', '-f', 'http://localhost:8080/api/v1/maps'], 
                  capture_output=True, timeout=2)
    print("✅ Сервер отвечает")
except:
    print("❌ Сервер не отвечает")
    stop_process(server)
    exit(1)

# Запускаем perf
perf = run_command(f'perf record -p {server.pid} -o perf.data -gF 99')

# Делаем запросы
make_shots()

# Останавливаем perf
perf.send_signal(signal.SIGINT)
time.sleep(1)

# Останавливаем сервер
stop_process(server, wait=True)
time.sleep(1)

# Генерируем flamegraph
with open("graph.svg", "w") as graph_file:
    perf_script = subprocess.Popen(shlex.split("perf script -i perf.data"), stdout=subprocess.PIPE)
    flamegraph_stackcollapse = subprocess.Popen(shlex.split("./FlameGraph/stackcollapse-perf.pl"), 
                                              stdin=perf_script.stdout, stdout=subprocess.PIPE)
    flamegraph_output = subprocess.Popen(shlex.split("./FlameGraph/flamegraph.pl"), 
                                       stdin=flamegraph_stackcollapse.stdout, stdout=graph_file)
    flamegraph_output.wait()

print('✅ Job done')