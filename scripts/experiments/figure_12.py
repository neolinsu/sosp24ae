from commons import run_item

url = "http://10.0.2.144:8080/figure/12"


def round_a(args, lc_bw_list):
    res = []
    for i in lc_bw_list:
        args["lc_bw"] = i
        args["op"] = "a"
        res_item = run_item(url, args)
        res_item["lc_bw"] = i
        res.append(res_item)
    return res


def round_b(args):
    res = []
    for i in [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]:
        args["th"] = i
        args["op"] = "b"
        res_item = run_item(url, args)
        res_item["th"] = i
        res.append(res_item)
    return res


print("(a)")
print("- Caladan")
args = {"type": "caladan", "op": "a"}
lc_bw_list = [0.3, 0.9, 2.1, 2.7, 3, 4.5, 6, 9]
caladan_res = round_a(args, lc_bw_list)
print("| lc_bw | bw | p999 | \n|---|---|---|")
for line in caladan_res:
    print(f"|{line['lc_bw']}\t|{line['bw']}\t|{line['p999']}\t|")

print("- Vessel")
args = {"type": "vessel", "op": "a"}
lc_bw_list = [0.3, 0.9, 2.1, 2.7, 3, 4.5, 6, 6.6, 9]
vessel_res = round_a(args, lc_bw_list)
print("| lc_bw | bw | p999 | \n|---|---|---|")
for line in vessel_res:
    print(f"|{line['lc_bw']}\t|{line['bw']}\t|{line['p999']}\t|")


print("(b)")
print("- MBA")
args = {"type": "mba"}
mba_res = round_b(args)

print("- Linux")
args = {"type": "linux"}
linux_res = round_b(args)

print("- Vessel")
args = {"type": "vessel"}
vessel_res = round_b(args)

print("### Bandwidth")
print("| th | mba | linux | vessel | \n|---|---|---|---|")
for index in range(len(vessel_res)):
    print(
        f"|{vessel_res[index]['th']}\t|{mba_res[index]['bw']}\t|{linux_res[index]['bw']}\t|{vessel_res[index]['bw']}\t|"
    )
