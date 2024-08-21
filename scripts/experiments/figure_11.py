from commons import run_item

url = "http://10.0.2.144:8080/figure/11"


def round(args):
    res = []
    for i in [16, 32, 64, 128, 256]:
        args["object_size"] = i
        res_item = run_item(url, args)
        res_item["object_size"] = i
        res_item["cache_miss_rate"] = res_item["l1-load-misses"] / res_item["l1-loads"]
        res.append(res_item)
    return res


print("- Caladan")
args = {"type": "caladan"}
caladan_res = round(args)

print("- Vessel")
args = {"type": "vessel"}
vessel_res = round(args)

print("### Cache Miss Rate")
print("|object_size|caladan|vessel| \n|---|---|---|")
for index in range(len(vessel_res)):
    object_size = vessel_res[index]["object_size"]
    caladan_cache_miss_rate = caladan_res[index]["cache_miss_rate"]
    vessel_cache_miss_rate = vessel_res[index]["cache_miss_rate"]
    print(f"|{object_size}\t|{caladan_cache_miss_rate}\t|{vessel_cache_miss_rate}\t|")

print("### Completion Time (Normalized)")
print("|object_size|caladan|vessel| \n|---|---|---|")
for index in range(len(vessel_res)):
    vessel_duration = vessel_res[index]["duration"]
    caladan_duration = caladan_res[index]["duration"]
    max_val = max(vessel_duration, caladan_duration)
    caladan_val = caladan_duration / max_val
    vessel_val = vessel_duration / max_val
    print(f"|{object_size}\t|{caladan_val}\t|{vessel_val}\t|")
