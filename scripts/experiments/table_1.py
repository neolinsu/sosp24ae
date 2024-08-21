from commons import run_item

url = "http://10.0.2.144:8080/table/1"


def round(args):
    return run_item(url, args)


print("- Linux (Caladan)")
args = {"type": "linux"}
linux_res = round(args)

print("- Vessel")
args = {"type": "vessel"}
vessel_res = round(args)

print("### Latency")
print("| lat | vessle | linux | \n|---|---|---|")
for key in ["mean", "p50", "p90", "p99", "p999"]:
    vessel_val = "0.4"
    print(f"|{key}\t|{vessel_res[key]/2100.0:.4f}\t|{linux_res[key]/2100.0:.4f}\t|")
