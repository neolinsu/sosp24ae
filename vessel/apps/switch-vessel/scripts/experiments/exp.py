#!/usr/bin/env python3
import argparse
import os

from utils import print_info

def parse_args(): #TODO
    parser = argparse.ArgumentParser(
                    prog = os.path.basename(__file__),
                    description = f"Run program in {__file__}")
    parser.add_argument('-v', '--verbose', action='store_true', default=False)
    parser.add_argument('-r', '--root-path', type=str, default='./')
    parser.add_argument('-p', '--password', type=str, default=None)
    parser.add_argument('cpus', type=str, default=None)
    parser.add_argument('nicpci', type=str, default=None)
    parser.add_argument('config', type=str, default=None)
    parser.add_argument('args', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    # 
    args.cpus = eval(args.cpus)
    args.iokernel_cpus = f"{args.cpus[0]}-{args.cpus[1]},{args.cpus[2]}-{args.cpus[3]}"
    args.vessel_cpus = f"{args.cpus[0]+1}-{args.cpus[1]},{args.cpus[2]+1}-{args.cpus[3]}"
    return args

if __name__ == '__main__':
    args = parse_args()
    print_info(args)
    
