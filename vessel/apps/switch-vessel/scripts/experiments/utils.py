import platform
import time
import pprint
import signal
import sys


def print_info(args):
    print(f"Env of this running scroipt:")
    print(f"Processor: {platform.processor()}")
    print(f"System: {platform.uname().system}-{platform.uname().release}")
    print(f"Pyhton: {platform.python_version()}")
    print(f"Args:\n---------------------")
    pprint.pprint(args)

def get_filename(file:str, suffix:str):
    time_str = time.strftime("%Y%m%d%H%M%S", time.localtime())
    return f"{file}-{time_str}{suffix}"

def reg_sigint_handler(func):
    signal.signal(signal.SIGINT, func)
