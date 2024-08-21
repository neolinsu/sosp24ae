import os
import subprocess
import pandas as pd
import time

def mine(line):
    metric = ['L1-dcache-loads', 'L1-dcache-load-misses','start_time','end_time']
    for m in metric:
        if(line.find(m) != -1):
            words =line.split() 
            number = words[0]
            div = 100
            #if len(words) > 2:
            #    div = line.split()[2]
            #    div = div.replace('(', '')
            #    div = div.replace(')', '')
            #    div = div.replace('%', '')
            #    div = float(div)
                
            number=number.replace(',', '')
            return m, int(int(number)*100.0/div)
    return None

for i in range (10,110,10):    
    cflags = f'-DUNAWARE -Dobject_size=512 -Dper_app_sets=1 -Dallocate_size=64'
    os.system(f"cflags='{cflags}' bash make.sh")
    outputfile1 = f"output/memb_duration_{i}.txt"
    outputfile2 = f"output/memb_duration_bw_{i}.txt"

    handle1 = open(outputfile1, 'w')
    handle2 = open(outputfile2, 'w')
    x1=subprocess.Popen("taskset -c 27 ./cache2", shell=True, stdout=handle1, stderr=handle1)
    print(x1.pid)
    subprocess.Popen(f"echo \"MB:0={i};1={i}\" > /sys/fs/resctrl/COS1/schemata", shell=True)
    subprocess.Popen(f"echo {x1.pid} > /sys/fs/resctrl/COS1/tasks", shell=True)
    subprocess.Popen("echo 27 > /sys/fs/resctrl/COS1/cpus_list", shell=True)
    x1.wait()