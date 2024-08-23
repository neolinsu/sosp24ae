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
    # Server
    is_vessel = False
    if args["type"] == "vessel":
        server = VCluster(
            "server",
            root_path= figure_9_vessel_server_config['root_path'],
            password= figure_9_vessel_server_config['password'],
            remotehost= figure_9_vessel_server_config['remote_host'],
            iokernel_cpus= figure_9_vessel_server_config['iokernel_cpus'],
            vessel_cpus= figure_9_vessel_server_config['vessel_cpus'],
            sched_config= figure_9_vessel_server_config['sched_config'],
            nicpci= figure_9_vessel_server_config['nicpci']
        )
        app_path = figure_9_app_path['vessel']
        is_vessel = True
    else:
        server = CCluster(
            root_path= figure_9_drl_server_config['root_path'],
            password= figure_9_drl_server_config['password'],
            remotehost= figure_9_drl_server_config['remote_host'],
            iokernel_cpus= figure_9_drl_server_config['iokernel_cpus'],
            sched_config= figure_9_drl_server_config['sched_config'],
            nicpci= figure_9_drl_server_config['nicpci']
        )
        app_path = figure_9_app_path['caladan']
    # Client
    clients = {}
    for client_id in client_infos:
        client = CCluster(
            root_path= figure_9_caladan_client_config_template['root_path'],
            password= figure_9_caladan_client_config_template['password'],
            remotehost= figure_9_caladan_client_config_template['remote_host'],
            sched_config= figure_9_caladan_client_config_template['sched_config'],
            nicpci= client_infos[client_id]["nicpci"]
        )
        clients[client_id] = client
    inst_num = args
    ports = []
    for i in range(inst_num):
        ports.append(5092 + i)
    # Run LC
    if is_vessel:
        for port in ports:
            server.run_app(
                app_path[args["lc_app"]]["path"],
                f"{app_path[args['lc_app']]['config']} -p {port} {app_path[args['lc_app']]['cmd']}",
                90,
                kill_before=True
            )
    else:
        server.run_app(
            app_path[args["lc_app"]]["path"],
            f"{app_path[args['lc_app']]['config']} -p {port} {app_path[args['lc_app']]['cmd']}",
            90,
            "LC",
            kill_before=True
        )
    mpps = args["lc_bw"] / len(clients)
    protocol = "memcached" if args["lc_app"] == "memcached" else "synthetic"
    for client in clients:
        dmpps = mpps / len(ports)
        for port in ports:
            clients[client].run_app(
                figure_9_syn['path'],
                f"{figure_9_syn['config']} 192.168.1.199:{port} --protocol {protocol} --mpps={dmpps} {figure_9_syn['cmd']}",
                90,
                "SYN",
                kill_before=True
            )
    asyncio.sleep(10)
    p999 = 0
    for client in clients:
        clients[client].bashes['SYN'].expect([pexpect.TIMEOUT, 'Latencies:'], timeout=60)
        log = clients[client].bashes['SYN'].process.before.decode()
        buckets = log.split("\n")[-1]
        buckets = buckets.split(",")
        dp999 = buckets[-3]
        p999 = max(p999, float(dp999))
    return {
        "p999": "{:.4f}".format(p999) if type(p999) == float else p999,
    }
