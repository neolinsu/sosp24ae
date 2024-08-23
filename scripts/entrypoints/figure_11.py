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
    # TODO wait for caladan running
    server = LCluster(
        figure_11_linux_server_config['root_path'],
        figure_11_linux_server_config['password'],
        figure_11_linux_server_config['remote_host'],
        "")
    if args[type] == 'vessel':
        server.run_app(
            'cd /home/sosp24ae/sosp24-ae/vessel/apps/cache-vessel && python ./scripts/vessel_cache.py ./scripts/cache/config.yaml ./',
            "",
            90,
            "CACHE",
            kill_before=False)
    else:
        server.run_app(
            'cd /home/sosp24ae/sosp24-ae/vessel/apps/cache && python ./scripts/build.py',
            "",
            90,
            "CACHE",
            kill_before=False)
    asyncio.sleep(10*60)
    if args[type] == 'vessel':
        server.run_app(
            'cat /home/sosp24ae/sosp24-ae/vessel/apps/cache-vessel/result.csv',
            "",
            90,
            "RES",
            kill_before=False)
    else:
        server.run_app(
            'cat /home/sosp24ae/sosp24-ae/vessel/apps/cache/result.csv',
            "",
            90,
            "RES",
            kill_before=False)
    server.bashes['RES'].expect([pexpect.TIMEOUT, 'sosp24ae@'], timeout=60)
    server.bashes['RES'].expect([pexpect.TIMEOUT, 'sosp24ae@'], timeout=60)
    server.bashes['RES'].expect([pexpect.TIMEOUT, 'sosp24ae@'], timeout=60)
    log = server.bashes['RES'].process.before.decode()
    lines = log.split("\n")
    res = {
        16: {},
        32: {},
        64: {},
        128: {},
        256: {},
    }
    for d in res:
        res[d] = {"l1-loads": 0, "l1-load-misses": 0, "duration": 0}

    assert len(lines) == 5
    # l1-loads
    elems = lines[0].strip().split(",")
    index = 0
    for d in res:
        res[d]["l1-loads"] = elems[2 + index]
        index += 2
    # l1-load-misses
    elems = lines[1].strip().split(",")
    index = 0
    for d in res:
        res[d]["l1-load-misses"] = elems[2 + index]
        index += 2
    # duration
    elems = lines[4].strip().split(",")
    index = 0
    for d in res:
        res[d]["duration"] = elems[2 + index]
        index += 2
    return res[args["object_size"]]
