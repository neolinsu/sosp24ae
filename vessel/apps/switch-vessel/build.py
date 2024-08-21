import os
import subprocess
import pandas as pd
def mine(line):
    metric = ['dTLB-load-misses', 'dTLB-store-misses', 'mem_load_retired.l1_hit', 'mem_load_retired.l1_miss', 'l1d.replacement','total run time']
    for m in metric:
        if(line.find(m) != -1):
            number=line.split()[0]
            number=number.replace(',', '')
            return m,int(number)
    return None
frame = pd.DataFrame()
for obj_size in [8,16,32,64,128,256]:
    for if_aware in ['AWARE', 'UNAWARE']:
        for ways in [1,2,4]:
            series = pd.Series()
            series['if_aware'] = if_aware
            series['obj_size'] = obj_size
            series['ways'] = ways
            cflags = f'-D{if_aware} -Dobject_size={obj_size} -Dper_app_sets={ways} -Dallocate_size={max(64, obj_size)}'
            print(f"if_aware={if_aware}, obj_size={obj_size} ways={ways}")
            print(f"cflags={cflags}")
            os.system(f"cflags='{cflags}' bash make.sh")
            exit()
frame.to_csv('result.csv')
