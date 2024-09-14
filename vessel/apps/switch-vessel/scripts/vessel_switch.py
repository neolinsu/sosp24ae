#!/usr/bin/env python3
import argparse
from easydict import EasyDict as edict
import yaml
import os
import numpy as np
from experiments.cluster import VCluster
from experiments.utils import print_info
import json
import pandas as pd

def get_memcached_client_cmd(config_path:str, ):
    return f"{config_path} "

ccswriter = None
csvfile = None
def parse_args(): #TODO
    global ccswriter
    global csvfile
    parser = argparse.ArgumentParser(
                    prog = os.path.basename(__file__),
                    description = f"Run program in {__file__}")
    parser.add_argument('-v', '--verbose', action='store_true', default=False)
    parser.add_argument('config_path', type=str, default=None)
    args = parser.parse_args()
    file = open(args.config_path, 'r', encoding="utf-8")
    file_data = file.read()
    file.close()
    data = yaml.load(file_data, Loader=yaml.Loader)
    data = edict(data)
    return data


def test_once(args):

    server_args = args.server[0]
    server_vcluster = VCluster(
        server_args.vessel_name,
        server_args.root_path,
        server_args.password,
        None,
        server_args.iokernel_cpus,
        server_args.vessel_cpus,
        server_args.sched_config,
        server_args.nicpci)

    server_vcluster.run_app(
        server_args.cache2_path,
        f"{server_args.cache2_config_path}",
        90)
    server_vcluster.run_app(
        server_args.cache_path,
        f"{server_args.cache_config_path}",
        90)

    res = server_vcluster.gather_info(1, isfirst=False)
    print(res)
    return {}


def testbed(args):
    frame = pd.DataFrame()
    for obj_size in [8]:
        for if_aware in ['AWARE']:
            for ways in [1]:
                series = pd.Series()
                series['if_aware'] = if_aware
                series['obj_size'] = obj_size
                series['ways'] = ways
                cflags = f'-D{if_aware} -Dobject_size={obj_size} -Dper_app_sets={ways} -Dallocate_size={max(64, obj_size)}'
                print(f"if_aware={if_aware}, obj_size={obj_size} ways={ways}")
                print(f"cflags={cflags}")
                os.system(f"cflags='{cflags}' bash make.sh")
                res = test_once(args)
                break
    res = []
    with open('res.csv', 'r') as f:
        for line in f.readlines():
            line = line.split(',')[0]
            data = int(line)
            res.append(data)
    data = np.array(res[1:])
    mean = np.mean(data)
    p50 = np.percentile(data, 50)
    p90 = np.percentile(data, 90)
    p99 = np.percentile(data, 99)
    p999 = np.percentile(data, 99.9)
    res_dict = {"mean": mean, "p50": p50, "p90": p90, "p99": p99, "p999": p999}
    print(json.dumps(res_dict), end='RES_END')

if __name__ == '__main__':
    args = parse_args()
    print_info(args)
    testbed(args)
