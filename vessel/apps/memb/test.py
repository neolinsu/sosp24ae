import subprocess
outputfile2 = f"output/test{i}.txt"
handle2 = open(outputfile2, 'w')

subprocess.Popen(f"sudo pqos -I -p 'mbl:805206;mbr:805206'",
    shell=True,
    stdout=handle1,
    stderr=handle1)
    
