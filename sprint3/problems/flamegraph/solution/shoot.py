#!/usr/bin/env python3
import argparse
import subprocess
import time
import random
import signal
import os
import sys
from pathlib import Path

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

def stop(perf_run, wait=False):
    if process.poll() is None and wait:
        process.wait()
    process.terminate()


def stop_server(process):
    try:
        process.send_signal(signal.SIGINT)
        process.wait(timeout=2)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()

def shoot(ammo):
    hit = run(f'curl -s "{ammo}"', output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)

def make_shots():
    for  in range(SHOOTCOUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')

server_command = start_server()
server = run(server_command, output=subprocess.DEVNULL)

time.sleep(2)

perf_command = f'perf record -g -p {server.pid} -o perf.data --call-graph dwarf'
perf_run = run(perf_command)

make_shots()

stop(perf_run, wait=True)
print('Perf stop')
stop_server(server)
print('Server stop')
time.sleep(1)

print("Processing perf data...")

p1 = subprocess.Popen([
    'perf', 'script',
    '-i', 'perf.data'
], stdout=subprocess.PIPE)

p2 = subprocess.Popen([
    './FlameGraph/stackcollapse-perf.pl'
], stdin=p1.stdout, stdout=subprocess.PIPE)

with open('graph.svg', 'w', encoding='utf-8') as out_file:
    p3 = subprocess.Popen([
        './FlameGraph/flamegraph.pl',
        '--title', 'Flame Graph',
        '--width', '1600',
        '--colors', 'java'
    ], stdin=p2.stdout, stdout=out_file)

p1.stdout.close()
p2.stdout.close()
p3.wait()

print('Job done')