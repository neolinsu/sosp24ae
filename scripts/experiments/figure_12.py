from commons import run_item

url = "http://10.0.2.144:8080/figure/12"


def round(args):
    res = []
    for i in [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]:
        args["th"] = i
        res_item = run_item(url, args)
        res_item["th"] = i
        res.append(res_item)
    return res


print("- MBA")
args = {"type": "mba"}
mba_res = round(args)

print("- Linux")
args = {"type": "linux"}
linux_res = round(args)

print("- Vessel")
args = {"type": "vessel"}
vessel_res = round(args)

print("### Bandwidth")
print("| th | mba | linux | vessel | \n|---|---|---|---|")
for index in range(len(vessel_res)):
    print(
        f"|{vessel_res[index]['th']}\t|{mba_res[index]['bw']}\t|{linux_res[index]['bw']}\t|{vessel_res[index]['bw']}\t|"
    )
