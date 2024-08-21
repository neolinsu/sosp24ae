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

max_bandwidth = 0.14955451561798175
print(f"Max bandwidth: {max_bandwidth}")
for i in range (10,110,10):
    cflags = f'-DUNAWARE -Dobject_size=1024 -Dper_app_sets=1 -Dallocate_size=64'
    os.system(f"cflags='{cflags}' bash make.sh")
    outputfile1 = f"output_gsw/linux_memb_duration_{i}.txt"

    handle1 = open(outputfile1, 'w')
    with open("bandwidth.config", "w") as f:
        f.write(f"{i}")
    veioerneld = subprocess.Popen("veiokerneld simple 1-17,21-37 nicpci 0000:16:00.0 2>&1", shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    app1=subprocess.Popen("taskset -c 27 ./cache2", shell=True, stdout=handle1, stderr=handle1)
    # x2 = subprocess.Popen(f"pqos -I -p 'mbl:{x1.pid};mbr:{x1.pid}'",
    #     shell=True,
    #     stdout=handle2,
    #     stderr=handle2)
    print(f"Test on app with {i}% bandwidth")
    # subprocess.Popen(f"echo {app1.pid} > /sys/fs/cgroup/t{i}/cgroup.procs", shell=True)
    app1.wait()
    app1_bandwidth = get_bandwidth(i)
    print(f"Threshold:{i}% App bandwidth: {app1_bandwidth/max_bandwidth*100}%")
    os.system("rm bandwidth.config")
# for i in range (10,110, 10):
#     print(f"Targe bandwidth percent:{i}%, Tested bandwidth:{get_bandwidth(i)*100/max_bandwidth}%")
#     # x2.kill()
    

