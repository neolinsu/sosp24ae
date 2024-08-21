for i in range (10,110, 10):
    f = open(f"output_gsw/linux_memb_duration_{i}.txt")
    idx = 0
    ans = [0] * 2
    for line in f:
        ans[idx] = int(line.split()[0])
        idx += 1
    print( f"{(ans[1] - ans[0]) / (2100 * 1000 * 1000)}")

