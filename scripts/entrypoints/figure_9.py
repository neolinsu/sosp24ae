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
        server_lcluster = LCluster(
            figure_9_vessel_server_config['root_path'],
            figure_9_vessel_server_config['password'],
            figure_9_vessel_server_config['remote_host'],
            "")
        app_path = figure_9_app_path['vessel']
        is_vessel = True
    elif args["type"] == "dr-l":
        server = CCluster(
            root_path= figure_9_drl_server_config['root_path'],
            password= figure_9_drl_server_config['password'],
            remotehost= figure_9_drl_server_config['remote_host'],
            iokernel_cpus= figure_9_drl_server_config['iokernel_cpus'],
            sched_config= figure_9_drl_server_config['sched_config'],
            nicpci= figure_9_drl_server_config['nicpci']
        )
        server_lcluster = LCluster(
            figure_9_drl_server_config['root_path'],
            figure_9_drl_server_config['password'],
            figure_9_drl_server_config['remote_host'],
            "")
        app_path = figure_9_app_path['caladan']
    elif args["type"] == "dr-h":
        server = CCluster(
            root_path= figure_9_drh_server_config['root_path'],
            password= figure_9_drh_server_config['password'],
            remotehost= figure_9_drh_server_config['remote_host'],
            iokernel_cpus= figure_9_drh_server_config['iokernel_cpus'],
            sched_config= figure_9_drh_server_config['sched_config'],
            nicpci= figure_9_drh_server_config['nicpci']
        )
        server_lcluster = LCluster(
            figure_9_caladan_server_config['root_path'],
            figure_9_caladan_server_config['password'],
            figure_9_caladan_server_config['remote_host'],
            "")
        app_path = figure_9_app_path['caladan']
    else:
        server = CCluster(
            root_path=figure_9_caladan_server_config['root_path'],
            password= figure_9_caladan_server_config['password'],
            remotehost= figure_9_caladan_server_config['remote_host'],
            iokernel_cpus= figure_9_caladan_server_config['iokernel_cpus'],
            sched_config= figure_9_caladan_server_config['sched_config'],
            nicpci= figure_9_caladan_server_config['nicpci']
        )
        server_lcluster = LCluster(
            figure_9_caladan_server_config['root_path'],
            figure_9_caladan_server_config['password'],
            figure_9_caladan_server_config['remote_host'],
            "")
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
    # Run LC
    if is_vessel:
        server.run_app(
            app_path[args["lc_app"]]["path"],
            f"{app_path[args['lc_app']]['config']} {app_path[args['lc_app']]['cmd']}",
            90,
            kill_before=True
        )
    elif args["type"] == "dr-l":
        server.run_app(
            app_path[args["lc_app"]]["path"],
            f"/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/drl_lc.config {app_path[args['lc_app']]['cmd']}",
            90,
            "LC",
            kill_before=True
        )
    elif args["type"] == "dr-h":
        server.run_app(
            app_path[args["lc_app"]]["path"],
            f"/home/sosp24ae/sosp24-ae/caladan-ae/caladan/apps/drh_lc.config {app_path[args['lc_app']]['cmd']}",
            90,
            "LC",
            kill_before=True
        )
    # Run BE
    if is_vessel:
        server.run_app(
            app_path['linpack']["path"],
            f"{app_path['linpack']['config']} {app_path['linpack']['cmd']}",
            90,
            kill_before=True
        )
        server.vessel_bash.wait_str("tls->id: 31")
    else:
        server.run_app(
            app_path['linpack']["path"],
            f"{app_path[args['linpack']]['config']} {app_path['linpack']['cmd']}",
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
            figure_9_syn['path'],
            f"{figure_9_syn['config']} --protocol {protocol} --mpps={mpps} {figure_9_syn['cmd']}",
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
