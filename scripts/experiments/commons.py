import requests
import time
import sys

global CNT

CNT = 0


def run_item(url, args):
    global CNT
    while True:
        response = requests.post(url, json=args)
        if response.status_code == 200:
            break
        elif response.status_code == 503:
            CNT += 1
            print(f"\rWaiting (it will take some time. cnt {CNT})", end="")
            sys.stdout.flush()
        else:
            print(f"Error {response.status_code}")
            exit(1)
        time.sleep(60)
        print("\r", end="")
    CNT = 0
    while True:
        response = requests.get(url)
        if response.status_code == 200:
            break
        else:
            CNT += 1
            print(f"\rWaiting (it will take some time. cnt {CNT})", end="")
            sys.stdout.flush()
        time.sleep(60)
        print("\r", end="")
    return response.json()
