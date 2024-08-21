from commons import run_item

url = "http://10.0.2.144:8080/figure/10"

global CNT


def round(args, lc_bw_list):
    res = []
    for i in lc_bw_list:
        args["lc_bw"] = i
        res_item = run_item(url, args)
        res_item["lc_bw"] = i
        res.append(res_item)

    return res


print("### 1 Instance")
lc_bw_list = [
    0.1,
    0.15,
    0.2,
    0.25,
    0.3,
    0.35,
    0.4,
    0.45,
    0.5,
    0.55,
    0.6,
    0.65,
    0.7,
    0.725,
    0.75,
    0.76,
    0.77,
    0.79,
    0.8,
    0.81,
    0.83,
    0.84,
    0.85,
    0.86,
    0.87,
]
args = {"type": "dr-l", "lc_app": "memcached", "lc_bw": 0, "inst_num": 1}
res = round(args, lc_bw_list)
print("dr-l")
print("|lc_bw\t|p999\t| \n|---|---|---|")
for line in res:
    print(f"|{line['lc_bw']}\t|{line['p999']}\t|")

args = {"type": "vessel", "lc_app": "memcached", "lc_bw": 0, "inst_num": 1}
res = round(args, lc_bw_list)
print("vessel")
print("|lc_bw\t| p999\t| \n|---|---|---|")
for line in res:
    print(f"|{line['lc_bw']}\t|{line['p999']}\t|")

print("### 4 Instance")
lc_bw_list = [
    0.1,
    0.15,
    0.2,
    0.25,
    0.3,
    0.35,
    0.4,
    0.45,
    0.5,
    0.55,
    0.6,
    0.65,
    0.665,
    0.68,
    0.7,
    0.75,
    0.76,
    0.77,
    0.79,
    0.8,
    0.81,
    0.83,
    0.84,
    0.85,
    0.86,
]
args = {"type": "dr-l", "lc_app": "memcached", "lc_bw": 0, "inst_num": 4}
res = round(args, lc_bw_list)
print("dr-l")
print("|lc_bw\t| p999\t| \n|---|---|---|")
for line in res:
    print(f"|{line['lc_bw']}\t|{line['p999']}\t|")

args = {"type": "vessel", "lc_app": "memcached", "lc_bw": 0, "inst_num": 4}
res = round(args, lc_bw_list)
print("vessel")
print("|lc_bw\t| p999\t| \n|---|---|---|")
for line in res:
    print(f"|{line['lc_bw']}\t|{line['p999']}\t|")

print("### 10 Instance")
lc_bw_list = [
    0.1,
    0.15,
    0.2,
    0.25,
    0.3,
    0.35,
    0.4,
    0.45,
    0.5,
    0.55,
    0.6,
    0.61,
    0.625,
    0.65,
    0.7,
    0.75,
    0.76,
    0.77,
    0.79,
    0.8,
    0.81,
    0.83,
]
args = {"type": "dr-l", "lc_app": "memcached", "lc_bw": 0, "inst_num": 10}
res = round(args, lc_bw_list)
print("dr-l")
print("|lc_bw\t| p999\t| \n|---|---|---|")
for line in res:
    print(f"|{line['lc_bw']}\t|{line['p999']}\ts|")

args = {"type": "vessel", "lc_app": "memcached", "lc_bw": 0, "inst_num": 10}
res = round(args, lc_bw_list)
print("vessel")
print("|lc_bw\t|p999\t| \n|---|---|---|")
for line in res:
    print(f"|{line['lc_bw']}\t|{line['p999']}\t|")
