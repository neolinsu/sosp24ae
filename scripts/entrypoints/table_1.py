import pickle
import numpy as np
import sys
import asyncio
import pexpect
from ctl_plane.terminal.bash import Bash
from ctl_plane.terminal.cluster import VCluster, CCluster, LCluster

from infos import *
import time

async def run_once(args):
    server = LCluster(
        figure_11_linux_server_config['root_path'],
        figure_11_linux_server_config['password'],
        figure_11_linux_server_config['remote_host'],
        "")
    if args[type] == 'vessel':
        server.run_app(
            'cd /home/sosp24ae/sosp24-ae/vessel/apps/switch-vessel && python ./scripts/vessel_cache.py ./scripts/cache/config.yaml ./',
            "",
            90,
            "CACHE",
            kill_before=False)
    else:
        server.run_app(
            'cd /home/sosp24ae/sosp24-ae/vessel/apps/switch && python ./scripts/build.py',
            "",
            90,
            "CACHE",
            kill_before=False)
    asyncio.sleep(10*60)
    if args[type] == 'vessel':
        server.run_app(
            'cat /home/sosp24ae/sosp24-ae/vessel/apps/switch-vessel/result.csv',
            "",
            90,
            "RES",
            kill_before=False)
    else:
        server.run_app(
            'cat /home/sosp24ae/sosp24-ae/vessel/apps/switch/result.csv',
            "",
            90,
            "RES",
            kill_before=False)
    server.bashes['RES'].expect([pexpect.TIMEOUT, 'sosp24ae@'], timeout=60)
    server.bashes['RES'].expect([pexpect.TIMEOUT, 'sosp24ae@'], timeout=60)
    server.bashes['RES'].expect([pexpect.TIMEOUT, 'sosp24ae@'], timeout=60)
    log = server.bashes['RES'].process.before.decode()
    lat_queue = log.split("\n")[10:]
    data = np.array(lat_queue)
    mean = np.mean(data)
    p50 = np.percentile(data, 50)
    p90 = np.percentile(data, 90)
    p99 = np.percentile(data, 99)
    p999 = np.percentile(data, 99.9)
    return {
        "mean": mean,
        "p50": p50,
        "p90": p90,
        "p99": p99,
        "p999": p999,
    }
