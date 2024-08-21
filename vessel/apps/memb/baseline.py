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

def get_bandwidth(i):
    f = open(f"output_gsw/linux_memb_duration_{i}.txt")
    idx = 0
    ans = [0] * 2
    for line in f:
        ans[idx] = int(line.split()[0])
        idx += 1
    return 1/((ans[1] - ans[0]) / (2100 * 1000 * 1000))
max_bandwidth = 0.18102396585918852
for i in range (40,110,20):   
    os.system("killall veiokerneld 2>/dev/null")
    cflags = f'-DUNAWARE -Dobject_size=1024 -Dper_app_sets=1 -Dallocate_size=64'
    os.system(f"cflags='{cflags}' bash make.sh")
    outputfile1 = f"output_gsw/linux_memb_duration_{i}.txt"
    outputfile2 = f"output_gsw/linux_memb_duration_bw_{i}.txt"
    handle1 = open(outputfile1, 'w')
    handle2 = open(outputfile2, 'w')
    app1=subprocess.Popen("taskset -c 27 ./cache2", shell=True, stdout=handle1, stderr=handle1)
    
    x2 = subprocess.Popen(f"pqos -I -p 'mbl:{app1.pid};mbr:{app1.pid}'",
        shell=True,
        stdout=handle2,
        stderr=handle2)
    print(f"Test on app with {i}% bandwidth")
    subprocess.Popen(f"echo {app1.pid} > /sys/fs/cgroup/t{i}/cgroup.procs", shell=True)
    app1.wait()
    app1_bandwidth = get_bandwidth(i)
    if(i == 100):
        max_bandwidth = app1_bandwidth + 0.003
    print(f"Threshold:{i}% App bandwidth: {app1_bandwidth/max_bandwidth*100}%")
# for i in range (10,110, 10):
#     print(f"Targe bandwidth percent:{i}%, Tested bandwidth:{get_bandwidth(i)*100/max_bandwidth}%")
    x2.kill()
    

