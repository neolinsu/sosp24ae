from commons import run_item

url = "http://10.0.2.144:8080/figure/9"


def memcached_round(args):
    res = []
    for i in range(1, 20):
        args["lc_bw"] = i
        res_item = run_item(url, args)
        res_item["lc_bw"] = i
        res.append(res_item)
    args["lc_bw"] = 19.5
    res_item = run_item(url, args)
    res_item["lc_bw"] = 19.5
    res.append(res_item)
    args["lc_bw"] = 20
    res_item = run_item(url, args)
    res_item["lc_bw"] = 20
    res.append(res_item)
    args["lc_bw"] = 20.5
    res_item = run_item(url, args)
    res_item["lc_bw"] = 20.5
    res.append(res_item)
    return res


def silo_round(args):
    res = []
    lc_bws = [
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
        0.75,
        0.8,
        0.85,
    ]
    for i in lc_bws:
        args["lc_bw"] = i
        res_item = run_item(url, args)
        res_item["lc_bw"] = i
        res.append(res_item)
    return res


print("### Memcached-Linpack")

args = {"type": "caladan", "lc_app": "memcached", "be_app": "linpack", "lc_bw": 0}
res = memcached_round(args)
print("caladan")
print("|lc_bw\t|bw\t|p999\t| \n|---|---|---|")
for line in res:
    print(f"|{line['lc_bw']}\t|{line['bw']}\t|{line['p999']}\t|")

args = {"type": "vessel", "lc_app": "memcached", "be_app": "linpack", "lc_bw": 0}
res = memcached_round(args)
print("vessel")
print("|lc_bw\t|bw\t|p999\t| \n|---|---|---|")
for line in res:
    print(f"|{line['lc_bw']}\t|{line['bw']}\t|{line['p999']}\t|")

args = {"type": "dr-l", "lc_app": "memcached", "be_app": "linpack", "lc_bw": 0}
res = memcached_round(args)
print("dr-l")
print("|lc_bw\t|bw\t|p999\t| \n|---|---|---|")
for line in res:
    print(f"|{line['lc_bw']}\t|{line['bw']}\t|{line['p999']}\t|")

args = {"type": "dr-h", "lc_app": "memcached", "be_app": "linpack", "lc_bw": 0}
res = memcached_round(args)
print("dr-h")
print("|lc_bw\t|bw\t|p999\t| \n|---|---|---|")
for line in res:
    print(f"|{line['lc_bw']}\t|{line['bw']}\t|{line['p999']}\t|")

print("### Silo-Linpack")

args = {"type": "caladan", "lc_app": "silo", "be_app": "linpack", "lc_bw": 0}
res = silo_round(args)
print("caladan")
print("|lc_bw\t|bw\t|p999\t| \n|---|---|---|")
for line in res:
    print(f"|{line['lc_bw']}\t|{line['bw']}\t|{line['p999']}\t|")

args = {"type": "vessel", "lc_app": "silo", "be_app": "linpack", "lc_bw": 0}
res = silo_round(args)
print("vessel")
print("|lc_bw\t|bw\t|p999\t| \n|---|---|---|")
for line in res:
    print(f"|{line['lc_bw']}\t|{line['bw']}\t|{line['p999']}\t|")

args = {"type": "dr-l", "lc_app": "silo", "be_app": "linpack", "lc_bw": 0}
res = silo_round(args)
print("dr-l")
print("|lc_bw\t|bw\t|p999\t| \n|---|---|---|")
for line in res:
    print(f"|{line['lc_bw']}\t|{line['bw']}\t|{line['p999']}\t|")

args = {"type": "dr-h", "lc_app": "silo", "be_app": "linpack", "lc_bw": 0}
res = silo_round(args)
print("dr-h")
print("|lc_bw\t|bw\t|p999\t| \n|---|---|---|")
for line in res:
    print(f"|{line['lc_bw']}\t|{line['bw']}\t|{line['p999']}\t|")
