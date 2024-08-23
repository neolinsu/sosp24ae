import pickle
import numpy as np
import sys
import asyncio
import pexpect
from ctl_plane.terminal.bash import Bash
from ctl_plane.terminal.cluster import VCluster, CCluster, LCluster

from infos import *
import time


async def run_once_a(args):
    # Server
    is_vessel = False
    if args["type"] == "vessel":
        server = VCluster(
            "server",
            root_path= figure_12_vessel_server_config['root_path'],
            password= figure_12_vessel_server_config['password'],
            remotehost= figure_12_vessel_server_config['remote_host'],
            iokernel_cpus= figure_12_vessel_server_config['iokernel_cpus'],
            vessel_cpus= figure_12_vessel_server_config['vessel_cpus'],
            sched_config= figure_12_vessel_server_config['sched_config'],
            nicpci= figure_12_vessel_server_config['nicpci']
        )
        server_lcluster = LCluster(
            figure_12_vessel_server_config['root_path'],
            figure_12_vessel_server_config['password'],
            figure_12_vessel_server_config['remote_host'],
            "")
        app_path = figure_12_app_path['vessel']
        is_vessel = True
    else:
        server = CCluster(
            root_path=figure_12_caladan_server_config['root_path'],
            password= figure_12_caladan_server_config['password'],
            remotehost= figure_12_caladan_server_config['remote_host'],
            iokernel_cpus= figure_12_caladan_server_config['iokernel_cpus'],
            sched_config= figure_12_caladan_server_config['sched_config'],
            nicpci= figure_12_caladan_server_config['nicpci']
        )
        server_lcluster = LCluster(
            figure_12_caladan_server_config['root_path'],
            figure_12_caladan_server_config['password'],
            figure_12_caladan_server_config['remote_host'],
            "")
        app_path = figure_12_app_path['caladan']
    # Client
    clients = {}
    for client_id in client_infos:
        client = CCluster(
            root_path= figure_12_caladan_client_config_template['root_path'],
            password= figure_12_caladan_client_config_template['password'],
            remotehost= figure_12_caladan_client_config_template['remote_host'],
            sched_config= figure_12_caladan_client_config_template['sched_config'],
            nicpci= client_infos[client_id]["nicpci"]
        )
        clients[client_id] = client
    # Run LC
    if is_vessel:
        server.run_app(
            app_path[args["lc_app"]]["path"],
            f"{app_path[args['lc_app']]['config']} {app_path[args['lc_app']]['cmd']}",
            90,
            kill_before=True
        )
    else:
        server.run_app(
            app_path[args["lc_app"]]["path"],
            f"{app_path[args['lc_app']]['config']} {app_path[args['lc_app']]['cmd']}",
            90,
            "LC",
            kill_before=True
        )
    # Run BE
    if is_vessel:
        server.run_app(
            app_path['membench']["path"],
            f"{app_path['membench']['config']} {app_path['membench']['cmd']}",
            90,
            kill_before=True
        )
        server.vessel_bash.wait_str("tls->id: 31")
    else:
        server.run_app(
            app_path['membench']["path"],
            f"{app_path[args['membench']]['config']} {app_path['membench']['cmd']}",
            90,
            "BE",
            kill_before=True
        )
        server.bashes['BE'].wait_str("tls->id: 31")
    server_lcluster.run_app(
        app_path['gather']["path"],
        app_path['gather']['cmd'],
        90,
        "PROBE",
        kill_before=True
    )
    mpps = args["lc_bw"] / len(clients)
    protocol = "memcached" if args["lc_app"] == "memcached" else "synthetic"
    for client in clients:
        clients[client].run_app(
            figure_12_syn['path'],
            f"{figure_12_syn['config']} 192.168.2.199:5092 --protocol {protocol} --mpps={mpps} {figure_12_syn['cmd']}",
            90,
            "SYN",
            kill_before=True
        )
    asyncio.sleep(10)
    curtime = time.time()
    def check_time(curdic):
        resdic = curdic['val']
        if not 'time' in resdic:
            raise RuntimeError("Need time")
        if float(resdic['time']) > curtime:
            return True
        else:
            return False
    be_res = server_lcluster.gather_info('PROBE', 10, check_time)
    bw = 0
    for i in range(1,5):
        bw += be_res[i]['val']['bandwidth']
    bw /= 4
    p999 = 0
    for client in clients:
        clients[client].bashes['SYN'].expect([pexpect.TIMEOUT, 'Latencies:'], timeout=60)
        log = clients[client].bashes['SYN'].process.before.decode()
        buckets = log.split("\n")[-1]
        buckets = buckets.split(",")
        dp999 = buckets[-3]
        p999 = max(p999, float(dp999))
    return {
        "bw": "{:.4f}".format(bw),
        "p999": "{:.4f}".format(p999) if type(p999) == float else p999,
    }
async def run_once(args):
    if args["op"] == "a":
        return await run_once_a(args)
    else:
        return {"bw": 0}
