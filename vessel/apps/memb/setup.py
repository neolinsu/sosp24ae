import subprocess
for i in range (10,110,10): 
    # subprocess.Popen(f"sudo mkdir /sys/fs/cgroup/t{i}", shell=True)
    # echo '+cpuset +cpu +io +memory +pids' > /sys/fs/cgroup/cgroup.subtree_control
    subprocess.Popen(f"echo '+cpuset +cpu +io +memory +pids' > /sys/fs/cgroup/cgroup.subtree_control", shell=True)
    subprocess.Popen(f"echo {i*1000} 100000  > /sys/fs/cgroup/t{i}/cpu.max", shell=True)
