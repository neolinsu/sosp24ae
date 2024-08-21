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
frame = pd.DataFrame()
for obj_size in [8,16,32,64,128,256,512,1024]:
    for if_aware in ['AWARE', 'UNAWARE']:
        for ways in [1]:
            series = pd.Series()
            series['if_aware'] = if_aware
            series['obj_size'] = obj_size
            series['ways'] = ways
            cflags = f'-D{if_aware} -Dobject_size={obj_size} -Dper_app_sets={ways} -Dallocate_size={max(64, obj_size)}'
            print(f"if_aware={if_aware}, obj_size={obj_size} ways={ways}")
            os.system(f"cflags='{cflags}' bash make.sh")
            outputfile1 = f"output/{if_aware}_{obj_size}_{ways}_cache2.txt"
            outputfile2 = f"output/{if_aware}_{obj_size}_{ways}_cache.txt"
            perfoutputfile1 = f"output/{if_aware}_{obj_size}_{ways}_cache2_perf.txt"
            perfoutputfile2 = f"output/{if_aware}_{obj_size}_{ways}_cache_perf.txt"
            handle1 = open(outputfile1, 'w')
            handle2 = open(outputfile2, 'w')
            phandle1 = open(perfoutputfile1, 'w')
            phandle2 = open(perfoutputfile2, 'w')
            x1=subprocess.Popen("taskset -c 3 ./cache2", shell=True, stdout=handle1, stderr=handle1)
            x2=subprocess.Popen("taskset -c 3 ./cache", shell=True, stdout=handle2, stderr=handle2)
            time.sleep(1.5)
            m1 = subprocess.Popen(f"perf stat -e L1-dcache-loads,L1-dcache-load-misses -C 3 --timeout 2000", shell=True, stdout=phandle1, stderr=phandle1);
            #m2 = subprocess.Popen(f"perf stat -e mem_load_retired.l1_hit,mem_load_retired.l1_miss,l1d.replacement -C 3 --timeout 1000", shell=True, stdout=phandle2, stderr=phandle2);
            x1.wait()
            x2.wait()
            m1.wait()
            #m2.wait()
            handle1.close()
            handle2.close()
            phandle1.close()
            #phandle2.close()
            handle1 = open(outputfile1, 'r')
            handle2 = open(outputfile2, 'r')
            phandle1 = open(perfoutputfile1, 'r')
            #phandle2 = open(perfoutputfile2, 'r')
            dict1={}
            dict2={}
            for line in handle1.readlines():
                result = mine(line)
                if(result != None):
                    dict1[result[0]]=result[1]
            for line in handle2.readlines():
                result = mine(line)
                if(result != None):
                    dict2[result[0]]=result[1]
            
            for line in phandle1.readlines():
                print(line)
                result = mine(line)
                if(result != None):
                    dict1[result[0]]=result[1]
            #for line in phandle2.readlines():
            #    result = mine(line)
            #    if(result != None):
            #        dict2[result[0]]=result[1]
            #for 'mem_load_retired.l1_hit', 'mem_load_retired.l1_miss', 'l1d.replacement' use add to aggreate
            #for start_time use min to aggreate for end_time
            print(dict1)
            for key in ['L1-dcache-loads', 'L1-dcache-load-misses']:
                series[key] = dict1[key] #+ dict2[key]
            series['start_time'] = min(dict1['start_time'], dict2['start_time'])
            series['end_time'] = max(dict1['end_time'], dict2['end_time'])
            series['duration'] = series['end_time'] - series['start_time']
            frame = pd.concat([frame, series], axis=1)
frame.to_csv('result.csv')
